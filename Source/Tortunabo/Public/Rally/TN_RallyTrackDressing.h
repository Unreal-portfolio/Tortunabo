// Límites y decorado del trazado del Rally (#303): vallas a los dos lados en las curvas y en los tramos con caída (palos y
// cuerda, sacos terreros, troncos, neumáticos apilados o castillos de arena) con un carril de colisión continuo y poco
// rozamiento, decorado de playa de la Carrera fuera del corredor (TNBeachDecorKit), público en las curvas y en la meta y
// los pórticos de /Game/Art/IA/rally en las puertas. Cada máquina lo construye igual a partir del eje de ATN_RallyTrack y
// de una semilla (como la pista y el decorado de la ronda de la playa): no se replica nada.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Beach/TN_BeachTypes.h"
#include "TN_RallyTrackDressing.generated.h"

class ATN_RallyTrack;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPhysicalMaterial;
class UStaticMesh;
struct FTNRallyDressingBatches;

/** Cómo se ve un tramo de límite (el carril de colisión es el mismo en todos). */
UENUM(BlueprintType)
enum class ETNRallyBarrierStyle : uint8
{
	PostRope UMETA(DisplayName = "Palos y cuerda"),
	Sandbags UMETA(DisplayName = "Sacos terreros"),
	Logs UMETA(DisplayName = "Troncos"),
	Tires UMETA(DisplayName = "Neumáticos apilados"),
	Castles UMETA(DisplayName = "Castillos y cubos de arena")
};

/** Hueco sin límite (atajo): de StartArcCm a EndArcCm del eje; en circuito puede dar la vuelta (End < Start). */
USTRUCT(BlueprintType)
struct FTNRallyDressingGap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rally")
	float StartArcCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rally")
	float EndArcCm = 0.f;

	/** -1 = izquierda, +1 = derecha, 0 = los dos lados. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rally", meta = (ClampMin = "-1", ClampMax = "1"))
	int32 Side = 0;
};

/** Elemento de playa del decorado y su peso en el reparto. */
USTRUCT(BlueprintType)
struct FTNRallyDecorEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rally")
	ETNBeachElement Element = ETNBeachElement::PlantedUmbrella;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rally", meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** Tamaño respecto a la huella nominal (TNBeach::FootprintRadius), entre 0,5 y 1,6. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rally", meta = (ClampMin = "0.5", ClampMax = "1.6"))
	float MinSize = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rally", meta = (ClampMin = "0.5", ClampMax = "1.6"))
	float MaxSize = 0.9f;
};

/** Reglas puras de los límites y del reparto del decorado (Tortunabo.Rally.Dressing.*): sin mundo, para poder probarlas. */
namespace TNRallyDressing
{
	inline constexpr int32 LeftSide = 0;
	inline constexpr int32 RightSide = 1;

	/** -1 a la izquierda y +1 a la derecha del sentido de la carrera. */
	inline double SideSign(int32 Side) { return Side == LeftSide ? -1.0 : 1.0; }

	/** Muestra del eje: punto a la cota de la calzada, dirección en planta (unitaria) y arco (cm). */
	struct FAxisSample
	{
		FVector Location = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		double Arc = 0.0;
	};

	/** Lo que el decorado necesita del trazado (SampleTrack lo saca de ATN_RallyTrack; los tests lo montan a mano). */
	struct FTrackData
	{
		/** Muestras equiespaciadas del eje; en circuito, la última no repite la primera. */
		TArray<FAxisSample> Samples;
		double StepCm = 400.0;
		double LengthCm = 0.0;
		bool bClosed = false;
		/** Ancho de la calzada (cm); 0 = el de FBarrierParams::DefaultRoadWidthCm. */
		double RoadWidthCm = 0.0;
		/** Puertas a la cota de la calzada con X en el sentido de la carrera, y la de la meta (INDEX_NONE si no hay). */
		TArray<FTransform> Gates;
		int32 FinishGate = INDEX_NONE;
		double GateHalfWidthCm = 1200.0;
		bool bHasWater = false;
		double WaterZ = 0.0;
		/** Atajos: sin límite en esos arcos. */
		TArray<FTNRallyDressingGap> Gaps;
	};

