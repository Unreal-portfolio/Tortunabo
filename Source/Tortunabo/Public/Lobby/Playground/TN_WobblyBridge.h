#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_WobblyBridge.generated.h"

class ACharacter;
class UBoxComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTN_PlaygroundSynthComponent;

/** Un tablón del puente: sale igual en todas las máquinas a partir de DamageSeed. */
struct FTNBridgePlank
{
	bool bMissing = false;
	bool bBroken = false;
	/** Medio largo (a lo ancho del puente) y centro del trozo que queda (los rotos conservan un lado). */
	double HalfLength = 55.0;
	double CenterY = 0.0;
	/** Lado del que falta el trozo en los rotos (+1 o -1). */
	double BrokenSide = 1.0;
	float Tone = 1.f;
	double YawJitter = 0.0;
	double LiftJitter = 0.0;
};

/** Personaje cerca del puente (cada máquina lleva la cuenta de los que ve). */
struct FTNBridgeRider
{
	/** Peso del hundimiento bajo sus pies (0..1, sube al pisar y baja al salir). */
	float Dip = 0.f;
	/** Posición a lo largo del puente (X local, cm). */
	float AlongX = 0.f;
	float LastVz = 0.f;
	float CreakTimer = 0.f;
	bool bOnDeck = false;
};

/**
 * Puente colgante de tablones y cuerdas del parque de pruebas del lobby, entre cuatro postes, que se bambolea mucho:
 * vaivén lateral, balanceo (giro sobre su eje), rebote y una onda que recorre el tablero; todo crece cuando alguien anda
 * o corre por encima y con cada aterrizaje, y cada tortuga hunde un poco los tablones que pisa. Con dos o tres tortugas
 * corriendo el balanceo del centro pasa de la pendiente caminable y resbalan fuera: la tortuga se puede caer. Faltan
 * y están rotos algunos tablones.
 *
 * Tablones cinemáticos: cada uno es una caja de colisión (subobjeto por defecto, con nombre estable para la red) que se
 * mueve en cada fotograma; el CharacterMovement sigue a las bases que se mueven y, como la caja se puede nombrar por
 * red, el cliente manda su posición relativa al tablón: servidor y cliente coinciden aunque su vaivén vaya unos
 * milisegundos desfasado. El vaivén usa el reloj del servidor (GetServerWorldTimeSeconds, suavizado) y la agitación se
 * calcula en cada máquina con las tortugas que ve: no replica nada por fotograma. Los crujidos son locales.
 *
 * Espacio del actor (cm): origen en el suelo bajo el centro del vano; el puente va a lo largo de X. Los extremos del
 * tablero están en X = ±SpanLength/2 a DeckHeight de alto; los postes, 12 cm por fuera. Con bSandTowers, cada extremo
 * lleva una torre de arena de 140 de fondo (X) y DeckWidth + 60 de ancho (Y) con la cima a DeckHeight y, con bStairs,
 * una escalera de peldaños de 40 cm como máximo y 45 de huella hacia fuera. Medidas totales en GetTotalLength.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_WobblyBridge : public AActor
{
	GENERATED_BODY()

public:
	/** Tablones como máximo (cajas creadas en el constructor; se usan las que caben en SpanLength). */
	static constexpr int32 MaxPlanks = 40;

	ATN_WobblyBridge();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Rehace tablones, marco y colisión con las propiedades actuales (tras cambiarlas en ejecución sin SpawnActorDeferred). */
	UFUNCTION(BlueprintCallable, Category = "Puente")
	void RebuildBridge();

	/** Largo total en X (cm) con torres y escaleras. */
	UFUNCTION(BlueprintPure, Category = "Puente")
	float GetTotalLength() const;

	/** Punto (espacio del actor) donde empieza el tablero en el extremo -X (bStart) o +X, a la altura de DeckHeight. */
	UFUNCTION(BlueprintPure, Category = "Puente")
	FVector GetDeckEnd(bool bStart) const;

	/** Distancia entre los extremos del tablero (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente", meta = (ClampMin = "600.0", ClampMax = "1200.0"))
	float SpanLength = 900.f;

	/** Altura de los extremos del tablero sobre el suelo (cm); el centro cuelga Sag más abajo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente", meta = (ClampMin = "150.0", ClampMax = "300.0"))
	float DeckHeight = 220.f;

	/** Ancho del tablero (largo de los tablones, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente", meta = (ClampMin = "80.0", ClampMax = "160.0"))
	float DeckWidth = 110.f;

	/** Cuánto cuelga el centro en reposo (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float Sag = 30.f;

	/** Multiplicador de todo el bamboleo (0 = quieto; 2 = locura). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente|Bamboleo", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Wobble = 1.f;

	/** Agitación en reposo, sin nadie encima (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente|Bamboleo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleWobble = 0.3f;

	/** Cuánto agita correr por encima (0 = igual que andar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente|Bamboleo", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float RunBoost = 1.f;

	/** Tablones que faltan (nunca dos seguidos ni en los tres primeros o últimos). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente|Tablones", meta = (ClampMin = "0", ClampMax = "4"))
	int32 MissingPlanks = 1;

	/** Tablones rotos: queda un trozo pegado a un lado, con astillas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente|Tablones", meta = (ClampMin = "0", ClampMax = "6"))
	int32 BrokenPlanks = 2;

	/** Semilla de qué tablones faltan o están rotos y de la veta de cada uno. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente|Tablones")
	int32 DamageSeed = 3;

	/** Torres de arena bajo los extremos (con su cima a DeckHeight): sin ellas, los postes nacen del suelo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente|Extremos")
	bool bSandTowers = true;

	/** Escaleras de arena para subir a las torres (hacia fuera, a lo largo de X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Puente|Extremos", meta = (EditCondition = "bSandTowers"))
	bool bStairs = true;

	/** Volumen de los crujidos de madera y cuerda. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puente|Sonido", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CreakVolume = 0.8f;

protected:
	UFUNCTION()
	void OnRep_Config();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puente")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Postes, torres, escaleras y adornos (malla en ejecución). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puente")
	TObjectPtr<UStaticMeshComponent> FrameMesh;

	/** Tablero y cuerdas en reposo para verlos en el editor; en juego lo sustituye DeckMesh, que se mueve. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puente")
	TObjectPtr<UStaticMeshComponent> DeckPreview;

	/** Colisión convexa de postes, torres, almenas y escaleras. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puente")
	TObjectPtr<UProceduralMeshComponent> FrameCollision;

	/** Cajas de colisión de los tablones (bases móviles). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puente")
	TArray<TObjectPtr<UBoxComponent>> PlankBoxes;

	/** Tablones y cuerdas que se mueven (malla procedural en ejecución, solo en máquinas con pantalla). */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> DeckMesh;

	UPROPERTY(Transient)
	TObjectPtr<UTN_PlaygroundSynthComponent> Voice;

