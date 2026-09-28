#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Scalability.h"
#include "Settings/TN_SettingsSaveGame.h"
#include "TN_GameSettingsSubsystem.generated.h"

class APlayerController;
class APlayerState;
class UAudioComponent;
class UCameraModifier;
class UInputComponent;
class UProximityVoiceComponent;
class USoundClass;
class USoundMix;
class UTN_FpsCounterWidget;
class UTN_PauseMenuWidget;

/** Grupos de ajustes que se pueden restablecer por separado (cada pestaña del menú de pausa). */
enum class ETNSettingsGroup : uint8
{
	Graphics,
	Sound,
	Voice,
	Controls,
	Game,
};

/**
 * @brief Ajustes del jugador y menú de pausa (para todos los modos: lobby, mapa procedural, carrera y solo terreno).
 *
 * Ajustes (FTNGameSettings, en la ranura TN_Settings; la parte gráfica, en UGameUserSettings): se cargan al crearse la
 * GameInstance y se aplican de verdad, sin tocar assets:
 *  - Volumen general: volumen principal del dispositivo de audio del mundo (SetTransientPrimaryVolume).
 *  - Música, ambiente y voz: tres USoundClass creadas en tiempo de ejecución. Cada fotograma se reparten los sonidos
 *    generados en código (los UAudioComponent cuyo sonido no es un asset): la música sintetizada
 *    (UTN_MusicSynthComponent) a Música, el paisaje sonoro (UTN_AmbientSynthComponent) a Ambiente y la voz de los
 *    compañeros (USoundWaveProcedural del grupo Voice) a Voz.
 *  - Efectos: todo lo demás (sintetizadores de pasos, trampas, enemigos, conchas... y los sonidos de asset) se queda en
 *    la clase de sonido por defecto del motor, que baja con una USoundMix propia (SetSoundMixClassOverride).
 *  - Voz de cada compañero y silenciar: multiplicador de volumen de su componente de reproducción.
 *  - Micrófono: umbral (SpeakingThreshold) y ganancia (VoiceGain) del UProximityVoiceComponent propio y su salida
 *    (SetTransmitEnabled) según silenciado y pulsar para hablar.
 *  - Sensibilidad e inversión de la cámara: escalas de giro del PlayerController (UInputSettings::bEnableLegacyInputScales
 *    está activo), con la sensibilidad del ratón o la del mando según el último dispositivo usado.
 *  - Campo de visión: CameraFOVDefault y CameraFOVSprint de la tortuga (su valor de clase + el desplazamiento).
 *  - Temblor de cámara: apaga los modificadores de cámara cuya clase se llama «...Shake...».
 *  - Brillo: gamma de salida del motor (GEngine->DisplayGamma); filtro para daltónicos: el de Slate.
 *
 * Menú de pausa: mete en la pila de entrada del PlayerController local (AMP_GamePlayerController, también de
 * espectador) un UInputComponent propio con Escape y Start del mando (y Tabulador en el editor, donde Escape corta la
 * partida) que abre y cierra UTN_PauseMenuWidget; así no hay que tocar el PlayerController. No pausa el mundo: el
 * juego es en red.
 *
 * En el editor, al acabar la partida se devuelven la calidad gráfica, el límite de fotogramas, la sincronización
 * vertical, la gamma y el filtro de color que tenía el editor.
 */
