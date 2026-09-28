#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Audio/TN_AmbientSynthComponent.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "TN_AmbienceDataAsset.generated.h"

class USoundBase;

/** Sustituye la parte de una capa sintetizada que aporta un bioma por un sonido propio en bucle. */
USTRUCT(BlueprintType)
struct FTNAmbienceOverride
{
	GENERATED_BODY()

	/** Bioma cuya parte de la capa se sustituye (en los degradados entre biomas suena en su proporción). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	ETNProcBiome Biome = ETNProcBiome::Jungle;

	/** Capa sintetizada que deja de sonar en ese bioma. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	ETNAmbientLayer Layer = ETNAmbientLayer::Birds;

	/** Sonido en bucle (marca Looping en la onda o usa un Sound Cue con Looping; si no, vuelve a empezar con un hueco). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	TSoftObjectPtr<USoundBase> Sound;

	/** Volumen respecto al que tendría la capa sintetizada (1 = el mismo nivel de capa). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float VolumeScale = 1.f;
};

/** Sustituye una capa del ambiente genérico (sin mapa procedural: lobby, mapa clásico). */
USTRUCT(BlueprintType)
struct FTNAmbienceGenericOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	ETNAmbientLayer Layer = ETNAmbientLayer::Wind;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float VolumeScale = 1.f;
};

/**
 * Sonidos de ambiente propios que sustituyen a capas sintetizadas, por bioma y capa. Lo que no aparece aquí se sigue
 * sintetizando. Se asigna en UTN_AmbientSoundscapeComponent::AmbienceData (en el Blueprint del PlayerController, o
 * en juego con SetAmbienceData). Los sonidos se cargan al arrancar el paisaje sonoro del jugador local.
 */
UCLASS(BlueprintType)
class TORTUNABO_API UTN_AmbienceDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Por bioma y capa. Si se repite una pareja, vale la primera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (TitleProperty = "Biome"))
	TArray<FTNAmbienceOverride> BiomeOverrides;

	/** Del ambiente genérico (sin mapa procedural). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (TitleProperty = "Layer"))
	TArray<FTNAmbienceGenericOverride> GenericOverrides;
};
