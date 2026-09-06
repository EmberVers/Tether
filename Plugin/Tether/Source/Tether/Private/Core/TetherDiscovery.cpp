#include "Core/TetherDiscovery.h"
#include "Core/TetherProtocol.h"

#include "Common/UdpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "HAL/RunnableThread.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherDiscovery, Log, All);

namespace
{
	constexpr int32 MaxDatagramBytes = 64 * 1024;

	// N-F8: request_id 去重窗口与每秒响应上限。discovery 响应泄露 project
	// path/pid/engine version，且 probe 的源地址可被伪造；去重 + 限速把伪造
	// 源地址的反射放大压到可忽略（单 request_id 最多一次响应，每秒至多
	// MaxResponsesPerSecond 条）。HMAC 共享密钥留作协议 v3 演进。
	// N-F8: request_id dedup window and per-second response cap. Responses
	// disclose project path/pid/engine version and a probe's source address
	// is spoofable; dedup + rate limiting collapses spoofed-source reflection
	// to negligible (at most one response per request_id, at most
	// MaxResponsesPerSecond responses overall). HMAC shared-secret is a
	// protocol-v3 candidate and deliberately out of scope here.
	constexpr double DedupWindowSeconds = 5.0;
	constexpr int32 MaxResponsesPerSecond = 20;
	constexpr int32 MaxTrackedRequestIds = 256;