private:
	/** Todo lo que depende de la configuración; no hace nada si no ha cambiado (salvo bForce). */
	void BuildAll(bool bForce);

	/** Número de tablones, huecos, rotos, tonos y medidas de cada uno. */
	void LayoutPlanks();

	/** Crea o rehace la malla procedural del tablero con la pose actual. */
	void BuildRuntimeDeck();

	/** Punto de la cara de arriba del tablero en S (0..1 a lo largo), con la agitación Agitation; con bDips, hundido bajo las tortugas. */
	FVector DeckPoint(double S, double WobbleTime, double Agitation, bool bDips) const;

	/** Balanceo (grados) del tablero en S. */
	double DeckRollDeg(double S, double WobbleTime, double Agitation) const;

	/** Transformación (espacio del actor) del centro del tablón Index. */
	FTransform PlankTransform(int32 Index, double WobbleTime, double Agitation, bool bDips) const;

	/** Reloj del servidor suavizado (todas las máquinas ven el mismo vaivén). */
	double AdvanceClock(float DeltaSeconds);

	/** Quién está encima, cuánto agita, hundimientos y crujidos. */
	void UpdateRiders(float DeltaSeconds);

	/** Mueve las cajas de los tablones a la pose de WobbleTime y guarda su velocidad (para heredarla al saltar). */
	void MovePlanks(double WobbleTime, float DeltaSeconds);

	void UpdateDeckMesh();
	void PlayCreak(float Volume, const FVector& WorldAt);
	uint32 ConfigHash() const;

	TArray<FTNBridgePlank> Planks;
	/** Pose actual de cada tablón (espacio del actor). */
	TArray<FTransform> PlankPose;
	TArray<FVector> PrevPlankWorld;
	/** Hundimientos de este fotograma: (X a lo largo, peso). */
	TArray<FVector2D> Dips;
	TMap<TWeakObjectPtr<ACharacter>, FTNBridgeRider> Riders;

	int32 NumPlanks = 0;
	double PlankDepth = 25.0;
	double Clock = 0.0;
	bool bClockValid = false;
	float Excitation = 0.3f;
	float IdleCreakTimer = 3.f;
	uint32 BuiltHash = 0;
};
