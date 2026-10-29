#include "SubSystem/CUSubsystem.h"
#include "CUUsersEndpoint.h"
#include "Engine/GameInstance.h"
#include "Log/CFLog.h"
#include "Misc/ScopeExit.h"
bool UCUSubsystem::ConfigureLiveUsers(const FCULiveUserPolicy& Policy)
{
    if (Policy.MaximumObjects < FMath::Max(1, Tracked.Num()) || Policy.MaximumObjects > 256 ||
        !FMath::IsFinite(Policy.RefreshInterval) || Policy.RefreshInterval < .02f) { return false; }
    if (!Policy.bAutomaticRefresh) { ResetFeed(); }
    LivePolicy = Policy; if (Endpoint) { Endpoint->NextRefresh = 0; Endpoint->Refresh.Empty(); Endpoint->RefreshIndex = 0; } return true;
}
void UCUSubsystem::Report(UCUUser* User, ECUUserFabricError Error)
{
    if (User) {
        if (User->LastError == Error) { return; } User->LastError = Error; User->OnError.Broadcast(User, Error);
    }
    if (OnFabricError.IsBound()) { OnFabricError.Broadcast(User, Error); }
    else { CF_WARN(TEXT("Users Fabric outcome %d"), int32(Error)); }
}
void UCUSubsystem::FailOperation(UCUUserOperation* Operation, ECUUserFabricError Error)
{
    if (auto* User = Operation->User.Get(); User && Operation->bCreate) {
        User->Replica.Phase = Error == ECUUserFabricError::Unknown ? ECCObjectReplicaPhase::Uncertain : ECCObjectReplicaPhase::Refused;
        User->OnChanged.Broadcast(User);
    }
    Report(Operation->User, Error); Operation->Finish(Operation->User, Error);
}
bool UCUSubsystem::Track(UCUUser* User)
{
    Tracked.RemoveAll([](const auto& W) { return !W.IsValid(); });
    if (!Tracked.Contains(User)) {
        if (Tracked.Num() >= LivePolicy.MaximumObjects) { return false; } Tracked.Add(User);
    }
    User->Context = this; return true;
}
bool UCUSubsystem::Install(UCUUser* User, const FCULiveUserState& State, TOptional<uint64> Sent)
{
    auto* Other = Identities.Find(State.UserIdentifier);
    if ((Other && Other->IsValid() && Other->Get() != User) || !User->Install(State, Sent)) { return false; }
    Identities.Add(State.UserIdentifier, User);
    if (User->Replica.Phase == ECCObjectReplicaPhase::Conflict) { Report(User, ECUUserFabricError::Conflict); }
    else { User->LastError = ECUUserFabricError::None; }
    return true;
}
UCUUserOperation* UCUSubsystem::CreateUser(UCUUser* User)
{
    check(IsInGameThread());
    auto* Operation = NewObject<UCUUserOperation>(this); Operation->User = User; Operation->bCreate = true;
    if (!bInitialized || !Endpoint || !IsValid(User) || User->Context.IsValid() ||
        !User->Replica.Identifier.IsEmpty()) { FailOperation(Operation, ECUUserFabricError::Refused); return Operation; }
    if (Operations.Num() >= LivePolicy.MaximumObjects || !Track(User)) { FailOperation(Operation, ECUUserFabricError::Capacity); return Operation; }
    bLiveActivated = true;
    User->CreationKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    User->CreationName = User->Name; User->CreationSequence = User->Replica.EditSequence; User->Replica.Phase = ECCObjectReplicaPhase::Creating;
    Operation->Request.Name = User->CreationName; Operation->Request.OperationIdentifier = User->CreationKey;
    Operations.Add(Operation); User->OnChanged.Broadcast(User); return Operation;
}
UCUUserOperation* UCUSubsystem::GetUser(const FString& Name)
{
    check(IsInGameThread());
    auto* Operation = NewObject<UCUUserOperation>(this);
    if (!bInitialized || !Endpoint) { FailOperation(Operation, ECUUserFabricError::Stopped); return Operation; }
    if (Operations.Num() >= LivePolicy.MaximumObjects) { FailOperation(Operation, ECUUserFabricError::Capacity); return Operation; }
    Operation->Request.Name = Name; Operation->Request.OperationIdentifier = FGuid::NewGuid().ToString();
    Operations.Add(Operation); bLiveActivated = true; return Operation;
}
void UCUSubsystem::PrepareLive(int32 Index, uint64 Generation)
{
    auto Selected = Endpoint;
    if (Index == 0) { ResetFeed(); }
    if (Index == Selected->Live.Num()) { Selected->Generation = Generation; Selected->bPreparing = false; return; }
    TWeakObjectPtr<UCUSubsystem> Weak(this);
    Selected->Live[Index]->Prepare(this, [Weak, Selected, Index, Generation](bool Ready) {
        auto* Self = Weak.Get(); if (!Self || Self->Endpoint != Selected || !Self->bInitialized) { return; }
        auto* Connect = Self->GetGameInstance()->GetSubsystem<UCCSubsystem>();
        if (!Ready || Connect->GetConnectionGeneration(Selected->Connection) != Generation) {
            Selected->bPreparing = false; Selected->NextRefresh = FPlatformTime::Seconds() + Self->LivePolicy.RefreshInterval;
            auto Pending = Self->Operations; Self->Operations.Empty();
            for (auto Operation : Pending) { Self->FailOperation(Operation, ECUUserFabricError::Refused); }
            return;
        }
        Self->PrepareLive(Index + 1, Generation);
    });
}
void UCUSubsystem::SendOperation(UCUUserOperation* Operation)
{
    auto Selected = Endpoint; Selected->bBusy = true;
    TWeakObjectPtr<UCUSubsystem> Weak(this);
    Selected->Live[Operation->bCreate ? 0 : 1]->Call(this, Operation->Request,
        [Weak, Selected, Operation](TCCObjectResult<FCULiveUserState> Result) {
            auto* Self = Weak.Get(); if (!Self || Self->Endpoint != Selected || !Self->bInitialized) { return; }
            Selected->bBusy = false;
            ECUUserFabricError Error = ECUUserFabricError::None;
            UCUUser* User = Operation->User;
            if (Result.Outcome != ECCObjectOutcome::Completed || !Result.Value.IsSet()) {
                Error = Result.Outcome == ECCObjectOutcome::Unknown ? ECUUserFabricError::Unknown : ECUUserFabricError::Refused;
            } else {
                const auto& State = Result.Value.GetValue();
                if (State.Outcome == TEXT("notFound")) { Error = ECUUserFabricError::NotFound; }
                else if (State.Outcome != (Operation->bCreate ? TEXT("created") : TEXT("current"))) { Error = ECUUserFabricError::Refused; }
                else {
                    if (!User) {
                        if (auto* Existing = Self->Identities.Find(State.UserIdentifier)) { User = Existing->Get(); }
                        if (!User) { User = NewObject<UCUUser>(Self->GetGameInstance()); }
                        if (!Self->Track(User)) { Error = ECUUserFabricError::Capacity; }
                    }
                    if (Error == ECUUserFabricError::None && !Self->Install(User, State, Operation->bCreate ? TOptional<uint64>(User->CreationSequence) : TOptional<uint64>())) { Error = ECUUserFabricError::IncompatibleState; }
                }
            }
            if (Error == ECUUserFabricError::None) { Operation->Finish(User, Error); }
            else { Self->FailOperation(Operation, Error); }
            Self->Operations.Remove(Operation);
        });
}
void UCUSubsystem::RefreshOne(UCUUser* User)
{
    auto Selected = Endpoint; Selected->bBusy = true;
    const bool Recovering = User->Replica.Identifier.IsEmpty();
    const bool Updating = !Recovering && User->Replica.HasEdits() &&
        (User->Replica.Phase == ECCObjectReplicaPhase::Live || User->Replica.Phase == ECCObjectReplicaPhase::Stale);
    const bool Subscribing = !Recovering && !Updating && LivePolicy.bAutomaticRefresh && !Selected->Subscribed.Contains(User->Replica.Identifier);
    if (Recovering && !Selected->bExplicitRefresh && Selected->RecoveryAttempted.Contains(User)) { Selected->bBusy = false; return; }
    if (Recovering) { Selected->RecoveryAttempted.Add(User); }
    if (!Recovering && !Updating && !Subscribing && !Selected->bExplicitRefresh) { Selected->bBusy = false; return; }
    if (Updating) { Selected->Updating.Add(User); }
    FCUUserChange Request;
    Request.UserIdentifier = User->Replica.Identifier; Request.Name = User->Name; Request.ExpectedRevision = User->Replica.EditBase;
    Request.OperationIdentifier = Recovering ? User->CreationKey : FGuid::NewGuid().ToString();
    if (Subscribing) {
        Request.OperationIdentifier = Selected->FeedKey; Request.ExpectedRevision = User->Replica.Revision;
        Selected->Subscribed.Add(User->Replica.Identifier); // Push and watch replies use independent streams.
    }
    const uint64 Sequence = User->Replica.EditSequence;
    TWeakObjectPtr<UCUSubsystem> Weak(this); TWeakObjectPtr<UCUUser> Target(User);
    Selected->Live[Recovering ? 3 : Updating ? 2 : Subscribing ? 4 : 1]->Call(this, Request,
        [Weak, Selected, Target, Recovering, Updating, Subscribing, Sequence](TCCObjectResult<FCULiveUserState> Result) {
            auto* Self = Weak.Get(); auto* User = Target.Get();
            if (!Self || Self->Endpoint != Selected || !Self->bInitialized) { return; }
            Selected->bBusy = false;
            if (!User || User->Context.Get() != Self) { return; }
            if (Subscribing) { Selected->Subscribed.Remove(User->Replica.Identifier); }
            ON_SCOPE_EXIT {
                Selected->Updating.Remove(User);
                if (auto* Deferred = Selected->DeferredStates.Find(User); Deferred && Deferred->Revision > User->Replica.Revision) { Self->Install(User, *Deferred); }
                Selected->DeferredStates.Remove(User);
            };
            if (Result.Outcome != ECCObjectOutcome::Completed || !Result.Value.IsSet()) {
                User->Replica.Phase = ECCObjectReplicaPhase::Uncertain; Selected->Generation = 0;
                Self->Report(User, ECUUserFabricError::Unknown); User->OnChanged.Broadcast(User); return;
            }
            const auto& State = Result.Value.GetValue();
            if (State.Outcome == TEXT("unknown") && Recovering) { return; }
            if (State.Outcome == TEXT("notFound")) { User->Replica.Phase = ECCObjectReplicaPhase::NotFound; User->OnChanged.Broadcast(User); return; }
            if (State.Outcome == TEXT("refused")) { User->Replica.Phase = ECCObjectReplicaPhase::Refused; Self->Report(User, ECUUserFabricError::Refused); return; }
            if (!(Recovering ? State.Outcome == TEXT("created") : Updating ?
                State.Outcome == TEXT("updated") || State.Outcome == TEXT("conflict") : State.Outcome == TEXT("current"))) {
                User->Replica.Phase = ECCObjectReplicaPhase::Uncertain; Self->Report(User, ECUUserFabricError::IncompatibleState); return;
            }
            if (Subscribing) {
                Selected->Subscribed.Add(User->Replica.Identifier);
                if (State.Revision < User->Replica.Revision) { return; }
            }
            TOptional<uint64> Sent;
            if (Recovering && State.Outcome == TEXT("created")) { Sent = User->CreationSequence; }
            if (Updating && State.Outcome == TEXT("updated")) { Sent = Sequence; }
            if (!Self->Install(User, State, Sent)) { User->Replica.Phase = ECCObjectReplicaPhase::Uncertain; Self->Report(User, ECUUserFabricError::IncompatibleState); }
            if (State.Outcome == TEXT("conflict")) { User->Replica.Phase = ECCObjectReplicaPhase::Conflict; Self->Report(User, ECUUserFabricError::Conflict); }
        });
}
void UCUSubsystem::Tick(float)
{
    check(IsInGameThread());
    if (!IsTickable() || !Endpoint) { return; }
    auto Selected = Endpoint;
    auto* Connect = GetGameInstance()->GetSubsystem<UCCSubsystem>();
    const uint64 Generation = Connect->GetConnectionGeneration(Selected->Connection);
    if (!Generation) {
        Selected->Generation = 0;
        for (auto Weak : Tracked) { if (auto* User = Weak.Get(); User && User->Replica.Phase == ECCObjectReplicaPhase::Live) { User->Replica.Phase = ECCObjectReplicaPhase::Stale; User->OnChanged.Broadcast(User); } }
        return;
    }
    if (!Selected->bPreparing && Generation == Selected->Generation) { ReceiveChanges(); }
    if (Selected->bBusy || Selected->bPreparing) { return; }
    const double Now = FPlatformTime::Seconds();
    if (Generation != Selected->Generation) {
        if (Now >= Selected->NextRefresh) { Selected->bPreparing = true; PrepareLive(0, Generation); }
        return;
    }
    for (auto Operation : Operations) { if (!Operation->IsFinished()) { SendOperation(Operation); return; } }
    Tracked.RemoveAll([](const auto& Weak) { return !Weak.IsValid(); });
    for (auto It = Identities.CreateIterator(); It; ++It) {
        if (!It.Value().IsValid()) {
            const FString Identifier = It.Key(); It.RemoveCurrent();
            if (Selected->Subscribed.Remove(Identifier)) {
                Selected->bBusy = true; FCUUserChange Change; Change.OperationIdentifier = Selected->FeedKey; Change.UserIdentifier = Identifier;
                Selected->Live[6]->Call(this, Change, [Selected](TCCObjectResult<FCULiveUserState>) { Selected->bBusy = false; }); return;
            }
        }
    }
    if (Selected->Subscribed.IsEmpty() && Selected->bReceiving) { ResetFeed(); }
    if (Selected->RefreshIndex >= Selected->Refresh.Num()) {
        if ((!LivePolicy.bAutomaticRefresh && !bRefreshRequested) || Now < Selected->NextRefresh) { return; }
        Selected->Refresh = Tracked; Selected->RefreshIndex = 0; Selected->bExplicitRefresh = bRefreshRequested; bRefreshRequested = false;
        Selected->NextRefresh = Now + LivePolicy.RefreshInterval;
    }
    while (Selected->RefreshIndex < Selected->Refresh.Num()) {
        auto* User = Selected->Refresh[Selected->RefreshIndex++].Get();
        if (!User || User->Replica.Phase == ECCObjectReplicaPhase::Creating || User->Replica.Phase == ECCObjectReplicaPhase::Refused) { continue; }
        RefreshOne(User); return;
    }
}
void UCUSubsystem::RefreshLiveUsers()
{ bRefreshRequested = true; if (Endpoint) { Endpoint->NextRefresh = 0; Endpoint->Refresh.Empty(); Endpoint->RefreshIndex = 0; } }
void UCUSubsystem::ResolveConflict(UCUUser* User, bool KeepLocal)
{
    if (!User || User->Context.Get() != this) { return; }
    if (KeepLocal) { User->Replica.RebaseLocal(); }
    else { User->Replica.AcceptRemote(); User->Name = User->Accepted.Name; }
    User->LastError = ECUUserFabricError::None; User->OnChanged.Broadcast(User); if (KeepLocal) { RefreshLiveUsers(); }
}