	bool MatchProjectFilter(const FString& Filter, const FString& ProjectName, const FString& ProjectPath)
	{
		if (Filter.IsEmpty() || Filter == TEXT("*"))
		{
			return true;
		}

		if (ProjectName.Equals(Filter, ESearchCase::IgnoreCase))
		{
			return true;
		}

		if (!ProjectPath.IsEmpty() && ProjectPath.Equals(Filter, ESearchCase::IgnoreCase))
		{
			return true;
		}

		// Allow path-suffix matches so users can give the directory containing
		// the .uproject instead of the full asset path.
		if (!ProjectPath.IsEmpty())
		{
			FString NormalizedProject = ProjectPath;
			FString NormalizedFilter = Filter;
			NormalizedProject.ReplaceInline(TEXT("\\"), TEXT("/"));
			NormalizedFilter.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (NormalizedProject.EndsWith(NormalizedFilter, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		// Allow project-name substring for casual use ("my" matches "MyGame").
		if (ProjectName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			return true;
		}

		return false;
	}
}


FTetherDiscoveryService::FTetherDiscoveryService(const FConfig& InConfig)
	: Config(InConfig)
{
	CurrentTcpPort.Set(InConfig.TcpPort);
}

FTetherDiscoveryService::~FTetherDiscoveryService()
{
	StopService();
}

bool FTetherDiscoveryService::StartService()
{
	if (bIsRunning)
	{
		return true;
	}

	// Bind to 0.0.0.0:GroupPort, join the multicast group, enable loopback so
	// localhost probes work on this same machine.
	Socket = FUdpSocketBuilder(TEXT("TetherDiscovery"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToAddress(FIPv4Address::Any)
		.BoundToPort(Config.GroupPort)
		.JoinedToGroup(Config.GroupAddress)
		.WithMulticastLoopback()
		.WithMulticastTtl(1)
		.Build();

	if (Socket == nullptr)
	{
		UE_LOG(LogTetherDiscovery, Warning,
			TEXT("Failed to bind UDP discovery socket on %s:%d — discovery disabled for this instance"),
			*Config.GroupAddress.ToString(), Config.GroupPort);
		return false;
	}

	bStopRequested = false;
	// N-F8: reset dedup/rate state so a restart (hot reload) starts with a
	// clean budget instead of inheriting the previous run's table.
	RecentRequestIds.Reset();
	ResponseBudget = (double)MaxResponsesPerSecond;
	LastRefillSeconds = FPlatformTime::Seconds();
	Thread.Reset(FRunnableThread::Create(this, TEXT("TetherDiscovery"), 0,
		TPri_BelowNormal));

	if (!Thread.IsValid())
	{
		UE_LOG(LogTetherDiscovery, Warning, TEXT("Failed to create discovery thread"));
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
		return false;
	}

	bIsRunning = true;
	UE_LOG(LogTetherDiscovery, Log,
		TEXT("Discovery listening on multicast %s:%d (tcp advertised: %s:%d, project='%s')"),
		*Config.GroupAddress.ToString(), Config.GroupPort,
		*Config.TcpBindAddress, CurrentTcpPort.GetValue(), *Config.ProjectName);
	return true;
}

void FTetherDiscoveryService::StopService()
{
	if (!bIsRunning && !Thread.IsValid() && Socket == nullptr)
	{
		return;
	}

	bStopRequested = true;

	if (Thread.IsValid())
	{
		Thread->Kill(/*bShouldWait=*/true);
		Thread.Reset();
	}

	if (Socket != nullptr)
	{
		ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (Subsystem != nullptr)
		{
			Subsystem->DestroySocket(Socket);
		}
		Socket = nullptr;
	}

	bIsRunning = false;
}

void FTetherDiscoveryService::SetTcpPort(int32 NewPort)
{
	CurrentTcpPort.Set(NewPort);
}

bool FTetherDiscoveryService::Init()
{
	return Socket != nullptr;
}

uint32 FTetherDiscoveryService::Run()
{
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (Subsystem == nullptr || Socket == nullptr)
	{
		return 1;
	}

	TSharedRef<FInternetAddr> SourceAddr = Subsystem->CreateInternetAddr();
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(MaxDatagramBytes);

	while (!bStopRequested)
	{
		uint32 PendingSize = 0;
		bool bHasData = Socket->HasPendingData(PendingSize);
		if (bHasData && PendingSize > 0)
		{
			const int32 ToRead = FMath::Min<int32>(PendingSize, Buffer.Num());
			int32 BytesRead = 0;
			if (Socket->RecvFrom(Buffer.GetData(), ToRead, BytesRead, *SourceAddr) && BytesRead > 0)
			{
				HandleDatagram(Buffer.GetData(), BytesRead, *SourceAddr);
			}
		}
		else
		{
			// 100ms poll — imperceptible discovery latency, negligible CPU.
			FPlatformProcess::Sleep(0.1f);
		}
	}

	return 0;
}

void FTetherDiscoveryService::Stop()
{
	bStopRequested = true;
}

void FTetherDiscoveryService::Exit()
{
}

void FTetherDiscoveryService::HandleDatagram(const uint8* Bytes, int32 Length, const FInternetAddr& Source)
{
	if (Length <= 0)
	{
		return;
	}

	// UE's TCHAR JSON parser wants TCHAR-encoded text; UDP payloads are UTF-8
	// on the wire so we convert here.
	const FString Payload = FString(Length, UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes)));

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Payload);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	double Version = 0.0;
	if (!Root->TryGetNumberField(TEXT("v"), Version)
		|| !FMath::IsFinite(Version)
		|| Version != static_cast<double>(Config.ProtocolVersion))
	{
		return;
	}

	FString Type;
	if (!Root->TryGetStringField(TEXT("type"), Type) || Type != TEXT("probe"))
	{
		return;
	}

	FString RequestId;
	if (!Root->TryGetStringField(TEXT("request_id"), RequestId) || RequestId.IsEmpty())
	{
		return;
	}

	// N-F8: duplicate request_id within the window is dropped — a legitimate
	// client probes once and retries with a fresh id, while a reflection
	// attacker replays the same captured datagram.
	const double Now = FPlatformTime::Seconds();
	if (const double* LastSeen = RecentRequestIds.Find(RequestId))
	{
		if (Now - *LastSeen <= DedupWindowSeconds)
		{
			return;
		}
	}

	// N-F8: responses are also capped per second. The rate state only tracks
	// responses actually about to be sent, so unmatched filters never consume
	// budget. Budget refill is continuous (token-ish) to stay smooth across
	// window boundaries.
	{
		const double Elapsed = Now - LastRefillSeconds;
		ResponseBudget = FMath::Min(
			(double)MaxResponsesPerSecond,
			ResponseBudget + Elapsed * (double)MaxResponsesPerSecond);
		LastRefillSeconds = Now;
		if (ResponseBudget < 1.0)
		{
			return;
		}
		--ResponseBudget;
	}

	// Record the id only when a response will actually go out; recording
	// before the budget check would let rate-limited probes poison the table.
	RecentRequestIds.Add(RequestId, Now);
	if (RecentRequestIds.Num() > MaxTrackedRequestIds)
	{
		// Cheap pruning: drop the oldest entries until back at the cap. A
		// linear scan is fine — the table only ever reaches this size under
		// deliberate flooding, and the cap bounds it to a few hundred keys.
		for (auto It = RecentRequestIds.CreateIterator(); It; ++It)
		{
			if (Now - It->Value > DedupWindowSeconds)
			{
				It.RemoveCurrent();
				if (RecentRequestIds.Num() <= MaxTrackedRequestIds)
				{
					break;
				}
			}
		}
	}

	FString Filter = TEXT("*");
	const TSharedPtr<FJsonObject>* FilterObj = nullptr;
	if (Root->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj && FilterObj->IsValid())
	{
		FString ProjectFilter;
		if ((*FilterObj)->TryGetStringField(TEXT("project"), ProjectFilter) && !ProjectFilter.IsEmpty())
		{
			Filter = ProjectFilter;
		}
	}

	if (!FilterMatchesUs(Filter))
	{
		return;
	}

	const FString ResponseJson = BuildResponseJson(RequestId);
	const FTCHARToUTF8 Utf8(*ResponseJson);
	const int32 NumBytes = Utf8.Length();

	int32 BytesSent = 0;
	if (!Socket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), NumBytes, BytesSent, Source))
	{
		UE_LOG(LogTetherDiscovery, Verbose, TEXT("SendTo(response) failed"));
	}
}

