#pragma once
#include "SubSystem/CUSubsystem.h"
#include "UObject/Object.h"
#include "CUPlatformProbe.generated.h"
UCLASS()
class UCUPlatformProbe : public UObject
{
    GENERATED_BODY()
public:
    bool bReady = false;
    int32 ReadyCalls = 0, Replies = 0;
    ECUUserOutcome Outcome = ECUUserOutcome::Unknown;
    FCUUserSnapshot Value;
    UFUNCTION() void Prepared(bool Ready) { bReady = Ready; ++ReadyCalls; }
    UFUNCTION() void Created(ECUUserOutcome Result, const FCUUserSnapshot& User) { Outcome = Result; Value = User; ++Replies; }
};
