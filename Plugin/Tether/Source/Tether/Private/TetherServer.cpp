#include "TetherServer.h"
#include "TetherCancellableWork.h"
#include "TetherEditorHealth.h"
#include "TetherEndpointIdentity.h"
#include "TetherExactRequestDispatcher.h"
#include "TetherProtocol.h"
#include "IPythonScriptPlugin.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "SocketSubsystem.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "TetherCallLog.h"

DEFINE_LOG_CATEGORY_STATIC(LogTether, Log, All);

namespace TetherLimits
{
	// Max request JSON payload. 10 MB is generous for human-authored scripts;
	// the upper bound exists mainly to stop a malicious/buggy client from
	// triggering an OOM in the editor via blind SetNumUninitialized.
	constexpr int32 MaxRequestBytes = 10 * 1024 * 1024;
}

/**
 * exec queue 的共享 control block；worker 等待它，GameThread ticker 尝试消费它。
 * Shared exec-queue control block waited by a worker and claimed by the GameThread ticker.
 */
struct FTetherServer::FPendingExec final
{
	FPendingExec(TFunction<FExecResult()>&& InBody, FString InRequestId)
		: Work(MoveTemp(InBody))
		, RequestId(MoveTemp(InRequestId))
	{
	}

	TTetherCancellableWork<FExecResult> Work;
	FString RequestId;
};

/**
 * 每连接专用 worker 线程（N-F4）。此前每个连接占一个 FFunctionGraphTask 的
 * AnyBackgroundThreadNormalTask 线程（HandleClient 最长阻塞 300s），16 个连接
 * 就能钉死整个 TG 后台线程池。专用 FRunnableThread 线程与阻塞 IO 天然匹配：
 * 互不占用任务图，退出由 FTetherServer::bShutdownRequested 驱动。
 *
 * 生命周期（收割者模式）：worker 的 Run() 返回前把自己从注册表移除、
 * Decrement ActiveClients、把自己推进 DeadWorkers MPSC 队列；GameThread 的
 * ticker（或 Stop）作为唯一收割者：Kill(true) 联接已退出的线程（瞬时）、
 * Close OS 句柄、从 ThreadManager 摘除，然后 delete worker（析构销毁 socket）。
 * worker 自己绝不 Kill 自己的线程句柄——Kill(true) 里的
 * WaitForSingleObject(自身) 是死锁。对象销毁与 socket 销毁都在收割者的
 * 线程上下文，注册表里的指针只在 gate 锁内访问，不存在悬垂窗口。
 *
 * Per-connection dedicated worker thread (N-F4). Previously each connection
 * pinned an AnyBackgroundThreadNormalTask task-graph thread for the (up to
 * 300 s) lifetime of HandleClient, so 16 connections could starve the whole
 * background thread pool. A dedicated FRunnableThread matches blocking IO
 * naturally: task-graph threads stay free, and shutdown is driven by
 * FTetherServer::bShutdownRequested.
 *
 * Lifecycle (reaper model): Run()'s epilogue removes the worker from the
 * registry, decrements ActiveClients, and pushes itself onto the DeadWorkers
 * MPSC queue; the GameThread ticker (or Stop) is the sole reaper — Kill(true)
 * joins the already-returned thread (instant), closes the OS handle,
 * deregisters from the ThreadManager, then deletes the worker (whose
 * destructor destroys the socket). A worker never kills its own thread
 * handle: Kill(true)'s WaitForSingleObject(self) is a deadlock. Object and
 * socket destruction both happen on the reaper's thread context, and
 * registry pointers are only touched under the gate lock — no dangling
 * window exists.
 */
class FTetherClientWorker final : public FRunnable
{
public:
	FTetherClientWorker(FTetherServer* InServer, FSocket* InClientSocket, const FString& InEndpointStr)
		: Server(InServer)
		, ClientSocket(InClientSocket)
		, EndpointStr(InEndpointStr)
	{
	}

	~FTetherClientWorker()
	{
		// Ownership: the socket is closed and destroyed here, and ONLY here.
		// Stop() never force-closes a socket a worker is blocked on (Windows
		// documents cross-thread closesocket on a blocked handle as
		// unreliable). If the thread failed to start, Run() never touched the
		// socket, so the destructor is still the sole cleanup point.
		if (ClientSocket != nullptr)
		{
			if (ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
			{
				Subsystem->DestroySocket(ClientSocket);
			}
			ClientSocket = nullptr;
		}
	}

	// FRunnable
	virtual uint32 Run() override
	{
		Server->HandleClient(ClientSocket, EndpointStr);
		// Retire onto the graveyard queue — see the comment above Run().
		// NOTE: Run must NOT touch Thread (its own handle) in any way.
		// FRunnableThreadWin::Run() calls Runnable->Exit() right after
		// Runnable->Run() returns, and a Kill(true) issued here would try to
		// join the CURRENT thread — a self-deadlock that wedges the worker
		// before it can decrement ActiveClients.
		{
			FScopeLock Lock(&Server->ClientWorkerThreadsLock);
			Server->ClientWorkerThreads.Remove(this);
		}
		Server->ActiveClients.Decrement();
		Server->DeadWorkers.Enqueue(this);
		return 0;
	}

	// worker 自持线程句柄，但从不由 worker 自己 Kill/销毁——自杀式
	// Kill(true) 会等待自己（死锁）。收割一律在 GameThread（ticker 或
	// Stop）进行：此刻 Run 已返回、线程即将退出或已退出，Kill(true) 的
	// WaitForSingleObject 是瞬时通过或最坏等到线程退出为止，绝不长阻塞。
	// The worker holds its thread handle but NEVER kills it itself — a
	// self-issued Kill(true) waits for the current thread (deadlock). Reaping
	// always happens on the GameThread (ticker or Stop): Run has already
	// returned there, so Kill(true)'s WaitForSingleObject passes instantly or
	// at worst waits for the thread's final exit — never a long block.
	TUniquePtr<FRunnableThread> Thread;

private:
	FTetherServer* Server;
	FSocket* ClientSocket;
	FString EndpointStr;
};

// Modal-dialog inspection and actions deliberately live outside the normal
// Python exec queue. A Slate modal loop does not tick FTSTicker, so an exec
// that opened a dialog cannot make forward progress until somebody resolves
// it. The nested Slate loop does keep pumping GameThread task-graph work,
// which makes AsyncTask(GameThread) a reliable side channel for observing and
// interacting with the active modal window.
namespace TetherModal
{
	struct FModalButton
	{
		int32 Id = INDEX_NONE;
		FString Label;
		TSharedPtr<SButton> Widget;
	};

	struct FModalInput
	{
		enum class EKind : uint8
		{
			SingleLine,
			MultiLine,
		};

		int32 Id = INDEX_NONE;
		EKind Kind = EKind::SingleLine;
		TSharedPtr<SEditableText> SingleLineWidget;
		TSharedPtr<SMultiLineEditableText> MultiLineWidget;

		FString GetValue() const
		{
			return Kind == EKind::SingleLine
				? SingleLineWidget->GetText().ToString()
				: MultiLineWidget->GetText().ToString();
		}

		bool IsReadOnly() const
		{
			return Kind == EKind::SingleLine
				? SingleLineWidget->IsTextReadOnly()
				: MultiLineWidget->IsTextReadOnly();
		}

		bool IsPassword() const
		{
			return Kind == EKind::SingleLine && SingleLineWidget->IsTextPassword();
		}

		bool IsEnabled() const
		{
			return Kind == EKind::SingleLine
				? SingleLineWidget->IsEnabled()
				: MultiLineWidget->IsEnabled();
		}

		void SetValue(const FString& Value) const
		{
			if (Kind == EKind::SingleLine)
			{
				SingleLineWidget->SetText(FText::FromString(Value));
			}
			else
			{
				MultiLineWidget->SetText(FText::FromString(Value));
			}
		}
	};

	struct FModalCheckBox
	{
		int32 Id = INDEX_NONE;
		FString Label;
		TSharedPtr<SCheckBox> Widget;
	};

	struct FModalSnapshot
	{
		bool bPresent = false;
		uint64 WindowGeneration = 0;
		FString SnapshotId;
		FString Title;
		TArray<FString> BodyText;
		TArray<FModalButton> Buttons;
		TArray<FModalInput> Inputs;
		TArray<FModalCheckBox> CheckBoxes;
		TSharedPtr<SWindow> Window;
	};

	uint64 TrackWindowGeneration(const TSharedPtr<SWindow>& Window)
	{
		// Content alone is not enough for stale-action protection: two
		// consecutive dialogs can have identical title/body/buttons. Give each
		// observed SWindow instance a process-local generation and include it in
		// the snapshot hash. This state is touched on the GameThread only.
		static TWeakPtr<SWindow> LastWindow;
		static uint64 Generation = 0;
		if (!Window.IsValid())
		{
			LastWindow.Reset();
			return 0;
		}
		if (LastWindow.Pin().Get() != Window.Get())
		{
			++Generation;
			LastWindow = Window;
		}
		return Generation;
	}

	bool IsExactWidgetType(const TSharedRef<SWidget>& Widget, const FName ExpectedType)
	{
		// SWidget::GetType() is available across Tether's full UE
		// 5.3+ support matrix. The newer GetWidgetClass metadata API is not.
		return Widget->GetType() == ExpectedType;
	}

	void AppendNonEmptyText(const FText& Text, TArray<FString>& Out)
	{
		FString Value = Text.ToString();
		Value.TrimStartAndEndInline();
		if (!Value.IsEmpty())
		{
			Out.AddUnique(MoveTemp(Value));
		}
	}

	void CollectDisplayText(const TSharedRef<SWidget>& Widget, TArray<FString>& Out)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		if (IsExactWidgetType(Widget, TEXT("STextBlock")))
		{
			AppendNonEmptyText(StaticCastSharedRef<STextBlock>(Widget)->GetText(), Out);
		}
		else if (IsExactWidgetType(Widget, TEXT("SRichTextBlock")))
		{
			AppendNonEmptyText(StaticCastSharedRef<SRichTextBlock>(Widget)->GetText(), Out);
		}

