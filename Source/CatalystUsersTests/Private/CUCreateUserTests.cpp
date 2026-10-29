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
    UCCSubsystem* RemoteConnect = nullptr; UCUSubsystem* RemoteUsers = nullptr;
    auto Wait = [&](TFunction<bool()> Done) {
        const double Deadline = FPlatformTime::Seconds() + 25;
        while (!Done() && FPlatformTime::Seconds() < Deadline) { Connect->Tick(0); Users->Tick(0); if (RemoteConnect) { RemoteConnect->Tick(0); RemoteUsers->Tick(0); } FPlatformProcess::SleepNoStats(.001f); }
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
    FCUUserFound Found; Found.BindDynamic(Probe.Get(), &UCUPlatformProbe::Created);
    Users->GetUser(TEXT("admin"), Found);
    TestTrue(TEXT("Lookup readiness required"), Probe->Outcome == ECUUserOutcome::Unavailable);
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
    auto Lookup = [&](const FString& Query, ECUUserOutcome Expected, const FString& ResultName, bool Super = false) {
        const int32 Replies = Probe->Replies;
        Users->GetUser(Query, Found);
        const bool Finished = Wait([&] { return Probe->Replies == Replies + 1; });
        TestTrue(TEXT("Lookup outcome"), Finished && Probe->Outcome == Expected);
        if (Expected == ECUUserOutcome::Completed) {
            TestEqual(TEXT("Lookup native name"), Probe->Value.Name, ResultName);
            TestTrue(TEXT("Lookup native super-user flag"), Probe->Value.bIsSuperUser == Super);
        } else { TestTrue(TEXT("Non-success has no user"), Probe->Value.Name.IsEmpty() && !Probe->Value.bIsSuperUser); }
    };
    Lookup(Input.Name, ECUUserOutcome::Completed, Input.Name);
    Lookup(Prefix + TEXT(":missing"), ECUUserOutcome::NotFound, TEXT(""));
    Lookup(Input.Name.ToUpper(), ECUUserOutcome::NotFound, TEXT(""));
    Lookup(TEXT("admin"), ECUUserOutcome::Completed, TEXT("admin"), true);
    Lookup(TEXT("unknownUser"), ECUUserOutcome::Completed, TEXT("unknownUser"));
    Input.Name = Prefix + TEXT(":duplicate");
    for (int32 I = 0; I < 2; ++I) {
        int32 Replies = Probe->Replies; Users->CreateUser(Input, Created);
        TestTrue(TEXT("Duplicate-name fixture created"), Wait([&] { return Probe->Replies == Replies + 1; }) && Probe->Outcome == ECUUserOutcome::Completed);
    }
    Lookup(Input.Name, ECUUserOutcome::Refused, TEXT(""));
    Input.Name = Prefix + TEXT(":rollback");
    int32 Count = Probe->Replies; Users->CreateUser(Input, Created);
    TestTrue(TEXT("Post-insertion rollback cannot return a user"), Wait([&] { return Probe->Replies == Count + 1; }) && Probe->Outcome == ECUUserOutcome::Unknown);
    Input.Name = Prefix + TEXT(":super"); Input.bIsSuperUser = true;
    Count = Probe->Replies; Users->CreateUser(Input, Created);
    TestTrue(TEXT("Super-user rejection cannot return a user"), Wait([&] { return Probe->Replies == Count + 1; }) && Probe->Outcome == ECUUserOutcome::Unknown);
    Lookup(Prefix + TEXT(":rollback"), ECUUserOutcome::NotFound, TEXT(""));
    Connect->StopConnection(Name); Connect->StartConnection(Name);
    TestTrue(TEXT("Explicit reconnect"), Wait([&] { return Connect->GetConnectionState(Name) == ECCConnectState::Ready; }));
    Input.Name = Prefix + TEXT(":stale"); Input.bIsSuperUser = false; Users->CreateUser(Input, Created);
    TestTrue(TEXT("Old agreement cannot cross activations"), Probe->Outcome == ECUUserOutcome::Unavailable);
    Users->GetUser(TEXT("admin"), Found);
    TestTrue(TEXT("Lookup cannot reuse stale agreement"), Probe->Outcome == ECUUserOutcome::Unavailable);
    Users->Prepare(Ready);
    TestTrue(TEXT("Fresh description/service probe"), Wait([&] { return Probe->ReadyCalls == 2; }) && Probe->bReady);
    Input.Name = Options->GetStringField(TEXT("reconnectedName")); Count = Probe->Replies; Users->CreateUser(Input, Created);
    TestTrue(TEXT("Reconnected native result"), Wait([&] { return Probe->Replies == Count + 1; }) && Probe->Outcome == ECUUserOutcome::Completed && Probe->Value.Name == Input.Name);
    Lookup(Input.Name, ECUUserOutcome::Completed, Input.Name);
    FCULiveUserPolicy Policy; Policy.bAutomaticRefresh = false; Policy.RefreshInterval = .05f;
    TestTrue(TEXT("Manual refresh policy"), Users->ConfigureLiveUsers(Policy));
    TStrongObjectPtr<UCUUser> Live{NewObject<UCUUser>(Instance.Game.Get())}; Live->SetUserName(Prefix + TEXT(":live"));
    TStrongObjectPtr<UCUUserOperation> Adopt{Users->CreateUser(Live.Get())};
    Live->SetUserName(Prefix + TEXT(":live-edited"));
    TestTrue(TEXT("Confirmed creation adopts original object"), Wait([&] { return Adopt->IsFinished(); }) && Adopt->GetError() == ECUUserFabricError::None && Adopt->GetUser() == Live.Get());
    TestTrue(TEXT("Identity and newer local edit retained"), !Live->GetUserIdentifier().IsEmpty() && Live->GetRevision() == 1 && Live->HasPendingEdits());
    Users->RefreshLiveUsers(); TestTrue(TEXT("Pending edit commits"), Wait([&] { return !Live->HasPendingEdits(); }) && Live->GetRevision() == 2);
    TStrongObjectPtr<UCUUserOperation> Cached{Users->GetUser(Live->GetUserName())};
    TestTrue(TEXT("Lookup shares scoped identity map"), Wait([&] { return Cached->IsFinished(); }) && Cached->GetUser() == Live.Get());
    FInstance RemoteInstance;
    RemoteConnect = RemoteInstance.Game->GetSubsystem<UCCSubsystem>(); RemoteUsers = RemoteInstance.Game->GetSubsystem<UCUSubsystem>();
    RemoteConnect->ConfigureConnection(Name, Profile); RemoteConnect->StartConnection(Name);
    TestTrue(TEXT("Second native scope ready"), Wait([&] { return RemoteConnect->GetConnectionState(Name) == ECCConnectState::Ready; }));
    RemoteUsers->UseConnection(Name); FCULiveUserPolicy RemotePolicy = Policy; RemotePolicy.bAutomaticRefresh = true; RemoteUsers->ConfigureLiveUsers(RemotePolicy);
    TStrongObjectPtr<UCUUserOperation> LookedUp{RemoteUsers->GetUser(Live->GetUserName())};
    if (!Wait([&] { return LookedUp->IsFinished(); }) || LookedUp->GetError() != ECUUserFabricError::None) { AddError(TEXT("Remote lookup failed")); return false; }
    TStrongObjectPtr<UCUUser> Other{LookedUp->GetUser()};
    TestTrue(TEXT("Identity stable across scopes"), Other.Get() != Live.Get() && Other->GetUserIdentifier() == Live->GetUserIdentifier());
    Live->SetUserName(Prefix + TEXT(":live-remote")); Users->RefreshLiveUsers();
    TestTrue(TEXT("Automatic remote refresh"), Wait([&] { return Other->GetUserName() == Live->GetUserName(); }));
    Live->SetUserName(Prefix + TEXT(":live-local-draft")); Other->SetUserName(Prefix + TEXT(":live-authority"));
    TestTrue(TEXT("Automatic edit submission"), Wait([&] { return !Other->HasPendingEdits(); }));
    Users->RefreshLiveUsers();
    TestTrue(TEXT("Conflict preserves local draft"), Wait([&] { return Live->GetSyncState() == ECCObjectReplicaPhase::Conflict; }) && Live->GetUserName() == Prefix + TEXT(":live-local-draft"));
    Users->ResolveConflict(Live.Get(), false);
    TestTrue(TEXT("Explicit remote resolution"), Live->GetUserName() == Prefix + TEXT(":live-authority") && !Live->HasPendingEdits());
    RemotePolicy.bAutomaticRefresh = false; RemoteUsers->ConfigureLiveUsers(RemotePolicy); RemoteConnect->StopConnection(Name);
    TStrongObjectPtr<UCUUser> Rollback{NewObject<UCUUser>(Instance.Game.Get())}; Rollback->SetUserName(Prefix + TEXT(":live-rollback"));
    TStrongObjectPtr<UCUUserOperation> RollbackOperation{Users->CreateUser(Rollback.Get())};
    TestTrue(TEXT("Live failed transaction remains unknown"), Wait([&] { return RollbackOperation->IsFinished(); }) && RollbackOperation->GetError() == ECUUserFabricError::Unknown);
    Users->RefreshLiveUsers();
    TStrongObjectPtr<UCUUserOperation> RollbackLookup{Users->GetUser(Rollback->GetUserName())};
    TestTrue(TEXT("Rolled back live object absent"), Wait([&] { return RollbackLookup->IsFinished(); }) && RollbackLookup->GetError() == ECUUserFabricError::NotFound);
    Policy.bAutomaticRefresh = true; Users->ConfigureLiveUsers(Policy);
    TStrongObjectPtr<UCUUser> Lost{NewObject<UCUUser>(Instance.Game.Get())}; Lost->SetUserName(Prefix + TEXT(":lost-create"));
    TStrongObjectPtr<UCUUserOperation> LostOperation{Users->CreateUser(Lost.Get())};
    const double CloseAt = FPlatformTime::Seconds() + 1;
    Wait([&] { return FPlatformTime::Seconds() >= CloseAt; }); Connect->StopConnection(Name);
    TestTrue(TEXT("Lost reply is unknown"), Wait([&] { return LostOperation->IsFinished(); }) && LostOperation->GetError() == ECUUserFabricError::Unknown && Lost->GetUserIdentifier().IsEmpty());
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"prefix\":\"%s\"}"), *Prefix), *(Directory / TEXT("live-disconnected.json")));
    Lost->SetUserName(Prefix + TEXT(":recovered-edit")); Connect->StartConnection(Name);
    TestTrue(TEXT("Recovery automatically adopts original object and submits newer edit"), Wait([&] { return !Lost->GetUserIdentifier().IsEmpty() && !Lost->HasPendingEdits(); }) && Lost->GetSyncState() == ECCObjectReplicaPhase::Live && Lost->GetUserName() == Prefix + TEXT(":recovered-edit"));
    TStrongObjectPtr<UCUUserOperation> Recovered{Users->GetUser(Lost->GetUserName())};
    TestTrue(TEXT("Recovered identity cached"), Wait([&] { return Recovered->IsFinished(); }) && Recovered->GetUser() == Lost.Get());
    FCCObjectReplicaState Uncertain; Uncertain.Accept(TEXT("id"), 1); Uncertain.Edited(); Uncertain.Phase = ECCObjectReplicaPhase::Uncertain;
    Uncertain.Accept(TEXT("id"), 1); TestTrue(TEXT("A read never authorizes replay"), Uncertain.Phase == ECCObjectReplicaPhase::Uncertain);
    Uncertain.Accept(TEXT("id"), 2); TestTrue(TEXT("A changed read never authorizes replay"), Uncertain.Phase == ECCObjectReplicaPhase::Uncertain);
    UE_LOG(LogTemp, Display, TEXT("CU_LIVE_RESULT %s adopted stable-identity shared-cache auto-refresh conflict lost-create-recovered rollback-absent"), *Prefix);
    return true;
}
#endif
