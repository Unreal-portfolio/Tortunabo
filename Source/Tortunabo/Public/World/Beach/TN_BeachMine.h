#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "TN_BeachMine.generated.h"

class ATortugaCharacter;
class UPointLightComponent;
class UStaticMeshComponent;
class UTN_BeachMineSynthComponent;

/** Tick por distancia de la mina (#59; lógica pura, tests Tortunabo.Perf.BeachTickWake). */
namespace TNBeachMineTick
{
	/**
	 * Distancia (cm) a la que apaga su Tick: el sensor (tapa de 70-112 cm de radio) despierta de sobra antes de que llegue
	 * nadie y, más lejos, el destello del piloto (8 % de cada 1,6 s) no se distingue.
	 */
	inline constexpr float WakeDistance = 4000.f;

	/** Segundos tras la explosión, además del rearme, en que siguen vivos el humo, los trozos y el polvo de rearmarse. */
	inline constexpr double AfterBlastSeconds = 4.0;

	/** En marcha: con la mecha encendida o desde la explosión hasta que se ha rearmado y se han posado los efectos. */
	inline bool IsBusy(float TriggeredAt, float ExplodedAt, float RearmSeconds, double ServerNow)
	{
		if (TriggeredAt > ExplodedAt)
		{
			return true;
		}
		if (ExplodedAt < 0.f)
		{
			return false;
		}
		return ServerNow < static_cast<double>(ExplodedAt) + FMath::Max(0.f, RearmSeconds) + AfterBlastSeconds;
	}
}

/**
 * Mina de juguete medio enterrada (Docs/Modo_Carrera.md, «Mina»): de un montoncito de arena removida asoman la tapa y el
 * pincho de la espoleta (y en algunas, una banderita roja de aviso a un lado); un piloto rojo parpadea despacio. Cuatro
 * aspectos según la semilla: plato verde oliva con franja amarilla, bote gris de tres pinchos, plato oxidado con
 * percebes y juguete caqui con botón rojo. Todo cabe en la huella del contrato (350·SizeScale). Las mallas son de SizeScale
 * 1 y compartidas por todas las minas del mismo aspecto (en una ronda hay decenas): cada mina escala sus componentes.
 *
 * Al pisarla (servidor): «clic», parpadea en rojo FuseSeconds pitando cada vez más deprisa y explota: fogonazo, bola de
 * fuego, nube de arena, terrones, trozos de la carcasa, humo, «¡BUM!», temblor de cámara cerca y el estallido
 * sintetizado. Quien la pisó y quien esté encima (BlastRadius) sale lanzada en bola hacia atrás, en sentido contrario a
 * la carrera (TNBeach::StunTurtle con LaunchBack y LaunchUp: ~7-8 m); a quien esté cerca (hasta PushRadius) la empuja
 * hacia fuera sin aturdirla, menos cuanto más lejos. Queda un cráter chamuscado y, a los RearmSeconds, la mina vuelve a
 * asomar armada en medio del cráter, que se queda de aviso (RearmSeconds = 0: no se rearma en la ronda).
 *
 * Sin colisión: la tapa asoma ~22 cm y se pisa; el cráter es de adorno. La pisan solo las tortugas vivas, fuera del
 * caparazón y sin aturdir (TNBeachTrapKit::IsFreeTurtle); la explosión lanza a toda tortuga viva que esté encima.
 *
 * Red: lo decide el servidor (sensor en su Tick) y replica dos horas del servidor, TriggeredAt (pisada) y ExplodedAt
 * (explosión); cada máquina anima el parpadeo, la explosión, el cráter y el rearme con su reloj del servidor suavizado, y
 * quien llega tarde ve ya el cráter. El lanzamiento y el empujón a las de alrededor los aplica solo el servidor: el dueño
 * los recibe con el movimiento replicado, sin repetirlos en local (antes el empujón se aplicaba dos veces).
 */
