#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_MapVariantLoader.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class FJsonObject;

/**
 * Herramienta de disenadores: carga en el nivel abierto una de las variantes de mapa que genera
 * Scripts/terrain_volumes/Variants/<nombre>/ (manifest.json + Chunks/r{fila}c{col}.bin en
 * TNTM2, indexadas en Scripts/terrain_volumes/Variants/index.json). Construye un
 * UProceduralMeshComponent por trozo con TNTerrainMesh::ParseChunk + ToTileMesh, el mismo
 * camino que usa ATN_TerrainMeshTile, sin pasar por un UTN_TerrainMeshAsset ni por el editor.
 *
 * Vive en Source/ pero lee de Scripts/, que NO se empaqueta: esta clase es solo para el editor
 * y para PIE/standalone lanzados desde el editor durante el desarrollo. No usarla en build.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_MapVariantLoader : public AActor
{
	GENERATED_BODY()

public:
	ATN_MapVariantLoader();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Variante elegida: Scripts/terrain_volumes/Variants/<Variant>/manifest.json. */
	UPROPERTY(EditAnywhere, Category = "MapVariant", meta = (GetOptions = "GetVariantNames"))
	FName Variant;

	/** Material del terreno (color de vertice), el mismo que usa ATN_TerrainMeshTile. */
	UPROPERTY(EditAnywhere, Category = "MapVariant")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** Campo "description" del manifest de la variante cargada. Solo lectura. */
	UPROPERTY(VisibleAnywhere, Category = "MapVariant")
	FString VariantDescription;

	/** Alimenta el desplegable de Variant: lee Variants/index.json o, si falta, las carpetas
	 * con manifest.json bajo Variants/. */
	UFUNCTION(BlueprintPure, Category = "MapVariant")
	TArray<FString> GetVariantNames() const;

	/** Reconstruye la malla desde cero. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "MapVariant")
	void Recargar();

private:
	void LoadVariant();
	void ClearMeshes();
	void MoveStartPlayerStart(const TSharedPtr<FJsonObject>& Manifest) const;
	static FString VariantsDir();

	/** Un UProceduralMeshComponent por trozo del manifest ("cells"). */
	UPROPERTY(VisibleAnywhere, Category = "MapVariant")
	TArray<TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;

	/** Variante con la que se construyeron ChunkMeshes, para no reconstruir en balde. */
	UPROPERTY()
	FName BuiltVariant;
};
