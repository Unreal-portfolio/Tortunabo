#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_LobbyValley.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UStaticMesh;
struct FTNLobbyValleyGrid;

/**
 * El valle que rodea el castillo del lobby (LVL_Lobby): el castillo queda abajo del todo y, alrededor, un bioma por hora
 * del reloj (las 12 son la puerta doble, +Y; las 3, -X) que sube hasta una sierra y, detrás, una cordillera lejana que
 * tapa todo el horizonte. Desde la azotea de la torre del homenaje se ven todos de un vistazo.
 *
 * - Las 12, laguna con isletas y cascada; la 1, playa con faro; las 2, dunas con la tortuga colosal; las 3, cañón de
 *   mesas; las 4, volcanes con lava y humo; las 5, acantilados con un castillo en ruinas; las 6, cumbres nevadas; las 7,
 *   bosque; las 8, pueblo en las colinas; las 9, granjas; las 10, selva con pirámide; las 11, manglar.
 * - Terreno de caras planas con color de vértice en una malla procedural sin colisión (nadie sale del castillo);
 *   formaciones y casitas en otra sección; agua, lava y cascada en otra malla. Vegetación instanciada (una HISM por
 *   especie y variante, con las mallas del mapa procedural). Todo se construye igual en el editor y en cada máquina con
 *   la semilla (no se replica ninguna malla); solo se rehace si cambian la semilla, la densidad o los animales.
 * - Fauna de suelo en los sectores (fuera del castillo y del valle cercano), local y cosmética: cuerpos rígidos que
 *   pasean, saltan, picotean o vigilan de pie. En el castillo solo hay pájaros: gaviotas y palomas que van de torre en
 *   torre y bandadas en círculo por encima (y más bandadas sobre los sectores).
 * - Humo y brasas en el volcán, bruma al pie de la cascada y humo en dos chimeneas (TNAmbientFX).
 *
 * Apaga el mar del castillo (ATN_SandCastleLobby::SetDrawSea): el valle ocupa su sitio.
 * Consola: TN.Lobby.Valley 0 lo esconde en ejecución (hay que volver a cargar el lobby). Ver Docs/Lobby_Castillo.md.
 */
UCLASS()
class TORTUNABO_API ATN_LobbyValley : public AActor
{
	GENERATED_BODY()

public:
	ATN_LobbyValley();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostRegisterAllComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** false con TN.Lobby.Valley 0. */
	static bool IsEnabled();

	/** Semilla del valle (la misma en todas las máquinas: cada una lo construye igual). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Valley")
	int32 Seed = 27092026;

	/** Multiplica la vegetación de todos los sectores. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Valley", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FloraDensity = 1.f;

	/** Tope de animales de suelo en todo el valle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Valley", meta = (ClampMin = "0", ClampMax = "96"))
	int32 MaxAnimals = 56;

	/** Pájaros que van de torre en torre por el castillo (en ejecución). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Valley", meta = (ClampMin = "0", ClampMax = "12"))
	int32 CastleBirds = 6;

	/** Apaga el mar y la orilla del castillo de arena más cercano (el valle ocupa su sitio). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Valley")
	bool bHideCastleSea = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Valley")
	TObjectPtr<USceneComponent> ValleyRoot;

	/**
	 * Terreno, sierra y cordillera lejana (sección 0) y formaciones, agujas y casitas (sección 1), sin colisión. Las
	 * mallas generadas son RF_Transient y su puntero, Transient: si se guardaran, al cargar el nivel llegarían a nulo.
	 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Valley")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** Agua de la laguna y del manglar (sección 0), lava que brilla (sección 1) y la cascada (sección 2). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Valley")
	TObjectPtr<UProceduralMeshComponent> WaterMesh;

private:
	/** Un animal de suelo (solo local). Posiciones en el espacio del valle. */
	struct FValleyAnimal
	{
		/** Raíz: el suelo bajo el cuerpo (o la superficie del agua, si flota). */
		FVector Pos = FVector::ZeroVector;
		FVector Home = FVector::ZeroVector;
		FVector Goal = FVector::ZeroVector;
		/** Salto de los peces: de dónde a dónde. */
		FVector JumpFrom = FVector::ZeroVector;
		FVector JumpTo = FVector::ZeroVector;
		float Yaw = 0.f;
		/** Rumbo alrededor del que mira en reposo. */
		float BaseYaw = 0.f;
		float Pitch = 0.f;
		float Roll = 0.f;
		/** Altura del saltito sobre el suelo. */
		float Air = 0.f;
		/** Escala del ejemplar (con el aumento para que se vea desde el castillo). */
		float Size = 1.f;
		float StateT = 0.f;
		float Dur = 1.f;
		/** Fase de la marcha o de los saltitos. */
		float Gait = 0.f;
		float Clock = 0.f;
		/** 0..1: garganta hinchada (ranas). */
		float Pulse = 0.f;
		int32 Kind = 0;
		int32 Slot = 0;
		uint8 Species = 0;
		uint8 State = 0;
		uint8 Act = 0;
		/** +1 o -1: hacia qué lado anda (cangrejos). */
		int8 SideSign = 1;
		/** No se mueve de su sitio (el águila en su aguja). */
		bool bFixed = false;
	};

