#include "Model/CULiveUser.h"
#include "CCMagritte.h"
#include "Containers/StringConv.h"
void UCUUser::SetUserName(const FString& Value)
{
    check(IsInGameThread());
    if (Name != Value) { Name = Value; Replica.Edited(); OnChanged.Broadcast(this); }
}
bool UCUUser::Install(const FCULiveUserState& State, TOptional<uint64> Sent)
{
    if (State.UserIdentifier.IsEmpty() || State.Revision <= 0 ||
        (!Replica.Identifier.IsEmpty() && Replica.Identifier != State.UserIdentifier) || State.Revision < Replica.Revision) { return false; }
    if (Replica.Accept(State.UserIdentifier, State.Revision, Sent)) { Name = State.Name; }
    Accepted = State; OnChanged.Broadcast(this); return true;
}
std::shared_ptr<magritte::Description> UCUUser::MagritteDescription() const
{
    auto Root = Accepted.MagritteDescription();
    for (auto Field : Root->children) {
        auto Read = Field->nativeRead;
        const bool IsName = Field->effectiveAccessor().name == "name";
        Field->nativeRead = [Read, IsName](const void* Object) {
            auto* User = static_cast<const UCUUser*>(Object);
            if (IsName) { FTCHARToUTF8 B(*User->Name, User->Name.Len()); return magritte::Value(std::string(B.Get(), B.Length())); }
            return Read(&User->Accepted);
        };
        Field->nativeWrite = [IsName](void* Object, const magritte::Value& Value) {
            if (!IsName) { throw magritte::Error("Server-owned User field is read-only"); }
            const auto& S = std::get<std::string>(Value.data); FUTF8ToTCHAR B(S.data(), S.size());
            static_cast<UCUUser*>(Object)->SetUserName(FString(B.Length(), B.Get()));
        };
    }
    return Root;
}
void UCUUserOperation::Finish(UCUUser* Value, ECUUserFabricError Failure)
{
    if (bFinished) { return; }
    bFinished = true; User = Value; Error = Failure;
    if (Failure == ECUUserFabricError::None) { OnReady.Broadcast(User); }
    else { OnError.Broadcast(User, Failure); }
}
