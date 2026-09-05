#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TetherCancellableWork.h"
#include "TetherServer.h"
#include "SocketSubsystem.h"
#include "Misc/ScopeExit.h"

/**
 * Automation 只读访问 Server lifecycle 计数，不改变生产行为。
 * Automation-only read access to Server lifecycle counters; it does not change production behavior.
 */
class FTetherServerTestAccessor
{
public:
	static int32 GetActiveClientCount(const FTetherServer& Server)
	{
		return Server.ActiveClients.GetValue();
	}

	static int32 GetTrackedWorkerCount(const FTetherServer& Server)
	{
		return Server.ClientWorkerTasks.Num();
	}

	static bool EnqueueExec(
		FTetherServer& Server,
		TFunction<void()>&& Body,
		bool bCancelBeforeConsume,
		const FString& RequestId)
	{
		return Server.EnqueueExecForTesting(MoveTemp(Body), bCancelBeforeConsume, RequestId);
	}

	static void TickExecQueue(FTetherServer& Server)
	{
		Server.TickConsumeQueue(0.0f);
	}
};

namespace TetherCancellableWorkTests
{
	using FIntWork = TTetherCancellableWork<int32>;

	int32 StateValue(ETetherWorkState State)
	{
		return static_cast<int32>(State);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherCancelledWorkSkipsLateConsumerTest,
	"Tether.Server.CancellableWork.CancelledWorkSkipsLateConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherCancelledWorkSkipsLateConsumerTest::RunTest(const FString& Parameters)
{
	using namespace TetherCancellableWorkTests;
	(void)Parameters;

	int32 SideEffectCount = 0;
	TFunction<int32()> Body = [&SideEffectCount]()
	{
		++SideEffectCount;
		return 42;
	};
	TSharedPtr<FIntWork, ESPMode::ThreadSafe> Work =
		MakeShared<FIntWork, ESPMode::ThreadSafe>(MoveTemp(Body));

	ETetherWorkState ObservedState = ETetherWorkState::Queued;
	TestTrue(TEXT("queued cancellation wins"), Work->TryCancel(-7, ObservedState));
	TestEqual(TEXT("cancel observes terminal state"), StateValue(ObservedState),
		StateValue(ETetherWorkState::Cancelled));
	TestFalse(TEXT("late consumer cannot claim cancelled work"), Work->TryExecute());
	TestEqual(TEXT("cancelled work has no late side effect"), SideEffectCount, 0);
	TestEqual(TEXT("state remains cancelled"), StateValue(Work->GetState()),
		StateValue(ETetherWorkState::Cancelled));

	TestTrue(TEXT("cancellation publishes one result"), Work->WaitFor(FTimespan::Zero()));
	if (Work->WaitFor(FTimespan::Zero()))
	{
		TestEqual(TEXT("published cancellation result is stable"), Work->GetResult(), -7);
	}

	ETetherWorkState SecondObservedState = ETetherWorkState::Queued;
	TestFalse(TEXT("terminal cancellation cannot publish twice"),
		Work->TryCancel(-9, SecondObservedState));
	TestEqual(TEXT("second cancellation observes first terminal state"),
		StateValue(SecondObservedState), StateValue(ETetherWorkState::Cancelled));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherRunningWorkCannotBeCancelledTest,
	"Tether.Server.CancellableWork.RunningWorkCannotBeCancelled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherRunningWorkCannotBeCancelledTest::RunTest(const FString& Parameters)
{
	using namespace TetherCancellableWorkTests;
	(void)Parameters;

	TSharedPtr<FIntWork, ESPMode::ThreadSafe> Work;
	bool bCancellationWon = true;
	ETetherWorkState StateSeenInsideBody = ETetherWorkState::Queued;
	TFunction<int32()> Body = [&Work, &bCancellationWon, &StateSeenInsideBody]()
	{
		// body 内的显式尝试稳定模拟 deadline 在 consumer claim 后到达。
		// An explicit in-body attempt deterministically models a deadline arriving after consumer claim.
		bCancellationWon = Work->TryCancel(-1, StateSeenInsideBody);
		return 42;
	};
	Work = MakeShared<FIntWork, ESPMode::ThreadSafe>(MoveTemp(Body));

	TestTrue(TEXT("consumer claims and executes queued work"), Work->TryExecute());
	TestFalse(TEXT("running work cannot be relabelled cancelled"), bCancellationWon);
	TestEqual(TEXT("deadline observes running"), StateValue(StateSeenInsideBody),
		StateValue(ETetherWorkState::Running));
	TestEqual(TEXT("successful body reaches completed"), StateValue(Work->GetState()),
		StateValue(ETetherWorkState::Completed));
	TestTrue(TEXT("completed work publishes its result"), Work->WaitFor(FTimespan::Zero()));
	if (Work->WaitFor(FTimespan::Zero()))
	{
		TestEqual(TEXT("completed result remains authoritative"), Work->GetResult(), 42);
	}

	ETetherWorkState ObservedState = ETetherWorkState::Queued;
	TestFalse(TEXT("completed work cannot be cancelled"), Work->TryCancel(-2, ObservedState));
	TestEqual(TEXT("post-completion cancellation observes completed"), StateValue(ObservedState),
		StateValue(ETetherWorkState::Completed));
	TestFalse(TEXT("completed work cannot execute twice"), Work->TryExecute());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherAdmissionGateRejectsAfterCloseTest,
	"Tether.Server.CancellableWork.AdmissionGateRejectsAfterClose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherAdmissionGateRejectsAfterCloseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FTetherWorkAdmissionGate Gate;
	int32 AdmissionCount = 0;
	TestFalse(TEXT("closed gate rejects initial work"), Gate.TryAdmit([&AdmissionCount]()
	{
		++AdmissionCount;
	}));

	Gate.Open();
	TestTrue(TEXT("open gate admits registration"), Gate.TryAdmit([&AdmissionCount]()
	{
		++AdmissionCount;
	}));
	Gate.Close();
	TestFalse(TEXT("closed shutdown gate rejects trailing work"), Gate.TryAdmit([&AdmissionCount]()
	{
		++AdmissionCount;
	}));
	TestEqual(TEXT("only pre-shutdown callback ran"), AdmissionCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherCancelledBacklogDrainsBeforeLiveWorkTest,
	"Tether.Server.CancellableWork.CancelledBacklogDrainsBeforeLiveWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherCancelledBacklogDrainsBeforeLiveWorkTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedRef<FTetherServer, ESPMode::ThreadSafe> Server =
		MakeShared<FTetherServer, ESPMode::ThreadSafe>();
	if (!TestTrue(TEXT("test server starts for production ticker coverage"), Server->Start(0)))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		if (Server->IsRunning())
		{
			Server->Stop();
		}
	};

	int32 CancelledSideEffects = 0;
	int32 LiveSideEffects = 0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TFunction<void()> Body = [&CancelledSideEffects]()
		{
			++CancelledSideEffects;
		};
		TestTrue(TEXT("cancelled backlog item entered the production queue"),
			FTetherServerTestAccessor::EnqueueExec(
				*Server, MoveTemp(Body), true, FString::Printf(TEXT("cancelled-%d"), Index)));
	}

	TFunction<void()> LiveBody = [&LiveSideEffects]()
	{
		++LiveSideEffects;
	};
	TestTrue(TEXT("live item entered behind cancelled tombstones"),
		FTetherServerTestAccessor::EnqueueExec(
			*Server, MoveTemp(LiveBody), false, TEXT("live")));

	FTetherServerTestAccessor::TickExecQueue(*Server);
	TestEqual(TEXT("cancelled production backlog produced no side effects"), CancelledSideEffects, 0);
	TestEqual(TEXT("production ticker reached one live body in the same drain"), LiveSideEffects, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherStopDrainsStragglingWorkerTest,
	"Tether.Server.CancellableWork.StopDrainsStragglingWorker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherStopDrainsStragglingWorkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedRef<FTetherServer, ESPMode::ThreadSafe> Server =
		MakeShared<FTetherServer, ESPMode::ThreadSafe>();
	if (!TestTrue(TEXT("test server starts on an ephemeral port"), Server->Start(0)))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		if (Server->IsRunning())
		{
			Server->Stop();
		}
	};

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!TestNotNull(TEXT("socket subsystem is available"), SocketSubsystem))
	{
		return false;
	}
	FSocket* ClientSocket = SocketSubsystem->CreateSocket(
		NAME_Stream, TEXT("Tether shutdown automation client"), false);
	if (!TestNotNull(TEXT("client socket was created"), ClientSocket))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		ClientSocket->Close();
		SocketSubsystem->DestroySocket(ClientSocket);
	};

	TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
	bool bValidAddress = false;
	Address->SetIp(TEXT("127.0.0.1"), bValidAddress);
	Address->SetPort(Server->GetBoundPort());
	TestTrue(TEXT("loopback address is valid"), bValidAddress);
	if (!TestTrue(TEXT("client connects to the test server"), ClientSocket->Connect(*Address)))
	{
		return false;
	}

	// 只发送 frame header，让真实 worker 阻塞在 payload recv；Stop 必须关 socket 并等待 graph event。
	// Send only a frame header so the real worker blocks in payload recv; Stop must close it and wait for its graph event.
	const uint8 Header[4] = { 0, 0, 0, 32 };
	int32 BytesSent = 0;
	TestTrue(TEXT("partial request header was sent"),
		ClientSocket->Send(Header, static_cast<int32>(UE_ARRAY_COUNT(Header)), BytesSent));
	TestEqual(TEXT("full frame header was sent"), BytesSent,
		static_cast<int32>(UE_ARRAY_COUNT(Header)));

	const double AcceptDeadline = FPlatformTime::Seconds() + 2.0;
	while (FTetherServerTestAccessor::GetActiveClientCount(*Server) == 0
		&& FPlatformTime::Seconds() < AcceptDeadline)
	{
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("one straggling worker is tracked before shutdown"),
		FTetherServerTestAccessor::GetActiveClientCount(*Server), 1);

	Server->Stop();
	TestEqual(TEXT("Stop waits for worker closure completion"),
		FTetherServerTestAccessor::GetActiveClientCount(*Server), 0);
	TestEqual(TEXT("Stop releases all tracked graph events"),
		FTetherServerTestAccessor::GetTrackedWorkerCount(*Server), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