		FChildren* Children = Widget->GetChildren();
		if (Children == nullptr)
		{
			return;
		}
		for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			CollectDisplayText(Children->GetChildAt(ChildIndex), Out);
		}
	}

	FString GetWidgetLabel(const TSharedRef<SWidget>& Widget)
	{
		TArray<FString> Parts;
		CollectDisplayText(Widget, Parts);
		return FString::Join(Parts, TEXT(" "));
	}

	void VisitWidgetTree(const TSharedRef<SWidget>& Widget, FModalSnapshot& Out, bool bInsideButton)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		const bool bIsButton = IsExactWidgetType(Widget, TEXT("SButton"));

		if (bIsButton)
		{
			const FString Label = GetWidgetLabel(Widget);
			// SWindow's title bar is also composed from SButtons (close,
			// maximize, etc.) but those icon-only controls are not semantic
			// dialog choices. Excluding unlabelled buttons keeps agent actions
			// constrained to explicit choices such as OK / Cancel / Retry.
			if (!Label.IsEmpty())
			{
				FModalButton& Button = Out.Buttons.AddDefaulted_GetRef();
				Button.Id = Out.Buttons.Num() - 1;
				Button.Label = Label;
				Button.Widget = StaticCastSharedRef<SButton>(Widget);
			}
		}
		else if (!bInsideButton && IsExactWidgetType(Widget, TEXT("STextBlock")))
		{
			AppendNonEmptyText(StaticCastSharedRef<STextBlock>(Widget)->GetText(), Out.BodyText);
		}
		else if (!bInsideButton && IsExactWidgetType(Widget, TEXT("SRichTextBlock")))
		{
			AppendNonEmptyText(StaticCastSharedRef<SRichTextBlock>(Widget)->GetText(), Out.BodyText);
		}

		if (IsExactWidgetType(Widget, TEXT("SEditableText")))
		{
			FModalInput& Input = Out.Inputs.AddDefaulted_GetRef();
			Input.Id = Out.Inputs.Num() - 1;
			Input.Kind = FModalInput::EKind::SingleLine;
			Input.SingleLineWidget = StaticCastSharedRef<SEditableText>(Widget);
		}
		else if (IsExactWidgetType(Widget, TEXT("SMultiLineEditableText")))
		{
			FModalInput& Input = Out.Inputs.AddDefaulted_GetRef();
			Input.Id = Out.Inputs.Num() - 1;
			Input.Kind = FModalInput::EKind::MultiLine;
			Input.MultiLineWidget = StaticCastSharedRef<SMultiLineEditableText>(Widget);
		}

		if (IsExactWidgetType(Widget, TEXT("SCheckBox")))
		{
			FModalCheckBox& CheckBox = Out.CheckBoxes.AddDefaulted_GetRef();
			CheckBox.Id = Out.CheckBoxes.Num() - 1;
			CheckBox.Label = GetWidgetLabel(Widget);
			CheckBox.Widget = StaticCastSharedRef<SCheckBox>(Widget);
		}

		FChildren* Children = Widget->GetChildren();
		if (Children == nullptr)
		{
			return;
		}
		for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			VisitWidgetTree(Children->GetChildAt(ChildIndex), Out, bInsideButton || bIsButton);
		}
	}

	FString CheckStateToString(ECheckBoxState State)
	{
		switch (State)
		{
		case ECheckBoxState::Checked:
			return TEXT("checked");
		case ECheckBoxState::Undetermined:
			return TEXT("undetermined");
		default:
			return TEXT("unchecked");
		}
	}

	FModalSnapshot CaptureSnapshot()
	{
		FModalSnapshot Out;
		if (!FSlateApplication::IsInitialized())
		{
			return Out;
		}

		Out.Window = FSlateApplication::Get().GetActiveModalWindow();
		if (!Out.Window.IsValid())
		{
			TrackWindowGeneration(nullptr);
			return Out;
		}

		Out.bPresent = true;
		Out.WindowGeneration = TrackWindowGeneration(Out.Window);
		Out.Title = Out.Window->GetTitle().ToString();
		VisitWidgetTree(Out.Window.ToSharedRef(), Out, false);
		// Some message-dialog layouts repeat the window title in their child
		// tree. Keep title and body separate in the wire format.
		Out.BodyText.Remove(Out.Title);

		FString Fingerprint = FString::Printf(TEXT("W:%llu\x1e"), Out.WindowGeneration)
			+ Out.Title + TEXT("\x1e") + FString::Join(Out.BodyText, TEXT("\x1f"));
		for (const FModalButton& Button : Out.Buttons)
		{
			Fingerprint += FString::Printf(TEXT("\x1eB:%d:%s:%d"), Button.Id, *Button.Label,
				Button.Widget->IsEnabled() ? 1 : 0);
		}
		for (const FModalInput& Input : Out.Inputs)
		{
			Fingerprint += FString::Printf(TEXT("\x1eI:%d:%s:%s"), Input.Id,
				Input.IsPassword() ? TEXT("password") : TEXT("plain"),
				Input.IsPassword() ? TEXT("<redacted>") : *Input.GetValue());
		}
		for (const FModalCheckBox& CheckBox : Out.CheckBoxes)
		{
			Fingerprint += FString::Printf(TEXT("\x1eC:%d:%s:%s"), CheckBox.Id, *CheckBox.Label,
				*CheckStateToString(CheckBox.Widget->GetCheckedState()));
		}

		FTCHARToUTF8 Utf8(*Fingerprint);
		FSHAHash Hash;
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
		Out.SnapshotId = Hash.ToString().Left(16).ToLower();
		return Out;
	}

	TSharedPtr<FJsonObject> SnapshotToJson(const FModalSnapshot& Snapshot)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("present"), Snapshot.bPresent);
		Json->SetStringField(TEXT("snapshot_id"), Snapshot.SnapshotId);
		Json->SetStringField(TEXT("title"), Snapshot.Title);
		Json->SetStringField(TEXT("body"), FString::Join(Snapshot.BodyText, TEXT("\n")));

		TArray<TSharedPtr<FJsonValue>> TextValues;
		for (const FString& Text : Snapshot.BodyText)
		{
			TextValues.Add(MakeShared<FJsonValueString>(Text));
		}
		Json->SetArrayField(TEXT("text"), MoveTemp(TextValues));

		TArray<TSharedPtr<FJsonValue>> ButtonValues;
		for (const FModalButton& Button : Snapshot.Buttons)
		{
			TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), Button.Id);
			Item->SetStringField(TEXT("label"), Button.Label);
			Item->SetBoolField(TEXT("enabled"), Button.Widget->IsEnabled());
			Item->SetBoolField(TEXT("visible"), Button.Widget->GetVisibility().IsVisible());
			ButtonValues.Add(MakeShared<FJsonValueObject>(MoveTemp(Item)));
		}
		Json->SetArrayField(TEXT("buttons"), MoveTemp(ButtonValues));

		TArray<TSharedPtr<FJsonValue>> InputValues;
		for (const FModalInput& Input : Snapshot.Inputs)
		{
			TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), Input.Id);
			Item->SetStringField(TEXT("kind"), Input.Kind == FModalInput::EKind::SingleLine
				? TEXT("single_line") : TEXT("multi_line"));
			Item->SetBoolField(TEXT("password"), Input.IsPassword());
			Item->SetBoolField(TEXT("read_only"), Input.IsReadOnly());
			Item->SetBoolField(TEXT("enabled"), Input.IsEnabled());
			if (Input.IsPassword())
			{
				Item->SetStringField(TEXT("value"), TEXT("<redacted>"));
			}
			else
			{
				Item->SetStringField(TEXT("value"), Input.GetValue());
			}
			InputValues.Add(MakeShared<FJsonValueObject>(MoveTemp(Item)));
		}
		Json->SetArrayField(TEXT("inputs"), MoveTemp(InputValues));

		TArray<TSharedPtr<FJsonValue>> CheckBoxValues;
		for (const FModalCheckBox& CheckBox : Snapshot.CheckBoxes)
		{
			TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), CheckBox.Id);
			Item->SetStringField(TEXT("label"), CheckBox.Label);
			Item->SetStringField(TEXT("state"), CheckStateToString(CheckBox.Widget->GetCheckedState()));
			Item->SetBoolField(TEXT("enabled"), CheckBox.Widget->IsEnabled());
			Item->SetBoolField(TEXT("visible"), CheckBox.Widget->GetVisibility().IsVisible());
			CheckBoxValues.Add(MakeShared<FJsonValueObject>(MoveTemp(Item)));
		}
		Json->SetArrayField(TEXT("checkboxes"), MoveTemp(CheckBoxValues));
		return Json;
	}

	TSharedPtr<FJsonObject> MakeResult(bool bSuccess, const FString& Output, const FString& Error,
		const FModalSnapshot& Snapshot)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetStringField(TEXT("output"), Output);
		Result->SetStringField(TEXT("error"), Error);
		Result->SetBoolField(TEXT("ready"), true);
		Result->SetObjectField(TEXT("modal"), SnapshotToJson(Snapshot));
		return Result;
	}

	TSharedPtr<FJsonObject> GetStatus()
	{
		const FModalSnapshot Snapshot = CaptureSnapshot();
		return MakeResult(true, Snapshot.bPresent ? TEXT("modal present") : TEXT("no active modal"),
			TEXT(""), Snapshot);
	}

	TSharedPtr<FJsonObject> PerformAction(const FString& ExpectedSnapshot, const FString& Action,
		int32 ControlId, const FString& Value, bool bChecked, bool bHasChecked)
	{
		FModalSnapshot Snapshot = CaptureSnapshot();
		if (!Snapshot.bPresent)
		{
			return MakeResult(false, TEXT(""), TEXT("no active modal"), Snapshot);
		}
		if (ExpectedSnapshot.IsEmpty())
		{
			return MakeResult(false, TEXT(""), TEXT("missing 'snapshot' field; inspect the modal first"), Snapshot);
		}
		if (ExpectedSnapshot != Snapshot.SnapshotId)
		{
			return MakeResult(false, TEXT(""),
				FString::Printf(TEXT("stale modal snapshot: expected %s, current %s; inspect again before acting"),
					*ExpectedSnapshot, *Snapshot.SnapshotId), Snapshot);
		}

		if (Action == TEXT("click_button"))
		{
			if (!Snapshot.Buttons.IsValidIndex(ControlId))
			{
				return MakeResult(false, TEXT(""), TEXT("button id is out of range"), Snapshot);
			}
			const FModalButton& Button = Snapshot.Buttons[ControlId];
			if (!Button.Widget->IsEnabled() || !Button.Widget->GetVisibility().IsVisible())
			{
				return MakeResult(false, TEXT(""), TEXT("button is disabled or hidden"), Snapshot);
			}

			// Build the response before invoking the delegate: the click can close
			// the window and unwind the nested modal loop immediately.
			TSharedPtr<FJsonObject> Result = MakeResult(true,
				FString::Printf(TEXT("clicked button %d (%s)"), ControlId, *Button.Label), TEXT(""), Snapshot);
			Button.Widget->SimulateClick();
			return Result;
		}

		if (Action == TEXT("set_text"))
		{
			if (!Snapshot.Inputs.IsValidIndex(ControlId))
			{
				return MakeResult(false, TEXT(""), TEXT("input id is out of range"), Snapshot);
			}
			const FModalInput& Input = Snapshot.Inputs[ControlId];
			if (!Input.IsEnabled() || Input.IsReadOnly())
			{
				return MakeResult(false, TEXT(""), TEXT("input is disabled or read-only"), Snapshot);
			}
			const bool bPassword = Input.IsPassword();
			Input.SetValue(Value);
			Snapshot = CaptureSnapshot();
			if (!bPassword
				&& (!Snapshot.Inputs.IsValidIndex(ControlId)
					|| Snapshot.Inputs[ControlId].GetValue() != Value))
			{
				return MakeResult(false, TEXT(""), TEXT("input did not accept the requested value"), Snapshot);
			}
			return MakeResult(true, FString::Printf(TEXT("updated input %d"), ControlId), TEXT(""), Snapshot);
		}

		if (Action == TEXT("set_checkbox"))
		{
			if (!bHasChecked)
			{
				return MakeResult(false, TEXT(""), TEXT("missing boolean 'checked' field"), Snapshot);
			}
			if (!Snapshot.CheckBoxes.IsValidIndex(ControlId))
			{
				return MakeResult(false, TEXT(""), TEXT("checkbox id is out of range"), Snapshot);
			}
			const FModalCheckBox& CheckBox = Snapshot.CheckBoxes[ControlId];
			if (!CheckBox.Widget->IsEnabled() || !CheckBox.Widget->GetVisibility().IsVisible())
			{
				return MakeResult(false, TEXT(""), TEXT("checkbox is disabled or hidden"), Snapshot);
			}
			const bool bCurrentlyChecked = CheckBox.Widget->GetCheckedState() == ECheckBoxState::Checked;
			if (bCurrentlyChecked != bChecked)
			{
				CheckBox.Widget->ToggleCheckedState();
			}
			Snapshot = CaptureSnapshot();
			if (!Snapshot.CheckBoxes.IsValidIndex(ControlId)
				|| (Snapshot.CheckBoxes[ControlId].Widget->GetCheckedState() == ECheckBoxState::Checked) != bChecked)
			{
				return MakeResult(false, TEXT(""), TEXT("checkbox did not accept the requested state"), Snapshot);
			}
			return MakeResult(true, FString::Printf(TEXT("updated checkbox %d"), ControlId), TEXT(""), Snapshot);
		}

		return MakeResult(false, TEXT(""), TEXT("unsupported modal action"), Snapshot);
	}

	/** GameThread wait 的权威终态。 / Authoritative outcome of a bounded GameThread wait. */
	enum class EGameThreadWorkWaitResult : uint8
	{
		Completed,
		CancelledBeforeStart,
		AlreadyRunning,
	};

	EGameThreadWorkWaitResult RunOnGameThread(
		TFunction<TSharedPtr<FJsonObject>()>&& Body,
		TSharedPtr<FJsonObject>& OutResult,
		float TimeoutSeconds = 3.0f)
	{
		using FModalWork = TTetherCancellableWork<TSharedPtr<FJsonObject>>;
		TSharedPtr<FModalWork, ESPMode::ThreadSafe> Pending =
			MakeShared<FModalWork, ESPMode::ThreadSafe>(MoveTemp(Body));

		AsyncTask(ENamedThreads::GameThread, [Pending]()
		{
			// timeout 取消成功后，这个晚到 task 只会失败 claim，绝不执行 Slate body。
			// After timeout cancellation wins, this late task can only fail its claim and never runs the Slate body.
			Pending->TryExecute();
		});

		if (Pending->WaitFor(FTimespan::FromSeconds(TimeoutSeconds)))
		{
			OutResult = Pending->GetResult();
			return EGameThreadWorkWaitResult::Completed;
		}

		ETetherWorkState ObservedState = ETetherWorkState::Queued;
		TSharedPtr<FJsonObject> CancelledResult;
		if (Pending->TryCancel(MoveTemp(CancelledResult), ObservedState))
		{
			return EGameThreadWorkWaitResult::CancelledBeforeStart;
		}
		if (ObservedState == ETetherWorkState::Completed)
		{
			// completion 与 deadline 同时发生时返回真实结果，不伪造 timeout。
			// If completion races the deadline, return the real result instead of fabricating a timeout.
			OutResult = Pending->GetResult();
			return EGameThreadWorkWaitResult::Completed;
		}
		if (ObservedState == ETetherWorkState::Cancelled)
		{
			return EGameThreadWorkWaitResult::CancelledBeforeStart;
		}
		return EGameThreadWorkWaitResult::AlreadyRunning;
	}
}

