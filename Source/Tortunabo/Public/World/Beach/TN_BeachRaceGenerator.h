#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Beach/TN_BeachLayout.h"
#include "TN_BeachRaceGenerator.generated.h"

class ACharacter;
class ATN_BeachElement;
class ATN_ProcWaterVolume;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UStaticMesh;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeachTurtleReachedWater, ACharacter*, Turtle);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBeachTurtleReachedWaterNative, ACharacter* /*Turtle*/);

/** Lo único que se replica de la ronda: con la semilla, cada cliente rehace los asientos (los elementos llegan solos). */
USTRUCT()
struct FTNBeachRoundNet
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Seed = 0;

	/** Se incrementa en cada GenerateRound (y en ClearRound); 0 = sin ronda. */
	UPROPERTY()
	int32 Round = 0;

	/** La ronda se ha quitado (ClearRound): playa vacía y sin asientos. */
	UPROPERTY()
	bool bCleared = false;
};

/**
 * La playa del modo carrera (LVL_BeachRace, Docs/Modo_Carrera.md): el terreno fijo y el reparto procedural de cada ronda.
 *
 * - Terreno fijo, igual en todas las máquinas y en el editor (TNBeachLayout): 1200 m de arena por 280 m jugables con
 *   leve desnivel hacia el mar y dunas suaves, en teselas de UProceduralMeshComponent con colisión y el material del
 *   terreno del mapa procedural (M_ProcTerrain con relieve y guijarros); a los lados y detrás, bancos que suben a la
 *   selva de palmeras y árboles de 200-300 m (vegetación instanciada del mapa procedural); al final, la repisa y el
 *   acantilado de roca de 15,5 m (TNBeach::CliffHeight) sobre el agua de meta, con el mar animado, las banderas que
 *   flotan y el arco de neumático de la meta del mapa procedural (a escala). Salida en el linde de la selva: entre las
 *   raíces de un árbol colosal y bajo hojas enormes, con el cartel «¡A LA META!». Muros invisibles a los lados, detrás
 *   y mar adentro.
 * - Ronda (GenerateRound, servidor): destruye los elementos de la anterior, reparte con la semilla
 *   (TNBeachLayout::GenerateRound), deja en la arena el asiento de cada elemento (el suelo liso bajo su huella; rehace
 *   solo las teselas tocadas) y crea cada elemento con ATN_BeachElement::SpawnElement. Se replica la semilla: cada
 *   cliente rehace los mismos asientos. El terreno no se cava: los hoyos (la plataforma) los traen los elementos.
 * - Meta: en el servidor, la primera vez por ronda que los pies de una tortuga tocan el agua de meta avisa con
 *   OnTurtleReachedWater; en cada máquina, un chapuzón al entrar en ella.
 *
 * Espacio local: X hacia el mar (la salida en X = 0, el filo en X ≈ 1200 m), Y a lo ancho y el agua en Z = 0 (ver
 * TN_BeachLayout.h). Consola: TN.Beach.ShowFootprints 1 enseña en juego las huellas del reparto.
 */
UCLASS()
class TORTUNABO_API ATN_BeachRaceGenerator : public AActor
{
	GENERATED_BODY()

public:
	ATN_BeachRaceGenerator();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostRegisterAllComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** El generador de la playa de este mundo (el primero; en LVL_BeachRace solo hay uno). */
	static ATN_BeachRaceGenerator* Find(const UObject* WorldContext);

	// ── Ronda ──────────────────────────────────────────────────────────────

	/**
	 * Servidor: reparte una ronda nueva con esta semilla (destruye los elementos de la anterior, deja los asientos en la
	 * arena y crea los elementos) y la replica. Vuelve a armar la meta (cada tortuga avisa otra vez al tocar el agua).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Beach")
	void GenerateRound(int32 InSeed);

	/** Servidor: quita los elementos y sus asientos (playa vacía). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Beach")
	void ClearRound();

	/** El terreno está hecho y la ronda replicada, con sus asientos, en esta máquina (en el servidor, con sus elementos). */
	UFUNCTION(BlueprintPure, Category = "Beach")
	bool IsRoundReady() const;

	UFUNCTION(BlueprintPure, Category = "Beach")
	int32 GetRoundNumber() const { return RoundNet.Round; }

	UFUNCTION(BlueprintPure, Category = "Beach")
	int32 GetRoundSeed() const { return RoundNet.Seed; }