	/** Especie presente: su malla rígida (y la parte que brilla) instanciada, y las transformadas de sus animales. */
	struct FValleyKind
	{
		uint8 Species = 0;
		float BodyZ = 0.f;
		float HalfLen = 0.f;
		float Draft = 0.f;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Solid;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Glow;
		TArray<FTransform> Xf;
	};

	/** Pájaro del castillo: posado en una almena o volando a otra (en curva, a veces con un rodeo por la plaza). */
	struct FCastleBird
	{
		FVector From = FVector::ZeroVector;
		FVector Ctrl = FVector::ZeroVector;
		FVector To = FVector::ZeroVector;
		FVector Via = FVector::ZeroVector;
		float T = 0.f;
		float Dur = 1.f;
		float Wait = 0.f;
		float Yaw = 0.f;
		float YawGoal = 0.f;
		float LookT = 0.f;
		float Roll = 0.f;
		int32 Perch = 0;
		int32 Target = 0;
		int32 Type = 0;
		int32 Slot = 0;
		bool bFlying = false;
		bool bVia = false;
	};

	/** Vegetación, animales y pájaros del castillo: componentes creados al construir (transitorios, no se duplican). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> GeneratedComps;

	/** Mallas construidas en ejecución (RF_Transient | RF_DuplicateTransient). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> GeneratedMeshes;

	/** Construye lo que falte; lo rehace si cambian la semilla, la densidad o los animales (moverlo no: todo cuelga de la raíz). */
	void BuildAll();
	void ClearGenerated();
	void BuildTerrain();
	void BuildWaterAndLava();
	void BuildFlora();
	void BuildFauna();

	/** Enciende o apaga el mar del castillo más cercano según bHideCastleSea y si el valle se ve. */
	void ApplyCastleSea(bool bValleyShown);

	/** En juego (no en un servidor dedicado): efectos, bandadas y pájaros del castillo. */
	void StartLiving();
	void StopLiving();
	void StartEmitters();
	void StartFlocks();
	void StartCastleBirds();
	void FindPerches();
	void TickFauna(float Dt);
	void TickCastleBirds(float Dt);
	void SimAnimal(FValleyAnimal& A, float Dt);
	bool PickGoal(FValleyAnimal& A);
	FTransform PoseAnimal(const FValleyAnimal& A) const;
	bool HabitatOk(uint8 InSpecies, const FVector2D& P, double Z) const;
	void StartBirdFlight(FCastleBird& B, int32 ToPerch, bool bDetour);
	void Splash(const FVector& LocalPos);
	float RandUnit();
	float RandIn(float Lo, float Hi);

	/** Componente instanciado propio (etiquetado, sin colisión, colgado de la raíz); WpoDistance <= 0 apaga el viento. */
	UInstancedStaticMeshComponent* MakeInstanced(UStaticMesh* Mesh, bool bHierarchical, bool bShadow, int32 WpoDistance);

	/** Cierto si P (local) cae en algún círculo reservado (formaciones, casitas, cráteres, lava, cascada) más Extra. */
	bool IsKeptOut(const FVector2D& P, double Extra) const;

	uint32 SeedU() const { return static_cast<uint32>(Seed); }

	TSharedPtr<FTNLobbyValleyGrid> Grid;
	/** Círculos reservados (x, y, radio), locales. */
	TArray<FVector> KeepOut;
	/** Cimas de las agujas (el águila se posa en la primera). */
	TArray<FVector> Lookouts;
	/** Bocas de las chimeneas con humo. */
	TArray<FVector> Chimneys;
	FVector CraterTop = FVector::ZeroVector;
	FVector SmallCraterTop = FVector::ZeroVector;
	FVector FallFoot = FVector::ZeroVector;

	TArray<FValleyAnimal> Animals;
	TArray<FValleyKind> Kinds;

	TArray<FCastleBird> CastleBirdList;
	/** Almenas y azoteas donde se posan los pájaros del castillo (locales del valle). */
	TArray<FVector> Perches;
	/** Por tipo (0 gaviota, 1 paloma): volando (aleteo del material) y posado (alas plegadas). */
	TWeakObjectPtr<UInstancedStaticMeshComponent> BirdFlyISM[2];
	TWeakObjectPtr<UInstancedStaticMeshComponent> BirdPerchISM[2];
	TArray<FTransform> BirdFlyXf[2];
	TArray<FTransform> BirdPerchXf[2];
	float PerchRetry = 0.f;
	int32 PerchTries = 0;
	bool bPerchesReady = false;

	int32 SplashFX = INDEX_NONE;
	uint32 SimRng = 0x51A7E11u;
	uint32 BuiltKey = 0;
	bool bBuilt = false;
	bool bLiving = false;
};
