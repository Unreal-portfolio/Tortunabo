#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TN_SettingsSaveGame.generated.h"

/**
 * Ajustes del jugador que no guarda UGameUserSettings: sonido por categorías, voz de los compañeros, micrófono,
 * controles, juego y accesibilidad, más el brillo y el contador de FPS. Los aplica UTN_GameSettingsSubsystem y se
 * guardan en la ranura TN_Settings (Saved/SaveGames/TN_Settings.sav). La parte gráfica (resolución, ventana, calidad,
 * sincronización vertical, límite de fotogramas y escala de resolución) va en UGameUserSettings (GameUserSettings.ini).
 *
 * Cada volumen va de 0 a 1 (la voz de cada compañero, hasta 2). Los valores por defecto dejan el juego como estaba antes
 * del menú de pausa.
 */
USTRUCT()
struct FTNGameSettings
{
	GENERATED_BODY()

	// ── Sonido ───────────────────────────────────────────────────────────────

	/** Volumen general: todo lo que suena, voz incluida (volumen principal del dispositivo de audio). */
	UPROPERTY()
	float MasterVolume = 1.f;

	/** Música sintetizada (tienda, probador, victoria, derrota...). */
	UPROPERTY()
	float MusicVolume = 1.f;

	/** Efectos: pasos, trampas, enemigos, conchas, bailes, interfaz... (todo lo que no es música, ambiente ni voz). */
	UPROPERTY()
	float EffectsVolume = 1.f;

	/** Paisaje sonoro: olas, viento, selva, cascadas... */
	UPROPERTY()
	float AmbientVolume = 1.f;

	// ── Voz de los compañeros ────────────────────────────────────────────────

	/** Volumen de la voz de todos los compañeros. */
	UPROPERTY()
	float VoiceVolume = 1.f;

	/** Volumen de cada compañero (0..2), por su clave (UTN_GameSettingsSubsystem::PlayerKey). Sin entrada = 1. */
	UPROPERTY()
	TMap<FString, float> PlayerVoiceVolumes;

	/** Compañeros silenciados, por su clave. */
	UPROPERTY()
	TArray<FString> MutedPlayers;

	// ── Micrófono ────────────────────────────────────────────────────────────

	/** true: solo se habla con la tecla pulsada; false: voz abierta (se habla al superar el umbral). */
	UPROPERTY()
	bool bPushToTalk = false;

	/** Tecla (teclado o ratón) de pulsar para hablar. */
	UPROPERTY()
	FName PushToTalkKey = TEXT("V");

	/** Botón del mando de pulsar para hablar. */
	UPROPERTY()
	FName PushToTalkPadKey = TEXT("Gamepad_DPad_Down");

	/** Micrófono silenciado: no se envía nada. */
	UPROPERTY()
	bool bMicMuted = false;

	/** Sensibilidad (0..1): más alta, voces más bajas abren el micrófono. 0,5 = el umbral de siempre (-40 dB). */
	UPROPERTY()
	float MicSensitivity = 0.5f;

	/** Ganancia del micrófono (multiplica la del componente de voz; 1 = la de siempre). */
	UPROPERTY()
	float MicGain = 1.f;

	// ── Controles ────────────────────────────────────────────────────────────

	/** Sensibilidad de la cámara con el ratón (1 = la de siempre). */
	UPROPERTY()
	float MouseSensitivity = 1.f;

	/** Sensibilidad de la cámara con el mando (1 = la de siempre). */
	UPROPERTY()
	float GamepadSensitivity = 1.f;

	UPROPERTY()
	bool bInvertMouseY = false;

	UPROPERTY()
	bool bInvertGamepadY = false;

	// ── Juego y accesibilidad ────────────────────────────────────────────────

	/** Temblor de cámara (golpes, quads de la carrera, tormenta...). */
	UPROPERTY()
	bool bCameraShake = true;

	/** Grados que se suman al campo de visión de la tortuga (en reposo y al correr). */
	UPROPERTY()
	float FieldOfViewOffset = 0.f;

	/** Filtro para daltónicos (EColorVisionDeficiency: 0 ninguno, 1 deuteranopía, 2 protanopía, 3 tritanopía). */
	UPROPERTY()
	uint8 ColorFilter = 0;

	/** Intensidad del filtro para daltónicos (0..1). */
	UPROPERTY()
	float ColorFilterStrength = 1.f;

	// ── Pantalla (lo que no guarda UGameUserSettings) ────────────────────────

	/** Brillo (0..1; 0,5 = el de siempre): cambia la gamma de salida del motor. */
	UPROPERTY()
	float Brightness = 0.5f;

	/** Contador de fotogramas por segundo en la esquina de abajo a la derecha. */
	UPROPERTY()
	bool bShowFps = false;
};

/** Ranura de guardado de FTNGameSettings. */
UCLASS()
class TORTUNABO_API UTN_SettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Versión del formato, por si algún día hay que convertir ajustes viejos. */
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	FTNGameSettings Settings;
};
