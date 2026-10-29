#include "Model/CUUserLookup.h"
#include "Model/CUUser.h"
#include "CCMagritte.h"
#include "Containers/StringConv.h"
std::shared_ptr<magritte::Description> FCUUserLookup::MagritteDescription() const
{
    using V = magritte::Value;
    auto Root = FCUUser{}.MagritteDescription();
    auto NameField = Root->children.front();
    Root->children = {NameField};
    NameField->nativeRead = [](const void* Object) {
        const auto& Name = static_cast<const FCUUserLookup*>(Object)->Name;
        FTCHARToUTF8 Text(*Name, Name.Len()); return V(std::string(Text.Get(), Text.Length()));
    };
    NameField->nativeWrite = [](void* Object, const V& Value) {
        const auto& Text = std::get<std::string>(Value.data); FUTF8ToTCHAR Converted(Text.data(), Text.size());
        static_cast<FCUUserLookup*>(Object)->Name = FString(Converted.Length(), Converted.Get());
    };
    return Root;
}
