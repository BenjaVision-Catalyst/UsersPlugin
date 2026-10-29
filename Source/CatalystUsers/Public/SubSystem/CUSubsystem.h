#pragma once
#include "Model/CUUser.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Model/CULiveUser.h"
#include "CUSubsystem.generated.h"
class FCCUsersEndpoint;
UENUM(BlueprintType)
enum class ECUUserOutcome : uint8 { Completed, InvalidInput, Unavailable, Refused, Unknown, NotFound };
DECLARE_DYNAMIC_DELEGATE_OneParam(FCUUsersReady, bool, Ready);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCUUserCreated, ECUUserOutcome, Outcome, const FCUUserSnapshot&, User);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCUUserFound, ECUUserOutcome, Outcome, const FCUUserSnapshot&, User);

USTRUCT(BlueprintType)
struct CATALYSTUSERS_API FCULiveUserPolicy
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Catalyst|Users") int32 MaximumObjects = 64;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Catalyst|Users") float RefreshInterval = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Catalyst|Users") bool bAutomaticRefresh = true;
};

/** Per-GameInstance Users API. The configured Connect connection is shared and caller owned. */
UCLASS()
class CATALYSTUSERS_API UCUSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    UCUSubsystem();
    virtual ~UCUSubsystem() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users")
    bool UseConnection(FName Name);
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users")
    void Prepare(FCUUsersReady Callback);
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users")
    void CreateUserSnapshot(const FCUUserSnapshot& User, FCUUserCreated Callback);
    void CreateUser(const FCUUserSnapshot& User, FCUUserCreated Callback) { CreateUserSnapshot(User, Callback); }
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users") UCUUserOperation* CreateUser(UCUUser* User);
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users") UCUUserOperation* GetUser(const FString& Name);
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users") bool ConfigureLiveUsers(const FCULiveUserPolicy& Policy);
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users") void RefreshLiveUsers();
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users") void ResolveConflict(UCUUser* User, bool KeepLocal);
    UPROPERTY(BlueprintAssignable, Category="Catalyst|Users") FCULiveUserError OnFabricError;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return bInitialized && bLiveActivated && !IsTemplate(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCUSubsystem, STATGROUP_Tickables); }
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users")
    void GetUserSnapshot(const FString& Name, FCUUserFound Callback);
    void GetUser(const FString& Name, FCUUserFound Callback) { GetUserSnapshot(Name, Callback); }
private:
    TSharedPtr<FCCUsersEndpoint> Endpoint;
    FCULiveUserPolicy LivePolicy;
    bool bInitialized = false, bLiveActivated = false, bRefreshRequested = false;
    TArray<TWeakObjectPtr<UCUUser>> Tracked;
    TMap<FString, TWeakObjectPtr<UCUUser>> Identities;
    UPROPERTY() TArray<TObjectPtr<UCUUserOperation>> Operations;
    void PrepareLive(int32 Index, uint64 Generation);
    void Report(UCUUser* User, ECUUserFabricError Error);
    void FailOperation(UCUUserOperation* Operation, ECUUserFabricError Error);
    bool Track(UCUUser* User);
    bool Install(UCUUser* User, const FCULiveUserState& State, TOptional<uint64> Sent = {});
    void SendOperation(UCUUserOperation* Operation);
    void RefreshOne(UCUUser* User);
};
