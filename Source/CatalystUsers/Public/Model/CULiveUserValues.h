#pragma once
#include "Model/CUUser.h"

struct CATALYSTUSERS_API FCUUserChange
{
    FString UserIdentifier, OperationIdentifier, Name;
    int64 ExpectedRevision = 0;
    std::shared_ptr<magritte::Description> MagritteDescription() const;
};
struct CATALYSTUSERS_API FCULiveUserState : FCUUser
{
    FString UserIdentifier, Outcome;
    int64 Revision = 0;
    std::shared_ptr<magritte::Description> MagritteDescription() const;
};
