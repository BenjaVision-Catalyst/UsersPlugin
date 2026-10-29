#pragma once
#include "CCObjectEndpoint.h"
#include "Model/CUUserLookup.h"
#include "Model/CULiveUserValues.h"
#include "Model/CULiveUser.h"
class FCCUsersEndpoint
{
public:
    TSharedPtr<TCCObjectEndpoint<FCUUser, FCUUser>> Binding;
    TSharedPtr<TCCObjectEndpoint<FCUUserLookup, FCUUser>> Lookup;
    TArray<TSharedPtr<TCCObjectEndpoint<FCUUserChange, FCULiveUserState>>> Live;
    FName Connection;
    uint64 Generation = 0;
    bool bPreparing = false, bBusy = false;
    double NextRefresh = 0;
    TArray<TWeakObjectPtr<UCUUser>> Refresh;
    int32 RefreshIndex = 0;
};
