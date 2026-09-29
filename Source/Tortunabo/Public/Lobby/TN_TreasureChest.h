#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcSearchSpot.h"
#include "TN_TreasureChest.generated.h"

class APawn;
class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Cofre del tesoro en lo alto de la torre del homenaje del lobby. Lo crea ATN_SandCastleLobby en el servidor, en su
 * BeginPlay: va en la azotea, sobre una tarima de arena delante del torreón y mirando a la plaza. No se guarda en el
 * nivel.
 *
 * Se rebusca igual que los decorados del mapa procedural (hereda de ATN_ProcSearchSpot: mantener E, aro del HUD,
 * tiempo contado en el servidor, «¡puf!» y saltito del objeto), con estas diferencias:
 *  - Cuesta más: 5 s de búsqueda (SearchSeconds). No hace caso de tn.Search.Seconds.
 *  - Siempre da un objeto al azar de DT_Items (LootChance 1). No hace caso de tn.Search.Luck.
 *  - Se puede volver a rebuscar tras 2,5 s de respiro (bRepeatable), para que salga el objeto antes. Quedan como mucho
 *    seis objetos sin recoger (MaxLootLying): al pasarse, se va el más viejo.
 *  - El objeto sale de dentro del cofre de un saltito y cae delante de él, en la tarima (nunca fuera de la azotea).
 *
 * Malla low-poly de madera con flejes de hierro, cantoneras, remaches y cerradura dorados y el tesoro dentro, hecha en
 * código (TNProcRuntimeMesh, colores de vértice; nada en servidor dedicado). La colisión es una caja (ChestBlock).
 * Mientras alguien rebusca, la tapa se entreabre a tirones (crujidos) con un brillo dorado dentro (luz y chispitas).
 * Al salir el objeto se abre del todo y luego se cierra con un «¡clonc!». Todo sale del estado replicado de la base en
 * cada máquina con pantalla; no se replica nada más. Ver Docs/Lobby_Castillo.md y Docs/Botin_Decorados.md.
 */
UCLASS()
class TORTUNABO_API ATN_TreasureChest : public ATN_ProcSearchSpot
{
	GENERATED_BODY()

public:
	ATN_TreasureChest();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ── ATN_InteractableBase ─────────────────────────────────────────────────
	virtual float GetHoldDuration() const override;

protected:
	// ── ATN_ProcSearchSpot ───────────────────────────────────────────────────
	virtual float GetLuck() const override;
	virtual FVector GetLootOrigin(const APawn* Pawn) const override;
	virtual FVector GetRummageOrigin(const APawn* Searcher) const override;
	virtual FVector FindLanding(const APawn* Pawn, const FVector& From) const override;
	virtual void OnSearchStateChanged(const FTNSearchSpotState& OldState) override;
	virtual bool WantsFrameTick() const override;

	/** Caja del cofre: madera, herrajes y el tesoro de dentro. No tiene colisión; la pone ChestBlock. */
	UPROPERTY(VisibleAnywhere, Category = "Chest")
	TObjectPtr<UStaticMeshComponent> ChestBody;

	/** Tapa de medio cañón, con el origen en la bisagra (borde de atrás de la caja): se abre girando hacia atrás. */
	UPROPERTY(VisibleAnywhere, Category = "Chest")
	TObjectPtr<UStaticMeshComponent> ChestLid;

	/** Colisión del cofre cerrado: se choca con él y se puede subir encima. */
	UPROPERTY(VisibleAnywhere, Category = "Chest")
	TObjectPtr<UBoxComponent> ChestBlock;

	/** Brillo dorado de dentro: se enciende con la tapa abierta. */
	UPROPERTY(VisibleAnywhere, Category = "Chest")
	TObjectPtr<UPointLightComponent> GlowLight;

private:
	/** Mallas de la caja y de la tapa (editor y ejecución; no en servidor dedicado). */
	void BuildChestMeshes();

	/** Tapa, luz, destellos y crujidos según el estado replicado (máquinas con pantalla). */
	void TickLid(float DeltaSeconds);

	/** Boca del cofre (centro de la caja a ras de su borde), en el mundo. */
	FVector GetMouthPoint() const;

	/** Ángulo de la tapa (grados; 0 = cerrada) y su velocidad (grados/s). */
	float LidAngle = 0.f;
	float LidSpeed = 0.f;
	/** Adónde iba la tapa en el último tick (grados). */
	float LidTarget = 0.f;
	float LidClock = 0.f;
	float CreakClock = 0.f;
	float GlintClock = 0.f;
	float ThumpCooldown = 0.f;
};
