#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachSandFleas.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UTN_BeachCritterSynthComponent;

/**
 * Enjambre de pulgas de arena (ETNBeachElement::SandFleas): una nube de 48 pulguitas (~18 cm cada una, 0,65 cm reales
 * a escala) que saltan sin parar sobre la arena. Barato: una sola malla instanciada, sin colisión, que solo se anima
 * con la cámara a menos de 120 m.
 *
 *  - Pasea despacio (0,7 m/s) por su claro; si ve una tortuga atacable a menos de 18 m (y dentro de su correa de 24 m),
 *    va hacia ella despacio (1,6 m/s: se le escapa andando).
 *  - Si la alcanza, se le sube encima: 2 s de saltitos sin control (un impulso cada 0,3 s, 3,2-4,2 m/s hacia arriba y
 *    1,4-2,6 m/s hacia un lado al azar, que le quitan el control), con picor («¡PICA, PICA!»), y al final la deja
 *    mareada 1 s en bola (TNBeach::StunTurtle).
 *  - Luego se dispersa (hasta 6,5 m) y se vuelve a juntar en 3,2 s. Mareado por un golpe, también se dispersa y no se
 *    mueve hasta que se le pasa.
 *
 * Red: el servidor mueve el centro del enjambre (Mover, 8 Hz) y decide a quién pica; la picada va en Infested y la hora
 * del estado. Los saltitos son deterministas (hora del estado y semilla): los aplica el servidor a su copia y el dueño de
 * la tortuga a la suya a la misma hora, así no hay correcciones y no hace falta un RPC por salto. Las pulgas son
 * locales de cada máquina.
 */
UCLASS()
class TORTUNABO_API ATN_BeachSandFleas : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachSandFleas();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const override;
	virtual FVector GetHitStunAnchor() const override;
	virtual float GetHitStunScale() const override;

protected:
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnMoverStateChanged(uint8 OldState) override;
	virtual float GetActiveRange() const override;
	virtual float GetVisualRange() const override { return 12000.f; }

private:
	/** La tortuga a la que están picando (replicada: la animan todas las máquinas y su dueño aplica los saltitos). */
	UPROPERTY(Replicated)
	TObjectPtr<ATortugaCharacter> Infested;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> FleaDots;

	/** Mancha oscura en la arena bajo la nube (para verla de lejos). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Haze;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachCritterSynthComponent> Sound;

	/** Una pulga: salta de From a To (respecto al ancla: el centro del enjambre o la tortuga picada). */
	struct FFlea
	{
		FVector From = FVector::ZeroVector;
		FVector To = FVector::ZeroVector;
		float Start = -10.f;
		float Duration = 0.3f;
		float Height = 60.f;
		float Rest = 0.1f;
		float Yaw = 0.f;
		float Scale = 1.f;
	};

	TArray<FFlea> Fleas;
	TArray<FTransform> FleaXf;

	/** Tamaño propio (SizeScale acotado) y radios ya escalados. */
	float SizeK = 1.f;
	float CloudRadius = 170.f;
	float DetectRadius = 1800.f;
	float LeashRadius = 2400.f;

	// Servidor.
	TWeakObjectPtr<ATortugaCharacter> Target;
	FVector WanderGoal = FVector::ZeroVector;
	float ScanTimer = 0.f;
	float GroundTimer = 0.f;
	float GroundZ = 0.f;
	int32 ServerHops = 0;

	// Dueño de la tortuga picada (cliente): saltitos ya aplicados en esta picada.
	int32 LocalHops = 0;
	uint8 LocalHopSerial = 0;

	// Visual.
	float VisualClock = 0.f;
	bool bAnchorOnVictim = false;
	FVector Anchor = FVector::ZeroVector;
	FVector PlaneOrigin = FVector::ZeroVector;
	FVector2D PlaneSlope = FVector2D::ZeroVector;
	float PlaneTimer = 0.f;
	float ItchTimer = 0.f;
	uint32 FleaRng = 0x51ED27u;
	TNAmbientFX::FEmitter SandPuffs;

	/** Cuántos saltitos da una picada, a qué hora (s desde que empieza) y con qué velocidad (determinista). */
	int32 NumHops() const;
	float HopTime(int32 Index) const;
	FVector HopVelocity(int32 Index) const;
	/** Aplica a Victim los saltitos que ya tocan (Done: los aplicados). bSkipLate: no aplica los que llegan tarde. */
	void ApplyHops(ATortugaCharacter* Victim, int32& Done, float Age, bool bSkipLate);
	/** Servidor: el centro del enjambre anda hacia Goal a Speed, pegado a la arena. */
	void CrawlToward(const FVector& Goal, float Speed, float DeltaSeconds);
	/** Servidor: termina la picada (con o sin mareo) y se dispersa. */
	void EndInfest(bool bDizzy);

	/** Suelo del plano local bajo P (se refresca con tres consultas cada poco). */
	float PlaneZ(const FVector& P) const;
	/** Cuánto está dispersa la nube ahora (0 = junta, 1 = del todo). */
	float ScatterAmount(uint8 State, float Age) const;
	/** Siguiente sitio (respecto al ancla) al que salta una pulga. */
	FVector PickTarget(uint8 State, float Age, float& OutHeight, float& OutDuration, float& OutRest);
	float RandUnit();

	void BuildSwarm();
	void TickFleas(uint8 State, float Age, float DeltaSeconds);
};