void FTetherServer::TickUpdateSlateHealth(float /*DeltaTime*/)
{
	RefreshCachedSlateHealth();
}

void FTetherServer::RefreshCachedSlateHealth()
{
	check(IsInGameThread());
	const TetherModal::FModalSnapshot Snapshot = TetherModal::CaptureSnapshot();

	FTetherModalAttention Attention;
	Attention.bPresent = Snapshot.bPresent;
	Attention.bDebugging = FSlateApplication::Get().InKismetDebuggingMode();
	Attention.WindowGeneration = Snapshot.WindowGeneration;
	Attention.SnapshotId = Snapshot.SnapshotId;
	Attention.Title = Snapshot.Title;
	Attention.ButtonCount = Snapshot.Buttons.Num();
	Attention.InputCount = Snapshot.Inputs.Num();
	Attention.CheckBoxCount = Snapshot.CheckBoxes.Num();
	EditorHealthCache->RecordSlateTick(Attention);
}

// ─────────────────────────────────────────────────────────────
// Server lifecycle
// ─────────────────────────────────────────────────────────────

FTetherServer::FTetherServer()
	: WorkAdmission(MakeUnique<FTetherWorkAdmissionGate>())
	, EditorHealthCache(MakeUnique<FTetherEditorHealthCache>())
{
}

FTetherServer::~FTetherServer()
{
	Stop();
}

bool FTetherServer::Start(int32 Port)
{
	FStartConfig Cfg;
	Cfg.Port = Port;
	return Start(Cfg);
}

bool FTetherServer::Start(const FStartConfig& Config)
{
	if (bIsRunning)
	{
		return true;
	}

	EditorHealthCache->Reset();

	// Safety gate: binding to a non-localhost interface exposes Python exec to
	// the LAN. Refuse if the caller didn't supply a token.
	const bool bIsLoopback =
		(Config.BindAddress == FIPv4Address(127, 0, 0, 1)) ||
		(Config.BindAddress == FIPv4Address::InternalLoopback);
	if (!bIsLoopback && Config.Token.IsEmpty())
	{
		UE_LOG(LogTether, Error,
			TEXT("Refusing to bind %s:%d without a token — set -TetherToken=... ")
			TEXT("or use -TetherBind=127.0.0.1"),
			*Config.BindAddress.ToString(), Config.Port);
		return false;
	}

	BindAddressStr = Config.BindAddress.ToString();
	Token = Config.Token;

	const FIPv4Endpoint Endpoint(Config.BindAddress, Config.Port);

	// 100ms poll (vs default 1s) collapses the accept-race window that produced
	// intermittent WSAECONNABORTED 10053 on clients. bInReusable=true lets Start()
	// reclaim a TIME_WAIT socket after a crash/quick-restart instead of failing
	// with "address in use". See docs/server-stability-plan.md #7.
	Listener = MakeUnique<FTcpListener>(
		Endpoint,
		FTimespan::FromMilliseconds(100),
		true /* bInReusable */
	);

	if (!Listener.IsValid() || !Listener->IsActive())
	{
		UE_LOG(LogTether, Error, TEXT("Failed to create TCP listener on %s:%d"),
			*BindAddressStr, Config.Port);
		Listener.Reset();
		return false;
	}

	// When Port=0 the kernel picks a free ephemeral port — read it back so
	// clients and the discovery responder know where to connect.
	ListenPort = Config.Port;
	if (Listener->GetSocket() != nullptr)
	{
		TSharedRef<FInternetAddr> LocalAddr =
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		Listener->GetSocket()->GetAddress(*LocalAddr);
		const int32 ResolvedPort = LocalAddr->GetPort();
		if (ResolvedPort > 0)
		{
			ListenPort = ResolvedPort;
		}
	}

	// Bind the accept delegate as soon as the listener is confirmed alive
	// (see below): FTcpListener starts its accept thread in the constructor,
	// and an unbound delegate makes it silently Close+Destroy accepted sockets
	// (TcpListener.h) — binding before the gate opens means an early connection
	// is answered with an explicit shutting_down frame by OnConnectionAccepted
	// instead of being dropped without a trace (the last remnant of the N-F10
	// startup window).
	Listener->OnConnectionAccepted().BindRaw(this, &FTetherServer::OnConnectionAccepted);

	// 身份只在 listener 成功建立后生成；每次成功 Start 都得到新的 UUID。
	// Identity is generated only after the listener succeeds; every successful Start receives a fresh UUID.
	const FTetherEndpointIdentity Identity = FTetherEndpointIdentity::Create();
	InstanceId = Identity.InstanceId;
	ProjectPath = Identity.ProjectPath;
	ProcessId = Identity.ProcessId;

	// N-F10①：bIsRunning 与 admission gate 在 listener 存活后立即开启（仍在
	// identity/ticker/delegate 注册之前）。窗口期内进入的连接不再被静默丢弃：
	// exec 请求由 bEditorReady gate 拒绝（有错误帧），ping/status 等只读命令
	// 可以直接得到响应。窗口期入队的 exec 在 ticker 注册后被正常消费，或被
	// Stop 经取消状态机拒绝。
	// N-F10①: flip bIsRunning and open the admission gate right after the
	// listener is alive (still before identity/ticker/delegate registration).
	// Connections accepted in the window are no longer dropped silently: exec
	// requests are rejected by the bEditorReady gate (with an error frame),
	// read-only commands answer immediately, and execs enqueued before the
	// ticker exists are consumed once it registers — or rejected by Stop
	// through the cancellation state machine.
	bIsRunning = true;
	WorkAdmission->Open();

	// Register the GameThread ticker that drains the exec queue.
	// Using FTSTicker instead of AsyncTask(GameThread) prevents reentrancy:
	// ticker callbacks fire only from FEngineLoop::Tick, not from TaskGraph
	// pumps triggered inside user scripts (asset loads, blueprint compiles, etc.).
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FTetherServer::TickConsumeQueue),
		0.0f /* tick every frame */
	);

	// Slate 模态循环会暂停 FTSTicker，但仍广播 Slate tick；在所属线程缓存摘要，
	// TCP worker 只读取普通值，绝不访问 SWindow/SWidget。
	// Slate modal loops pause FTSTicker but still broadcast Slate ticks; cache the
	// summary on the owning thread so TCP workers never access SWindow/SWidget.
	if (FSlateApplication::IsInitialized())
	{
		SlatePreTickHandle = FSlateApplication::Get().OnPreTick().AddRaw(
			this, &FTetherServer::TickUpdateSlateHealth);
		RefreshCachedSlateHealth();
	}

	// PIE transition guard (item #11). The transition *window* is:
	//   BeginPIE  ─────────→  PostPIEStarted   (editor subsystems torn
	//       [unsafe to exec]                   down and rebuilt here)
	//   PrePIEEnded  ──────→  EndPIE           (shutdown sequence —
	//       [unsafe again]                     same teardown/rebuild)
	// Between PostPIEStarted and PrePIEEnded PIE is running stably and
	// execs are safe. An earlier version used only BeginPIE/EndPIE which
	// kept the flag True for the entire PIE session and blocked agent
	// observation calls.
	PieBeginHandle = FEditorDelegates::BeginPIE.AddLambda([this](const bool /*bIsSimulating*/)
	{
		bPieTransitionActive = true;
	});
	PiePostStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda([this](const bool /*bIsSimulating*/)
	{
		bPieTransitionActive = false;
	});
	PiePreEndedHandle = FEditorDelegates::PrePIEEnded.AddLambda([this](const bool /*bIsSimulating*/)
	{
		bPieTransitionActive = true;
	});
	PieEndHandle = FEditorDelegates::EndPIE.AddLambda([this](const bool /*bIsSimulating*/)
	{
		bPieTransitionActive = false;
	});

	// bIsRunning / WorkAdmission 已经在 listener 创建成功后置位（N-F10①）。
	// 此处保留 idempotent 收尾日志；早退路径（初始化中途失败）由 bIsRunning=false 兜底。
	// bIsRunning / WorkAdmission were flipped right after the listener came up (N-F10①).
	// Only the summary log remains here; early-failure paths are covered by bIsRunning=false.
	UE_LOG(LogTether, Log, TEXT("Listening on %s:%d%s"),
		*BindAddressStr, ListenPort,
		HasToken() ? TEXT(" (token auth enforced)") : TEXT(""));
	return true;
}

