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

	/** Restaura la stamina al máximo efectivo e invalida la penalización de agotamiento. */
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	void RestoreStaminaToFull();

	/** Sobreescribe en tiempo de ejecución la penalización post-boost (útil para ítems con distinto penalty). */
	void SetPostBoostExhaustionSeconds(float NewValue) { PostBoostExhaustionSeconds = FMath::Max(0.f, NewValue); }

	/**
	 * Limita MaxWalkSpeed a Cap mientras sea activo (ej. zona de ralentización).
	 * ApplyMovementSpeed lo respeta: MaxWalkSpeed = Min(WalkSpeed|SprintSpeed, ActiveSpeedCap).
	 * Llamar en todas las máquinas (sin HasAuthority) — cada una aplica localmente.
	 */
	void SetSpeedCap(float Cap);
	void ClearSpeedCap();

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

	/** True si el efecto post-boost está activo (penalización tras expirar stamina ilimitada). */
	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool IsPostBoostPenalized() const { return bPostBoostPenaltyActive; }

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

	/**
	 * Segundos de penalización al expirar la stamina ilimitada (Barrita Energética).
	 * Durante este tiempo: MaxWalkSpeed *= PostBoostSpeedMultiplier y el sprint drena
	 * a PostBoostDrainMultiplier × ritmo normal. La recarga NO se bloquea.
	 * 0 = sin penalización. Configurable por BP.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
	float PostBoostExhaustionSeconds = 4.0f;

	/** Multiplicador de velocidad máxima mientras dura la penalización post-boost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PostBoostSpeedMultiplier = 0.75f;

	/** Multiplicador de drenaje de stamina al sprintar durante la penalización. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "1.0"))
	float PostBoostDrainMultiplier = 2.0f;

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
	float PostBoostPenaltyTimer = 0.0f;

	UPROPERTY(Replicated)
	bool bIsExhausted = false;

	UPROPERTY(Replicated)
	bool bPostBoostPenaltyActive = false;

	/** Referencia al inventario del propietario — necesaria para calcular el peso. */
	TWeakObjectPtr<UTN_InventoryComponent> InventoryComponentRef;

	/**
	 * Velocidad máxima impuesta por zonas externas (ej. TN_SlowZoneVolume).
	 * MAX_FLT = sin límite activo. ApplyMovementSpeed hace Min(baseSpeed, cap).
	 */
	float ActiveSpeedCap = TNumericLimits<float>::Max();

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

