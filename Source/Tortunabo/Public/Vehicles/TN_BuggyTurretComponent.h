// Torreta del buggy: apuntado replicado, calentamiento del coco, munición especial y disparo con retroceso. Todo lo
// decide el servidor; los clientes solo piden por RPC validada (ATN_Buggy para la conductora sola,
// ATN_BuggyGunnerPawn para la artillera). Lógica pura en TNRallyTurret (TN_RallyTurretLogic.h).
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Rally/TN_RallyVehicle.h"
#include "Vehicles/TN_RallyTurretLogic.h"
#include "TN_BuggyTurretComponent.generated.h"

class ATN_Buggy;
class ATN_RallyProjectile;

UCLASS(ClassGroup = (Rally), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_BuggyTurretComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	/** Altura del pivote de la torreta sobre el asiento de la artillera (cm). */
	static constexpr float PivotAboveSeatCm = 70.f;
	/** Distancia de la boca del cañón al pivote (cm): el proyectil sale de ahí. */
	static constexpr float MuzzleDistanceCm = 140.f;

	UTN_BuggyTurretComponent();

	// ── HUD (cualquier máquina) ─────────────────────────────────────────────────

	/** Calor del coco en [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Rally|Torreta")
	float GetHeat01() const { return Heat01; }

	UFUNCTION(BlueprintPure, Category = "Rally|Torreta")
	bool IsOverheated() const { return bOverheated; }

	UFUNCTION(BlueprintPure, Category = "Rally|Torreta")
	ETNRallyAmmo GetSpecialAmmo() const { return SpecialAmmo; }

	UFUNCTION(BlueprintPure, Category = "Rally|Torreta")
	int32 GetSpecialCharges() const { return SpecialCharges; }

	/** Apuntado relativo al buggy que se ve en esta máquina (el local para la artillera, el replicado para el resto). */
	FRotator GetDisplayAim() const;

	/** Dirección en mundo del apuntado replicado. */
	FVector GetAimWorldDirection() const;

	// ── Servidor ───────────────────────────────────────────────────────────────

	/** Apuntado relativo (se limita a -10..+45 de cabeceo). */
	void SetAimRelative(const FRotator& RelativeAim);

	/** Dispara la munición básica (coco) o la especial hacia WorldDir. False si la cadencia, el calor o las cargas no lo permiten. */
	bool TryFire(bool bSpecial, const FVector& WorldDir);

	/** Carga de una caja: sustituye a la que hubiera. */
	void GiveSpecial(ETNRallyAmmo Ammo, int32 Charges);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Torreta")
	TSubclassOf<ATN_RallyProjectile> ProjectileClass;

private:
	ATN_Buggy* GetBuggy() const;
	void SyncReplicatedState();

	UPROPERTY(Replicated)
	float AimYaw = 0.f;

	UPROPERTY(Replicated)
	float AimPitch = 0.f;

	UPROPERTY(Replicated)
	float Heat01 = 0.f;

	UPROPERTY(Replicated)
	bool bOverheated = false;

	UPROPERTY(Replicated)
	ETNRallyAmmo SpecialAmmo = ETNRallyAmmo::None;

	UPROPERTY(Replicated)
	int32 SpecialCharges = 0;

	TNRallyTurret::FHeat HeatState;
	TNRallyTurret::FSpecial Special;
	double LastCocoShot = -1000.0;
	double LastSpecialShot = -1000.0;
};