void FTetherServer::Stop()
{
	if (!bIsRunning && !WorkAdmission->IsOpen() && !bShutdownRequested)
	{
		return;
	}
	checkf(IsInGameThread(), TEXT("Tether Server must stop on the GameThread"));

	// 1. 置位取消标志：RecvAll/SendAll 在每轮 50ms Wait 之前检查并立刻退出，
	//    worker 线程随后收尾并独占销毁自己的 socket。绝不跨线程 close 一个
	//    worker 正阻塞的 socket（Windows 文档化 UB，N-F1）。
	// Set the cancellation flag first: RecvAll/SendAll check it before every
	// 50 ms Wait and bail immediately, after which each worker thread unwinds
	// and exclusively destroys its own socket. Stop() never force-closes a
	// socket a worker is blocked on (documented UB on Windows, N-F1).
	bShutdownRequested = true;

	// 2. 先原子关闭 work admission；Close 返回后，accept/enqueue callback 均已离开，
	//    gate 持锁期间创建的 worker 线程也已全部进入本地的等待快照。
	// Close work admission atomically; after Close returns, accept/enqueue callbacks have
	// left, and every worker thread registered under the gate lock is in the local snapshot.
	TArray<FTetherClientWorker*> WorkersToWait;
	{
		WorkAdmission->Close();
		FScopeLock Lock(&ClientWorkerThreadsLock);
		WorkersToWait = ClientWorkerThreads.Array();
	}
	bIsRunning = false;

	// 3. Stop accepting new connections.
	if (Listener.IsValid())
	{
		Listener.Reset();
	}

	// 4. Unregister GameThread ticker and editor delegates (items #11 #12).
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	if (SlatePreTickHandle.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().OnPreTick().Remove(SlatePreTickHandle);
		}
		SlatePreTickHandle.Reset();
	}
	if (PieBeginHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(PieBeginHandle);
		PieBeginHandle.Reset();
	}
	if (PiePostStartedHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(PiePostStartedHandle);
		PiePostStartedHandle.Reset();
	}
	if (PiePreEndedHandle.IsValid())
	{
		FEditorDelegates::PrePIEEnded.Remove(PiePreEndedHandle);
		PiePreEndedHandle.Reset();
	}
	if (PieEndHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(PieEndHandle);
		PieEndHandle.Reset();
	}

	// 5. 通过共同状态机取消 queued exec；gate 已关闭，所以 drain 后不能再有新 item。
	// Cancel queued execs through the shared state machine; the closed gate prevents post-drain admission.
	TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending;
	while (ExecQueue.Dequeue(Pending) && Pending.IsValid())
	{
		FExecResult ShutdownResult;
		ShutdownResult.bSuccess = false;
		ShutdownResult.Error = TEXT("server shutting down");
		ETetherWorkState ObservedState = ETetherWorkState::Queued;
		Pending->Work.TryCancel(MoveTemp(ShutdownResult), ObservedState);
	}

	// 6. 等待每个 worker 线程收尾并收割（全权移交墓园收割者）。
	//    快照里的 worker 一定还在 Run() 中（Run 返回前会持锁自移除出表）。
	//    取消标志在步骤 1 已置位，worker 最迟 50 ms 内脱离 IO 循环、跑完
	//    Run 尾部（出表 + Decrement + 入墓园队列）。
	//    【注意】这里对 Thread 调的 Kill(true) 是【纯联接】用途：联接等待
	//    线程完全退出（等待上限一两个轮询周期）。但 Kill 会同时把 OS 句柄
	//    Close 并置 NULL——同一个 FRunnableThread 对象不能 Kill 两次
	//    （第二次撞 check(Thread)）。而墓园收割者也会 Kill——为避免双
	//    Kill，这里联接完后把 Thread 所有权【Release 放弃】，使收割者的
	//    Thread.IsValid() 为 false、跳过 Kill 直接 delete。联接已保证线程
	//    退出，句柄已在本次 Kill 中关闭，收割者无需再碰它。
	//    GameThread 的等待从"最多 30 s/连接"降到毫秒级；对象与 socket 的
	//    delete 统一在 ReapDeadWorkers（唯一 delete 点，指针唯一入队点
	//    是 Run 尾部）。
	// Wait for each worker thread to unwind (ownership fully handed to the
	// graveyard reaper). Every worker in the snapshot is still inside Run()
	// (Run's epilogue removes itself from the table under the lock before
	// returning). The cancellation flag was set in step 1, so workers leave
	// their IO loops within one 50 ms poll and run their epilogue (leave
	// registry + decrement + enqueue onto the graveyard).
	// NOTE: the Kill(true) below is a PURE join: it waits for the thread to
	// fully exit (a poll cycle or two at most). But Kill also closes the OS
	// handle and NULLs it — the same FRunnableThread object must not be
	// killed twice (the second call trips check(Thread)). The graveyard
	// reaper kills too, so to avoid the double kill, Release() the thread
	// ownership after the join: the reaper then sees Thread.IsValid() ==
	// false and skips its Kill, going straight to delete. The join already
	// guaranteed the thread's exit and this Kill closed the handle, so the
	// reaper has nothing left to do.
	// The GameThread wait drops from "up to 30 s per connection" to
	// milliseconds; the object + socket delete happens exclusively in
	// ReapDeadWorkers (the only delete site; Run's epilogue is the only
	// enqueue site).
	for (FTetherClientWorker* Worker : WorkersToWait)
	{
		if (Worker && Worker->Thread.IsValid())
		{
			Worker->Thread->Kill(/*bShouldWait=*/true);
			// 放弃所有权防二次 Kill；对象内存随 worker 一起被收割者 delete。
			// Abandon ownership to prevent a second Kill; the object's
			// memory is deleted by the reaper along with the worker.
			(void)Worker->Thread.Release();
		}
	}
	{
		// 表里的项已全部 Kill 联接完毕（对象本体由墓园收割）；gate 已关，
		// 不会有新注册。清表即可。
		// Every table entry has been joined (the objects themselves are
		// reaped via the graveyard); the gate is closed, so no new
		// registrations can arrive. Just clear the table.
		FScopeLock Lock(&ClientWorkerThreadsLock);
		ClientWorkerThreads.Empty();
	}
	// 线程创建失败路径 delete 过的 worker 从未入表也从未入墓园（Run 没跑
	// 过），此处排干墓园就是唯一一次 delete，无双重收割。
	// Workers deleted on the thread-creation-failure path were never in the
	// table nor in the graveyard (Run never ran), so draining the graveyard
	// here performs the single delete — no double reap.
	ReapDeadWorkers();

	checkf(ActiveClients.GetValue() == 0,
		TEXT("Tether client workers completed with %d active clients"),
		ActiveClients.GetValue());
	EditorHealthCache->Reset();
	bShutdownRequested = false;
	UE_LOG(LogTether, Log, TEXT("Stop(): all client workers and GameThread closures drained cleanly"));
}

void FTetherServer::ReapDeadWorkers()
{
	// GameThread-only（ticker 与 Stop 调用）。队列里每个 worker 的 Run() 都已
	// 返回：Kill(true) 的 WaitForSingleObject 是瞬时通过（线程只差 harness 的
	// Exit/TLS 清理即退出，或已完全退出），Kill 顺带关闭 OS 句柄并从
	// ThreadManager 摘除；随后 delete worker——析构是 socket 的唯一销毁点。
	// 每个指针恰好入队一次（Run 尾部是唯一 enqueue 点），本函数是唯一 delete
	// 点——Stop 的 Kill 循环只联接不删除（见 Stop 第 6 步注释）。
	// GameThread only (called from the ticker and Stop). Every worker in the
	// queue has already returned from Run(): Kill(true)'s
	// WaitForSingleObject passes instantly (the thread is at worst finishing
	// the harness's Exit/TLS cleanup, or already gone), and Kill closes the
	// OS handle and deregisters from the ThreadManager; then delete the
	// worker — its destructor is the socket's sole destruction point. Each
	// pointer is enqueued exactly once (Run's epilogue is the only enqueue
	// site) and deleted exactly once here — Stop's kill loop only joins,
	// never deletes (see step 6 in Stop).
	checkf(IsInGameThread(), TEXT("ReapDeadWorkers must run on the GameThread"));
	FTetherClientWorker* Worker = nullptr;
	while (DeadWorkers.Dequeue(Worker))
	{
		if (!Worker)
		{
			continue;
		}
		if (Worker->Thread.IsValid())
		{
			Worker->Thread->Kill(/*bShouldWait=*/true);
		}
		delete Worker;
	}
}

bool FTetherServer::IsRunning() const
{
	return bIsRunning;
}

int32 FTetherServer::GetProtocolVersion()
{
	return TetherProtocol::Version;
}

void FTetherServer::SetEditorReady(bool bReady)
{
	bEditorReady = bReady;
	EditorHealthCache->SetReady(bReady);
	if (bReady)
	{
		UE_LOG(LogTether, Log, TEXT("Editor reported ready — Python exec now accepted"));
	}
}

bool FTetherServer::IsEditorReady() const
{
	return bEditorReady;
}

// ─────────────────────────────────────────────────────────────
// Connection handling
// ─────────────────────────────────────────────────────────────

