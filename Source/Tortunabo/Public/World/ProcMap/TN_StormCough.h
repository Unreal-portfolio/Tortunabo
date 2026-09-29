#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_StormCough.generated.h"

namespace TNStormCough
{
	// Parámetros atómicos compartidos con el generador del hilo de audio (Private/World/ProcMap/TN_StormCough.cpp).
	struct FSharedParams;
}

/**
 * Tos de una tortuga metida en la tormenta del camino (ATN_PathStorm), sintetizada en tiempo real sin archivos de
 * audio: golpes de tos de dibujos animados pero creíbles (estallido seco, aire con los formantes de una vocal y cola
 * con voz), ataques de 2-4 golpes con pausa, carraspeos e inspiraciones entre ataques, con tono, duración y timbre al
 * azar y una voz propia por tortuga (más grave o más aguda según su semilla, la misma en todas las máquinas).
 *
 * Intensidad: al entrar, carraspeos sueltos; con el tiempo dentro y al acercarse la muerte, ataques cada vez más
 * seguidos y fuertes, con jadeos y pitido al coger aire; al salir, un último carraspeo y silencio; al morir, calla en
 * seco.
 *
 * Estado local por máquina, sin RPC: la tormenta, en cada máquina con audio, da este componente a cada tortuga viva y
 * le pasa cada 0,1 s si está dentro según el frente replicado y qué fracción lleva del tiempo que mata
 * (SetStormExposure). Fuente 3D mono en la raíz de la tortuga; la del jugador local suena algo más alta.
 *
 * El sonido lo genera un ISoundGenerator en el hilo de render de audio (TNStormCough::FEngine, en el .cpp): sin
 * UObjects, asignaciones ni bloqueos allí. Este componente solo escribe objetivos en unos parámetros atómicos; el
 * sintetizador arranca al hacer falta y se para al callar o lejos del oyente (sin gastar CPU).
 *
 * Pruebas: TN.Storm.Cough <0|1|2> en la tortuga local, sin tormenta (0 apagado; 1 carraspeo; 2 tos fuerte).
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_StormCoughComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_StormCoughComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * El componente de tos de InOwner; si no tiene, se lo crea (adjunto a su raíz y registrado). Null en servidor
	 * dedicado, sin audio, fuera de un mundo de juego o con el actor destruyéndose.
	 */
	static UTN_StormCoughComponent* FindOrAddTo(AActor* InOwner);

	/**
	 * Lo llama la tormenta cada 0,1 s en las máquinas con audio: si esta tortuga está dentro según esta máquina y qué
	 * fracción lleva del tiempo que mata (0..1). Si deja de llamarse (tormenta destruida), cuenta como fuera.
	 */
	void SetStormExposure(bool bInside, float DeathFraction);

	/** Calla en seco y sin carraspeo final (la tortuga ha muerto). La siguiente SetStormExposure lo deshace. */
	void Hush();

	/** Pruebas (TN.Storm.Cough): 0 = sin forzar (manda la tormenta), 1 = carraspeos sueltos, 2 = tos fuerte. */
	void SetDebugLevel(int32 InLevel);

	/** Intensidad enviada por última vez (0 = callada; 1 = la peor tos). */
	float GetSeverity() const { return Severity; }

	/** Volumen de la tos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StormCough", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Loudness = 1.f;

	/** Multiplica el volumen si es la tortuga del jugador local (se oye a sí misma algo más alta). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StormCough", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float LocalPlayerBoost = 1.5f;

	/** Segundos dentro para llegar a la peor tos aunque la tormenta tarde más en matar (o no mate, en las pruebas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StormCough", meta = (ClampMin = "1.0"))
	float SecondsToWorstCough = 12.f;

	/** Radio con volumen pleno (cm). Se aplica en el siguiente arranque. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StormCough|3D", meta = (ClampMin = "0.0"))
	float InnerRadius = 300.f;

	/** Distancia (cm) desde el radio interior en la que se apaga. Se aplica en el siguiente arranque. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StormCough|3D", meta = (ClampMin = "100.0"))
	float FalloffDistance = 2700.f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	/** Mono, 3D y atenuación hecha en código (Start la copia al componente de audio en cada arranque). */
	void ConfigureAttenuation();

	/** Recalcula la intensidad, la manda al hilo de audio y arranca o para el sintetizador. */
	void UpdateCough();

	/** Si es la tortuga que controla esta máquina. */
	bool IsLocalTurtle() const;

	/** Distancia (cm) al oyente más cercano; 0 si no hay dispositivo de audio. */
	float GetListenerDistance() const;

	/** Parámetros compartidos con el generador del hilo de audio (atómicos; los dos los mantienen vivos). */
	TSharedPtr<TNStormCough::FSharedParams, ESPMode::ThreadSafe> SharedParams;

	/** Intensidad enviada por última vez. */
	float Severity = 0.f;
	/** Fracción del tiempo que mata según la tormenta (0..1). */
	float StormFraction = 0.f;
	/** Lo último que ha dicho la tormenta. */
	bool bStormInside = false;
	/** Dentro con margen de salida: un parpadeo en el borde del frente no cuenta como salir. */
	bool bExposed = false;
	bool bHushed = false;
	int32 DebugLevel = 0;
	/** Tiempos del mundo (s). */
	double LastExposureTime = -1.0;
	double LastInsideTime = -1.0;
	double ExposedSince = 0.0;
	double QuietSince = 0.0;
	double HushTime = 0.0;
	/** Arranques del sintetizador: cambia el azar de cada arranque (la voz sale siempre de la tortuga). */
	uint32 StartCount = 0;
};
