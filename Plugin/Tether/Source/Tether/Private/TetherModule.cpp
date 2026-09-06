#include "TetherModule.h"
#include "TetherDiscovery.h"
#include "TetherServer.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

// Forward decls for the debug-hook registration that lives in the blueprint
// library's .cpp (keeps the delegate handle private while still letting the
// module control init / teardown lifetime).
namespace TetherDebugState
{
	void Register();
	void Unregister();
}

// Always-on perf hook (frame-time histogram + hitch log) — defined in
// TetherPerfLibrary.cpp, lifetime tied to the module.
namespace TetherPerfFrameHook
{
	void Register();
	void Unregister();
}

// Opt-in periodic perf sampler — defined in TetherPerfLibrary.cpp.
// No Start at module init (caller-driven) but Shutdown must release the
// FTSTicker handle if a sampling run is still active when the module exits.
namespace TetherPerfSampler
{
	void Shutdown();
}

DEFINE_LOG_CATEGORY_STATIC(LogTetherModule, Log, All);

namespace
{
	/**
	 * Layered config resolution: CLI > env > EditorPerProjectUserSettings.ini > hardcoded default.
	 * Returns `Out` on success, leaves it untouched on miss.
	 */
	void ResolveStringConfig(const TCHAR* CliKey, const TCHAR* EnvKey, const TCHAR* IniKey, FString& Out)
	{
		FString Value;
		if (FParse::Value(FCommandLine::Get(), CliKey, Value) && !Value.IsEmpty())
		{
			Out = Value;
			return;
		}
		Value = FPlatformMisc::GetEnvironmentVariable(EnvKey);
		if (!Value.IsEmpty())
		{
			Out = Value;
			return;
		}
		if (GConfig)
		{
			FString Tmp;
			if (GConfig->GetString(TEXT("Tether"), IniKey, Tmp, GEditorPerProjectIni) && !Tmp.IsEmpty())
			{
				Out = Tmp;
				return;
			}
		}
	}

	void ResolveIntConfig(const TCHAR* CliKey, const TCHAR* EnvKey, const TCHAR* IniKey, int32& Out)
	{
		int32 Value = 0;
		if (FParse::Value(FCommandLine::Get(), CliKey, Value))
		{
			Out = Value;
			return;
		}
		const FString EnvStr = FPlatformMisc::GetEnvironmentVariable(EnvKey);
		if (!EnvStr.IsEmpty() && EnvStr.IsNumeric())
		{
			Out = FCString::Atoi(*EnvStr);
			return;
		}
		if (GConfig)
		{
			int32 Tmp = 0;
			if (GConfig->GetInt(TEXT("Tether"), IniKey, Tmp, GEditorPerProjectIni))
			{
				Out = Tmp;
				return;
			}
		}
	}

	/** sha1(token) first 8 bytes, lowercase hex. Empty input → empty output.
	 *  Not cryptographic — only used so clients can verify "I'm talking to the
	 *  editor that holds this token" without leaking the token itself.
	 *  X-TOKEN③: this only MATCHES a strong random token; a weak (low-entropy)
	 *  token can be brute-forced offline against this 8-byte prefix, so the
	 *  token must always come from a cryptographic RNG. */
	FString TokenFingerprint(const FString& Token)
	{
		if (Token.IsEmpty())
		{
			return FString();
		}
		const FTCHARToUTF8 Utf8(*Token);
		FSHA1 Hasher;
		Hasher.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		Hasher.Final();
		uint8 Digest[20];
		Hasher.GetHash(Digest);
		return BytesToHex(Digest, 8).ToLower();
	}

}

