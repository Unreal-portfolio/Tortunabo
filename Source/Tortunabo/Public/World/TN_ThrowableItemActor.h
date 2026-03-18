#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_ThrowableItemActor.generated.h"

class UProjectileMovementComponent;
class UStaticMeshComponent;

UCLASS()
class TORTUNABO_API ATN_ThrowableItemActor : public AActor
{
	GENERATED_BODY()

public:
	ATN_ThrowableItemActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Throwable")
	void InitializeThrow(const FVector& SpawnLocation, const FVector& InitialVelocity);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable")
	float MaxLifeSeconds = 8.0f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_ThrowData)
	FVector ReplicatedSpawnLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_ThrowData)
	FVector ReplicatedLaunchVelocity = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_ThrowData)
	bool bLaunchDataReady = false;

	bool bLaunchApplied = false;

	UFUNCTION()
	void OnRep_ThrowData();

	void ApplyLaunchDataIfReady();
};

