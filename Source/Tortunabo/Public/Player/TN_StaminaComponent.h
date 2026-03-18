#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_StaminaComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TORTUNABO_API UTN_StaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_StaminaComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Stamina")
	void SetSprintRequested(bool bRequested);

	UFUNCTION(BlueprintCallable, Category = "Stamina")
	void GrantUnlimitedStamina(float DurationSeconds);

	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetCurrentStamina() const { return CurrentStamina; }

	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool HasUnlimitedStamina() const { return bUnlimitedStamina; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float SprintDrainPerSecond = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float RechargeDelaySeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float RechargeBasePerSecond = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float RechargeExponentGrowth = 1.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina|Visual")
	float SprintMeshPitchDegrees = 75.0f;

private:
	UFUNCTION(Server, Reliable)
	void ServerSetSprintRequested(bool bRequested);

	UFUNCTION(Server, Reliable)
	void ServerGrantUnlimitedStamina(float DurationSeconds);

	UPROPERTY(ReplicatedUsing = OnRep_CurrentStamina)
	float CurrentStamina = 100.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(Replicated)
	bool bSprintRequested = false;

	UPROPERTY(ReplicatedUsing = OnRep_UnlimitedStamina)
	bool bUnlimitedStamina = false;

	float UnlimitedStaminaRemaining = 0.0f;
	float RechargeElapsed = 0.0f;
	float TimeSinceSprintStopped = 0.0f;
	FRotator CachedMeshRelativeRotation = FRotator::ZeroRotator;
	bool bHasCachedMeshRotation = false;

	UFUNCTION()
	void OnRep_CurrentStamina();

	UFUNCTION()
	void OnRep_IsSprinting();

	UFUNCTION()
	void OnRep_UnlimitedStamina();

	void TickUnlimitedTimer(float DeltaTime);
	void TickStamina(float DeltaTime);
	void RecomputeSprintState();
	void ApplyMovementSpeed() const;
	void ApplySprintVisual() const;
};