	/** Reparto de la ronda actual en esta máquina (en los clientes, rehecho con la semilla). */
	const TNBeachLayout::FRoundLayout& GetRoundLayout() const { return Layout; }

	/** Elementos creados en esta ronda (solo servidor). */
	const TArray<TObjectPtr<ATN_BeachElement>>& GetRoundElements() const { return RoundElements; }

	// ── Salida, meta y consultas (cualquier máquina) ────────────────────────

	/** Dónde empieza el jugador PlayerIndex (4 en fila en la salida y más filas detrás), 110 cm sobre el suelo, mirando al mar. */
	UFUNCTION(BlueprintPure, Category = "Beach")
	FTransform GetStartTransform(int32 PlayerIndex) const;

	UFUNCTION(BlueprintPure, Category = "Beach")
	int32 GetNumStartSpots() const { return TNBeachLayout::NumStartSpots; }

	/** Si un punto (los pies de la tortuga) está en el agua de meta: más allá del filo y a ras del agua o por debajo. */
	UFUNCTION(BlueprintPure, Category = "Beach")
	bool IsFinishWater(const FVector& WorldLocation) const;

	/**
	 * Franja de la zambullida: los últimos 7,5 m de la repisa antes del filo y el vacío sobre el agua (hasta 40 m más
	 * allá). La animación pone a la tortuga de cabeza si salta desde aquí.
	 */
	UFUNCTION(BlueprintPure, Category = "Beach")
	bool IsCliffJumpZone(const FVector& WorldLocation) const;

	/** Distancia (cm) al filo del acantilado a lo largo del recorrido: negativa antes del filo, positiva sobre el agua. */
	UFUNCTION(BlueprintPure, Category = "Beach")
	float GetCliffEdgeDistance(const FVector& WorldLocation) const;

	/** Progreso 0..1 de la salida al filo (para el HUD o para ordenar a quien no llegó). */
	UFUNCTION(BlueprintPure, Category = "Beach")
	float GetCourseProgress(const FVector& WorldLocation) const;

	/** Cota del suelo (arena, roca o fondo del mar) bajo un punto, con los asientos de la ronda; sin trazas. */
	UFUNCTION(BlueprintPure, Category = "Beach")
	float GetGroundHeightAt(const FVector& WorldLocation) const;

	/** Hacia dónde está el mar (el eje X del generador). */
	UFUNCTION(BlueprintPure, Category = "Beach")
	FVector GetSeaDirection() const { return GetActorForwardVector(); }

	/** Servidor: vuelve a armar la meta sin repartir de nuevo (GenerateRound ya lo hace). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Beach")
	void ResetFinishWater();

	/** Servidor: una tortuga ha tocado el agua de meta (una vez por tortuga y ronda). */
	UPROPERTY(BlueprintAssignable, Category = "Beach")
	FOnBeachTurtleReachedWater OnTurtleReachedWater;

	FOnBeachTurtleReachedWaterNative OnTurtleReachedWaterNative;

	// ── Editor ─────────────────────────────────────────────────────────────

	/** Reparte una ronda en el editor con EditorSeed (o una al azar) para ver el reparto; no se guarda con el nivel. */
	UFUNCTION(CallInEditor, Category = "Beach|Editor")
	void PreviewRound();

	/** Quita la ronda de prueba del editor. */
	UFUNCTION(CallInEditor, Category = "Beach|Editor")
	void ClearPreview();

	UPROPERTY(EditAnywhere, Category = "Beach|Editor")
	int32 EditorSeed = 1;

	UPROPERTY(EditAnywhere, Category = "Beach|Editor")
	bool bEditorRandomSeed = false;

	/** Dibuja en el suelo las huellas del reparto (en el editor; en juego, con TN.Beach.ShowFootprints 1). */
	UPROPERTY(EditAnywhere, Category = "Beach|Editor")
	bool bShowFootprints = true;