UCLASS()
class TORTUNABO_API UTN_GameSettingsSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** El subsistema de la GameInstance de WorldContext (null en servidor dedicado o sin GameInstance). */
	static UTN_GameSettingsSubsystem* Get(const UObject* WorldContext);

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

	// ── Ajustes ──────────────────────────────────────────────────────────────

	const FTNGameSettings& GetSettings() const { return Settings; }

	/** Cambia los ajustes y los aplica en el acto; se guardan al cerrar el menú (o a los pocos segundos). */
	void EditSettings(TFunctionRef<void(FTNGameSettings&)> Edit);

	/** Vuelve a los valores de serie de una pestaña (en la gráfica, solo el brillo y el contador de FPS). */
	void ResetGroup(ETNSettingsGroup Group);

	/** Guarda ya los ajustes pendientes (los propios y los de UGameUserSettings). */
	void SaveNow();

	/** La parte gráfica cambió en UGameUserSettings: la aplica (sin la resolución) y la guardará. */
	void ApplyGraphicsChange();

	/** Aplica la resolución y el modo de ventana de UGameUserSettings (fuera del editor); se confirma o se deshace. */
	void ApplyVideoMode();

	/** Confirma (bKeep) o deshace la última resolución o modo de ventana aplicados. */
	void FinishVideoModeChange(bool bKeep);

	/** true si en este proceso se puede cambiar la ventana (no en el editor, donde la ventana es la suya). */
	static bool CanChangeVideoMode();

	/** Umbral de voz (RMS, ya con la ganancia) para una sensibilidad 0..1: de -20 dB (0) a -60 dB (1). */
	static float SensitivityToThreshold(float Sensitivity);

	// ── Cámara (cualquier cámara del juego, también la del espectador) ────────

	/**
	 * Sensibilidad de la cámara (1 = la de serie) y eje Y invertido, para el ratón (bGamepad = false) o para el mando.
	 * OJO: lo que gira con AddControllerYawInput/AddControllerPitchInput (o APlayerController::AddYawInput/AddPitchInput)
	 * ya las lleva aplicadas, porque el subsistema pone las escalas de giro del PlayerController; úsalas solo en una
	 * cámara que gire a mano con el valor crudo de la acción (o con ApplyLookSettings), para no aplicarlas dos veces.
	 */
	float GetLookSensitivity(bool bGamepad) const;
	bool IsLookYInverted(bool bGamepad) const;

	/** true si el último aparato que ha tocado el jugador de PC es un mando (así se elige la sensibilidad). */
	static bool IsUsingGamepad(const APlayerController* PC);

	/**
	 * Valor crudo de mirar (X giro, Y cabeceo, el de IA_Look) con la sensibilidad y la inversión del aparato en uso de PC.
	 * Solo para cámaras que no pasan por AddYawInput/AddPitchInput (ver GetLookSensitivity).
	 */
	FVector2D ApplyLookSettings(const APlayerController* PC, const FVector2D& RawLook) const;

	/** Temblor de cámara permitido (el subsistema ya apaga los modificadores «...Shake...» del PlayerCameraManager). */
	bool IsCameraShakeEnabled() const { return Settings.bCameraShake; }

	/** Grados que el jugador suma al campo de visión (la tortuga ya lo lleva; otras cámaras lo pueden sumar al suyo). */
	float GetFieldOfViewOffset() const { return Settings.FieldOfViewOffset; }

	// ── Voz ──────────────────────────────────────────────────────────────────

	/** Clave estable de un jugador para sus ajustes de voz: su id de la plataforma o, si no hay, su nombre. */
	static FString PlayerKey(const APlayerState* PlayerState);

	float GetPlayerVoiceVolume(const FString& Key) const;
	void SetPlayerVoiceVolume(const FString& Key, float Volume);
	bool IsPlayerMuted(const FString& Key) const;
	void SetPlayerMuted(const FString& Key, bool bMuted);

	/** Componente de voz de la tortuga local (null si no hay tortuga o aún no tiene voz). */
	UProximityVoiceComponent* GetLocalVoice() const;

	/** Nivel del micrófono propio (RMS) y si hay micrófono capturando. */
	float GetMicLevel() const;
	bool IsMicCapturing() const;

	/** true si ahora mismo sale tu voz (no silenciado y, con pulsar para hablar, con la tecla pulsada). */
	bool IsTransmitAllowed() const { return bTransmitAllowed; }

	// ── Menú de pausa ────────────────────────────────────────────────────────

	/** Abre o cierra el menú de pausa del jugador local PC (no lo abre encima de otros menús o de la pantalla de carga). */
	void TogglePauseMenu(APlayerController* PC);
	void OpenPauseMenu(APlayerController* PC);
	void ClosePauseMenu();
	bool IsPauseMenuOpen() const;

	/** true si ahora se puede abrir el menú de pausa para PC. */
	bool CanOpenPauseMenu(const APlayerController* PC) const;

	/** Lo llama el menú al quitarse de la pantalla (también en un viaje): guarda lo pendiente. */
	void NotifyPauseMenuClosed(UTN_PauseMenuWidget* Menu);

private:
	FTNGameSettings Settings;

	/** Hay cambios sin guardar (propios o de UGameUserSettings) y desde cuándo (segundos de la aplicación). */
	bool bSettingsDirty = false;
	bool bGraphicsDirty = false;
	double DirtySince = 0.0;

	/** Resolución o modo de ventana aplicados y aún sin confirmar (no se guardan hasta confirmarlos). */
	bool bVideoModePending = false;

	/** Clases de sonido creadas en tiempo de ejecución y la mezcla de los efectos. */
	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MusicClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> AmbientClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> VoiceClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> EffectsMix;

	/** Mundo para el que se prepararon el dispositivo de audio (mezcla, volumen principal) y el último volumen puesto. */
	TWeakObjectPtr<UWorld> AudioWorld;
	float AppliedMasterVolume = -1.f;
	float AppliedEffectsVolume = -1.f;

	/** Entrada del menú de pausa, metida en la pila del PlayerController local. */
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> PauseInput;

	TWeakObjectPtr<APlayerController> PauseInputOwner;

	UPROPERTY(Transient)
	TObjectPtr<UTN_PauseMenuWidget> PauseMenu;

	UPROPERTY(Transient)
	TObjectPtr<UTN_FpsCounterWidget> FpsWidget;

	/** Modificadores de temblor apagados por el ajuste (para volver a encenderlos). */
	TArray<TWeakObjectPtr<UCameraModifier>> DisabledShakes;

	/** Voz propia: si sale (último cálculo) y la ganancia de serie del componente. */
	bool bTransmitAllowed = true;

	/** Gamma de salida del motor antes de tocar el brillo (y la de serie para el 0,5). */
	float BaseDisplayGamma = 2.2f;

	/** Filtro para daltónicos puesto ahora (para no llamar a Slate cada fotograma). */
	uint8 AppliedColorFilter = 0;
	float AppliedColorFilterStrength = -1.f;

	/** Editor: lo que había antes de la partida, para devolverlo al acabar. */
	bool bRestoreEditorState = false;
	Scalability::FQualityLevels EditorQualityLevels;
	float EditorMaxFPS = 0.f;
	int32 EditorVSync = 0;

	void LoadSettings();
	void MarkDirty(bool bGraphics);
	void CreateSoundClasses();

	/** Aplica lo que no depende del mundo (brillo, filtro de color) y lo de audio del mundo actual. */
	void ApplyGlobalSettings();
	void ApplyAudioVolumes(UWorld* World, bool bForce);

	void EnsurePauseInput(APlayerController* PC);
	void HandlePauseKey(FKey Key);

	void UpdateSounds(UWorld* World);
	void UpdateLocalVoice(APlayerController* PC);
	void UpdateCamera(APlayerController* PC);
	void UpdateFpsCounter(APlayerController* PC);

	/** Clase de sonido que toca a un componente (null: se queda en la de por defecto, efectos). */
	USoundClass* ClassFor(const UAudioComponent* Component) const;

	float BrightnessToGamma(float Brightness) const;
};
