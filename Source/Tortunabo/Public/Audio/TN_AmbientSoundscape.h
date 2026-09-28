#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Audio/TN_AmbientSynthComponent.h"
#include "TN_AmbientSoundscape.generated.h"

class UTN_AmbienceDataAsset;
class UAudioComponent;
class USoundClass;
class ATN_ProcMapGenerator;
class ATN_PathStorm;
class APlayerController;

/** Un sonido del DataAsset que suena en lugar de la parte de una capa sintetizada que aporta un bioma. */
USTRUCT()
struct FTNAmbienceOverrideVoice
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Component = nullptr;

	/** Hueco de bioma (índice de ETNProcBiome; ETNProcBiome::Count = ambiente sin mapa) y capa. */
	int32 BiomeSlot = 0;
	int32 LayerIndex = 0;
	float VolumeScale = 1.f;
	/** Segundos seguidos sin volumen (se para del todo a los 2 s). */
	float SilentFor = 0.f;
	bool bPlaying = false;
};

/**
 * Paisaje sonoro del jugador local según el bioma del mapa procedural. Va en el PlayerController (o en el pawn): cada
 * UpdateInterval mira dónde está la cámara y fija los volúmenes de un UTN_AmbientSynthComponent 2D que crea él mismo:
 *   - Biomas: el punto de la cámara y un anillo de BlendRadius alrededor sobre el campo de pesos de bioma del layout
 *     (el mismo que usa la tormenta): el módulo en el que se está manda, pero los vecinos se oyen al acercarse a ellos
 *     y el cambio de bioma es un degradado.
 *   - Agua: terreno bajo el nivel del mar en tres anillos (15, 40 y 80 m) y la costa norte (mar abierto: rompientes
 *     grandes); cerca del río, agua corriente.
 *   - Altura: sobre el mar y sobre el suelo (cimas, puentes y acantilados: más viento, más ráfaga y silbido).
 *   - Tormenta del camino: dentro, mucho más viento, truenos y casi sin fauna; al acercarse el frente, algo de eso.
 *   - Cierre: con techo encima (cueva, torre) o bajo el agua, paso bajo general y menos fauna.
 *   - Noche (NightAmount): menos aves y cigarras, más grillos y ranas.
 * Sin generador (lobby, mapa clásico) suena un ambiente genérico suave (bPlayWithoutGenerator). En servidor dedicado y
 * para controladores que no son locales no hace nada. Las capas con sonido en AmbienceData suenan con ese sonido en
 * bucle en vez de sintetizarse.
 *
 * Depuración: TN.Ambience.Debug 1 enseña la mezcla en pantalla; TN.Ambience.Volume escala el volumen (0 = apagado).
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_AmbientSoundscapeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_AmbientSoundscapeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Crea el paisaje sonoro en InOwner si aún no lo tiene (atajo desde código, p. ej. en el BeginPlay del
	 * PlayerController local). Devuelve el que haya; null en servidor dedicado.
	 */
	static UTN_AmbientSoundscapeComponent* EnsureOn(AActor* InOwner);

	/** Cambia los sonidos de sustitución (también en juego). */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void SetAmbienceData(UTN_AmbienceDataAsset* InData);

	/** El sintetizador 2D (null hasta que el jugador local arranca). */
	UFUNCTION(BlueprintPure, Category = "Ambience")
	UTN_AmbientSynthComponent* GetSynth() const { return AmbientSynth; }

	/** Texto con la mezcla actual: biomas, capas y contexto (lo enseña TN.Ambience.Debug 1). */
	FString GetDebugString() const;

	/** Sonidos propios por bioma y capa (opcional): lo que tenga sonido deja de sintetizarse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<UTN_AmbienceDataAsset> AmbienceData;

	/** Volumen general del ambiente (lo multiplica la variable de consola TN.Ambience.Volume). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterVolume = 0.8f;

	/** Clase de sonido del ambiente y de sus sustituciones (para un control de volumen de «Ambiente»). Opcional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<USoundClass> SoundClassOverride;

	/** Cada cuánto se mira dónde está la cámara (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float UpdateInterval = 0.25f;

	/** Radio (cm) del anillo en el que se mezclan los biomas vecinos. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (ClampMin = "0.0"))
	float BlendRadius = 3500.f;

	/** Segundos que tarda una capa en llegar a su volumen nuevo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float LevelSmoothingSeconds = 1.5f;

	/** 0 = día, 1 = noche (para un ciclo de día y noche): menos aves y cigarras, más grillos y ranas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NightAmount = 0.f;

	/** Sin mapa procedural (lobby, mapa clásico): ambiente genérico suave. Si no, silencio. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	bool bPlayWithoutGenerator = true;

	/** Cuevas, torres y bajo el agua: trazas cortas hacia arriba que cierran el sonido (paso bajo y menos fauna). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	bool bDetectEnclosure = true;

private:
	/** En una máquina con audio y para el jugador local (dueño local del PlayerController o del pawn). */
	bool IsLocalViewer() const;
	APlayerController* ResolvePlayerController() const;
	bool GetViewLocation(FVector& OutLocation) const;

	void StartAudio();
	void StopAudio();
	void RebuildOverrides();
	void ClearOverrides();
	void UpdateMix(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UTN_AmbientSynthComponent> AmbientSynth;

	UPROPERTY(Transient)
	TArray<FTNAmbienceOverrideVoice> OverrideVoices;

	/** Por hueco de bioma (8 biomas y el genérico), un bit por capa con sonido de sustitución. */
	uint16 OverrideMask[9] = {};

	TWeakObjectPtr<ATN_ProcMapGenerator> Generator;
	TWeakObjectPtr<ATN_PathStorm> Storm;
	float LookupTimer = 0.f;

	// Último estado (depuración).
	FTNAmbientMix LastMix;
	float LastBiomeW[9] = {};
	float LastWaterNear = 0.f;
	float LastSeaNear = 0.f;
	float LastRiverNear = 0.f;
	float LastHeight = 0.f;
	float LastStorm = 0.f;
	float LastEnclosure = 0.f;
	bool bLastHasMap = false;
};