	struct FBarrierParams
	{
		double DefaultRoadWidthCm = 1400.0;
		/** Arcén entre el borde de la calzada y el límite en recta, y mínimo para no pisar los postes de las puertas. */
		double ShoulderCm = 400.0;
		double MinOffsetCm = 1500.0;
		/** Curva: radio de 250 m o menos; con 60 m o menos, toda la escapatoria por fuera. */
		double CurveMinCurvature = 1.0 / 25000.0;
		double CurveFullCurvature = 1.0 / 6000.0;
		double MaxRunoffCm = 800.0;
		/** Por dentro de la curva, el límite no pasa de esta fracción del radio ni baja del borde más este margen. */
		double InsideRadiusFraction = 0.6;
		double InsideMinMarginCm = 150.0;
		/** En una caída, el límite va al borde de la calzada más este margen. */
		double DropEdgeMarginCm = 250.0;
		/** Ventana para medir la curvatura y tramo que se alarga el límite antes y después de una curva o una caída. */
		double CurvatureWindowCm = 2000.0;
		double CurveLeadCm = 3000.0;
		double DropLeadCm = 1500.0;
		/** Huecos más cortos que esto entre dos tramos de límite se cierran (los atajos no). */
		double MinGapCm = 2500.0;
		/** Ventana de suavizado del desplazamiento lateral a lo largo del límite. */
		double SmoothWindowCm = 1600.0;
		/** Otro tramo del trazado (horquillas): se ignora a menos de este arco y cuenta si está a menos de MaxDz de altura. */
		double OtherSectionExcludeArcCm = 3000.0;
		double OtherSectionMaxDzCm = 800.0;
		double OtherSectionMarginCm = 400.0;
	};

	/** Desplazamiento lateral de cada muestra en un lado: 0 = sin límite. Runs: tramos seguidos, en orden de la carrera. */
	struct FBarrierSide
	{
		TArray<double> OffsetCm;
		TArray<TArray<int32>> Runs;

		bool IsLimited(int32 Index) const { return OffsetCm.IsValidIndex(Index) && OffsetCm[Index] > 0.0; }
	};

	struct FBarrierPlan
	{
		FBarrierSide Sides[2];
		/** Curvatura con signo (1/cm): positiva si la curva gira a la derecha. */
		TArray<double> Curvature;
		double RoadHalfCm = 0.0;
		double BaseOffsetCm = 0.0;

		/** Hasta dónde llega el corredor por ese lado: el límite si lo hay y, si no, el desplazamiento base. */
		double EdgeCm(int32 Side, int32 Index) const
		{
			return Sides[Side].IsLimited(Index) ? Sides[Side].OffsetCm[Index] : BaseOffsetCm;
		}
	};

	/** Semilla derivada estable (igual en todas las máquinas, no negativa) para el elemento (A, B). */
	TORTUNABO_API int32 SubSeed(int32 Seed, int32 A, int32 B);

	/** Semiancho de la calzada y desplazamiento del límite en recta. */
	TORTUNABO_API double RoadHalfWidthCm(const FTrackData& Track, const FBarrierParams& Params);
	TORTUNABO_API double BaseOffsetCm(double RoadHalfCm, const FBarrierParams& Params);

	/** Curvatura con signo de cada muestra (cambio de rumbo en la ventana / arco de la ventana). */
	TORTUNABO_API TArray<double> SignedCurvature(const TArray<FAxisSample>& Samples, bool bClosed, double WindowCm);

	/** Desplazamiento del límite en una curva: por fuera, base más escapatoria; por dentro, acotado por el radio. */
	TORTUNABO_API double BarrierOffsetCm(double Curvature, int32 Side, double RoadHalfCm, const FBarrierParams& Params);

	/** Punto a OffsetCm del eje por el lado Side, a la cota de la muestra. */
	TORTUNABO_API FVector LateralPoint(const FAxisSample& Sample, int32 Side, double OffsetCm);

	/**
	 * Límites de los dos lados: curvas (alargadas CurveLeadCm) y caídas (DropMask: bit 0 izquierda, bit 1 derecha; alargadas
	 * DropLeadCm), huecos cortos cerrados, atajos abiertos, desplazamiento suavizado y recortado donde pisaría otro tramo.
	 */
	TORTUNABO_API FBarrierPlan PlanBarriers(const FTrackData& Track, const TArray<uint8>& DropMask, const FBarrierParams& Params);

