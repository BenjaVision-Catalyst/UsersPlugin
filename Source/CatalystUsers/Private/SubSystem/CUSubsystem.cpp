#include "SubSystem/CUSubsystem.h"
#include "CCObjectEndpoint.h"
#include "Model/CUUserLookup.h"
#include "Engine/GameInstance.h"
class FCCUsersEndpoint
{
public:
    TSharedPtr<TCCObjectEndpoint<FCUUser, FCUUser>> Binding;
    TSharedPtr<TCCObjectEndpoint<FCUUserLookup, FCUUser>> Lookup;
};
UCUSubsystem::~UCUSubsystem() = default;
void UCUSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{ Collection.InitializeDependency<UCCSubsystem>(); Super::Initialize(Collection); }
void UCUSubsystem::Deinitialize() { Endpoint.Reset(); Super::Deinitialize(); }
bool UCUSubsystem::UseConnection(FName Name)
{
    check(IsInGameThread());
    auto Bound = TCCObjectEndpoint<FCUUser, FCUUser>::Create(GetGameInstance()->GetSubsystem<UCCSubsystem>(), Name, 1);
    auto Lookup = TCCObjectEndpoint<FCUUserLookup, FCUUser>::Create(GetGameInstance()->GetSubsystem<UCCSubsystem>(), Name, 2);
    if (!Bound || !Lookup) { return false; }
    Endpoint = MakeShared<FCCUsersEndpoint>(); Endpoint->Binding = Bound; Endpoint->Lookup = Lookup; return true;
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
void UCUSubsystem::CreateUser(const FCUUser& User, FCUUserCreated Callback)
{
    check(IsInGameThread());
    if (!Endpoint) { Callback.ExecuteIfBound(ECUUserOutcome::Unavailable, FCUUser{}); return; }
    Endpoint->Binding->Call(this, User, [Callback](TCCObjectResult<FCUUser> Result) {
        Callback.ExecuteIfBound(static_cast<ECUUserOutcome>(Result.Outcome), Result.Value.IsSet() ? Result.Value.GetValue() : FCUUser{});
    });
}

void UCUSubsystem::GetUser(const FString& Name, FCUUserFound Callback)
{
    check(IsInGameThread());
    if (!Endpoint) { Callback.ExecuteIfBound(ECUUserOutcome::Unavailable, FCUUser{}); return; }
    FCUUserLookup Query; Query.Name = Name;
    Endpoint->Lookup->Call(this, Query, [Callback](TCCObjectResult<FCUUser> Result) {
        Callback.ExecuteIfBound(static_cast<ECUUserOutcome>(Result.Outcome), Result.Value.IsSet() ? Result.Value.GetValue() : FCUUser{});
    });
}