UCLASS()
class TORTUNABO_API ATN_BeachMine : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachMine();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float GetTickWakeDistance() const override { return TNBeachMineTick::WakeDistance; }
	virtual bool IsTickBusy() const override;
	virtual void OnTickWakeChanged(bool bAwake) override;

	/** Armada (se puede pisar) con el estado replicado a la hora del servidor Now. */
	bool IsArmedAt(double Now) const;

	/** Segundos entre el clic y la explosión (parpadeo y pitidos). */
	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float FuseSeconds = 0.4f;

	/** Segundos de aturdimiento (en bola) de quien sale lanzada. */
	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "0.2", ClampMax = "8.0"))
	float StunSeconds = 2.4f;

	/** Lanzamiento de quien está encima: hacia atrás (contrario a la carrera) y hacia arriba (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "0.0"))
	float LaunchBack = 420.f;

	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "0.0"))
	float LaunchUp = 950.f;

	/** Radio (cm, con SizeScale 1) en el que la explosión lanza en bola. Quien la pisó sale lanzada hasta el doble. */
	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "50.0"))
	float BlastRadius = 230.f;

	/** Radio (cm, con SizeScale 1) hasta el que empuja sin aturdir. */
	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "100.0"))
	float PushRadius = 700.f;

	/** Empujón a quien está cerca (cm/s): entero junto a BlastRadius y un tercio en PushRadius. */
	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "0.0"))
	float PushSpeed = 750.f;

	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "0.0"))
	float PushUp = 450.f;

	/** Segundos tras la explosión en que vuelve a asomar armada (0 = no se rearma en la ronda). */
	UPROPERTY(EditAnywhere, Category = "Mina", meta = (ClampMin = "0.0"))
	float RearmSeconds = 9.f;

protected:
	virtual void BeginPlay() override;
	virtual void ApplySpec() override;

	UFUNCTION()
	void OnRep_TriggeredAt();

	UFUNCTION()
	void OnRep_ExplodedAt();

	/** Hora del servidor de la última pisada (< 0: nunca). */
	UPROPERTY(ReplicatedUsing = OnRep_TriggeredAt)
	float TriggeredAt = -1.f;

	/** Hora del servidor de la última explosión (< 0: nunca). */
	UPROPERTY(ReplicatedUsing = OnRep_ExplodedAt)
	float ExplodedAt = -1.f;

	/** Arena removida, cuerpo, tapa y pincho: se hunde con el clic, desaparece al explotar y vuelve a subir al rearmarse. */
	UPROPERTY(VisibleAnywhere, Category = "Mina")
	TObjectPtr<UStaticMeshComponent> MineMesh;

	/** Piloto rojo de la tapa (parpadea despacio armada y se queda encendido en la mecha). */
	UPROPERTY(VisibleAnywhere, Category = "Mina")
	TObjectPtr<UStaticMeshComponent> LedMesh;

	/** Tapa roja del parpadeo de la mecha. */
	UPROPERTY(VisibleAnywhere, Category = "Mina")
	TObjectPtr<UStaticMeshComponent> BlinkMesh;

	/** Banderita de aviso (vacía si esta mina no la lleva). */
	UPROPERTY(VisibleAnywhere, Category = "Mina")
	TObjectPtr<UStaticMeshComponent> FlagMesh;

	/** Cráter chamuscado con su reborde de arena y trozos de la carcasa. */
	UPROPERTY(VisibleAnywhere, Category = "Mina")
	TObjectPtr<UStaticMeshComponent> CraterMesh;

	/** Fogonazo (bola que crece y se apaga en un cuarto de segundo). */
	UPROPERTY(VisibleAnywhere, Category = "Mina")
	TObjectPtr<UStaticMeshComponent> FlashMesh;

	UPROPERTY(VisibleAnywhere, Category = "Mina")
	TObjectPtr<UPointLightComponent> FlashLight;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachMineSynthComponent> Voice;

private:
	/** Servidor: ¿la pisa alguien? */
	void CheckStep(double Now);

	/** Servidor: lanza y empuja a las de alrededor. */
	void Explode(double Now);

	/** Hacia atrás en la carrera (contrario al mar del generador o, sin él, a su +X), en horizontal. */
	FVector GetBackDirection() const;

	void UpdateVisuals(double Now, float DeltaSeconds);

	/** Con pantalla: enciende el Tick si estaba dormida por distancia (llega un cambio replicado). */
	void WakeForNews();

	/** Crea las partículas la primera vez que hacen falta (casi todas las minas de la ronda no llegan a explotar). */
	void EnsureFX();

	void PlayClickFX();
	void PlayExplosionFX();
	void PlayRearmFX();

	/** Medidas de esta mina (con SizeScale): alto de la tapa sobre la arena y radio que se pisa. */
	double CapTop = 22.0;
	double CapRadius = 70.0;
	double SizeK = 1.0;

	/** Pitidos que ya han sonado en esta mecha. */
	int32 BeepsPlayed = 0;
	float FlashAge = 10.f;
	float LedPhase = 0.f;
	bool bHasScreen = false;
	bool bFXReady = false;
	bool bRearmFXPending = false;

	/** Quien la pisó (sale lanzada aunque se haya alejado algo en la mecha). */
	TWeakObjectPtr<ATortugaCharacter> Stepper;

	FTNTrapClock Clock;
	FTNTrapBurst Fire;
	FTNTrapBurst Dust;
	FTNTrapBurst Clods;
	FTNTrapBurst Chips;
	FTNTrapBurst Smoke;
	FTNTrapPopText ClickText;
	FTNTrapPopText BoomText;
};
