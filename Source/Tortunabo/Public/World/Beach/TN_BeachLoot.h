#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/Beach/TN_BeachTypes.h"
#include "World/ProcMap/TN_ProcSearchSpot.h"
#include "TN_BeachLoot.generated.h"

class AActor;
class ATN_BeachElement;
class ATN_BeachRaceGenerator;
class UDataTable;
struct FTN_InventoryItem;

/**
 * Botín en la playa del modo carrera (Docs/Modo_Carrera.md, «Botín en la playa»). En cada ronda, en el servidor y con
 * la semilla de la ronda:
 *  - casi todo el decorado se puede rebuscar (ATN_BeachSearchSpot: el rebuscable del mapa procedural con las reglas de
 *    la carrera);
 *  - hay objetos y power-ups sueltos por todo el recorrido (sueltos y en filas de lado a lado de la playa);
 *  - hay conchas de puntos como en el cooperativo (ATN_ScorePickup): rachas de 1 por los caminos alternativos, arcos de 1
 *    sobre palas, trampolines y catapultas, normales de 25 junto a los peligros y especiales de 50 y 100 en lo difícil o
 *    escondido (lo alto de los castillos, la sala de arriba del castillo con salas, tras el alambre y las minas, sobre
 *    las plataformas móviles).
 * Los objetos, del catálogo de siempre (DT_Items) con los pesos de la carrera.
 */
namespace TNBeachLoot
{
	/**
	 * Servidor: reparte el botín de la ronda actual del generador (quita antes el de la anterior). Pensado para llamarlo
	 * desde ATN_BeachRaceGenerator::GenerateRound justo después de crear los elementos (SpawnRoundElements); si nadie lo
	 * llama, UTN_BeachLootSubsystem lo hace solo en cuanto ve la ronda nueva (el mismo fotograma o el siguiente).
	 */
	TORTUNABO_API void SpawnRoundLoot(ATN_BeachRaceGenerator& Generator);

	/** Catálogo de objetos (el de los rebuscables y las zonas de objetos). */
	inline const TCHAR* CatalogPath() { return TEXT("/Game/Blueprints/Gameplay/Items/DT_Items.DT_Items"); }

	/** Probabilidad de que salga algo al rebuscar en la playa (en el cooperativo, 55 %). */
	constexpr float SearchLuck = 0.7f;

	/** Rebuscables por ronda como mucho, en cuántos tramos iguales del recorrido se reparten y cuántos por tramo. */
	constexpr int32 MaxSearchSpots = 90;
	constexpr int32 Sections = 6;
	constexpr int32 MaxSearchSpotsPerSection = 20;

	/** Separación entre rebuscables (cm): de centro a centro como poco, y de borde a borde (un rebuscable por corrillo). */
	constexpr double MinSearchSpacing = 900.0;
	constexpr double MinSearchRimGap = 300.0;

	/** Objetos sueltos por ronda: sueltos (uno por tramo igual del recorrido) y filas de lado a lado de la playa. */
	constexpr int32 MinLooseItems = 30;
	constexpr int32 MaxLooseItems = 38;
	constexpr int32 ItemRows = 4;
	/** Separación (cm) de los objetos de una fila, de lado a lado. */
	constexpr double ItemRowStep = 3200.0;
	/** Desde dónde hay objetos sueltos (cm desde la línea de salida: nada en la salida). */
	constexpr double LooseStartX = 9000.0;

	/**
	 * Probabilidad de que un decorado de este tipo sea rebuscable en una ronda: casi todo (lo grande, siempre o casi); 0
	 * = nunca (lo diminuto o fino, la medusa y lo que es camino).
	 */
	float SearchChance(ETNBeachElement Element);

	/**
	 * Peso de cada objeto del catálogo en la carrera (rebuscables y sueltos), por su uso: los que ayudan a correr o
	 * fastidian a las demás, más; la cabezota, poco (en la playa no protege de nada); el tótem, nada (no se muere).
	 */
	float RaceWeight(FName RowName, const FTN_InventoryItem& Row);

	/** Color del polvo de rebuscar en la arena. */
	FLinearColor SandDust();