	/** Punto de una polilínea con su rumbo y la separación real con el siguiente. */
	struct FPolySpot
	{
		FVector Location = FVector::ZeroVector;
		double YawDeg = 0.0;
		double SeparationCm = 0.0;
	};

	/** Puntos repartidos a lo largo de la polilínea, centrados en tramos iguales de como mucho SpacingCm. */
	TORTUNABO_API TArray<FPolySpot> ResamplePolyline(const TArray<FVector>& Points, double SpacingCm);

	/** True si Point (en planta) está a ClearCm o más de todas las muestras del eje. */
	TORTUNABO_API bool IsClearOfTrack(const FTrackData& Track, const FVector& Point, double ClearCm);

	enum class ESpotKind : uint8
	{
		Beach,
		Crab,
		Spectator
	};

	/** Sitio de una pieza del decorado (a la cota del eje: el actor busca el suelo). Entry: índice de la entrada o variante. */
	struct FSpot
	{
		ESpotKind Kind = ESpotKind::Beach;
		int32 Entry = 0;
		FVector Location = FVector::ZeroVector;
		double YawDeg = 0.0;
		double RadiusCm = 0.0;
		float Size = 1.f;
		int32 Seed = 0;
	};

	struct FDecorParams
	{
		/** Elementos de playa y cangrejos de atrezo por kilómetro y lado. */
		double BeachPerKm = 30.0;
		double CrabsPerKm = 12.0;
		/** Hueco entre el borde del corredor y el decorado, y franja (más allá) en la que se reparte. */
		double ClearanceCm = 800.0;
		double BandCm = 4000.0;
		double CrabRadiusCm = 150.0;
		/** Público: grupos en las curvas cerradas (por fuera) y en la meta (a los dos lados). */
		int32 SpectatorsPerGroup = 8;
		double SpectatorGroupSpacingCm = 6000.0;
		double SpectatorMinCurvature = 1.0 / 12000.0;
		double SpectatorSetbackCm = 500.0;
		double SpectatorSpacingCm = 200.0;
		int32 SpectatorsAtFinish = 16;
		int32 SpectatorVariants = 4;
	};

	/** Decorado de playa y cangrejos fuera del corredor, sin solaparse; determinista con Seed. */
	TORTUNABO_API TArray<FSpot> PlanDecor(const FTrackData& Track, const FBarrierPlan& Plan, const TArray<FTNRallyDecorEntry>& Entries,
		const FDecorParams& Params, int32 Seed);

	/** Público mirando a la calzada detrás del límite, en las curvas cerradas y en la meta; determinista con Seed. */
	TORTUNABO_API TArray<FSpot> PlanSpectators(const FTrackData& Track, const FBarrierPlan& Plan, const FDecorParams& Params, int32 Seed);

	/** Muestrea la pista ya construida cada StepCm (eje, puertas, meta y agua). */
	TORTUNABO_API FTrackData SampleTrack(const ATN_RallyTrack& Track, double StepCm);
}

/**
 * Decorado del trazado del Rally. Local en cada máquina (bReplicates = false): todo sale del eje de la pista, que cada máquina
 * construye con el mismo manifest, y de la semilla; el suelo se busca con trazas sobre el mismo terreno. Así la colisión es
 * idéntica en el servidor y en los clientes sin coste de red ni problemas de entrada tardía.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_RallyTrackDressing : public AActor
{
	GENERATED_BODY()

public:
	ATN_RallyTrackDressing();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Construye límites, decorado, público y pórticos (quita antes lo que hubiera). False sin trazado. */
	bool Build(const TNRallyDressing::FTrackData& Track, int32 Seed);

	/** Build con la pista ya construida; esconde los arcos provisionales de las puertas que reciben pórtico. */
	UFUNCTION(BlueprintCallable, Category = "Rally|Decorado")
	bool BuildFromTrack(ATN_RallyTrack* Track, int32 Seed);

	/** Usa el decorado del nivel (o crea uno) y lo construye para Track. Lo llama ATN_RallyGameState::PrepareTrack. */
	static ATN_RallyTrackDressing* BuildForTrack(ATN_RallyTrack* Track, int32 Seed);

	UFUNCTION(BlueprintCallable, Category = "Rally|Decorado")
	void ClearDressing();

	UFUNCTION(BlueprintPure, Category = "Rally|Decorado")
	int32 GetRailSegmentCount() const { return RailSegmentCount; }

	UFUNCTION(BlueprintPure, Category = "Rally|Decorado")
	int32 GetBarrierPieceCount() const { return BarrierPieceCount; }

	UFUNCTION(BlueprintPure, Category = "Rally|Decorado")
	int32 GetDecorCount() const { return DecorCount; }

	UFUNCTION(BlueprintPure, Category = "Rally|Decorado")
	int32 GetSpectatorCount() const { return SpectatorCount; }

	UFUNCTION(BlueprintPure, Category = "Rally|Decorado")
	int32 GetGateMeshCount() const { return GateMeshCount; }

