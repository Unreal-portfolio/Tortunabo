#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Beach/TN_BeachTypes.h"
#include "TN_BeachElement.generated.h"

/**
 * Base de todo lo replicado que el generador de la playa coloca (trampas, lanzadores, fortalezas, cofres y enemigos; el
 * decorado de la ronda no: es local e instanciado, ATN_BeachDecorField). El servidor la crea con SpawnElement (clase por
 * nombre, TNBeach::ClassNameOf) y replica Spec; cada máquina construye su malla en ApplySpec (determinista con
 * Spec.Seed). Las subclases sobrescriben ApplySpec y, si quieren, GetFootprintRadius, WantsNetDormancy y
 * GetNetRelevanceDistance.
 *
 * Red (Docs/Modo_Carrera.md, «Rendimiento y red»): SpawnElement aplica ApplyRoundNetProfile antes de FinishSpawning (manda
 * sobre lo que ponga el constructor de la subclase): relevante hasta GetNetRelevanceDistance, no en toda la playa (salvo
 * las estructuras enormes, WantsAlwaysRelevant), y, si WantsNetDormancy, dormido desde que aparece (DORM_Initial) con
 * pocas actualizaciones por segundo. Regla para dormidos: cada cambio de una propiedad replicada va seguido de
 * ForceNetUpdate() (la despierta un rato y la manda ya); los multicast llegan igual. Nada del GameMode ni de las
 * pantallas depende de ver elementos lejanos.
 */
UCLASS(Abstract)
class TORTUNABO_API ATN_BeachElement : public AActor
{
	GENERATED_BODY()

public:
	ATN_BeachElement();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Servidor: crea el elemento Spec.Element (su clase, por nombre) en Transform y le pasa Spec antes de BeginPlay, con su
	 * red de la ronda (ApplyRoundNetProfile). Devuelve null si la clase aún no existe (se registra en el log y el
	 * generador sigue con el resto).
	 */
	static ATN_BeachElement* SpawnElement(UWorld* World, const FTransform& Transform, const FTNBeachElementSpec& InSpec);

	const FTNBeachElementSpec& GetSpec() const { return Spec; }

	/** Radio de la huella en planta (cm): por defecto, la nominal del contrato por SizeScale. */
	virtual float GetFootprintRadius() const;

	/**
	 * Duerme en red al aparecer (DORM_Initial): por defecto, todo menos los enemigos (trampas, lanzadores, fortalezas y
	 * cofres están quietos y avisan de cada cambio con ForceNetUpdate). Lo que se mueve solo por la red, false.
	 */
	virtual bool WantsNetDormancy() const;

	/**
	 * Distancia (cm) hasta la que un cliente tiene el elemento: de 260 m a 450 m según lo que ocupa (huella y largo); la
	 * niebla empieza a los 300 m. Más allá, el cliente no lo tiene (ni sus mallas) y no cuesta red. Los enemigos (que no
	 * duermen: se mueven y mandan su estado a 10 Hz), de 200 m a 300 m, también las zonas de gaviotas y los pasos de quads.
	 */
	virtual float GetNetRelevanceDistance() const;

	/**
	 * Relevante en toda la playa: por defecto, las estructuras enormes y quietas (20 m de huella o más: castillos con
	 * salas y fortalezas). Son pocas, duermen (no cuestan red) y así cada cliente las monta en la espera de la ronda y no
	 * a mitad de carrera (sus mallas son las más caras) ni aparecen de golpe a lo lejos.
	 */
	virtual bool WantsAlwaysRelevant() const;

	/** Huella (cm, con su tamaño) desde la que una estructura quieta es relevante en toda la playa. */
	static constexpr float LandmarkFootprint = 2000.f;

	/** Relevancia mínima y máxima de los elementos de la ronda (cm). */
	static constexpr float MinNetRelevance = 26000.f;
	static constexpr float MaxNetRelevance = 45000.f;
	/** Relevancia mínima y máxima de los enemigos (cm): despiertos siempre, cuestan red por cada cliente que los tiene. */
	static constexpr float EnemyMinNetRelevance = 20000.f;
	static constexpr float EnemyMaxNetRelevance = 30000.f;

	/** Frecuencia de réplica como mucho (Hz) de lo que duerme: despierto, cada cambio va con ForceNetUpdate. */
	static constexpr float DormantNetFrequency = 2.f;

	/** Segundos que sigue despierto tras el último ForceNetUpdate antes de volver a dormirse. */
	static constexpr float NetWakeSeconds = 3.f;

	/**
	 * Servidor: en los que duermen (WantsNetDormancy), despierta la réplica (DORM_Awake) antes de mandar el cambio y la
	 * vuelve a dormir NetWakeSeconds después del último (el patrón de los rebuscables): así cada cambio de estado llega
	 * aunque el actor estuviera dormido. En los demás, lo de siempre.
	 */
	virtual void ForceNetUpdate() override;

	/**
	 * Tick por distancia (UTN_BeachTickWakeSubsystem, #59): a partir de esta distancia (cm) a la tortuga, el caparazón o la
	 * cámara local más cercanos, el elemento apaga su Tick. 0 (por defecto): no se duerme nunca. Solo para lo que en su
	 * Tick no hace nada que se note sin nadie cerca (sensores de pisada, animaciones de reposo).
	 */
	virtual float GetTickWakeDistance() const { return 0.f; }

	/** Tiene algo en marcha (mecha, explosión, puerta abierta, tortuga enganchada…): sigue despierto aunque no haya nadie. */
	virtual bool IsTickBusy() const { return false; }

	/** Lo llama el subsistema al encender o apagar el Tick (p. ej., para dejar quieto lo que parpadea). */
	virtual void OnTickWakeChanged(bool bAwake) {}

protected:
	/**
	 * Servidor: la red del elemento en la ronda (relevancia por distancia, dormancy y frecuencia). La aplica SpawnElement
	 * antes de FinishSpawning; las subclases pueden ampliarla (llamando a la base).
	 */
	virtual void ApplyRoundNetProfile();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Construye lo visual y la colisión a partir de Spec (en todas las máquinas; puede llamarse más de una vez). */
	virtual void ApplySpec() {}

	UPROPERTY(ReplicatedUsing = OnRep_Spec, BlueprintReadOnly, Category = "Beach")
	FTNBeachElementSpec Spec;

	UFUNCTION()
	void OnRep_Spec();

	bool bSpecApplied = false;

private:
	/** La red de la ronda lo duerme (ApplyRoundNetProfile con WantsNetDormancy): ForceNetUpdate lo despierta un rato. */
	bool bRoundNetDormancy = false;
	FTimerHandle NetSleepTimer;

	/** Apuntado en UTN_BeachTickWakeSubsystem (GetTickWakeDistance > 0 al empezar). */
	bool bTickWakeRegistered = false;
};
