#pragma once
#include "CoreMinimal.h"
#include <memory>
namespace magritte { struct Description; }

/** Installed exact-name query; the name metadata comes from FCUUser. */
struct CATALYSTUSERS_API FCUUserLookup
{
    FString Name;
    std::shared_ptr<magritte::Description> MagritteDescription() const;
};
