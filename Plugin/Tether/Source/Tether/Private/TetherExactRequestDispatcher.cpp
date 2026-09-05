#include "TetherExactRequestDispatcher.h"

#include "Dom/JsonObject.h"
#include "TetherEndpointIdentity.h"
#include "TetherProtocol.h"

namespace
{
	bool TryMapCommand(const FString& WireCommand, ETetherExactCommand& OutCommand)
	{
		if (WireCommand == TetherProtocol::ExactExec)
		{
			OutCommand = ETetherExactCommand::Exec;
		}
		else if (WireCommand == TetherProtocol::ExactPing)
		{
			OutCommand = ETetherExactCommand::Ping;
		}
		else if (WireCommand == TetherProtocol::ExactEditorStatus)
		{
			OutCommand = ETetherExactCommand::EditorStatus;
		}
		else if (WireCommand == TetherProtocol::ExactGameThreadPing)
		{
			OutCommand = ETetherExactCommand::GameThreadPing;
		}
		else if (WireCommand == TetherProtocol::ExactDebugResume)
		{
			OutCommand = ETetherExactCommand::DebugResume;
		}
		else if (WireCommand == TetherProtocol::ExactModalStatus)
		{
			OutCommand = ETetherExactCommand::ModalStatus;
		}
		else if (WireCommand == TetherProtocol::ExactModalAction)
		{
			OutCommand = ETetherExactCommand::ModalAction;
		}
		else
		{
			return false;
		}
		return true;
	}
}

bool FTetherExactRequestDispatcher::TryDispatch(
	const TSharedPtr<FJsonObject>& WireRequest,
	const FTetherEndpointIdentity& Identity,
	TFunctionRef<void(const FTetherAcceptedRequest&)> OnAccepted,
	FString& OutErrorCode,
	FString& OutError)
{
	OutErrorCode.Reset();
	OutError.Reset();

	FString WireCommand;
	if (!WireRequest.IsValid()
		|| !WireRequest->TryGetStringField(TetherProtocol::CommandField, WireCommand)
		|| !WireCommand.StartsWith(TetherProtocol::ExactPrefix, ESearchCase::CaseSensitive))
	{
		OutErrorCode = TEXT("exact_command_required");
		OutError = TEXT("legacy or unknown wire form rejected; use an exact_* command with expected identity");
		return false;
	}

	if (!Identity.ValidateRequest(WireRequest, OutErrorCode, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
	if (!WireRequest->TryGetObjectField(TetherProtocol::RequestField, PayloadPtr)
		|| PayloadPtr == nullptr
		|| !PayloadPtr->IsValid())
	{
		OutErrorCode = TEXT("invalid_request");
		OutError = TEXT("missing or non-object field 'request'");
		return false;
	}

	ETetherExactCommand Command = ETetherExactCommand::Exec;
	if (!TryMapCommand(WireCommand, Command))
	{
		OutErrorCode = TEXT("unsupported_command");
		OutError = FString::Printf(TEXT("unsupported exact command '%s'"), *WireCommand);
		return false;
	}

	FTetherAcceptedRequest Accepted;
	Accepted.Command = Command;
	Accepted.Payload = *PayloadPtr;
	OnAccepted(Accepted);
	return true;
}
