#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_JellyfishTrampoline.generated.h"

class ACharacter;
class APawn;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTN_PlaygroundSynthComponent;

/** Paletas de la medusa: campana con degradado, motas, brillo y panza más intensa. */
UENUM(BlueprintType)
enum class ETNJellyfishColor : uint8
{
	Pink    UMETA(DisplayName = "Rosa"),
	Lilac   UMETA(DisplayName = "Lila"),
	Sky     UMETA(DisplayName = "Celeste"),
	Custom  UMETA(DisplayName = "Personalizado"),
};

/**
 * Medusa cama elástica del parque de pruebas del lobby: campana gorda con aspecto de gelatina translúcida (degradado
 * pálido en la cima y en el borde, panza más intensa, trébol de órganos y motas claras, un brillo de goma y una cara
 * simpática en su +X) sentada en la arena sobre tentáculos ondulados que se mecen.
 *
 * Al caer encima, la tortuga sale disparada hacia arriba (LaunchCharacter con LaunchZ y la velocidad horizontal intacta),
 * la campana se aplasta y rebota (squash & stretch) y suena un «boing» sintetizado (UTN_PlaygroundSynthComponent).
 *
 * Red (listen server): la medusa está en todas las máquinas (colocada en el nivel o creada por el servidor, replicada).
 * El rebote lo aplican a la vez el servidor y el cliente que controla a la tortuga, en el mismo movimiento (el golpe con
 * la campana llega dentro del movimiento del personaje en las dos máquinas), así que la predicción cuadra sin
 * correcciones. El resto ve el aplastamiento y oye el boing por un multicast no fiable. Los caparazones con física
 * (ATN_ShellBody) también rebotan: los lanza el servidor.
 *
 * Detección: colisión convexa con la forma de la cúpula (OnComponentHit) y un sensor fino, el mismo casco 15 cm más
 * grande (solapamiento): basta con que la tortuga toque la parte de arriba (el 85 % central de la cúpula) sin ir subiendo.
 *
 * Medidas con Size = 1 (cm; origen en la arena, en el centro): borde de la campana a 42 de alto y radio 110; cima a 124;
 * tentáculos hasta ~190 del centro. Todo escala con Size (0,8 / 1,1 / 1,45 dan cimas a 99, 136 y 180 cm).
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_JellyfishTrampoline : public AActor
{
	GENERATED_BODY()

public:
	ATN_JellyfishTrampoline();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Rehace malla y colisión con las propiedades actuales (tras cambiarlas en ejecución sin SpawnActorDeferred). */
	UFUNCTION(BlueprintCallable, Category = "Medusa")
	void RebuildJellyfish();

	/** Altura de la cima de la campana sobre el origen del actor (cm, con Size y la escala del actor). */
	UFUNCTION(BlueprintPure, Category = "Medusa")
	float GetBellTopHeight() const;

	/** Radio del borde de la campana (cm, con Size y la escala del actor). */
	UFUNCTION(BlueprintPure, Category = "Medusa")
	float GetBellRadius() const;

	/** Paleta de la campana y los tentáculos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Medusa")
	ETNJellyfishColor ColorPreset = ETNJellyfishColor::Pink;

	/** Color base con la paleta Personalizado (el resto de tonos salen de él). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Medusa",
		meta = (EditCondition = "ColorPreset == ETNJellyfishColor::Custom", EditConditionHides))
	FLinearColor CustomColor = FLinearColor(0.85f, 0.32f, 0.62f, 1.f);

	/** Escala de la medusa entera (malla, colisión y tono del boing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Medusa", meta = (ClampMin = "0.5", ClampMax = "2.5", UIMin = "0.5", UIMax = "2.5"))
	float Size = 1.f;

	/** Velocidad vertical del rebote (cm/s); sustituye a la vertical que traía, la horizontal se conserva. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Medusa", meta = (ClampMin = "300.0", ClampMax = "3000.0"))
	float LaunchZ = 1100.f;

	/** Segundos mínimos entre dos rebotes de la misma tortuga. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Medusa", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float BounceCooldown = 0.3f;

	/** Semilla de las motas (cambia el dibujo; igual en todas las máquinas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Medusa")
	int32 SpotSeed = 1;

	/** Volumen del boing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medusa|Sonido", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BoingVolume = 1.f;

protected:
	UFUNCTION()
	void OnRep_Config();

	/** Aplastamiento y boing en todas las máquinas (el cliente dueño ya los ha hecho al predecir su rebote). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBounceFX(APawn* Bouncer, float Strength);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Medusa")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Pivote del aplastamiento, en el centro del borde de la campana. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Medusa")
	TObjectPtr<USceneComponent> BellPivot;

	/** Campana con la cara, las motas y el brillo (malla en ejecución). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Medusa")
	TObjectPtr<UStaticMeshComponent> BellMesh;

	/** Tentáculos quietos para verlos en el editor; en juego los sustituye TentacleMesh, que se mece. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Medusa")
	TObjectPtr<UStaticMeshComponent> TentaclePreview;

	/** Colisión de la cúpula: casco convexo del perfil de la campana, hasta la arena (solo colisión, sin secciones). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Medusa")
	TObjectPtr<UProceduralMeshComponent> BellCollider;

	/** Sensor de aterrizaje: esfera que envuelve la cúpula con margen; solo solapa con personajes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Medusa")
	TObjectPtr<UProceduralMeshComponent> BounceSensor;

	/** Tentáculos que se mecen (malla procedural en ejecución, solo en máquinas con pantalla). */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> TentacleMesh;

	UPROPERTY(Transient)
	TObjectPtr<UTN_PlaygroundSynthComponent> Voice;

private:
	UFUNCTION()
	void OnBellHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnSensorOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	/** Malla, colisión y tentáculos; no hace nada si la configuración no ha cambiado (salvo bForce). */
	void BuildAll(bool bForce);

	/** Crea (o rehace con la configuración actual) la malla de tentáculos que se mece. */
	void BuildRuntimeTentacles();

	void UpdateTentacles();
	void AnimateVisuals(float DeltaSeconds);

	/** true si los pies del personaje están sobre la parte de arriba de la cúpula. */
	bool IsOnBell(const ACharacter* Character) const;

	/** Rebota al personaje si toca y lo simula esta máquina; true si ha rebotado. */
	bool TryBounce(ACharacter* Character);

	/** Servidor: caparazones con física encima de la campana. */
	void BounceShells(double Now);

	/** Decide dónde se ven y oyen los efectos de un rebote (multicast desde el servidor o solo aquí). */
	void SpreadBounceFX(APawn* Bouncer, float Strength);

	/** Aplastamiento, meneo de tentáculos y boing, solo en esta máquina. */
	void PlayBounceFX(float Strength);

	uint32 ConfigHash() const;

	uint32 BuiltHash = 0;
	double AnimClock = 0.0;
	float SquashAge = 10.f;
	float SquashAmp = 0.f;
	float Wiggle = 0.f;
	float BreathPhase = 0.f;

	/** Último rebote de cada personaje (tiempo del mundo), en cada máquina. */
	TMap<TWeakObjectPtr<ACharacter>, double> LastBounceTime;

	/** Servidor: último rebote de cada caparazón con física. */
	TMap<TWeakObjectPtr<AActor>, double> LastShellBounce;
};
