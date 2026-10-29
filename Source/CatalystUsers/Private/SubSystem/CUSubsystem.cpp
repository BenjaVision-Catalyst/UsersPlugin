#include "SubSystem/CUSubsystem.h"
#include "CCObjectEndpoint.h"
#include "Model/CUUserLookup.h"
#include "Engine/GameInstance.h"
#include "CUUsersEndpoint.h"
UCUSubsystem::UCUSubsystem() : FTickableGameObject(ETickableTickType::Never) {}
UCUSubsystem::~UCUSubsystem() = default;
void UCUSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{ Collection.InitializeDependency<UCCSubsystem>(); Super::Initialize(Collection); bInitialized = true; SetTickableTickType(ETickableTickType::Conditional); }
void UCUSubsystem::Deinitialize()
{
    bInitialized = false; SetTickableTickType(ETickableTickType::Never);
    auto Pending = Operations; Operations.Empty();
    for (auto Operation : Pending) { FailOperation(Operation, ECUUserFabricError::Unknown); }
    for (auto Weak : Tracked) { if (auto* User = Weak.Get()) { User->Replica.Phase = ECCObjectReplicaPhase::Stale; User->OnChanged.Broadcast(User); } }
    Tracked.Empty(); Identities.Empty(); Endpoint.Reset(); Super::Deinitialize();
}
bool UCUSubsystem::UseConnection(FName Name)
{
    check(IsInGameThread());
    if (Operations.Num() || Tracked.ContainsByPredicate([](const auto& W) { return W.IsValid(); })) { return false; }
    auto Bound = TCCObjectEndpoint<FCUUser, FCUUser>::Create(GetGameInstance()->GetSubsystem<UCCSubsystem>(), Name, 1);
    auto Lookup = TCCObjectEndpoint<FCUUserLookup, FCUUser>::Create(GetGameInstance()->GetSubsystem<UCCSubsystem>(), Name, 2);
    if (!Bound || !Lookup) { return false; }
    Endpoint = MakeShared<FCCUsersEndpoint>(); Endpoint->Binding = Bound; Endpoint->Lookup = Lookup; Endpoint->Connection = Name;
    for (uint16 Code = 3; Code <= 6; ++Code) {
        auto Live = TCCObjectEndpoint<FCUUserChange, FCULiveUserState>::Create(GetGameInstance()->GetSubsystem<UCCSubsystem>(), Name, Code, ECCObjectProfile::StateV2);
        if (!Live) { Endpoint.Reset(); return false; } Endpoint->Live.Add(Live);
    }
    return true;
}
void UCUSubsystem::Prepare(FCUUsersReady Callback)
{
    check(IsInGameThread());
    if (!Endpoint) { Callback.ExecuteIfBound(false); return; }
    auto Selected = Endpoint;
    TWeakObjectPtr<UCUSubsystem> Owner(this);
    Selected->Binding->Prepare(this, [Selected, Owner, Callback](bool Ready) {
        if (!Owner.IsValid()) { return; }
        if (!Ready || Owner->Endpoint != Selected) { Callback.ExecuteIfBound(false); return; }
        Selected->Lookup->Prepare(Owner.Get(), [Selected, Owner, Callback](bool LookupReady) {
            if (Owner.IsValid()) { Callback.ExecuteIfBound(LookupReady && Owner->Endpoint == Selected); }
        });
    });
}
void UCUSubsystem::CreateUserSnapshot(const FCUUser& User, FCUUserCreated Callback)
{
    check(IsInGameThread());
    if (!Endpoint) { Callback.ExecuteIfBound(ECUUserOutcome::Unavailable, FCUUser{}); return; }
    Endpoint->Binding->Call(this, User, [Callback](TCCObjectResult<FCUUser> Result) {
        Callback.ExecuteIfBound(static_cast<ECUUserOutcome>(Result.Outcome), Result.Value.IsSet() ? Result.Value.GetValue() : FCUUser{});
    });
}

void UCUSubsystem::GetUserSnapshot(const FString& Name, FCUUserFound Callback)
{
    check(IsInGameThread());
    if (!Endpoint) { Callback.ExecuteIfBound(ECUUserOutcome::Unavailable, FCUUser{}); return; }
    FCUUserLookup Query; Query.Name = Name;
    Endpoint->Lookup->Call(this, Query, [Callback](TCCObjectResult<FCUUser> Result) {
        Callback.ExecuteIfBound(static_cast<ECUUserOutcome>(Result.Outcome), Result.Value.IsSet() ? Result.Value.GetValue() : FCUUser{});
    });
}