	/**
	 * Si un disco de Radius cm en Local (espacio del generador) queda libre de lo que ocupa cada elemento del reparto de
	 * la ronda (TNBeachLayout::Clearance; las gaviotas, que van por encima, no cuentan), salvo el de SkipIndex, y fuera
	 * del agua de las pozas.
	 */
	bool IsClearOfLayout(const ATN_BeachRaceGenerator& Generator, const FVector2D& Local, double Radius, int32 SkipIndex = INDEX_NONE);
}

/**
 * Decorado de la playa que se puede rebuscar: el ATN_ProcSearchSpot de siempre (mantener E, aro, «¡puf!» u «¡pof!»,
 * saltito del objeto, una vez para todas) con las reglas de la carrera: más suerte (TNBeachLoot::SearchLuck), los pesos
 * de TNBeachLoot::RaceWeight, polvo de arena y las pistas visuales a la escala de la playa (chispitas desde más lejos y
 * el anillo que marca dónde rebuscar, más grande). Lo crea UTN_BeachLootSubsystem con la huella de su decorado.
 */
UCLASS()
class TORTUNABO_API ATN_BeachSearchSpot : public ATN_ProcSearchSpot
{
	GENERATED_BODY()

public:
	ATN_BeachSearchSpot();

protected:
	virtual float GetLootWeight(FName RowName, const FTN_InventoryItem& Row) const override;
};

/**
 * Reparte el botín de cada ronda de la playa, en el servidor, sin tocar el generador (ATN_BeachRaceGenerator): cuando
 * cambia la ronda quita el botín de la anterior (los rebuscables se llevan lo que nadie recogió; las conchas y los
 * objetos sueltos que quedan, también) y reparte el de la nueva (TNBeachLoot::SpawnRoundLoot lo adelanta):
 *  - Rebuscables: entre el decorado de la ronda, con la probabilidad de TNBeachLoot::SearchChance, uno por corrillo (9 m
 *    entre centros y 3 m entre bordes), hasta TNBeachLoot::MaxSearchSpots repartidos a lo largo. Cada uno con la huella
 *    real de su decorado (la caja de su malla, girada como ella: cápsula a lo largo del lado largo).
 *  - Objetos sueltos: uno por tramo igual del recorrido desde los 90 m y filas de lado a lado en cuatro sitios, en la
 *    arena libre (a 3 m de cualquier huella del reparto, pasos de quads incluidos).
 *  - Conchas de puntos (TN_BeachLootShells.cpp).
 *
 * Consola: TN.Beach.Loot 0 (sin botín ni conchas desde la ronda siguiente) y TN.Beach.Loot.Reroll (lo vuelve a
 * repartir ya). Registro: «[Playa] botín de la ronda N: ...» y «[Playa] conchas de la ronda N: ...».
 */
UCLASS()
class TORTUNABO_API UTN_BeachLootSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Servidor: reparte el botín de la ronda actual de Gen (quita antes el que hubiera). */
	void SpawnForRound(ATN_BeachRaceGenerator& Gen);

	/** Servidor: quita el botín de la ronda actual y lo reparte otra vez. */
	void Reroll();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	int32 SpawnSearchSpots(ATN_BeachRaceGenerator& Gen, FRandomStream& Rng, int32& OutCandidates);
	int32 SpawnLooseItems(ATN_BeachRaceGenerator& Gen, FRandomStream& Rng, const UDataTable& Catalog);
	/** Conchas de puntos de la ronda (TN_BeachLootShells.cpp); devuelve el resumen para el registro. */
	FString SpawnRoundShells(ATN_BeachRaceGenerator& Gen, int32 Seed);
	void ClearLoot();

	TWeakObjectPtr<ATN_BeachRaceGenerator> CachedGenerator;

	/** Lo repartido en esta ronda (servidor): rebuscables, objetos sueltos y conchas. */
	TArray<TWeakObjectPtr<AActor>> SpawnedSpots;
	TArray<TWeakObjectPtr<AActor>> SpawnedItems;
	TArray<TWeakObjectPtr<AActor>> SpawnedShells;

	/** Centro (XYZ) y radio del borde (W) de cada rebuscable de la ronda, para no poner objetos sueltos pegados a ellos. */
	TArray<FVector4> SpotDiscs;

	/** Ronda cuyo botín está repartido (0 = ninguna). */
	int32 LootRound = 0;
	/** Tiradas de TN.Beach.Loot.Reroll (cambian la semilla del botín de la misma ronda). */
	int32 RerollSalt = 0;
	float FindClock = 0.f;
};
