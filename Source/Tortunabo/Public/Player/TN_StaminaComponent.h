#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_StaminaComponent.generated.h"

class UTN_InventoryComponent;

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

	/** Vincula el componente de inventario para calcular el peso total cargado. */
	void SetInventoryComponent(UTN_InventoryComponent* InvComp);

	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetCurrentStamina() const { return CurrentStamina; }

	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	/**
	 * Stamina máxima efectiva tras aplicar la penalización por peso.
	 * EffectiveMax = MaxStamina - (TotalWeight * StaminaPerWeightUnit).
	 * La stamina no puede superar este valor mientras se lleva peso.
	 */
	UFUNCTION(BlueprintPure, Category = "Stamina|Weight")
	float GetEffectiveMaxStamina() const;

	/**
	 * Stamina "bloqueada" por el peso: MaxStamina - EffectiveMaxStamina.
	 * Usar en la UI como relleno de la barra de penalización (zona oscura).
	 */
	UFUNCTION(BlueprintPure, Category = "Stamina|Weight")
	float GetWeightPenalty() const { return MaxStamina - GetEffectiveMaxStamina(); }

	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool HasUnlimitedStamina() const { return bUnlimitedStamina; }

	/** True mientras la stamina está penalizada por haberse agotado completamente. */
	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool IsExhausted() const { return bIsExhausted; }

protected:
	/** Stamina máxima base. Configurable desde Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "1.0"))
	float MaxStamina = 200.0f;

	/**
	 * Stamina máxima que se reduce por cada unidad de peso cargado.
	 * Ejemplo: StaminaPerWeightUnit=20, ítem con ItemWeight=2 → -40 stamina máx.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Stamina|Weight", meta = (ClampMin = "0.0"))
	float StaminaPerWeightUnit = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float SprintDrainPerSecond = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float RechargeDelaySeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float RechargeBasePerSecond = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float RechargeExponentGrowth = 1.1f;

	/**
	 * Penalización por agotar la stamina completamente.
	 * Bloquea la recuperación este tiempo extra (en segundos) además de RechargeDelaySeconds.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float ExhaustionPenaltySeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina|Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 800.0f;

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
	float ExhaustionTimer = 0.0f;

	UPROPERTY(Replicated)
	bool bIsExhausted = false;

	/** Referencia al inventario del propietario — necesaria para calcular el peso. */
	TWeakObjectPtr<UTN_InventoryComponent> InventoryComponentRef;

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

