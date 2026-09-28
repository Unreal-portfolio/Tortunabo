#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "TN_LoadingScreenSubsystem.generated.h"

class STN_EggLoadingScreen;
class SWidget;
class UGameViewportClient;
struct FWorldContext;

namespace TNEggAudio
{
	// Parámetros atómicos compartidos con el generador de audio (TN_LoadingScreenSubsystem.cpp).
	struct FEggSharedParams;
}

/**
 * Sonidos del huevo, sintetizados en tiempo real (sin archivos): crujidos de cáscara (ruido filtrado con chasquidos
 * cortos) y el «¡pum!» (golpe grave que cae de tono, con soplido y chasquido). 2D: suena en la interfaz.
 */
UCLASS(ClassGroup = (Audio))
class TORTUNABO_API UTN_EggSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_EggSynthComponent(const FObjectInitializer& ObjectInitializer);

	/** Un crujido (Strength 0-1). */
	void PlayCrack(float Strength);

	/** El «¡pum!» al reventar. */
	void PlayPop();

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	TSharedPtr<TNEggAudio::FEggSharedParams, ESPMode::ThreadSafe> SharedParams;
};

/**
 * Pantalla de carga del huevo (Tortunavy): en cada cambio de mapa las dos mitades de un huevo se cierran desde arriba
 * y desde abajo, cuatro tortugas (turquesa, coral, dorada y morada) caminan por la arena y, cuando el mapa está listo
 * (partida empezada, tortuga propia y, en el mapa procedural, terreno generado), el huevo tiembla, se agrieta y
 * revienta con crujidos y un «¡pum!» sintetizados, y la pantalla se funde dejando ver la partida.
 *
 * - Viaje sin cortes (seamless): el hilo de juego sigue vivo, así que la escena se anima todo el rato en el viewport.
 * - LoadMap bloqueante: fuera del editor, MoviePlayer pinta la misma escena en su hilo de carga mientras tanto; en
 *   PIE (sin MoviePlayer) el viewport se queda quieto durante la carga y sigue al terminar.
 * - UMP_GameInstance le pasa sus mensajes de estado cuando está a la vista (no se apilan dos pantallas).
 *
 * Pruebas por consola: TN.Loading.Test (se rompe a los 3 s), TN.Loading.Test.Hold (se queda cargando) y
 * TN.Loading.Test.Break (rompe la que esté a la vista).
 */
UCLASS()
class TORTUNABO_API UTN_LoadingScreenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	/** Enseña el huevo (si no está ya) con ese texto de estado. */
	void BeginLoading(const FString& InStatus, bool bStartClosed = false);

	/** Cambia el texto de estado (sin puntos suspensivos: se animan solos). */
	void SetStatus(const FString& InStatus);

	/** Rompe el huevo ya, esté o no listo el mapa. */
	void BreakNow();

	/** Mantiene el huevo cargando hasta BreakNow (pruebas). */
	void SetHold(bool bInHold) { bHold = bInHold; }

	/** Rompe solo pasados unos segundos, sin mirar el mapa (pruebas). */
	void BreakAfter(float Seconds);

	/** true mientras el huevo está en pantalla (cargando o rompiéndose). */
	bool IsShowing() const { return Screen.IsValid(); }

	/** Texto de estado amable para un mapa («Rumbo al cuartel», «Incubando la partida»...). */
	static FString FriendlyStatusForMap(const FString& MapName);

private:
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	void AddToViewport();
	void RemoveFromViewport();
	void Hide();
	bool IsWorldReady(UWorld* World) const;
	void TickBreakSounds();
	UTN_EggSynthComponent* EnsureSynth();

	TSharedPtr<STN_EggLoadingScreen> Screen;
	TSharedPtr<SWidget> ScreenInViewport;
	TWeakObjectPtr<UGameViewportClient> ViewportUsed;

	UPROPERTY(Transient)
	TObjectPtr<UTN_EggSynthComponent> Synth;

	FDelegateHandle PreLoadHandle;
	FDelegateHandle PostLoadHandle;

	/** Entre PreLoadMap y PostLoadMap (LoadMap bloqueante) y desde cuándo. */
	bool bInHardLoad = false;
	double HardLoadStartTime = 0.0;
	bool bHold = false;
	/** Momento (FPlatformTime) en que el mapa terminó de cargar; < 0 mientras sigue viajando. */
	double LoadDoneTime = -1.0;
	/** Momento en que salió en pantalla. */
	double ShownTime = 0.0;
	/** Rotura programada (pruebas); < 0 = ninguna. */
	double BreakAtTime = -1.0;
	/** Sonidos de la rotura ya disparados (bit por golpe). */
	int32 FiredSounds = 0;
	FString LoadingMapName;
};