bool FTetherServer::OnConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	const FString EndpointStr = ClientEndpoint.ToString();
	bool bAccepted = false;

	// gate callback 同时完成容量检查、worker 注册与线程创建；Stop::Close
	// 返回后不会出现“已快照 worker 列表之后才注册”的尾随线程。
	// The gate callback performs capacity check, worker registration, and thread creation together;
	// after Stop::Close returns, no trailing worker can register after the snapshot.
	const bool bAdmissionOpen = WorkAdmission->TryAdmit([this, ClientSocket, EndpointStr, &bAccepted]()
	{
		const int32 Active = ActiveClients.Increment();
		if (Active > MaxConcurrentClients)
		{
			ActiveClients.Decrement();
			UE_LOG(LogTether, Warning,
				TEXT("[conn] rejecting %s — at concurrency limit (%d/%d)"),
				*EndpointStr, Active - 1, MaxConcurrentClients);
			// 容量拒绝发生在任何 request 解析之前（尚无 RequestId）；回最小
			// 错误帧让客户端看到明确拒绝而不是静默 EOF（N-F6）。短超时：发送
			// 在 gate 锁内进行，一个不读数据的客户端不能把 listener 线程
			// （连带持同一把锁的 Stop）钉住 8 秒。
			// Capacity is rejected before any request parsing (no RequestId
			// yet); answer with a minimal error frame so the client sees an
			// explicit refusal instead of a silent EOF (N-F6). Short timeout:
			// the send happens under the gate lock, and a client that never
			// reads must not pin the listener thread (and Stop, which takes
			// the same lock) for 8 seconds.
			SendErrorFrame(ClientSocket, TEXT("<unknown>"), TEXT("capacity"),
				FString::Printf(TEXT("server at concurrency limit (%d clients)"), MaxConcurrentClients),
				1.0f);
			return;
		}

		UE_LOG(LogTether, Verbose,
			TEXT("[conn] accepted %s (active=%d)"), *EndpointStr, Active);

		// Disable Nagle on the accepted connection (N-F3): responses are a
		// single request/reply pair, and header/body going out as one write
		// would otherwise sit in the sender queue until Nagle+delayed-ACK
		// interplay releases it. FSocketBSD implements SetNoDelay via
		// TCP_NODELAY on the accepted socket.
		ClientSocket->SetNoDelay(true);

		// 每连接一个专用 FRunnableThread（N-F4）。worker 在 Run() 尾部把自己
		// 移出注册表、Decrement、推进 DeadWorkers 墓园队列；GameThread 的
		// ticker/Stop 负责联接与 delete（reaper 模型，详见类头注释）。
		// One dedicated FRunnableThread per connection (N-F4). Run()'s
		// epilogue removes the worker from the registry, decrements, and
		// pushes it onto the DeadWorkers graveyard; the GameThread
		// ticker/Stop joins and deletes it (reaper model — see the class
		// header comment).
		FTetherClientWorker* Worker = new FTetherClientWorker(this, ClientSocket, EndpointStr);
		Worker->Thread.Reset(FRunnableThread::Create(
			Worker, TEXT("TetherClientWorker"), 0, TPri_Normal));
		if (!Worker->Thread.IsValid())
		{
			// 线程创建失败：worker 析构销毁 socket（唯一所有权点），归还容量槽。
			// bAccepted 置 true 声明 socket 所有权已由本方接管销毁——FTcpListener
			// 对返回 false 的连接会再次 Close+Destroy，必须阻止它对已销毁句柄
			// 二次操作。
			// Thread creation failure: the worker destructor destroys the
			// socket (sole ownership point) and we free the capacity slot.
			// Set bAccepted to claim the socket was consumed on our side —
			// FTcpListener Close+Destroys again on a false return, which
			// would double-destroy the already-freed handle.
			UE_LOG(LogTether, Error,
				TEXT("[conn] failed to spawn worker thread for %s"), *EndpointStr);
			delete Worker;
			ActiveClients.Decrement();
			bAccepted = true;
			return;
		}

		FScopeLock Lock(&ClientWorkerThreadsLock);
		ClientWorkerThreads.Add(Worker);
		bAccepted = true;
	});

	if (!bAdmissionOpen)
	{
		// Start/Stop 竞态路径（N-F10②）：gate 已关说明 server 正在停机；回明确
		// 错误帧而不是静默断连。socket 未被任何 worker 接管，就地回收。
		// Start/Stop race path (N-F10②): a closed gate means the server is
		// shutting down; answer with an explicit error frame instead of a
		// silent disconnect. No worker owns this socket, so reclaim it here.
		SendErrorAndClose(ClientSocket, TEXT("<unknown>"), TEXT("shutting_down"),
			TEXT("server is shutting down"));
	}

	return bAdmissionOpen && bAccepted;
}

bool FTetherServer::SendResponseFrame(FSocket* ClientSocket, const TSharedRef<FJsonObject>& Response,
	float TimeoutSeconds)
{
	FString ResponseStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseStr);
	FJsonSerializer::Serialize(Response, Writer);

	const FTCHARToUTF8 Utf8Response(*ResponseStr);
	const int32 ResponseLen = Utf8Response.Length();

	// Prepend the 4-byte big-endian length prefix and send the whole frame
	// in one SendAll call (N-F3): one syscall, and no Nagle window between
	// the header and body writes.
	TArray<uint8> Frame;
	Frame.Reserve(4 + ResponseLen);
	Frame.Add((uint8)((ResponseLen >> 24) & 0xFF));
	Frame.Add((uint8)((ResponseLen >> 16) & 0xFF));
	Frame.Add((uint8)((ResponseLen >> 8) & 0xFF));
	Frame.Add((uint8)(ResponseLen & 0xFF));
	Frame.Append((const uint8*)Utf8Response.Get(), ResponseLen);

	return SendAll(ClientSocket, Frame.GetData(), Frame.Num());
}

void FTetherServer::SendErrorAndClose(FSocket* ClientSocket, const FString& RequestId,
	const FString& ErrorCode, const FString& Message)
{
	SendErrorFrame(ClientSocket, RequestId, ErrorCode, Message, SendTimeoutSeconds);
}

void FTetherServer::SendErrorFrame(FSocket* ClientSocket, const FString& RequestId,
	const FString& ErrorCode, const FString& Message, float TimeoutSeconds)
{
	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("id"), RequestId);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("output"), TEXT(""));
	Response->SetStringField(TEXT("error"), Message);
	Response->SetStringField(TEXT("error_code"), ErrorCode);
	SendResponseFrame(ClientSocket, Response, TimeoutSeconds);
}

