#pragma once
#include "CoreMinimal.h"
#include <memory>
#include "CUUser.generated.h"
namespace magritte { struct Description; }

USTRUCT(BlueprintType)
struct CATALYSTUSERS_API FCUUser
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Catalyst|Users")
    FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Catalyst|Users")
    bool bIsSuperUser = false;
    std::shared_ptr<magritte::Description> MagritteDescription() const;
};