bool FTetherDiscoveryService::FilterMatchesUs(const FString& Filter) const
{
	return MatchProjectFilter(Filter, Config.ProjectName, Config.ProjectPath);
}

FString FTetherDiscoveryService::BuildResponseJson(const FString& RequestId) const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("v"), Config.ProtocolVersion);
	Root->SetNumberField(TEXT("protocol_version"), Config.ProtocolVersion);
	Root->SetStringField(TEXT("type"), TEXT("response"));
	Root->SetStringField(TEXT("request_id"), RequestId);
	Root->SetStringField(TEXT("instance_id"), Config.InstanceId);
	Root->SetNumberField(TEXT("pid"), Config.ProcessId);
	Root->SetStringField(TEXT("project"), Config.ProjectName);
	Root->SetStringField(TEXT("project_path"), Config.ProjectPath);
	Root->SetStringField(TEXT("engine_version"), Config.EngineVersion);
	Root->SetStringField(TEXT("tcp_bind"), Config.TcpBindAddress);
	Root->SetNumberField(TEXT("tcp_port"), CurrentTcpPort.GetValue());
	// X-TOKEN③: the fingerprint only MATCHES a strong random token; it is not
	// a proof of secrecy. A weak (low-entropy) token can be brute-forced
	// offline against this 8-byte SHA1 prefix, so tokens must come from a
	// cryptographic RNG (the module generates them that way; user-supplied
	// tokens via -TetherToken are the residual risk).
	Root->SetStringField(TEXT("token_fingerprint"), Config.TokenFingerprint);
	TArray<TSharedPtr<FJsonValue>> Capabilities;
	for (const TCHAR* Capability : TetherProtocol::ExactCapabilities)
	{
		Capabilities.Add(MakeShared<FJsonValueString>(Capability));
	}
	Root->SetArrayField(TEXT("capabilities"), MoveTemp(Capabilities));

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}