void FTetherServer::HandleClient(FSocket* ClientSocket, const FString& EndpointStr)
{
	// One request-response per connection. tether.py opens a fresh socket
	// per call; keep-alive would tie worker threads up in idle waits.
	// N-F10②: with bIsRunning/Open now flipped right after the listener comes
	// up, this branch is only reachable during a Stop() race; answer with an
	// explicit error frame instead of a silent EOF.
	if (!bIsRunning)
	{
		SendErrorAndClose(ClientSocket, TEXT("<unknown>"), TEXT("shutting_down"),
			TEXT("server is shutting down"));
		return;
	}
	const double T0 = FPlatformTime::Seconds();

	// Per-request telemetry collected throughout this function and flushed
	// to the tether-call ring buffer right before we return. Written-to in
	// several branches below — see each `else if` for where the fields
	// are populated. See TetherCallLog.h.
	FTetherCallRecord CallRecord;
	CallRecord.Endpoint = EndpointStr;
	{
		// ToUnixTimestamp truncates to whole seconds; compute fractional by
		// subtracting the epoch as FTimespan and using GetTotalSeconds().
		static const FDateTime UnixEpoch(1970, 1, 1);
		CallRecord.UnixSeconds = (FDateTime::UtcNow() - UnixEpoch).GetTotalSeconds();
	}
	ON_SCOPE_EXIT
	{
		CallRecord.TotalDurationMs = (FPlatformTime::Seconds() - T0) * 1000.0;
		FTetherCallLog::Get().Append(MoveTemp(CallRecord));
	};

	// 1. Read 4-byte length prefix (big-endian)
	uint8 LenBuf[4];
	if (!RecvAll(ClientSocket, LenBuf, 4, 5.0f))
	{
		UE_LOG(LogTether, Verbose,
			TEXT("[%s] recv header failed (client gave up or idle timeout)"),
			*EndpointStr);
		return;
	}

	const uint32 PayloadLen = (uint32(LenBuf[0]) << 24)
							| (uint32(LenBuf[1]) << 16)
							| (uint32(LenBuf[2]) << 8)
							| (uint32(LenBuf[3]));

	if (PayloadLen == 0 || PayloadLen > (uint32)TetherLimits::MaxRequestBytes)
	{
		UE_LOG(LogTether, Warning,
			TEXT("[%s] invalid payload length %u (max %d) — closing"),
			*EndpointStr, PayloadLen, TetherLimits::MaxRequestBytes);
		// N-F6: reject with an error frame instead of a silent disconnect.
		SendErrorAndClose(ClientSocket, TEXT("<missing>"), TEXT("bad_length"),
			FString::Printf(TEXT("invalid payload length %u (max %d)"),
				PayloadLen, TetherLimits::MaxRequestBytes));
		return;
	}

	// 2. Read JSON payload — Reserve first so an allocation failure is detected
	// before we commit to a SetNumUninitialized of PayloadLen bytes.
	TArray<uint8> PayloadBuf;
	PayloadBuf.Reserve((int32)PayloadLen);
	if (PayloadBuf.Max() < (int32)PayloadLen)
	{
		UE_LOG(LogTether, Warning,
			TEXT("[%s] failed to allocate %u bytes for payload"),
			*EndpointStr, PayloadLen);
		return;
	}
	PayloadBuf.SetNumUninitialized((int32)PayloadLen);
	if (!RecvAll(ClientSocket, PayloadBuf.GetData(), (int32)PayloadLen, 30.0f))
	{
		UE_LOG(LogTether, Warning,
			TEXT("[%s] recv payload failed (expected %u bytes)"),
			*EndpointStr, PayloadLen);
		return;
	}

	FUTF8ToTCHAR Converter((const ANSICHAR*)PayloadBuf.GetData(), PayloadLen);
	FString JsonStr(Converter.Length(), Converter.Get());

	// 3. Parse JSON
	TSharedPtr<FJsonObject> Request;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Request) || !Request.IsValid())
	{
		UE_LOG(LogTether, Warning,
			TEXT("[%s] JSON parse failed (payload=%u bytes)"),
			*EndpointStr, PayloadLen);
		// N-F6: JSON 失败时回帧最划算——客户端通常只错一个引号。
		// N-F6: an error frame pays off most here — clients usually only miss a quote.
		SendErrorAndClose(ClientSocket, TEXT("<missing>"), TEXT("bad_json"),
			FString::Printf(TEXT("JSON parse failed (payload=%u bytes)"), PayloadLen));
		return;
	}

	FString RequestId;
	if (!Request->TryGetStringField(TEXT("id"), RequestId))
	{
		RequestId = TEXT("<missing>");
	}
	CallRecord.RequestId = RequestId;

	// 4. Build response
	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("id"), RequestId);
	FTetherEndpointIdentity ResponseIdentity;
	ResponseIdentity.InstanceId = InstanceId;
	ResponseIdentity.ProjectPath = ProjectPath;
	ResponseIdentity.ProcessId = ProcessId;
	ResponseIdentity.AppendToResponse(Response);

	// 4a. Token auth — only enforced when the server was started with a token
	// (i.e. binding to a non-localhost interface). Constant-time compare so a
	// timing oracle can't whittle the secret out.
	if (!Token.IsEmpty())
	{
		FString GivenToken;
		Request->TryGetStringField(TEXT("token"), GivenToken);

		const auto* A = (const TCHAR*)*Token;
		const auto* B = (const TCHAR*)*GivenToken;
		const int32 LenA = Token.Len();
		const int32 LenB = GivenToken.Len();
		uint32 Diff = (uint32)(LenA ^ LenB);
		const int32 Cmp = FMath::Min(LenA, LenB);
		for (int32 i = 0; i < Cmp; ++i)
		{
			Diff |= (uint32)(A[i] ^ B[i]);
		}

		if (Diff != 0)
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("output"), TEXT(""));
			Response->SetStringField(TEXT("error"), TEXT("unauthorized: missing or invalid token"));

			// N-F3: header + body in one SendAll — no Nagle/delayed-ACK window
			// between the two writes.
			SendResponseFrame(ClientSocket, Response);

			UE_LOG(LogTether, Warning,
				TEXT("[%s] unauthorized request id=%s (bad token)"),
				*EndpointStr, *RequestId);
			return;
		}
	}

	FString WireCommand;
	Request->TryGetStringField(TetherProtocol::CommandField, WireCommand);
	CallRecord.Command = WireCommand.IsEmpty() ? TEXT("<missing>") : WireCommand;

	UE_LOG(LogTether, Verbose,
		TEXT("[%s] request id=%s cmd=%s payload=%u"),
		*EndpointStr, *RequestId, WireCommand.IsEmpty() ? TEXT("(missing)") : *WireCommand, PayloadLen);

	FTetherAcceptedRequest AcceptedRequest;
	FString DispatchErrorCode;
	FString DispatchError;
	const bool bExactRequestValid = FTetherExactRequestDispatcher::TryDispatch(
		Request,
		ResponseIdentity,
		[&AcceptedRequest](const FTetherAcceptedRequest& Accepted)
		{
			AcceptedRequest = Accepted;
		},
		DispatchErrorCode,
		DispatchError);
	if (!bExactRequestValid)
	{
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("output"), TEXT(""));
		Response->SetStringField(TEXT("error"), DispatchError);
		Response->SetStringField(TEXT("error_code"), DispatchErrorCode);
		Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
	}
	else
	{
		Request = AcceptedRequest.Payload;
	}

	const ETetherExactCommand Command = AcceptedRequest.Command;
	if (!bExactRequestValid)
	{
		// 唯一 dispatcher 拒绝后不进入任何命令 body、work admission 或 GameThread dispatch。
		// After the sole dispatcher rejects, no command body, work admission, or GameThread dispatch is entered.
	}
	else if (Command == ETetherExactCommand::Ping)
	{
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("output"), TEXT("pong"));
		Response->SetStringField(TEXT("error"), TEXT(""));
		Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
	}
	else if (Command == ETetherExactCommand::EditorStatus)
	{
		// 该分支只读取加锁缓存；不会排队 exec、派发 GameThread 任务或访问 Slate。
		// This branch only reads the locked cache; it never queues exec work,
		// dispatches a fresh GameThread task, or touches Slate.
		constexpr double StaleAfterSeconds = 2.0;
		const FTetherEditorHealthReadout Health = EditorHealthCache->Read(StaleAfterSeconds);

		TSharedPtr<FJsonObject> Status = MakeShared<FJsonObject>();
		Status->SetNumberField(TEXT("schema_version"), 1);
		Status->SetBoolField(TEXT("editor_ready"), Health.bReady);
		Status->SetNumberField(TEXT("slate_tick_sequence"), static_cast<double>(Health.SlateTickSequence));
		Status->SetStringField(TEXT("last_slate_tick_utc"), Health.LastSlateTickUtc);
		Status->SetNumberField(TEXT("slate_tick_age_ms"),
			Health.bSlateTickObserved ? Health.SlateTickAgeMs : -1.0);
		Status->SetBoolField(TEXT("slate_stale"), Health.bSlateStale);
		Status->SetNumberField(TEXT("engine_tick_sequence"), static_cast<double>(Health.EngineTickSequence));
		Status->SetStringField(TEXT("last_engine_tick_utc"), Health.LastEngineTickUtc);
		Status->SetNumberField(TEXT("engine_tick_age_ms"),
			Health.bEngineTickObserved ? Health.EngineTickAgeMs : -1.0);
		Status->SetBoolField(TEXT("engine_stale"), Health.bEngineStale);
		Status->SetBoolField(TEXT("stale"), Health.bStale);
		Status->SetNumberField(TEXT("stale_after_ms"), StaleAfterSeconds * 1000.0);
		Status->SetStringField(TEXT("ui_state"), Health.UiState);
		Status->SetNumberField(TEXT("attention_id"), static_cast<double>(Health.AttentionId));
		Status->SetBoolField(TEXT("attention_required"), Health.Modal.bPresent);

		TSharedPtr<FJsonObject> Modal = MakeShared<FJsonObject>();
		Modal->SetBoolField(TEXT("present"), Health.Modal.bPresent);
		Modal->SetStringField(TEXT("title"), Health.Modal.Title);
		Modal->SetStringField(TEXT("first_seen_utc"), Health.ActiveModalFirstSeenUtc);
		Modal->SetStringField(TEXT("snapshot_id"), Health.Modal.SnapshotId);
		Modal->SetNumberField(TEXT("button_count"), Health.Modal.ButtonCount);
		Modal->SetNumberField(TEXT("input_count"), Health.Modal.InputCount);
		Modal->SetNumberField(TEXT("checkbox_count"), Health.Modal.CheckBoxCount);
		Status->SetObjectField(TEXT("active_modal"), MoveTemp(Modal));

		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("output"), Health.UiState);
		Response->SetStringField(TEXT("error"), TEXT(""));
		Response->SetBoolField(TEXT("ready"), Health.bReady);
		Response->SetStringField(TEXT("ui_state"), Health.UiState);
		Response->SetObjectField(TEXT("editor_status"), MoveTemp(Status));
	}
	else if (Command == ETetherExactCommand::DebugResume)
	{
		// Recovery path for a stuck blueprint breakpoint.
		//
		// When a BP breakpoint fires, UE enters `FSlateApplication::EnterDebuggingMode`
		// — a nested Slate loop that keeps pumping the task graph on the
		// GameThread but does NOT pump the FTSTicker-based Python exec queue.
		// A prior `invoke_*` that triggered the break is still blocked inside
		// `ProcessEvent`, so the Python interpreter is occupied and new
		// `exec` commands can't land.
		//
		// Recovery requires TWO things, both dispatched via AsyncTask (task
		// graph is pumped during the nested Slate loop; FTSTicker is not):
		//
		//   1. `FSlateApplication::LeaveDebuggingMode()` — exits the nested
		//      Slate loop, unblocking `AttemptToBreakExecution` so the BP
		//      VM resumes.
		//   2. `FKismetDebugUtilities::RequestAbortingExecution()` — sets
		//      `bAbortingExecution` on the stack frame so when the VM
		//      resumes, it unwinds rather than continuing past the
		//      breakpoint (which would hit the same break again).
		//
		// Together these pop the debug-mode stack and let ProcessEvent
		// return, which unblocks the stuck Python exec, which unblocks the
		// TCP response to the original caller.
		TSharedRef<FTetherServer, ESPMode::ThreadSafe> ServerOwner = AsShared();
		AsyncTask(ENamedThreads::GameThread, [ServerOwner]()
		{
			if (!ServerOwner->IsRunning())
			{
				return;
			}
			FKismetDebugUtilities::RequestAbortingExecution();
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().LeaveDebuggingMode(/*bLeavingDebugForSingleStep*/ false);
			}
		});
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("output"), TEXT("resume requested"));
		Response->SetStringField(TEXT("error"), TEXT(""));
		Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
	}
	else if (Command == ETetherExactCommand::GameThreadPing)
	{
		// Probe whether the GameThread is responsive without going through
		// the FTSTicker exec queue. Submits a no-op AsyncTask(GameThread)
		// and waits with a short bounded timeout (default 2s, max 10s).
		//
		// Diagnostic interpretation:
		//   - alive, low latency (~ms): GT idle, exec queue healthy
		//   - alive, high latency (~hundreds ms): GT mid-exec but TaskGraph
		//     is being pumped (asset load / BP compile inside Python) —
		//     editor is not deadlocked but the exec queue may be backed up
		//   - unresponsive: GT is fully stuck (native OS dialog, deadlock,
		//     pure-Python tight loop holding the GIL with no TG pump). Slate
		//     modals normally remain responsive here; use modal_status to
		//     distinguish them from ordinary long-running work. The
		//     FTSTicker exec queue cannot drain in this state.
		double ProbeTimeoutNum = 2.0;
		Request->TryGetNumberField(TEXT("timeout"), ProbeTimeoutNum);
		const float ProbeTimeout = FMath::Clamp((float)ProbeTimeoutNum, 0.1f, 10.0f);

		auto Probe = MakeShared<TPromise<bool>, ESPMode::ThreadSafe>();
		TFuture<bool> ProbeFuture = Probe->GetFuture();
		const double ProbeT0 = FPlatformTime::Seconds();

		TSharedRef<FTetherServer, ESPMode::ThreadSafe> ServerOwner = AsShared();
		AsyncTask(ENamedThreads::GameThread, [Probe, ServerOwner]()
		{
			Probe->SetValue(ServerOwner->IsRunning());
		});

		const bool bAlive = ProbeFuture.WaitFor(FTimespan::FromSeconds(ProbeTimeout));
		const double LatencyMs = (FPlatformTime::Seconds() - ProbeT0) * 1000.0;

		Response->SetBoolField(TEXT("success"), bAlive);
		Response->SetStringField(TEXT("output"), bAlive ? TEXT("alive") : TEXT("unresponsive"));
		Response->SetStringField(TEXT("error"), bAlive
			? TEXT("")
			: FString::Printf(TEXT("GameThread did not respond within %.1fs"), ProbeTimeout));
		Response->SetNumberField(TEXT("latency_ms"), LatencyMs);
		Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
	}
	else if (Command == ETetherExactCommand::ModalStatus
		|| Command == ETetherExactCommand::ModalAction)
	{
		// This path intentionally bypasses both Python and the FTSTicker exec
		// queue. It remains callable while an earlier exec is suspended inside
		// a nested Slate modal loop.
		FString ExpectedSnapshot;
		FString Action;
		FString Value;
		Request->TryGetStringField(TEXT("snapshot"), ExpectedSnapshot);
		Request->TryGetStringField(TEXT("action"), Action);
		Request->TryGetStringField(TEXT("value"), Value);

		double ControlIdNumber = -1.0;
		Request->TryGetNumberField(TEXT("control_id"), ControlIdNumber);
		const int32 ControlId = FMath::FloorToInt(ControlIdNumber);

		bool bChecked = false;
		const bool bHasChecked = Request->TryGetBoolField(TEXT("checked"), bChecked);

		TSharedRef<FTetherServer, ESPMode::ThreadSafe> ServerOwner = AsShared();
		TSharedPtr<FJsonObject> ModalResult;
		const TetherModal::EGameThreadWorkWaitResult WaitResult =
			TetherModal::RunOnGameThread(
				[ServerOwner, Command, ExpectedSnapshot, Action, ControlId, Value, bChecked, bHasChecked]()
				{
					// Stop 关闭 admission 后，晚到 task 只能返回 shutdown，不得触碰 Slate。
					// After Stop closes admission, a late task may only return shutdown and must not touch Slate.
					if (!ServerOwner->IsRunning())
					{
						return TetherModal::MakeResult(
							false, TEXT(""), TEXT("server shutting down"), TetherModal::FModalSnapshot());
					}
					if (Command == ETetherExactCommand::ModalStatus)
					{
						return TetherModal::GetStatus();
					}
					return TetherModal::PerformAction(
						ExpectedSnapshot, Action, ControlId, Value, bChecked, bHasChecked);
				},
				ModalResult);

		if (WaitResult == TetherModal::EGameThreadWorkWaitResult::Completed
			&& ModalResult.IsValid())
		{
			Response = ModalResult.ToSharedRef();
			Response->SetStringField(TEXT("id"), RequestId);
		}
		else
		{
			FString Error;
			if (WaitResult == TetherModal::EGameThreadWorkWaitResult::CancelledBeforeStart)
			{
				Error = TEXT("GameThread did not service the modal request within 3.0s; queued modal work cancelled before execution");
			}
			else if (WaitResult == TetherModal::EGameThreadWorkWaitResult::AlreadyRunning)
			{
				Error = TEXT("modal request timed out after 3.0s; work already started and outcome is unknown");
			}
			else
			{
				Error = TEXT("modal request completed without a result");
			}
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("output"), TEXT(""));
			Response->SetStringField(TEXT("error"), Error);
			Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
		}
	}
	else if (!bEditorReady)
	{
		// Reject Python exec while the editor is still initializing.
		// Dispatching to the GameThread during SlateRHIRenderer::CreateViewport's
		// render-fence can crash the editor, so fail fast with a clear signal.
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("output"), TEXT(""));
		Response->SetStringField(TEXT("error"), TEXT("editor not ready — main frame not yet created"));
		Response->SetBoolField(TEXT("ready"), false);
	}
	else if (bPieTransitionActive)
	{
		// Reject exec during Begin/EndPIE because editor subsystems (world,
		// GAS, anim) are torn down and rebuilt — Python running in that
		// window reliably crashes (item #11).
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("output"), TEXT(""));
		Response->SetStringField(TEXT("error"), TEXT("editor in PIE transition — retry in a moment"));
		Response->SetBoolField(TEXT("ready"), true);
	}
	else
	{
		// Execute Python script — serialized through the GameThread ticker queue.
		FString Script;
		if (!Request->TryGetStringField(TEXT("script"), Script))
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("output"), TEXT(""));
			Response->SetStringField(TEXT("error"), TEXT("missing 'script' field"));
			Response->SetBoolField(TEXT("ready"), true);
		}
		else
		{
			double TimeoutNum = 30.0;
			Request->TryGetNumberField(TEXT("timeout"), TimeoutNum);
			const float Timeout = FMath::Clamp((float)TimeoutNum, 0.1f, 300.0f);

			// Capture a preview of the script for the call-log ring. Cap at
			// ~80 chars; newlines collapse to spaces so the log stays
			// single-line-scannable.
			CallRecord.ScriptPreview = Script.Left(80).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(""));

			const double ExecT0 = FPlatformTime::Seconds();
			FExecResult Result = EnqueueAndWaitForExec(Script, Timeout, RequestId);
			const double ExecMs = (FPlatformTime::Seconds() - ExecT0) * 1000.0;
			CallRecord.ExecDurationMs = ExecMs;

			UE_LOG(LogTether, Log,
				TEXT("[%s] exec id=%s ok=%s out=%dB err=%dB took=%.1fms"),
				*EndpointStr, *RequestId,
				Result.bSuccess ? TEXT("true") : TEXT("false"),
				Result.Output.Len(), Result.Error.Len(), ExecMs);

			Response->SetBoolField(TEXT("success"), Result.bSuccess);
			Response->SetStringField(TEXT("output"), Result.Output);
			Response->SetStringField(TEXT("error"), Result.Error);
			// X-OUT: signal truncation so the agent knows to page output down
			// instead of assuming the script printed everything it saw.
			if (Result.bTruncated)
			{
				Response->SetBoolField(TEXT("truncated"), true);
			}
			Response->SetBoolField(TEXT("ready"), true);
		}
	}

	// modal result 会替换 Response 对象，因此在所有分支结束后再次写入权威 identity。
	// Modal results replace the Response object, so append authoritative identity again after all branches.
	ResponseIdentity.AppendToResponse(Response);

	// Mirror the authoritative Response fields into the call record so
	// every branch (ping / resume / exec / rejected-not-ready / etc.)
	// logs consistent success/output/error sizes without bespoke wiring.
	{
		bool bOk = false;
		Response->TryGetBoolField(TEXT("success"), bOk);
		CallRecord.bSuccess = bOk;
		FString OutStr, ErrStr;
		Response->TryGetStringField(TEXT("output"), OutStr);
		Response->TryGetStringField(TEXT("error"), ErrStr);
		CallRecord.OutputBytes = OutStr.Len();
		CallRecord.ErrorBytes = ErrStr.Len();
		if (!bOk)
		{
			CallRecord.ErrorPreview = ErrStr.Left(200);
		}
	}

	// 5. Serialize and send response — header + body in a single SendAll
	// (N-F3: halves syscalls and removes the Nagle/delayed-ACK exposure
	// between the two writes).
	if (!SendResponseFrame(ClientSocket, Response))
	{
		UE_LOG(LogTether, Warning,
			TEXT("[%s] send response failed (id=%s cmd=%s)"),
			*EndpointStr, *RequestId, *WireCommand);
		return;
	}

	UE_LOG(LogTether, Verbose,
		TEXT("[%s] done id=%s total=%.1fms"),
		*EndpointStr, *RequestId, (FPlatformTime::Seconds() - T0) * 1000.0);
}