void UCUSubsystem::ResetFeed()
{
    if (!Endpoint) { return; } auto Selected = Endpoint;
    ++Selected->FeedGeneration;
    if (Selected->ReceiveRequest.RequestId) {
        GetGameInstance()->GetSubsystem<UCCSubsystem>()->RetireRequest(Selected->Connection, Selected->ReceiveRequest.RequestId, Selected->ReceiveRequest.Generation);
    }
    Selected->ReceiveRequest = {}; Selected->bReceiving = false; Selected->FeedKey = FGuid::NewGuid().ToString();
    Selected->Subscribed.Empty(); Selected->RecoveryAttempted.Empty();
}
void UCUSubsystem::ReceiveChanges()
{
    auto Selected = Endpoint;
    if (!LivePolicy.bAutomaticRefresh || Selected->bBusy || Selected->bReceiving || Selected->Subscribed.IsEmpty()) { return; }
    FCUUserChange Change; Change.OperationIdentifier = Selected->FeedKey;
    const uint64 Stamp = Selected->FeedGeneration; Selected->bReceiving = true;
    TWeakObjectPtr<UCUSubsystem> Weak(this);
    Selected->ReceiveRequest = Selected->Live[5]->Call(this, Change, [Weak, Selected, Stamp](TCCObjectResult<FCULiveUserState> Result) {
        auto* Self = Weak.Get();
        if (!Self || !Self->bInitialized || Self->Endpoint != Selected || Stamp != Selected->FeedGeneration) { return; }
        if (Result.Outcome != ECCObjectOutcome::Completed || !Result.Value.IsSet()) {
            for (auto Target : Self->Tracked) { if (auto* User = Target.Get()) {
                if (User->Replica.Phase == ECCObjectReplicaPhase::Live) { User->Replica.Phase = ECCObjectReplicaPhase::Stale; }
                Self->Report(User, ECUUserFabricError::Unknown);
            } }
            Selected->Generation = 0; Self->ResetFeed(); return;
        }
        const auto& State = Result.Value.GetValue();
        if (State.Outcome == TEXT("subscribed")) { return; }
        if (State.Outcome != TEXT("current")) { Selected->Generation = 0; Self->ResetFeed(); return; }
        if (!Selected->Subscribed.Contains(State.UserIdentifier)) { return; } // Retired watches may have queued events.
        if (auto* WeakUser = Self->Identities.Find(State.UserIdentifier)) {
            if (auto* User = WeakUser->Get()) {
                if (Selected->Updating.Contains(User)) {
                    const auto* Deferred = Selected->DeferredStates.Find(User);
                    if (!Deferred || State.Revision > Deferred->Revision) { Selected->DeferredStates.Add(User, State); }
                }
                else if (State.Revision > User->Replica.Revision) { Self->Install(User, State); }
            }
        }
    }, true);
}
