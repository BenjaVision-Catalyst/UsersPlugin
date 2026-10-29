#include "SubSystem/CUSubsystem.h"
#include "CCObjectEndpoint.h"
#include "Engine/GameInstance.h"
class FCCUsersEndpoint
{
public: TSharedPtr<TCCObjectEndpoint<FCUUser, FCUUser>> Binding;
};
UCUSubsystem::~UCUSubsystem() = default;
void UCUSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{ Collection.InitializeDependency<UCCSubsystem>(); Super::Initialize(Collection); }
void UCUSubsystem::Deinitialize() { Endpoint.Reset(); Super::Deinitialize(); }
bool UCUSubsystem::UseConnection(FName Name)
{
    check(IsInGameThread());
    auto Bound = TCCObjectEndpoint<FCUUser, FCUUser>::Create(GetGameInstance()->GetSubsystem<UCCSubsystem>(), Name, 1);
    if (!Bound) { return false; }
    Endpoint = MakeShared<FCCUsersEndpoint>(); Endpoint->Binding = Bound; return true;
}
void UCUSubsystem::Prepare(FCUUsersReady Callback)
{
    check(IsInGameThread());
    if (!Endpoint) { Callback.ExecuteIfBound(false); return; }
    Endpoint->Binding->Prepare(this, [Callback](bool Ready) { Callback.ExecuteIfBound(Ready); });
}
void UCUSubsystem::CreateUser(const FCUUser& User, FCUUserCreated Callback)
{
    check(IsInGameThread());
    if (!Endpoint) { Callback.ExecuteIfBound(ECUUserOutcome::Unavailable, FCUUser{}); return; }
    Endpoint->Binding->Call(this, User, [Callback](TCCObjectResult<FCUUser> Result) {
        Callback.ExecuteIfBound(static_cast<ECUUserOutcome>(Result.Outcome), Result.Value.IsSet() ? Result.Value.GetValue() : FCUUser{});
    });
}