// ─────────────────────────────────────────────────────────────
// Python execution pipeline
// ─────────────────────────────────────────────────────────────
//
// worker 把共享 FPendingExec control block 入队并等待；GameThread 的单一 FTSTicker
// 每帧最多消费一项，并由 bExecInFlight 防止重入。共同状态机保证：保留 Python 的
// 非重入 ticker 调度；timeout/shutdown/ticker 只有一个 terminal winner；ticker
// 取到 cancelled tombstone 时不会执行 body。
// Workers enqueue shared FPendingExec control blocks and wait. One GameThread
// FTSTicker drains at most one item per frame under bExecInFlight. The shared
// state machine preserves non-reentrant Python dispatch, gives timeout/shutdown/
// ticker one terminal winner, and makes cancelled queue tombstones harmless.
// ─────────────────────────────────────────────────────────────

FTetherServer::FExecResult FTetherServer::EnqueueAndWaitForExec(
	const FString& Script, float TimeoutSeconds, const FString& RequestId)
{
	TFunction<FExecResult()> Body = [this, Script]()
	{
		return DoPythonExec(Script);
	};
	TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending =
		MakeShared<FPendingExec, ESPMode::ThreadSafe>(MoveTemp(Body), RequestId);

	const bool bEnqueued = WorkAdmission->TryAdmit([this, &Pending]()
	{
		ExecQueue.Enqueue(Pending);
	});
	if (!bEnqueued)
	{
		FExecResult ShutdownResult;
		ShutdownResult.bSuccess = false;
		ShutdownResult.Error = TEXT("server shutting down");
		return ShutdownResult;
	}

	if (Pending->Work.WaitFor(FTimespan::FromSeconds(TimeoutSeconds)))
	{
		return Pending->Work.GetResult();
	}

	FExecResult TimeoutResult;
	TimeoutResult.bSuccess = false;
	TimeoutResult.Error = FString::Printf(
		TEXT("exec timeout after %.1fs; queued work cancelled before execution"),
		TimeoutSeconds);

	ETetherWorkState ObservedState = ETetherWorkState::Queued;
	if (Pending->Work.TryCancel(MoveTemp(TimeoutResult), ObservedState))
	{
		return Pending->Work.GetResult();
	}
	if (ObservedState == ETetherWorkState::Completed
		|| ObservedState == ETetherWorkState::Cancelled)
	{
		// completion/shutdown 与 deadline 同时发生时返回权威 terminal result。
		// If completion/shutdown races the deadline, return the authoritative terminal result.
		return Pending->Work.GetResult();
	}

	FExecResult RunningResult;
	RunningResult.bSuccess = false;
	RunningResult.Error = FString::Printf(
		TEXT("exec timeout after %.1fs; work already started and outcome is unknown"),
		TimeoutSeconds);
	return RunningResult;
}

#if WITH_DEV_AUTOMATION_TESTS
bool FTetherServer::EnqueueExecForTesting(
	TFunction<void()>&& Body,
	bool bCancelBeforeConsume,
	const FString& RequestId)
{
	TFunction<FExecResult()> ExecBody = [Body = MoveTemp(Body)]() mutable
	{
		Body();
		FExecResult Result;
		Result.bSuccess = true;
		return Result;
	};
	TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending =
		MakeShared<FPendingExec, ESPMode::ThreadSafe>(MoveTemp(ExecBody), RequestId);
	const bool bEnqueued = WorkAdmission->TryAdmit([this, &Pending]()
	{
		ExecQueue.Enqueue(Pending);
	});
	if (bEnqueued && bCancelBeforeConsume)
	{
		FExecResult CancelledResult;
		CancelledResult.Error = TEXT("cancelled by automation driver");
		ETetherWorkState ObservedState = ETetherWorkState::Queued;
		Pending->Work.TryCancel(MoveTemp(CancelledResult), ObservedState);
	}
	return bEnqueued;
}
#endif

bool FTetherServer::TickConsumeQueue(float /*DeltaTime*/)
{
	if (!bIsRunning)
	{
		return true; // still ticking; will be removed by Stop()
	}

	// N-F4 reaper: drain workers that finished this frame. Run() already
	// returned for each of them, so the Kill(true) join inside is instant.
	ReapDeadWorkers();

	EditorHealthCache->RecordEngineTick();
	if (!SlatePreTickHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		SlatePreTickHandle = FSlateApplication::Get().OnPreTick().AddRaw(
			this, &FTetherServer::TickUpdateSlateHealth);
		RefreshCachedSlateHealth();
	}
	if (bExecInFlight)
	{
		return true; // belt-and-suspenders guard against ticker reentrancy
	}

	// 同一 tick 丢弃任意数量的 cancelled tombstone，但最多执行一个 Python body。
	// Discard any number of cancelled tombstones in this tick, but execute at most one Python body.
	TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending;
	while (ExecQueue.Dequeue(Pending))
	{
		if (!Pending.IsValid())
		{
			continue;
		}

		bExecInFlight = true;
		const bool bExecuted = Pending->Work.TryExecute();
		bExecInFlight = false;
		if (bExecuted)
		{
			return true;
		}

		UE_LOG(LogTether, Verbose,
			TEXT("Skipping cancelled exec id=%s"), *Pending->RequestId);
	}
	return true;
}

