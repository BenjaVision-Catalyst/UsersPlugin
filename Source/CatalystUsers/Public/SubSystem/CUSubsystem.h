#pragma once
#include "Model/CUUser.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CUSubsystem.generated.h"
class FCCUsersEndpoint;
UENUM(BlueprintType)
enum class ECUUserOutcome : uint8 { Completed, InvalidInput, Unavailable, Refused, Unknown, NotFound };
DECLARE_DYNAMIC_DELEGATE_OneParam(FCUUsersReady, bool, Ready);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCUUserCreated, ECUUserOutcome, Outcome, const FCUUser&, User);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCUUserFound, ECUUserOutcome, Outcome, const FCUUser&, User);

/** Per-GameInstance Users API. The configured Connect connection is shared and caller owned. */
UCLASS()
class CATALYSTUSERS_API UCUSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual ~UCUSubsystem() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users")
    bool UseConnection(FName Name);
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users")
    void Prepare(FCUUsersReady Callback);
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users")
    void CreateUser(const FCUUser& User, FCUUserCreated Callback);
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users")
    void GetUser(const FString& Name, FCUUserFound Callback);
private:
    TSharedPtr<FCCUsersEndpoint> Endpoint;
};
