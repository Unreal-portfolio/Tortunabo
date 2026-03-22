#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "TN_ItemSpawnZone.generated.h"

class UBoxComponent;
class ATN_PickupInteractableBase;

/**
 * Zona de spawn de ítems para el mapa de carrera.
 * Coloca este actor en el nivel, ajusta el BoxExtent y configura
 * las filas del DataTable + SpawnCount en Class Defaults o por instancia.
 * En BeginPlay (server), genera los ítems aleatoriamente dentro de la zona
 * validando que cada posición tiene suelo y no hay obstáculos.
 */
UCLASS()
class TORTUNABO_API ATN_ItemSpawnZone : public AActor
{
	GENERATED_BODY()

public:
	ATN_ItemSpawnZone();

protected:
	virtual void BeginPlay() override;

	/** Zona de spawn visible en editor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpawnZone")
	TObjectPtr<UBoxComponent> SpawnBox;

	/**
	 * DataTable con filas FTN_InventoryItem.
	 * Cada spawn elige una fila aleatoria de ItemRowNames.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnZone")
	TObjectPtr<UDataTable> ItemDataTable;

	/**
	 * Nombres de filas válidas dentro del DataTable.
	 * Cada spawn elige una fila aleatoria de este array.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnZone")
	TArray<FName> ItemRowNames;

	/** Cuántos ítems generar en esta zona. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnZone", meta = (ClampMin = "1", ClampMax = "50"))
	int32 SpawnCount = 5;

	/** Distancia mínima entre ítems spawneados (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnZone", meta = (ClampMin = "0"))
	float MinSpacing = 100.f;

	/** Máx. reintentos por ítem si la posición es inválida. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnZone", meta = (ClampMin = "1"))
	int32 MaxRetries = 10;

private:
	/** Genera una posición random dentro del box, valida suelo y obstáculos. */
	bool FindValidSpawnPoint(FVector& OutLocation, const TArray<FVector>& ExistingLocations) const;
};