FTetherServer::FExecResult FTetherServer::DoPythonExec(const FString& Script)
{
	FExecResult Result;

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		Result.bSuccess = false;
		Result.Error = TEXT("PythonScriptPlugin is not available");
		return Result;
	}

	// Wrap user script to capture stdout/stderr in Python-land,
	// then print the captured content so ExecPythonCommandEx can collect it via LogOutput.
	//
	// We base64-encode the user script instead of inlining it inside a
	// Python triple-quoted string. Triple-quoted strings in Python don't
	// honour backslash-escape for quotes, so any user script containing
	// `"""` (docstrings, embedded SQL/markdown) would break the old
	// escape scheme. Base64 sidesteps quoting entirely.
	const FTCHARToUTF8 ScriptUtf8(*Script);
	const FString ScriptB64 = FBase64::Encode(
		reinterpret_cast<const uint8*>(ScriptUtf8.Get()),
		ScriptUtf8.Length());

	// Output is base64-encoded before being printed because UE's Python stdout
	// shim mangles non-ASCII characters into U+FFFD on the way back to FString.
	// base64 is pure ASCII, so it survives the shim intact; the C++ side
	// decodes it back to UTF-8 bytes and rebuilds an FString via FUTF8ToTCHAR.
	FString WrappedScript = FString::Printf(TEXT(
		"import base64 as _b64, sys, io as _io, traceback as _tb\n"
		"_src = _b64.b64decode('%s').decode('utf-8')\n"
		"_ub_out, _ub_err = _io.StringIO(), _io.StringIO()\n"
		"_ub_old = sys.stdout, sys.stderr\n"
		"sys.stdout, sys.stderr = _ub_out, _ub_err\n"
		"try:\n"
		"    exec(compile(_src, '<tether>', 'exec'))\n"
		"except Exception:\n"
		"    sys.stderr.write(_tb.format_exc())\n"
		"finally:\n"
		"    sys.stdout, sys.stderr = _ub_old\n"
		"    _ub_o, _ub_e = _ub_out.getvalue(), _ub_err.getvalue()\n"
		"    _ub_out.close(); _ub_err.close()\n"
		"    _eo = _b64.b64encode(_ub_o.encode('utf-8')).decode('ascii') if _ub_o else ''\n"
		"    _ee = _b64.b64encode(_ub_e.encode('utf-8')).decode('ascii') if _ub_e else ''\n"
		"    print('__UB_B64__' + _eo + '|' + _ee + '__UB_END__')\n"
	), *ScriptB64);

	FPythonCommandEx CommandEx;
	CommandEx.Command = WrappedScript;
	CommandEx.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	CommandEx.FileExecutionScope = EPythonFileExecutionScope::Public;

	bool bExecSuccess = PythonPlugin->ExecPythonCommandEx(CommandEx);

	FString FullOutput;
	for (const FPythonLogOutputEntry& Entry : CommandEx.LogOutput)
	{
		FullOutput += Entry.Output + TEXT("\n");
	}
	if (FullOutput.IsEmpty() && !CommandEx.CommandResult.IsEmpty())
	{
		FullOutput = CommandEx.CommandResult;
	}

	// Wrapper emits `__UB_B64__<out_b64>|<err_b64>__UB_END__`. Decode each
	// half from base64 → UTF-8 bytes → FString via FUTF8ToTCHAR. Fallback to
	// raw FullOutput-as-Error when the envelope is missing (catastrophic
	// failure inside the wrapper itself, before the print could run).
	const FString EnvBegin = TEXT("__UB_B64__");
	const FString EnvEnd = TEXT("__UB_END__");
	const int32 BeginIdx = FullOutput.Find(EnvBegin);
	const int32 EndIdx = (BeginIdx != INDEX_NONE)
		? FullOutput.Find(EnvEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, BeginIdx + EnvBegin.Len())
		: INDEX_NONE;

	if (BeginIdx != INDEX_NONE && EndIdx != INDEX_NONE)
	{
		const int32 PayloadStart = BeginIdx + EnvBegin.Len();
		const FString Payload = FullOutput.Mid(PayloadStart, EndIdx - PayloadStart);

		int32 SepIdx = INDEX_NONE;
		Payload.FindChar(TEXT('|'), SepIdx);
		const FString OutB64 = (SepIdx != INDEX_NONE) ? Payload.Left(SepIdx) : Payload;
		const FString ErrB64 = (SepIdx != INDEX_NONE) ? Payload.Mid(SepIdx + 1) : FString();

		auto DecodeB64ToUtf8FString = [](const FString& B64) -> FString
		{
			if (B64.IsEmpty()) return FString();
			TArray<uint8> Bytes;
			if (!FBase64::Decode(B64, Bytes) || Bytes.Num() == 0) return FString();
			// Match the inbound-decode pattern used at line ~380 — FUTF8ToTCHAR
			// with explicit length, then construct FString from .Get() + .Length()
			// so we don't accidentally hit FString's ANSI-interpret constructor.
			FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
			return FString(Conv.Length(), Conv.Get());
		};

		Result.Output = DecodeB64ToUtf8FString(OutB64);
		Result.Error = DecodeB64ToUtf8FString(ErrB64);
		Result.bSuccess = bExecSuccess;
	}
	else
	{
		// Wrapper crashed before emitting the envelope — surface whatever UE captured.
		Result.Output = FString();
		Result.Error = FullOutput;
		Result.bSuccess = false;
	}

	Result.Output.TrimEndInline();
	Result.Error.TrimEndInline();

	// X-OUT: script output has no natural upper bound while the inbound script
	// is capped at MaxRequestBytes; an unbounded reply either OOMs the editor
	// (3-5x amplification through StringIO + base64 + JSON) or exceeds the
	// client's frame limit AFTER the script's side effects happened, which
	// reports "protocol error" for a script that actually ran. Cap at
	// MaxExecOutputUtf8Bytes of UTF-8 wire bytes (NOT TCHARs: 8M UTF-16 code
	// units of CJK become ~24 MB on the wire and would still blow the client's
	// 10 MB frame limit after the side effects ran). The clamp backs off over
	// UTF-8 continuation bytes so the cut lands on a code-point boundary (the
	// engine's UTF-8 converter would turn a lone surrogate into '?', which is
	// harmless, but whole-code-point cuts cost nothing).
	// bTruncated rides along in the response as "truncated": true.
	auto ClampToUtf8Budget = [](FString& InOut, bool& bOutTruncated)
	{
		const FTCHARToUTF8 Utf8(*InOut);
		if (Utf8.Length() <= (int32)MaxExecOutputUtf8Bytes)
		{
			return;
		}
		// FTCHARToUTF8::Get() returns const ANSICHAR* (the converter's ToType
		// predates char8_t); the bytes are UTF-8 either way, so reinterpret
		// for the continuation-byte arithmetic below.
		const ANSICHAR* Start = Utf8.Get();
		int32 Cut = (int32)MaxExecOutputUtf8Bytes;
		while (Cut > 0 && (Start[Cut] & 0xC0) == 0x80)
		{
			--Cut; // back off continuation bytes so we cut on a code point
		}
		// Trim from the end until the re-encoded string fits the byte budget.
		while (!InOut.IsEmpty() && FTCHARToUTF8(*InOut).Length() > Cut)
		{
			InOut.LeftChopInline(1);
		}
		bOutTruncated = true;
	};
	ClampToUtf8Budget(Result.Output, Result.bTruncated);
	ClampToUtf8Budget(Result.Error, Result.bTruncated);
	return Result;
}

// ─────────────────────────────────────────────────────────────
// Socket helpers
// ─────────────────────────────────────────────────────────────

bool FTetherServer::RecvAll(FSocket* Socket, uint8* Buffer, int32 NumBytes, float TimeoutSeconds)
{
	int32 BytesRead = 0;
	const double StartTime = FPlatformTime::Seconds();
	int32 ZeroReadTries = 0;

	while (BytesRead < NumBytes)
	{
		// N-F1: shutdown cancellation outranks every other exit below — a
		// blocked RecvAll leaves within one 50 ms Wait poll after Stop()
		// sets the flag, instead of spinning to the full deadline.
		if (bShutdownRequested)
		{
			return false;
		}

		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			return false;
		}

		// Select-level readiness probe. Without this, UE's FSocket::Recv on
		// a just-accepted FTcpListener socket can return Read=0 before the
		// kernel has delivered any data, which we'd mis-interpret as a FIN
		// and close the connection — producing WSAECONNABORTED 10053 on
		// the client mid-recv. Wait() reports readable only once real data
		// (or a real FIN) is present.
		const bool bReadable = Socket->Wait(
			ESocketWaitConditions::WaitForRead,
			FTimespan::FromMilliseconds(50));
		if (!bReadable)
		{
			continue; // Keep checking until TimeoutSeconds elapses.
		}

		uint32 PendingBytes = 0;
		const bool bHasPending = Socket->HasPendingData(PendingBytes);

		int32 Read = 0;
		const bool bRecvOk = Socket->Recv(Buffer + BytesRead, NumBytes - BytesRead, Read);
		if (bRecvOk)
		{
			if (Read == 0)
			{
				// Confirm genuine FIN: no pending kernel data AND a small retry budget
				// exhausted. Spurious zero-reads (UE socket edge case) are rare but
				// documented above.
				if (bHasPending && PendingBytes > 0)
				{
					++ZeroReadTries;
					FPlatformProcess::Sleep(0.001f);
					continue;
				}
				if (Socket->GetConnectionState() == SCS_Connected && ZeroReadTries < 5)
				{
					++ZeroReadTries;
					FPlatformProcess::Sleep(0.002f);
					continue;
				}
				return false;
			}
			BytesRead += Read;
			ZeroReadTries = 0;
		}
		else
		{
			if (Socket->GetConnectionState() != SCS_Connected)
			{
				return false;
			}
			FPlatformProcess::Sleep(0.001f);
		}
	}

	return true;
}

bool FTetherServer::SendAll(FSocket* Socket, const uint8* Buffer, int32 NumBytes, float TimeoutSeconds)
{
	int32 BytesSent = 0;
	const double StartTime = FPlatformTime::Seconds();

	while (BytesSent < NumBytes)
	{
		// N-F1: shutdown cancellation outranks the deadline — blocked sends
		// release the worker within one 50 ms Wait poll after Stop().
		if (bShutdownRequested)
		{
			return false;
		}

		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			// N-F2: a stalled (zero-window) reader must not pin the worker
			// until kernel-level TCP timeouts. Drop the response; the client
			// sees a short read/EOF and retries on its own terms.
			UE_LOG(LogTether, Warning,
				TEXT("SendAll timed out after %.1fs (%d/%d bytes sent) — giving up"),
				TimeoutSeconds, BytesSent, NumBytes);
			return false;
		}

		// Select-level writability probe (N-F2): only call Send when the
		// socket can actually accept more bytes, mirroring RecvAll's
		// readability probe. Keeps the loop responsive to both cancellation
		// and the deadline at 50 ms granularity.
		const bool bWritable = Socket->Wait(
			ESocketWaitConditions::WaitForWrite,
			FTimespan::FromMilliseconds(50));
		if (!bWritable)
		{
			continue; // Keep checking until TimeoutSeconds elapses.
		}

		int32 Sent = 0;
		if (!Socket->Send(Buffer + BytesSent, NumBytes - BytesSent, Sent))
		{
			return false;
		}
		if (Sent > 0)
		{
			BytesSent += Sent;
		}
		else
		{
			// Zero-byte sends on a healthy socket are transient; avoid a tight
			// spin while still respecting the deadline above.
			FPlatformProcess::Sleep(0.001f);
		}
	}

	return true;
}

