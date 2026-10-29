#include "Model/CUUser.h"
#include "CCMagritte.h"
#include "Containers/StringConv.h"
std::shared_ptr<magritte::Description> FCUUser::MagritteDescription() const
{
    using V = magritte::Value;
    auto Root = std::make_shared<magritte::Description>(); Root->id = "CUUser"; Root->className = "MAContainer";
    Root->properties["label"] = V("");
    auto NameField = std::make_shared<magritte::Description>(); NameField->id = "CUUser.name";
    NameField->className = "MAStringDescription";
    magritte::Accessor NameAccessor; NameAccessor.className = "MASelectorAccessor"; NameAccessor.name = "name"; NameAccessor.writeName = "name:";
    NameField->accessor = NameAccessor;
    NameField->properties["label"] = V("Name"); NameField->properties["required"] = V(true);
    NameField->nativeRead = [](const void* Object) {
        const auto& Name = static_cast<const FCUUser*>(Object)->Name;
        FTCHARToUTF8 Text(*Name, Name.Len()); return V(std::string(Text.Get(), Text.Length()));
    };
    NameField->nativeWrite = [](void* Object, const V& Value) {
        const auto& Text = std::get<std::string>(Value.data); FUTF8ToTCHAR Converted(Text.data(), Text.size());
        static_cast<FCUUser*>(Object)->Name = FString(Converted.Length(), Converted.Get());
    };
    auto Super = std::make_shared<magritte::Description>(); Super->id = "CUUser.isSuperUser"; Super->className = "MABooleanDescription";
    magritte::Accessor SuperAccessor; SuperAccessor.className = "MASelectorAccessor"; SuperAccessor.name = "isSuperUser"; SuperAccessor.writeName = "isSuperUser:";
    Super->accessor = SuperAccessor; Super->properties["label"] = V("Super user"); Super->properties["required"] = V(true);
    Super->nativeRead = [](const void* Object) { return V(static_cast<const FCUUser*>(Object)->bIsSuperUser); };
    Super->nativeWrite = [](void* Object, const V& Value) { static_cast<FCUUser*>(Object)->bIsSuperUser = std::get<bool>(Value.data); };
    Root->children = {NameField, Super}; return Root;
}