void FTetherModule::StartupModule()
{
	TetherDebugState::Register();
	TetherPerfFrameHook::Register();

	// Map /Plugin/Tether/ -> this plugin's Shaders/ dir so UMaterialExpressionCustom
	// nodes can #include "/Plugin/Tether/TetherSnippets.ush" and friends.
	// N-F9: UE 5.7's RenderCore has no RemoveShaderSourceDirectoryMapping (only
	// ResetAllShaderSourceDirectoryMappings, which would wipe unrelated
	// mappings), so hot-reload re-entry must instead be made idempotent at the
	// Add site: skip when the mapping already points at this plugin's dir.
	// AllShaderSourceDirectoryMappings() is safe to read here (GameThread).
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Tether"));
		if (Plugin.IsValid())
		{
			const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"), TEXT("Private"));
			if (FPaths::DirectoryExists(ShaderDir))
			{
				const FString* Existing = AllShaderSourceDirectoryMappings().Find(TEXT("/Plugin/Tether"));
				if (!Existing || *Existing != ShaderDir)
				{
					AddShaderSourceDirectoryMapping(TEXT("/Plugin/Tether"), ShaderDir);
					UE_LOG(LogTetherModule, Log,
						TEXT("registered shader dir '%s' under /Plugin/Tether"), *ShaderDir);
				}
			}
		}
	}

	// ---- resolve config (CLI > env > ini > default) ---------------------
	FString BindStr = TEXT("127.0.0.1");
	int32 Port = 0;                   // 0 = OS-assigned ephemeral (discovery tells clients)
	FString Token;
	FString DiscoveryGroup = TEXT("239.255.42.99:9876");
	int32 DiscoveryEnabled = 1;

	ResolveStringConfig(TEXT("TetherBind="), TEXT("TETHER_BIND"), TEXT("Bind"), BindStr);
	ResolveIntConfig(TEXT("TetherPort="), TEXT("TETHER_PORT"), TEXT("Port"), Port);
	ResolveStringConfig(TEXT("TetherToken="), TEXT("TETHER_TOKEN"), TEXT("Token"), Token);
	ResolveStringConfig(TEXT("TetherDiscoveryGroup="), TEXT("TETHER_DISCOVERY_GROUP"),
		TEXT("DiscoveryGroup"), DiscoveryGroup);
	ResolveIntConfig(TEXT("TetherDiscoveryEnabled="), TEXT("TETHER_DISCOVERY"),
		TEXT("DiscoveryEnabled"), DiscoveryEnabled);

	// Accept `-TetherNoDiscovery` as a convenient shorthand toggle.
	if (FParse::Param(FCommandLine::Get(), TEXT("TetherNoDiscovery")))
	{
		DiscoveryEnabled = 0;
	}

	// ---- parse bind + discovery group ---------------------------------
	FIPv4Address BindAddress = FIPv4Address(127, 0, 0, 1);
	if (!FIPv4Address::Parse(BindStr, BindAddress))
	{
		UE_LOG(LogTetherModule, Warning,
			TEXT("invalid -TetherBind='%s' — falling back to 127.0.0.1"), *BindStr);
		BindAddress = FIPv4Address(127, 0, 0, 1);
		BindStr = TEXT("127.0.0.1");
	}

	FIPv4Endpoint DiscoveryEndpoint(FIPv4Address(239, 255, 42, 99), 9876);
	if (!FIPv4Endpoint::Parse(DiscoveryGroup, DiscoveryEndpoint))
	{
		UE_LOG(LogTetherModule, Warning,
			TEXT("invalid -TetherDiscoveryGroup='%s' — using 239.255.42.99:9876"), *DiscoveryGroup);
		DiscoveryEndpoint = FIPv4Endpoint(FIPv4Address(239, 255, 42, 99), 9876);
	}

	// ---- start the TCP server -----------------------------------------
	Server = MakeShared<FTetherServer, ESPMode::ThreadSafe>();
	FTetherServer::FStartConfig StartCfg;
	StartCfg.BindAddress = BindAddress;
	StartCfg.Port = Port;
	StartCfg.Token = Token;

	if (!Server->Start(StartCfg))
	{
		UE_LOG(LogTetherModule, Error,
			TEXT("server failed to start (bind=%s port=%d) — tether disabled this session"),
			*BindStr, Port);
		Server.Reset();
		return;
	}

	UE_LOG(LogTetherModule, Log,
		TEXT("Server up on %s:%d%s (protocol=%d instance=%s pid=%d project=%s)"),
		*Server->GetBoundAddress(), Server->GetBoundPort(),
		Server->HasToken() ? TEXT(" (token enforced)") : TEXT(""),
		Server->GetProtocolVersion(), *Server->GetInstanceId(), Server->GetProcessId(),
		*Server->GetProjectPath());

	if (Server->HasToken())
	{
		// Write the token to a predictable place so the client can consume it
		// without copy-pasting from the log. 0 on the mode because UE's
		// FFileHelper doesn't expose ACL control — the Saved/ folder is
		// already user-scoped.
		const FString TokenPath = FPaths::Combine(FPaths::ProjectSavedDir(),
			TEXT("Tether"), TEXT("token.txt"));
		// X-TOKEN①: a swallowed write failure would leave the client unable
		// to authenticate while the log still claims success — check the
		// return value and only announce the path on success.
		if (FFileHelper::SaveStringToFile(Token, *TokenPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogTetherModule, Log, TEXT("token written to %s"), *TokenPath);
		}
		else
		{
			UE_LOG(LogTetherModule, Error,
				TEXT("failed to write token to %s — clients cannot read it from disk; "
					"pass the token explicitly or fix the directory permissions"),
				*TokenPath);
		}
	}

	// ---- start the discovery responder --------------------------------
	if (DiscoveryEnabled != 0)
	{
		FTetherDiscoveryService::FConfig DiscCfg;
		DiscCfg.GroupAddress = DiscoveryEndpoint.Address;
		DiscCfg.GroupPort = DiscoveryEndpoint.Port;
		DiscCfg.TcpBindAddress = Server->GetBoundAddress();
		DiscCfg.TcpPort = Server->GetBoundPort();
		DiscCfg.ProjectName = FApp::GetProjectName();
		DiscCfg.ProjectPath = Server->GetProjectPath();
		DiscCfg.ProtocolVersion = Server->GetProtocolVersion();
		DiscCfg.InstanceId = Server->GetInstanceId();
		DiscCfg.ProcessId = Server->GetProcessId();
		DiscCfg.EngineVersion = FEngineVersion::Current().ToString();
		DiscCfg.TokenFingerprint = TokenFingerprint(Token);

		Discovery = MakeUnique<FTetherDiscoveryService>(DiscCfg);
		if (!Discovery->StartService())
		{
			UE_LOG(LogTetherModule, Warning,
				TEXT("discovery failed to start — direct clients need the full endpoint/instance/pid/project tuple from the Server up line"));
			Discovery.Reset();
		}
	}
	else
	{
		UE_LOG(LogTetherModule, Log,
			TEXT("discovery disabled (opt-out) — direct clients need the full endpoint/instance/pid/project tuple from the Server up line"));
	}

	// ---- editor-ready gate --------------------------------------------
	TWeakPtr<FTetherServer, ESPMode::ThreadSafe> WeakServer = Server;
	auto OnMainFrameReady = [WeakServer](TSharedPtr<SWindow>, bool)
	{
		if (TSharedPtr<FTetherServer, ESPMode::ThreadSafe> Pinned = WeakServer.Pin())
		{
			Pinned->SetEditorReady(true);
		}
	};

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	if (MainFrame.IsWindowInitialized())
	{
		Server->SetEditorReady(true);
	}
	else
	{
		MainFrameReadyHandle = MainFrame.OnMainFrameCreationFinished().AddLambda(OnMainFrameReady);
	}
}

void FTetherModule::ShutdownModule()
{
	if (MainFrameReadyHandle.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("MainFrame")))
	{
		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrame.OnMainFrameCreationFinished().Remove(MainFrameReadyHandle);
		MainFrameReadyHandle.Reset();
	}

	// N-F9: no RemoveShaderSourceDirectoryMapping exists in UE 5.7's RenderCore
	// (only ResetAllShaderSourceDirectoryMappings, which would wipe unrelated
	// plugins' mappings). Duplicate prevention is handled at the Add site in
	// StartupModule by re-checking AllShaderSourceDirectoryMappings().

	TetherPerfSampler::Shutdown();
	TetherPerfFrameHook::Unregister();
	TetherDebugState::Unregister();

	if (Discovery.IsValid())
	{
		Discovery->StopService();
		Discovery.Reset();
	}

	if (Server.IsValid())
	{
		Server->Stop();
		Server.Reset();
		UE_LOG(LogTetherModule, Log, TEXT("Server stopped."));
	}
}

IMPLEMENT_MODULE(FTetherModule, Tether)
