#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_RaceItemActor.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_RaceStormCloud.generated.h"

class ATortugaCharacter;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTN_RaceItemSynthComponent;

/**
 * Nube de tormenta (el rayo de Mario Kart, Docs/Modo_Carrera.md, «Objetos de carrera»): al usarla, una nube negra se forma
 * sobre CADA otra tortuga en carrera y, tras un aviso, les cae un rayo que las marea. Va toda por el reloj del servidor
 * (ATN_RaceItemActor::GetAge), sin Track:
 *
 *  - 0 a StormTelegraphSeconds: sobre cada víctima crece una nube oscura (bolas grises muy oscuras, ~14 m de ancho, a 20 m
 *    por encima de la tortuga; la sigue con suavizado) con su sombra en la arena, chispitas y lluvia, un trueno lejano y
 *    unos pitidos de aviso en el sitio de cada víctima. Quien la lanza ve un pequeño remolino oscuro sobre su cabeza (0,4 s).
 *  - A StormTelegraphSeconds el servidor marea UNA sola vez (bStruck) a cada víctima que siga en carrera y pueda ser golpeada
 *    (TNBeach::StunTurtle, bola temblando StormStunSeconds): en todas las máquinas cae un rayo dentado de la nube a la tortuga
 *    (o a su lado, si va con el protector solar), con un destello blanco cerca de la cámara y el «¡zap!».
 *  - Después la nube sube y se desvanece hasta StormTelegraphSeconds + 1,6 s y el servidor la termina. Si una víctima desaparece
 *    o llega a la meta antes, su nube se disuelve sin más.
 *
 * Como mucho hay 2 a la vez en el mundo (ServerCast devuelve false si no). Ajustes en TNRaceStormCloudDetail (.cpp).
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API ATN_RaceStormCloud : public ATN_RaceItemActor
{
	GENERATED_BODY()

public:
	ATN_RaceStormCloud();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Servidor: Caster lanza la nube. Crea la nube sobre cada otra tortuga en carrera que pueda ser golpeada (no va con el
	 * protector solar ni en el pelícano, ni está aturdida, derribada o en el pico de un enemigo). Devuelve true si la ha creado;
	 * false (sin crear nada) si no hay a quién fastidiar o ya hay 2 nubes en el mundo.
	 */
	static bool ServerCast(ATortugaCharacter* Caster);

protected:
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildVisuals() override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnFinished() override;
	virtual bool UsesTrack() const override { return false; }
	virtual float GetTrackFrequency() const override { return 5.f; }

private:
	/** Lo que se ve de la nube de una víctima en esta máquina (no se replica: cada máquina lo anima con el reloj del servidor). */
	struct FCloudView
	{
		bool bBuilt = false;
		bool bShown = false;
		bool bPlaced = false;
		bool bVanishing = false;
		bool bBoltStarted = false;
		bool bBoltDone = false;
		bool bDeflected = false;
		bool bThunderDone = false;
		bool bRumbled = false;
		int32 BeepsPlayed = 0;

		/** Centro de la nube (suavizado) y último sitio conocido de la víctima. */
		FVector CloudAt = FVector::ZeroVector;
		FVector LastVictimAt = FVector::ZeroVector;

		/** Extremos del rayo y giro y escala con que se coloca. */
		FVector BoltTop = FVector::ZeroVector;
		FVector BoltBottom = FVector::ZeroVector;
		FQuat BoltRotation = FQuat::Identity;
		double BoltLengthScale = 1.0;

		/** Reloj del mundo (esta máquina) al empezar el rayo, y edad de la nube al empezar a disolverse. */
		double BoltStartTime = 0.0;
		double VanishStartAge = 0.0;

		float SparkAccum = 0.f;
		float RainAccum = 0.f;

		TWeakObjectPtr<USceneComponent> CloudRoot;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Blobs;
		TWeakObjectPtr<UStaticMeshComponent> Shadow;
		TWeakObjectPtr<UStaticMeshComponent> BoltCore;
		TWeakObjectPtr<UStaticMeshComponent> BoltSheath;
		TWeakObjectPtr<UTN_RaceItemSynthComponent> Voice;
	};

	/** Las tortugas sobre las que se forma una nube (las fija el servidor al crearla; se replican). */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ATortugaCharacter>> Victims;

	/** El servidor ya ha marcado el rayo (una sola vez). */
	UPROPERTY(ReplicatedUsing = OnRep_Struck)
	bool bStruck = false;

	UFUNCTION()
	void OnRep_Struck();

	// ── Servidor ──

	/** El rayo: marea a cada víctima que siga en carrera y pueda ser golpeada, y avisa. */
	void ServerStrike();

	// ── Visual (máquinas con pantalla) ──

	void BuildCloud(int32 Index, const FVector& VictimAt);
	void TickCloud(int32 Index, float DeltaSeconds, double Age, const TArray<ATortugaCharacter*>& Racers);
	void StartBolt(int32 Index, FCloudView& Cloud, const FVector& VictimAt);
	void TickBolt(int32 Index, FCloudView& Cloud, double LocalNow);
	void HideCloud(FCloudView& Cloud);
	void TickSwirl(double Age);
	void TickFlash(const FVector& View, bool bHasView);

	TArray<FCloudView> Clouds;

	/** Chispas eléctricas, lluvia y polvo del impacto (uno solo de cada, compartido por todas las nubes). */
	TNAmbientFX::FEmitter SparkFX;
	TNAmbientFX::FEmitter RainFX;
	TNAmbientFX::FEmitter DustFX;
	bool bEmittersReady = false;

	/** Remolino oscuro sobre quien la lanza. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SwirlRoot;

	TArray<TWeakObjectPtr<UStaticMeshComponent>> SwirlBlobs;
	bool bSwirlShown = false;
	bool bSwirlSounded = false;

	/** Destello blanco cerca de la cámara mientras cae un rayo. */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlashLight;

	/** Lo más fuerte que ha sido el destello de este fotograma y dónde cae ese rayo. */
	float FlashLevel = 0.f;
	FVector FlashPoint = FVector::ZeroVector;
	bool bFlashLit = false;

	/** Esta máquina sabe que el servidor ya ha marcado el rayo (llega antes que el reloj, si va un pelo adelantado). */
	bool bStrikeAnnounced = false;
};
