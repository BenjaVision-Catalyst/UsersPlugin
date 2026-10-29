#pragma once
#include "CCObjectReplica.h"
#include "Model/CULiveUserValues.h"
#include "UObject/Object.h"
#include "CULiveUser.generated.h"
class UCUUser;
class UCUSubsystem;
UENUM(BlueprintType)
enum class ECUUserFabricError : uint8 { None, Refused, Conflict, Unknown, Capacity, NotFound, Stopped, IncompatibleState };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCULiveUserChanged, UCUUser*, User);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCULiveUserError, UCUUser*, User, ECUUserFabricError, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCULiveUserReady, UCUUser*, User);

/** Stable, observed native User. The Fabric adopts this exact UObject after confirmed creation. */
UCLASS(BlueprintType)
class CATALYSTUSERS_API UCUUser : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") FString GetUserName() const { return Name; }
    UFUNCTION(BlueprintCallable, Category="Catalyst|Users") void SetUserName(const FString& Value);
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") FString GetUserIdentifier() const { return Replica.Identifier; }
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") int64 GetRevision() const { return Replica.Revision; }
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") bool IsSuperUser() const { return Accepted.bIsSuperUser; }
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") ECCObjectReplicaPhase GetSyncState() const { return Replica.Phase; }
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") bool HasPendingEdits() const { return Replica.HasEdits(); }
    UPROPERTY(BlueprintAssignable, Category="Catalyst|Users") FCULiveUserChanged OnChanged;
    UPROPERTY(BlueprintAssignable, Category="Catalyst|Users") FCULiveUserError OnError;
    std::shared_ptr<magritte::Description> MagritteDescription() const;
private:
    friend class UCUSubsystem;
    FString Name, CreationKey, CreationName;
    uint64 CreationSequence = 0;
    FCCObjectReplicaState Replica;
    FCULiveUserState Accepted;
    ECUUserFabricError LastError = ECUUserFabricError::None;
    TWeakObjectPtr<UCUSubsystem> Context;
    bool Install(const FCULiveUserState& State, TOptional<uint64> Sent = {});
};

/** Optional one-shot observation. Dropping it does not cancel the Fabric-owned operation. */
UCLASS(BlueprintType)
class CATALYSTUSERS_API UCUUserOperation : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") bool IsFinished() const { return bFinished; }
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") UCUUser* GetUser() const { return User; }
    UFUNCTION(BlueprintPure, Category="Catalyst|Users") ECUUserFabricError GetError() const { return Error; }
    UPROPERTY(BlueprintAssignable, Category="Catalyst|Users") FCULiveUserReady OnReady;
    UPROPERTY(BlueprintAssignable, Category="Catalyst|Users") FCULiveUserError OnError;
private:
    friend class UCUSubsystem;
    UPROPERTY() TObjectPtr<UCUUser> User;
    bool bFinished = false, bCreate = false;
    ECUUserFabricError Error = ECUUserFabricError::None;
    FCUUserChange Request;
    void Finish(UCUUser* Value, ECUUserFabricError Failure);
};
