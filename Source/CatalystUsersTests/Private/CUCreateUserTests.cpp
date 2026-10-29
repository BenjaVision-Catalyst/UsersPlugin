#include "CUPlatformProbe.h"
#include "CCObjectEndpoint.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FInstance
{
    TStrongObjectPtr<UGameInstance> Game{NewObject<UGameInstance>()};
    FInstance() { Game->Init(); }
    ~FInstance() { Game->Shutdown(); }
};
struct FMismatchedUser : FCUUser
{
    std::shared_ptr<magritte::Description> MagritteDescription() const
    {
        auto D = FCUUser::MagritteDescription(); D->properties["label"] = magritte::Value("incompatible local description"); return D;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCUCreateUserTest, "Catalyst.Users.Native.CreateUser",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCUCreateUserTest::RunTest(const FString&)
{
    FString Path, Directory, Json;
    if (!FParse::Value(FCommandLine::Get(), TEXT("CUFixture="), Path) ||
        !FParse::Value(FCommandLine::Get(), TEXT("CUOutput="), Directory) || !FFileHelper::LoadFileToString(Json, *Path))
    { AddError(TEXT("Use the managed GemStone platform qualification runner")); return false; }
    TSharedPtr<FJsonObject> Options;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Options)) { return false; }
    FString Prefix = Options->GetStringField(TEXT("prefix"));
    FCCConnectProfile Profile;
    Profile.BindAddress = TEXT("127.0.0.1:0"); Profile.PeerAddress = Options->GetStringField(TEXT("peer"));
    Profile.ServerName = TEXT("localhost"); Profile.Capacity = 4;
    Profile.CertificatePath = Options->GetStringField(TEXT("certificate"));
    Profile.PrivateKeyPath = Options->GetStringField(TEXT("private_key"));
    Profile.TrustedCertificatePath = Options->GetStringField(TEXT("trusted_certificate"));
    FInstance Instance;
    auto* Connect = Instance.Game->GetSubsystem<UCCSubsystem>();
    auto* Users = Instance.Game->GetSubsystem<UCUSubsystem>();
    if (!Connect || !Users) { AddError(TEXT("GameInstance did not install its subsystems")); return false; }
    const FName Name(TEXT("Users"));
    TestTrue(TEXT("Configured transport"), Connect->ConfigureConnection(Name, Profile) == ECCConnectResult::Ok);
    TestTrue(TEXT("Start"), Connect->StartConnection(Name) == ECCConnectResult::Ok);
    auto Wait = [&](TFunction<bool()> Done) {
        const double Deadline = FPlatformTime::Seconds() + 25;
        while (!Done() && FPlatformTime::Seconds() < Deadline) { Connect->Tick(0); FPlatformProcess::SleepNoStats(.001f); }
        return Done();
    };
    if (!Wait([&] { return Connect->GetConnectionState(Name) == ECCConnectState::Ready; })) { AddError(TEXT("Secure connection unavailable")); return false; }
    TestTrue(TEXT("Select connection"), Users->UseConnection(Name));
    TStrongObjectPtr<UCUPlatformProbe> Probe{NewObject<UCUPlatformProbe>(Instance.Game.Get())};
    FCUUsersReady Ready; Ready.BindDynamic(Probe.Get(), &UCUPlatformProbe::Prepared);
    FCUUserCreated Created; Created.BindDynamic(Probe.Get(), &UCUPlatformProbe::Created);
    FCUUser Input; Input.Name = Prefix + TEXT(":not-ready");
    Users->CreateUser(Input, Created);
    TestTrue(TEXT("Readiness required"), Probe->Outcome == ECUUserOutcome::Unavailable);
    Users->Prepare(Ready);
    if (!Wait([&] { return Probe->ReadyCalls == 1; }) || !Probe->bReady) { AddError(TEXT("Magritte/service probe failed")); return false; }
    FString Error; auto Schema = FCCObjectSchema::Freeze(*Input.MagritteDescription(), Error);
    TestTrue(TEXT("Native Magritte revision agrees"), Schema && Schema->Revision() == TEXT("742282f3bd6d47c824c6c12a2244dcb79ac5b13ba1d94aa4bd60b843608185cb"));
    auto Wrong = TCCObjectEndpoint<FMismatchedUser, FCUUser>::Create(Connect, Name, 1);
    bool WrongFinished = false, WrongReady = true;
    Wrong->Prepare(Users, [&](bool Agreed) { WrongReady = Agreed; WrongFinished = true; });
    TestTrue(TEXT("Mismatched description rejected"), Wait([&] { return WrongFinished; }) && !WrongReady);

    Input.Name = Prefix + TEXT(":hold"); const int32 Before = Probe->Replies;
    Users->CreateUser(Input, Created);
    if (!Wait([&] { return Probe->Replies == Before + 1; })) { AddError(TEXT("Held createUser did not finish")); return false; }
    TestTrue(TEXT("Committed native result"), Probe->Outcome == ECUUserOutcome::Completed && Probe->Value.Name == Input.Name && !Probe->Value.bIsSuperUser);
    if (Probe->Outcome != ECUUserOutcome::Completed) { return false; }
    // The outside controller verifies this marker appears only after the gate is released.
    FFileHelper::SaveStringToFile(TEXT("{\"nativeType\":\"FCUUser\",\"completed\":true}"), *(Directory / TEXT("client-success.json")));
    Input.Name = Prefix + TEXT(":rollback");
    int32 Count = Probe->Replies; Users->CreateUser(Input, Created);
    TestTrue(TEXT("Post-insertion rollback cannot return a user"), Wait([&] { return Probe->Replies == Count + 1; }) && Probe->Outcome == ECUUserOutcome::Unknown);
    Input.Name = Prefix + TEXT(":super"); Input.bIsSuperUser = true;
    Count = Probe->Replies; Users->CreateUser(Input, Created);
    TestTrue(TEXT("Super-user rejection cannot return a user"), Wait([&] { return Probe->Replies == Count + 1; }) && Probe->Outcome == ECUUserOutcome::Unknown);
    Connect->StopConnection(Name); Connect->StartConnection(Name);
    TestTrue(TEXT("Explicit reconnect"), Wait([&] { return Connect->GetConnectionState(Name) == ECCConnectState::Ready; }));
    Input.Name = Prefix + TEXT(":stale"); Input.bIsSuperUser = false; Users->CreateUser(Input, Created);
    TestTrue(TEXT("Old agreement cannot cross activations"), Probe->Outcome == ECUUserOutcome::Unavailable);
    Users->Prepare(Ready);
    TestTrue(TEXT("Fresh description/service probe"), Wait([&] { return Probe->ReadyCalls == 2; }) && Probe->bReady);
    Input.Name = Options->GetStringField(TEXT("reconnectedName")); Count = Probe->Replies; Users->CreateUser(Input, Created);
    TestTrue(TEXT("Reconnected native result"), Wait([&] { return Probe->Replies == Count + 1; }) && Probe->Outcome == ECUUserOutcome::Completed && Probe->Value.Name == Input.Name);
    return true;
}
#endif
