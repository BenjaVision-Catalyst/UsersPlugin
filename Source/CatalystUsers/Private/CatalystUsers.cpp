#include "Modules/ModuleManager.h"
class FCUModule final : public IModuleInterface { public: bool SupportsDynamicReloading() override { return false; } };
IMPLEMENT_MODULE(FCUModule, CatalystUsers)
