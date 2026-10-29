#include "Model/CULiveUserValues.h"
#include "Model/CUUserLookup.h"
#include "CCMagritte.h"
#include "Containers/StringConv.h"
namespace
{
using V = magritte::Value;
std::string Text(const FString& S) { FTCHARToUTF8 B(*S, S.Len()); return std::string(B.Get(), B.Length()); }
FString Text(const V& V) { const auto& S = std::get<std::string>(V.data); FUTF8ToTCHAR B(S.data(), S.size()); return FString(B.Length(), B.Get()); }
std::shared_ptr<magritte::Description> Field(const char* Name, const char* Label, bool Number, bool ReadOnly,
    std::function<V(const void*)> Read, std::function<void(void*, const V&)> Write)
{
    auto D = std::make_shared<magritte::Description>(); D->id = std::string("Fabric.") + Name;
    D->className = Number ? "MANumberDescription" : "MAStringDescription";
    magritte::Accessor A; A.className = "MASelectorAccessor"; A.name = Name; A.writeName = std::string(Name) + ":"; D->accessor = A;
    D->properties["label"] = V(Label); D->properties["required"] = V(true); D->properties["readOnly"] = V(ReadOnly);
    if (Number) { D->properties["min"] = V(int64(0)); D->properties["kind"] = V(V::Class{"Integer"}); }
    D->nativeRead = MoveTemp(Read); D->nativeWrite = MoveTemp(Write); return D;
}
}
std::shared_ptr<magritte::Description> FCUUserChange::MagritteDescription() const
{
    auto Root = FCUUserLookup{}.MagritteDescription();
    Root->children[0]->nativeRead = [](const void* O) { return V(Text(static_cast<const FCUUserChange*>(O)->Name)); };
    Root->children[0]->nativeWrite = [](void* O, const V& V) { static_cast<FCUUserChange*>(O)->Name = Text(V); };
    Root->children.push_back(Field("userIdentifier", "User identity", false, false,
        [](const void* O) { return V(Text(static_cast<const FCUUserChange*>(O)->UserIdentifier)); },
        [](void* O, const V& V) { static_cast<FCUUserChange*>(O)->UserIdentifier = Text(V); }));
    Root->children.push_back(Field("operationIdentifier", "Operation identity", false, false,
        [](const void* O) { return V(Text(static_cast<const FCUUserChange*>(O)->OperationIdentifier)); },
        [](void* O, const V& V) { static_cast<FCUUserChange*>(O)->OperationIdentifier = Text(V); }));
    Root->children.push_back(Field("expectedRevision", "Expected revision", true, false,
        [](const void* O) { return V(static_cast<const FCUUserChange*>(O)->ExpectedRevision); },
        [](void* O, const V& V) { static_cast<FCUUserChange*>(O)->ExpectedRevision = std::get<int64>(V.data); }));
    return Root;
}
std::shared_ptr<magritte::Description> FCULiveUserState::MagritteDescription() const
{
    auto Root = FCUUser::MagritteDescription();
    for (auto D : Root->children) { if (D->effectiveAccessor().name == "isSuperUser") { D->properties["readOnly"] = V(true); } }
    Root->children.push_back(Field("userIdentifier", "User identity", false, true,
        [](const void* O) { return V(Text(static_cast<const FCULiveUserState*>(O)->UserIdentifier)); },
        [](void* O, const V& V) { static_cast<FCULiveUserState*>(O)->UserIdentifier = Text(V); }));
    Root->children.push_back(Field("revision", "Revision", true, true,
        [](const void* O) { return V(static_cast<const FCULiveUserState*>(O)->Revision); },
        [](void* O, const V& V) { static_cast<FCULiveUserState*>(O)->Revision = std::get<int64>(V.data); }));
    Root->children.push_back(Field("outcome", "Outcome", false, true,
        [](const void* O) { return V(Text(static_cast<const FCULiveUserState*>(O)->Outcome)); },
        [](void* O, const V& V) { static_cast<FCULiveUserState*>(O)->Outcome = Text(V); }));
    return Root;
}