protected:
	// ── Límites ──

	/** Separación de las muestras del eje (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "100"))
	float SampleStepCm = 400.f;

	/** Ancho de la calzada (cm); 0 = el del trazado o 14 m. */
	UPROPERTY(EditAnywhere, Category = "Rally|Límites", meta = (ClampMin = "0"))
	float RoadWidthOverrideCm = 0.f;

	/** Atajos sin límite (el manifest no los trae). */
	UPROPERTY(EditAnywhere, Category = "Rally|Límites")
	TArray<FTNRallyDressingGap> ShortcutGaps;

	/** Estilos que se reparten por tramo de límite (con la semilla). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites")
	TArray<ETNRallyBarrierStyle> BarrierStyles;

	/** Carril de colisión: alto, grueso y cuánto se hunde en el suelo (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "50"))
	float RailHeightCm = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "10"))
	float RailThicknessCm = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "0"))
	float RailSinkCm = 40.f;

	/** Rozamiento y rebote del carril: bajos para que el buggy resbale a lo largo en vez de pararse en seco. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "0", ClampMax = "1"))
	float RailFriction = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "0", ClampMax = "1"))
	float RailRestitution = 0.1f;

	/** Material físico del carril; si no hay, se crea uno con RailFriction y RailRestitution (combinación Min). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites")
	TObjectPtr<UPhysicalMaterial> RailPhysicalMaterial;

	/** Dibuja el carril de colisión (depuración). */
	UPROPERTY(EditAnywhere, Category = "Rally|Límites")
	bool bShowRails = false;

	/** Caída: el suelo, a esta distancia más allá del límite base, queda más abajo que esto (o es agua). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "50"))
	float DropThresholdCm = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites")
	TSoftObjectPtr<UStaticMesh> TireMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "30"))
	float TireDiameterCm = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Límites", meta = (ClampMin = "1", ClampMax = "6"))
	int32 TireStackCount = 3;

	// ── Decorado y público ──

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado")
	TArray<FTNRallyDecorEntry> DecorEntries;

	/** Densidad: elementos de playa y cangrejos por kilómetro y lado. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado", meta = (ClampMin = "0"))
	float DecorPerKm = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado", meta = (ClampMin = "0"))
	float CrabPropsPerKm = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado", meta = (ClampMin = "0"))
	float DecorBandCm = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado")
	TSoftObjectPtr<UStaticMesh> CrabPropMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado", meta = (ClampMin = "50"))
	float CrabPropSizeCm = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado", meta = (ClampMin = "0", ClampMax = "24"))
	int32 SpectatorsPerGroup = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado", meta = (ClampMin = "0", ClampMax = "64"))
	int32 SpectatorsAtFinish = 16;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado", meta = (ClampMin = "1000"))
	float SpectatorGroupSpacingCm = 6000.f;

	/** Material de color de vértice de las tortugas del público (el de los cosméticos y el decorado de playa). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Decorado")
	TSoftObjectPtr<UMaterialInterface> SpectatorMaterial;

	// ── Pórticos ──

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Pórticos")
	bool bPlaceGateMeshes = true;

	/** Esconde el arco provisional (BasicShapes) de ATN_RallyGate donde se pone un pórtico, para no duplicarlo. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Pórticos")
	bool bReplaceTrackGateArches = true;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Pórticos")
	TSoftObjectPtr<UStaticMesh> StartGateMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Pórticos")
	TSoftObjectPtr<UStaticMesh> FinishGateMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Pórticos")
	TSoftObjectPtr<UStaticMesh> CheckpointMesh;

	/** Margen de los postes del pórtico fuera del volumen de la puerta, y alto si la malla es un poste suelto (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Pórticos", meta = (ClampMin = "0"))
	float GateMarginCm = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Pórticos", meta = (ClampMin = "100"))
	float GatePostHeightCm = 900.f;

private:
	TNRallyDressing::FBarrierParams MakeBarrierParams() const;
	TNRallyDressing::FDecorParams MakeDecorParams() const;
	TNRallyDressing::FTrackData WithOverrides(const TNRallyDressing::FTrackData& Track) const;

	bool TraceGround(const FVector& Location, double UpCm, double DownCm, FVector& OutGround) const;
	/** Suelo cerca de la cota de referencia (ni agua ni a más de 6 m de desnivel). */
	bool FindGroundNear(const TNRallyDressing::FTrackData& Track, const FVector& Location, double ReferenceZ, FVector& OutGround) const;
	bool IsWater(const TNRallyDressing::FTrackData& Track, double Z) const;
	TArray<uint8> ProbeDrops(const TNRallyDressing::FTrackData& Track, double BaseOffsetCm) const;

	/** Base de cada punto de un tramo de límite: el suelo si lo hay cerca y, si no (caída), la cota del eje. */
	TArray<FVector> RunPoints(const TNRallyDressing::FTrackData& Track, const TNRallyDressing::FBarrierSide& Barrier, const TArray<int32>& Run,
		int32 Side, TArray<bool>& OutGrounded) const;
	void AddRails(const TNRallyDressing::FTrackData& Track, const TNRallyDressing::FBarrierPlan& Plan, int32 Seed, FTNRallyDressingBatches& Batches);
	void AddRailSegment(UStaticMesh* Cube, const FVector& A, const FVector& B, FTNRallyDressingBatches& Batches);
	void AddBarrierRun(const TArray<FVector>& Points, ETNRallyBarrierStyle Style, int32 RunSeed, FTNRallyDressingBatches& Batches);
	void AddPostRopeRun(const TArray<TNRallyDressing::FPolySpot>& Spots, int32 RunSeed, FTNRallyDressingBatches& Batches);
	void AddPieceRun(const TArray<FVector>& Points, ETNBeachElement First, ETNBeachElement Second, float Size, int32 RunSeed,
		FTNRallyDressingBatches& Batches);
	void AddTireRun(const TArray<FVector>& Points, FTNRallyDressingBatches& Batches);
	/** Receta de playa: libre (giro, inclinación y hundimiento de su semilla) o alineada con ItemXf (límites). */
	bool AddBeachPiece(ETNBeachElement Element, int32 Seed, float Size, const FTransform& ItemXf, bool bFreePlacement, bool bCollision,
		FTNRallyDressingBatches& Batches);

	void AddDecor(const TNRallyDressing::FTrackData& Track, const TNRallyDressing::FBarrierPlan& Plan, int32 Seed, FTNRallyDressingBatches& Batches);
	void AddSpectators(const TNRallyDressing::FTrackData& Track, const TNRallyDressing::FBarrierPlan& Plan, int32 Seed,
		FTNRallyDressingBatches& Batches);
	UStaticMesh* GetSpectatorMesh(int32 Variant);
	void AddGateMeshes(const TNRallyDressing::FTrackData& Track, FTNRallyDressingBatches& Batches);
	bool PlaceGateMesh(UStaticMesh* Mesh, const TNRallyDressing::FTrackData& Track, const FTransform& Gate, FTNRallyDressingBatches& Batches);
	void HideTrackGateArches(const ATN_RallyTrack& Track) const;

	void CreateComponents(const FTNRallyDressingBatches& Batches);
	/** Collision: FTNRallyDressingBatches::ECollision (definido en el .cpp). */
	void ApplyCollision(UInstancedStaticMeshComponent* Comp, uint8 Collision);
	UPhysicalMaterial* GetRailMaterial();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> MeshComponents;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> RuntimeRailMaterial;

	/** Tortugas del público (mallas en ejecución, una por variante de color). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> SpectatorMeshes;

	/** Puertas que han recibido pórtico (índice de puerta). */
	TArray<bool> DressedGates;

	bool bVisuals = true;
	int32 RailSegmentCount = 0;
	int32 BarrierPieceCount = 0;
	int32 DecorCount = 0;
	int32 SpectatorCount = 0;
	int32 GateMeshCount = 0;
};
