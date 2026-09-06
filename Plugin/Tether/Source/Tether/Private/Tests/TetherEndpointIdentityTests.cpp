#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Core/TetherEndpointIdentity.h"
#include "Core/TetherExactRequestDispatcher.h"
#include "Core/TetherProtocol.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherEndpointIdentityPreconditionTest,
	"Tether.Server.EndpointIdentity.Preconditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherEndpointIdentityPreconditionTest::RunTest(const FString& Parameters)
{
	FTetherEndpointIdentity Identity;
	Identity.InstanceId = TEXT("11111111-2222-3333-4444-555555555555");
	Identity.ProcessId = 4242;
	Identity.ProjectPath = TEXT("C:/Projects/TestProject/TestProject.uproject");

	auto BuildWireRequest = [&Identity](
		const FString& Command,
		double Protocol,
		double Pid,
		const FString& ProjectPath)
	{
		TSharedPtr<FJsonObject> Expected = MakeShared<FJsonObject>();
		Expected->SetNumberField(TetherProtocol::ProtocolVersionField, Protocol);
		Expected->SetStringField(TetherProtocol::InstanceIdField, Identity.InstanceId);
		Expected->SetNumberField(TetherProtocol::ProcessIdField, Pid);
		Expected->SetStringField(TetherProtocol::ProjectPathField, ProjectPath);

		TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetStringField(TetherProtocol::CommandField, Command);
		Request->SetObjectField(TetherProtocol::ExpectedField, Expected);
		Request->SetObjectField(TetherProtocol::RequestField, MakeShared<FJsonObject>());
		return Request;
	};

	auto BuildValidRequest = [&BuildWireRequest, &Identity](const FString& Command)
	{
		return BuildWireRequest(
			Command,
			FTetherEndpointIdentity::ProtocolVersion,
			Identity.ProcessId,
			Identity.ProjectPath);
	};

	struct FCommandCase
	{
		const TCHAR* WireCommand;
		ETetherExactCommand ExpectedCommand;
	};
	const FCommandCase CommandCases[] = {
		{TetherProtocol::ExactExec, ETetherExactCommand::Exec},
		{TetherProtocol::ExactPing, ETetherExactCommand::Ping},
		{TetherProtocol::ExactEditorStatus, ETetherExactCommand::EditorStatus},
		{TetherProtocol::ExactGameThreadPing, ETetherExactCommand::GameThreadPing},
		{TetherProtocol::ExactDebugResume, ETetherExactCommand::DebugResume},
		{TetherProtocol::ExactModalStatus, ETetherExactCommand::ModalStatus},
		{TetherProtocol::ExactModalAction, ETetherExactCommand::ModalAction},
	};

	for (const FCommandCase& CommandCase : CommandCases)
	{
		int32 AdmissionCount = 0;
		ETetherExactCommand ObservedCommand = ETetherExactCommand::Exec;
		FString Code;
		FString Error;
		const bool bAccepted = FTetherExactRequestDispatcher::TryDispatch(
			BuildValidRequest(CommandCase.WireCommand),
			Identity,
			[&AdmissionCount, &ObservedCommand](const FTetherAcceptedRequest& Accepted)
			{
				++AdmissionCount;
				ObservedCommand = Accepted.Command;
			},
			Code,
			Error);
		TestTrue(FString::Printf(TEXT("%s is admitted"), CommandCase.WireCommand), bAccepted);
		TestEqual(FString::Printf(TEXT("%s admits exactly one body"), CommandCase.WireCommand), AdmissionCount, 1);
		TestEqual(FString::Printf(TEXT("%s maps to its typed command"), CommandCase.WireCommand),
			static_cast<uint8>(ObservedCommand), static_cast<uint8>(CommandCase.ExpectedCommand));
	}

	auto ExpectRejected = [this, &Identity](
		const FString& Label,
		const TSharedPtr<FJsonObject>& Request,
		const FString& ExpectedCode)
	{
		int32 AdmissionCount = 0;
		FString Code;
		FString Error;
		const bool bAccepted = FTetherExactRequestDispatcher::TryDispatch(
			Request,
			Identity,
			[&AdmissionCount](const FTetherAcceptedRequest&)
			{
				++AdmissionCount;
			},
			Code,
			Error);
		TestFalse(Label + TEXT(" is rejected"), bAccepted);
		TestEqual(Label + TEXT(" does not enter the body/admission callback"), AdmissionCount, 0);
		TestEqual(Label + TEXT(" has a structured error code"), Code, ExpectedCode);
	};

	ExpectRejected(TEXT("legacy command"), BuildValidRequest(TEXT("ping")), TEXT("exact_command_required"));
	ExpectRejected(TEXT("unknown exact alias"), BuildValidRequest(TEXT("exact_exec_alias")), TEXT("unsupported_command"));
	TSharedPtr<FJsonObject> MissingCommand = BuildValidRequest(TetherProtocol::ExactPing);
	MissingCommand->RemoveField(TetherProtocol::CommandField);
	ExpectRejected(TEXT("missing command"), MissingCommand, TEXT("exact_command_required"));

	TSharedPtr<FJsonObject> MissingExpected = BuildValidRequest(TetherProtocol::ExactPing);
	MissingExpected->RemoveField(TetherProtocol::ExpectedField);
	ExpectRejected(TEXT("missing expected"), MissingExpected, TEXT("identity_required"));
	TSharedPtr<FJsonObject> MalformedExpected = BuildValidRequest(TetherProtocol::ExactPing);
	MalformedExpected->SetStringField(TetherProtocol::ExpectedField, TEXT("not-an-object"));
	ExpectRejected(TEXT("non-object expected"), MalformedExpected, TEXT("identity_required"));

	TSharedPtr<FJsonObject> MalformedProtocolType = BuildValidRequest(TetherProtocol::ExactPing);
	MalformedProtocolType->GetObjectField(TetherProtocol::ExpectedField)->SetStringField(
		TetherProtocol::ProtocolVersionField, TEXT("2"));
	ExpectRejected(TEXT("non-number protocol"), MalformedProtocolType, TEXT("protocol_mismatch"));
	TSharedPtr<FJsonObject> MalformedPidType = BuildValidRequest(TetherProtocol::ExactPing);
	MalformedPidType->GetObjectField(TetherProtocol::ExpectedField)->SetStringField(
		TetherProtocol::ProcessIdField, TEXT("4242"));
	ExpectRejected(TEXT("non-number pid"), MalformedPidType, TEXT("pid_mismatch"));
	TSharedPtr<FJsonObject> MalformedInstanceType = BuildValidRequest(TetherProtocol::ExactPing);
	MalformedInstanceType->GetObjectField(TetherProtocol::ExpectedField)->SetNumberField(
		TetherProtocol::InstanceIdField, 1.0);
	ExpectRejected(TEXT("non-string instance"), MalformedInstanceType, TEXT("identity_required"));
	TSharedPtr<FJsonObject> MalformedPathType = BuildValidRequest(TetherProtocol::ExactPing);
	MalformedPathType->GetObjectField(TetherProtocol::ExpectedField)->SetNumberField(
		TetherProtocol::ProjectPathField, 1.0);
	ExpectRejected(TEXT("non-string project path"), MalformedPathType, TEXT("identity_required"));

	TSharedPtr<FJsonObject> MissingPayload = BuildValidRequest(TetherProtocol::ExactPing);
	MissingPayload->RemoveField(TetherProtocol::RequestField);
	ExpectRejected(TEXT("missing request"), MissingPayload, TEXT("invalid_request"));
	TSharedPtr<FJsonObject> MalformedPayload = BuildValidRequest(TetherProtocol::ExactPing);
	MalformedPayload->SetStringField(TetherProtocol::RequestField, TEXT("not-an-object"));
	ExpectRejected(TEXT("non-object request"), MalformedPayload, TEXT("invalid_request"));

	TSharedPtr<FJsonObject> WrongInstance = BuildValidRequest(TetherProtocol::ExactExec);
	WrongInstance->GetObjectField(TetherProtocol::ExpectedField)->SetStringField(
		TetherProtocol::InstanceIdField, TEXT("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
	ExpectRejected(TEXT("stale instance"), WrongInstance, TEXT("instance_mismatch"));

	ExpectRejected(
		TEXT("project case mismatch"),
		BuildWireRequest(TetherProtocol::ExactExec, FTetherEndpointIdentity::ProtocolVersion,
			Identity.ProcessId, TEXT("c:/Projects/TestProject/TestProject.uproject")),
		TEXT("project_mismatch"));
	ExpectRejected(
		TEXT("project separator mismatch"),
		BuildWireRequest(TetherProtocol::ExactExec, FTetherEndpointIdentity::ProtocolVersion,
			Identity.ProcessId, TEXT("C:\\Projects\\TestProject\\TestProject.uproject")),
		TEXT("project_mismatch"));

	struct FNumberCase
	{
		const TCHAR* Label;
		double Value;
	};
	const FNumberCase InvalidProtocols[] = {
		{TEXT("fractional protocol"), 2.4},
		{TEXT("zero protocol"), 0.0},
		{TEXT("negative protocol"), -2.0},
		{TEXT("non-finite protocol"), std::numeric_limits<double>::quiet_NaN()},
		{TEXT("infinite protocol"), std::numeric_limits<double>::infinity()},
		{TEXT("out-of-range protocol"), static_cast<double>(TNumericLimits<int32>::Max()) + 1.0},
	};
	for (const FNumberCase& NumberCase : InvalidProtocols)
	{
		ExpectRejected(
			NumberCase.Label,
			BuildWireRequest(TetherProtocol::ExactPing, NumberCase.Value, Identity.ProcessId, Identity.ProjectPath),
			TEXT("protocol_mismatch"));
	}

	const FNumberCase InvalidPids[] = {
		{TEXT("fractional pid"), 4242.4},
		{TEXT("zero pid"), 0.0},
		{TEXT("negative pid"), -4242.0},
		{TEXT("non-finite pid"), std::numeric_limits<double>::quiet_NaN()},
		{TEXT("infinite pid"), std::numeric_limits<double>::infinity()},
		{TEXT("out-of-range pid"), static_cast<double>(TNumericLimits<int32>::Max()) + 1.0},
	};
	for (const FNumberCase& NumberCase : InvalidPids)
	{
		ExpectRejected(
			NumberCase.Label,
			BuildWireRequest(TetherProtocol::ExactPing,
				FTetherEndpointIdentity::ProtocolVersion, NumberCase.Value, Identity.ProjectPath),
			TEXT("pid_mismatch"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