	/** Multiplica la selva de los bordes. */
	UPROPERTY(EditAnywhere, Category = "Beach", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float JungleDensity = 1.f;

	/**
	 * Si nadie reparte en unos segundos (nivel abierto sin el GameMode de la carrera), el servidor reparte una ronda al azar
	 * para que se pueda jugar.
	 */
	UPROPERTY(EditAnywhere, Category = "Beach")
	bool bAutoGenerateIfIdle = true;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Beach")
	TObjectPtr<USceneComponent> BeachRoot;

	/**
	 * Mallas generadas en código (RF_Transient; sus punteros, Transient: si se guardaran, al cargar el nivel llegarían a
	 * nulo). Repisa, acantilado y rocas del pie (con colisión).
	 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> CliffMesh;

	/** Fondo del mar (con colisión). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> SeabedMesh;

	/** Superficie del mar (sin colisión: el agua es el volumen nadable). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> SeaMesh;

	/** Salida: tronco, raíces, tallos y postes del cartel (con colisión). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> GroveSolidMesh;

	/** Salida: copa, hojas enormes y el cartel (sin colisión). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> GroveDecoMesh;

	/** Meta: neumático en arco y mástiles (con colisión). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> FinishSolidMesh;

	/** Meta: rótulos, banderines y banderolas (sin colisión). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> FinishDecoMesh;

	/** Boyas con banderas a cuadros que flotan (y se mecen) delante del acantilado. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> FloatMesh;

	/** Huellas del reparto dibujadas en el suelo (editor; en juego, con TN.Beach.ShowFootprints 1). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Beach")
	TObjectPtr<UProceduralMeshComponent> FootprintMesh;

private:
	UPROPERTY(ReplicatedUsing = OnRep_RoundNet)
	FTNBeachRoundNet RoundNet;

	UFUNCTION()
	void OnRep_RoundNet();

	/** Teselas del terreno (creadas al construir; transitorias, no se duplican). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> TerrainTiles;

	/** Selva instanciada y sus mallas construidas en ejecución (RF_Transient | RF_DuplicateTransient). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> FloraComps;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> FloraMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> Walls;

	/** Material del mar con la hondura y la espuma a la escala de la playa. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SeaMaterial;

	/** Elementos de la ronda (servidor, o la ronda de prueba en el editor). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATN_BeachElement>> RoundElements;

	TWeakObjectPtr<ATN_ProcWaterVolume> WaterVolume;

	/** Reparto aplicado en esta máquina (sus asientos están en el terreno). */
	TNBeachLayout::FRoundLayout Layout;
	int32 AppliedRound = 0;

	/** Rejilla del terreno (coordenadas de las filas y columnas de vértices) y teselas. */
	TArray<double> GridXs;
	TArray<double> GridYs;
	int32 TilesX = 0;
	int32 TilesY = 0;

	/** Tortugas que ya han tocado el agua de meta en esta ronda (servidor). */
	TSet<TWeakObjectPtr<ACharacter>> Finishers;
	/** Si cada tortuga estaba en el agua de meta el fotograma anterior (chapuzón al entrar, en cada máquina). */
	TMap<TWeakObjectPtr<ACharacter>, bool> WetTurtles;

	int32 SplashDropsFX = INDEX_NONE;
	int32 SplashFoamFX = INDEX_NONE;
	int32 SplashRingFX = INDEX_NONE;
	float FloatClock = 0.f;
	float IdleTime = 0.f;
	uint32 BuiltKey = 0;
	bool bBuilt = false;
	bool bRoundReady = false;
	bool bLiving = false;

	// ── Construcción (TN_BeachRaceGenerator_Build.cpp) ──
	void BuildAll();
	void ClearGenerated();
	void BuildTerrain();
	/** Rehace la tesela Index del terreno con los asientos Stamps (crea su componente si falta). */
	void BuildTerrainTile(int32 Index, const TArray<TNBeachLayout::FStamp>& Stamps);
	void BuildCliff();
	void BuildSeabedAndSea();
	void BuildWalls();
	void SpawnWaterVolume();

	// ── Escenografía (TN_BeachRaceGenerator_Scenery.cpp) ──
	void BuildStartGrove();
	void BuildFinishDecor();
	void BuildJungle();
	void BuildFootprints();
	UInstancedStaticMeshComponent* MakeFlora(UStaticMesh* Mesh, bool bShadow, int32 WpoDistance);
	void StartLiving();
	void StopLiving();
	void Splash(const FVector& WorldLocation);

	// ── Ronda (TN_BeachRaceGenerator.cpp) ──
	/** Aplica un reparto en esta máquina: deja sus asientos en la arena (y quita los de la anterior) y dibuja sus huellas. */
	void ApplyLayoutLocal(const TNBeachLayout::FRoundLayout& NewLayout);
	/** Crea los elementos del reparto; devuelve las clases que faltan (vacío si están todas). */
	FString SpawnRoundElements();
	void DestroyRoundElements();
	void TickTurtles(float DeltaSeconds);
	void TickAutoGenerate(float DeltaSeconds);
	bool ShouldShowFootprints() const;
};
