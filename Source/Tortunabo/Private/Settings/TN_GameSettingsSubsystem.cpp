#include "Settings/TN_GameSettingsSubsystem.h"
#include "Core/TN_Log.h"
#include "Audio/TN_AmbientSoundscape.h"
#include "Audio/TN_AmbientSynthComponent.h"
#include "Audio/TN_MusicSynthComponent.h"
#include "Player/MP_GamePlayerController.h"
#include "Player/TortugaCharacter.h"
#include "UI/Loading/TN_LoadingScreenSubsystem.h"
#include "UI/Pause/TN_PauseMenuWidget.h"
#include "Voice/ProximityVoiceComponent.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraModifier.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/SynthComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Rendering/RenderingCommon.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/UObjectHash.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNGameSettingsDetail
{
	/** Ranura de guardado de los ajustes propios (Saved/SaveGames/TN_Settings.sav). */
	const TCHAR* const SlotName = TEXT("TN_Settings");
	constexpr int32 SlotUser = 0;

	/** Segundos sin cambios antes de guardar solos (con el menú abierto se guarda al cerrarlo). */
	constexpr double AutoSaveDelay = 3.0;

	/**
	 * Capas: el menú de pausa por encima del HUD (4-10), las ruedas (30-31), la tienda y el general (40) y las pantallas
	 * de la carrera (20-21); por debajo de la pantalla de carga (20000). El contador de FPS, encima de todo eso.
	 */
	constexpr int32 PauseMenuZOrder = 60;
	constexpr int32 FpsZOrder = 70;

	/** Prioridad de la entrada del menú en la pila del PlayerController (por delante de la tortuga y del propio PC). */
	constexpr int32 PauseInputPriority = 100;

	/** Volumen máximo de cada compañero (se puede subir a quien habla bajito). */
	constexpr float MaxPlayerVoice = 2.f;

	void ClampSettings(FTNGameSettings& S)
	{
		S.MasterVolume = FMath::Clamp(S.MasterVolume, 0.f, 1.f);
		S.MusicVolume = FMath::Clamp(S.MusicVolume, 0.f, 1.f);
		S.EffectsVolume = FMath::Clamp(S.EffectsVolume, 0.f, 1.f);
		S.AmbientVolume = FMath::Clamp(S.AmbientVolume, 0.f, 1.f);
		S.VoiceVolume = FMath::Clamp(S.VoiceVolume, 0.f, 1.f);
		for (TPair<FString, float>& Pair : S.PlayerVoiceVolumes) { Pair.Value = FMath::Clamp(Pair.Value, 0.f, MaxPlayerVoice); }
		S.MicSensitivity = FMath::Clamp(S.MicSensitivity, 0.f, 1.f);
		S.MicGain = FMath::Clamp(S.MicGain, 0.1f, 4.f);
		S.MouseSensitivity = FMath::Clamp(S.MouseSensitivity, 0.1f, 5.f);
		S.GamepadSensitivity = FMath::Clamp(S.GamepadSensitivity, 0.1f, 5.f);
		S.FieldOfViewOffset = FMath::Clamp(S.FieldOfViewOffset, -30.f, 30.f);
		S.ColorFilter = static_cast<uint8>(FMath::Clamp<int32>(S.ColorFilter, 0, 3));
		S.ColorFilterStrength = FMath::Clamp(S.ColorFilterStrength, 0.f, 1.f);
		S.Brightness = FMath::Clamp(S.Brightness, 0.f, 1.f);
		if (!FKey(S.PushToTalkKey).IsValid()) { S.PushToTalkKey = TEXT("V"); }
		if (!FKey(S.PushToTalkPadKey).IsValid()) { S.PushToTalkPadKey = TEXT("Gamepad_DPad_Down"); }
	}

	/** Un modificador de cámara que tiembla (el de serie del motor, UCameraModifier_CameraShake, o uno propio «...Shake...»). */
	bool IsShakeModifier(const UCameraModifier* Modifier)
	{
		return Modifier && Modifier->GetClass()->GetName().Contains(TEXT("Shake"));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Ciclo de vida
// ─────────────────────────────────────────────────────────────────────────────

UTN_GameSettingsSubsystem* UTN_GameSettingsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UTN_GameSettingsSubsystem>() : nullptr;
}

bool UTN_GameSettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UTN_GameSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BaseDisplayGamma = GEngine ? GEngine->DisplayGamma : 2.2f;
#if WITH_EDITOR
	// En el editor, la calidad, los FPS y la sincronización son los del editor: se guardan para devolverlos al acabar.
	if (GIsEditor)
	{
		bRestoreEditorState = true;
		EditorQualityLevels = Scalability::GetQualityLevels();
		if (const IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) { EditorMaxFPS = MaxFps->GetFloat(); }
		if (const IConsoleVariable* VSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))) { EditorVSync = VSync->GetInt(); }
	}
#endif
	LoadSettings();
	CreateSoundClasses();
	ApplyGlobalSettings();
	UE_LOG(LogTortunabo, Log, TEXT("[Ajustes] Cargados: general %.0f%%, música %.0f%%, efectos %.0f%%, ambiente %.0f%%, voz %.0f%%, %s."),
		Settings.MasterVolume * 100.f, Settings.MusicVolume * 100.f, Settings.EffectsVolume * 100.f, Settings.AmbientVolume * 100.f,
		Settings.VoiceVolume * 100.f, Settings.bPushToTalk ? TEXT("pulsar para hablar") : TEXT("voz abierta"));
}

void UTN_GameSettingsSubsystem::Deinitialize()
{
	if (bVideoModePending) { FinishVideoModeChange(false); }
	SaveNow();
	if (UTN_PauseMenuWidget* Menu = PauseMenu.Get())
	{
		PauseMenu = nullptr;
		Menu->RemoveFromParent();
	}
	if (FpsWidget)
	{
		FpsWidget->RemoveFromParent();
		FpsWidget = nullptr;
	}
	if (APlayerController* Owner = PauseInputOwner.Get())
	{
		if (PauseInput) { Owner->PopInputComponent(PauseInput); }
	}
	PauseInput = nullptr;
	PauseInputOwner.Reset();
	for (const TWeakObjectPtr<UCameraModifier>& Shake : DisabledShakes)
	{
		if (UCameraModifier* Modifier = Shake.Get()) { Modifier->EnableModifier(); }
	}
	DisabledShakes.Reset();

	// Lo que es de todo el proceso vuelve a como estaba (en el juego da igual, se cierra con él; en el editor, no).
	if (GEngine) { GEngine->DisplayGamma = BaseDisplayGamma; }
	if (AppliedColorFilter != 0)
	{
		UWidgetBlueprintLibrary::SetColorVisionDeficiencyType(EColorVisionDeficiency::NormalVision, 0.f, false, false);
		AppliedColorFilter = 0;
	}
#if WITH_EDITOR
	if (bRestoreEditorState)
	{
		Scalability::SetQualityLevels(EditorQualityLevels);
		if (IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) { MaxFPS->Set(EditorMaxFPS, ECVF_SetByGameSetting); }
		if (IConsoleVariable* VSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))) { VSync->Set(EditorVSync, ECVF_SetByGameSetting); }
		if (FAudioDevice* MainDevice = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr) { MainDevice->SetTransientPrimaryVolume(1.f); }
	}
#endif
	Super::Deinitialize();
}

ETickableTickType UTN_GameSettingsSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UTN_GameSettingsSubsystem::IsTickable() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance && GameInstance->GetGameViewportClient();
}

TStatId UTN_GameSettingsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTN_GameSettingsSubsystem, STATGROUP_Tickables);
}

UWorld* UTN_GameSettingsSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

void UTN_GameSettingsSubsystem::Tick(float DeltaTime)
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || World->bIsTearingDown)
	{
		return;
	}
	ApplyAudioVolumes(World, false);
	UpdateSounds(World);

	APlayerController* PC = GameInstance->GetFirstLocalPlayerController(World);
	// El paisaje sonoro del jugador (en su PlayerController): si no trae clase propia, la de Ambiente; así también sus
	// sonidos de sustitución (assets, AmbienceData) bajan con Ambiente en vez de con Efectos.
	if (UTN_AmbientSoundscapeComponent* Soundscape = PC ? PC->FindComponentByClass<UTN_AmbientSoundscapeComponent>() : nullptr)
	{
		if (!Soundscape->SoundClassOverride) { Soundscape->SoundClassOverride = AmbientClass; }
	}
	EnsurePauseInput(PC);
	UpdateLocalVoice(PC);
	UpdateCamera(PC);
	UpdateFpsCounter(PC);

	// Guardado diferido: con el menú abierto se espera a que se cierre.
	if ((bSettingsDirty || bGraphicsDirty) && !IsPauseMenuOpen() && FApp::GetCurrentTime() - DirtySince > TNGameSettingsDetail::AutoSaveDelay)
	{
		SaveNow();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Ajustes: cargar, guardar, cambiar
// ─────────────────────────────────────────────────────────────────────────────

void UTN_GameSettingsSubsystem::LoadSettings()
{
	using namespace TNGameSettingsDetail;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SlotUser))
	{
		if (const UTN_SettingsSaveGame* Saved = Cast<UTN_SettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, SlotUser)))
		{
			Settings = Saved->Settings;
		}
	}
	ClampSettings(Settings);
}

void UTN_GameSettingsSubsystem::SaveNow()
{
	using namespace TNGameSettingsDetail;
	if (bSettingsDirty)
	{
		if (UTN_SettingsSaveGame* Save = Cast<UTN_SettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(UTN_SettingsSaveGame::StaticClass())))
		{
			Save->Settings = Settings;
			UGameplayStatics::SaveGameToSlot(Save, SlotName, SlotUser);
		}
		bSettingsDirty = false;
	}
	// Una resolución sin confirmar no se guarda (si el juego se cerrase con ella, arrancaría otra vez así).
	if (bGraphicsDirty && !bVideoModePending)
	{
		if (UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings()) { GUS->SaveSettings(); }
		bGraphicsDirty = false;
	}
}

void UTN_GameSettingsSubsystem::MarkDirty(bool bGraphics)
{
	(bGraphics ? bGraphicsDirty : bSettingsDirty) = true;
	DirtySince = FApp::GetCurrentTime();
}

void UTN_GameSettingsSubsystem::EditSettings(TFunctionRef<void(FTNGameSettings&)> Edit)
{
	Edit(Settings);
	TNGameSettingsDetail::ClampSettings(Settings);
	MarkDirty(false);
	ApplyGlobalSettings();
}

void UTN_GameSettingsSubsystem::ResetGroup(ETNSettingsGroup Group)
{
	const FTNGameSettings Defaults;
	switch (Group)
	{
	case ETNSettingsGroup::Sound:
		Settings.MasterVolume = Defaults.MasterVolume;
		Settings.MusicVolume = Defaults.MusicVolume;
		Settings.EffectsVolume = Defaults.EffectsVolume;
		Settings.AmbientVolume = Defaults.AmbientVolume;
		break;
	case ETNSettingsGroup::Voice:
		Settings.VoiceVolume = Defaults.VoiceVolume;
		Settings.PlayerVoiceVolumes.Reset();
		Settings.MutedPlayers.Reset();
		Settings.bPushToTalk = Defaults.bPushToTalk;
		Settings.PushToTalkKey = Defaults.PushToTalkKey;
		Settings.PushToTalkPadKey = Defaults.PushToTalkPadKey;
		Settings.bMicMuted = Defaults.bMicMuted;
		Settings.MicSensitivity = Defaults.MicSensitivity;
		Settings.MicGain = Defaults.MicGain;
		break;
	case ETNSettingsGroup::Controls:
		Settings.MouseSensitivity = Defaults.MouseSensitivity;
		Settings.GamepadSensitivity = Defaults.GamepadSensitivity;
		Settings.bInvertMouseY = Defaults.bInvertMouseY;
		Settings.bInvertGamepadY = Defaults.bInvertGamepadY;
		break;
	case ETNSettingsGroup::Game:
		Settings.bCameraShake = Defaults.bCameraShake;
		Settings.FieldOfViewOffset = Defaults.FieldOfViewOffset;
		Settings.ColorFilter = Defaults.ColorFilter;
		Settings.ColorFilterStrength = Defaults.ColorFilterStrength;
		break;
	default:
		// Gráficos: el brillo y el contador; la calidad se elige con «Calidad recomendada» (UGameUserSettings).
		Settings.Brightness = Defaults.Brightness;
		Settings.bShowFps = Defaults.bShowFps;
		break;
	}
	MarkDirty(false);
	ApplyGlobalSettings();
}

void UTN_GameSettingsSubsystem::ApplyGraphicsChange()
{
	if (UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings())
	{
		GUS->ApplyNonResolutionSettings();
		MarkDirty(true);
	}
}

bool UTN_GameSettingsSubsystem::CanChangeVideoMode()
{
	// En el editor la ventana es la del editor (y la de PIE): la resolución y el modo solo se cambian en el juego.
	return !GIsEditor;
}

void UTN_GameSettingsSubsystem::ApplyVideoMode()
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	if (!GUS || !CanChangeVideoMode())
	{
		return;
	}
	GUS->ApplyResolutionSettings(false);
	bVideoModePending = true;
	MarkDirty(true);
}

void UTN_GameSettingsSubsystem::FinishVideoModeChange(bool bKeep)
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	if (!GUS || !bVideoModePending)
	{
		return;
	}
	bVideoModePending = false;
	if (bKeep)
	{
		GUS->ConfirmVideoMode();
		GUS->SaveSettings();
	}
	else
	{
		// Vuelve a la última resolución y modo confirmados.
		GUS->RevertVideoMode();
		GUS->ApplyResolutionSettings(false);
	}
}

float UTN_GameSettingsSubsystem::SensitivityToThreshold(float Sensitivity)
{
	const float Db = FMath::Lerp(-20.f, -60.f, FMath::Clamp(Sensitivity, 0.f, 1.f));
	return FMath::Pow(10.f, Db / 20.f);
}

float UTN_GameSettingsSubsystem::BrightnessToGamma(float Brightness) const
{
	// 0,5 es la gamma de serie del motor; cada extremo la mueve 0,7 (más gamma, imagen más clara).
	return FMath::Clamp(BaseDisplayGamma + (Brightness - 0.5f) * 1.4f, 1.2f, 3.6f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cámara (getters para cualquier cámara, también la del espectador)
// ─────────────────────────────────────────────────────────────────────────────

float UTN_GameSettingsSubsystem::GetLookSensitivity(bool bGamepad) const
{
	return bGamepad ? Settings.GamepadSensitivity : Settings.MouseSensitivity;
}

bool UTN_GameSettingsSubsystem::IsLookYInverted(bool bGamepad) const
{
	return bGamepad ? Settings.bInvertGamepadY : Settings.bInvertMouseY;
}

bool UTN_GameSettingsSubsystem::IsUsingGamepad(const APlayerController* PC)
{
	const UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get();
	if (!PC || !Devices)
	{
		return false;
	}
	return Devices->GetMostRecentlyUsedHardwareDevice(PC->GetPlatformUserId()).PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad;
}

FVector2D UTN_GameSettingsSubsystem::ApplyLookSettings(const APlayerController* PC, const FVector2D& RawLook) const
{
	const bool bPad = IsUsingGamepad(PC);
	const float Sensitivity = GetLookSensitivity(bPad);
	return FVector2D(RawLook.X * Sensitivity, RawLook.Y * Sensitivity * (IsLookYInverted(bPad) ? -1.f : 1.f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Sonido
// ─────────────────────────────────────────────────────────────────────────────

void UTN_GameSettingsSubsystem::CreateSoundClasses()
{
	auto MakeClass = [this](const TCHAR* Name)
	{
		// Raíces propias (sin padre): la mezcla de los efectos, que baja la clase por defecto del motor, no las toca.
		USoundClass* Class = NewObject<USoundClass>(this, FName(Name), RF_Transient);
		Class->Properties.Volume = 1.f;
		return Class;
	};
	MusicClass = MakeClass(TEXT("TN_Music"));
	MusicClass->Properties.bIsMusic = true;
	AmbientClass = MakeClass(TEXT("TN_Ambient"));
	VoiceClass = MakeClass(TEXT("TN_Voice"));
	EffectsMix = NewObject<USoundMix>(this, TEXT("TN_EffectsMix"), RF_Transient);
	// Las clases creadas en ejecución no se apuntan solas en los dispositivos de audio (solo las que se cargan).
	if (FAudioDeviceManager* Devices = GEngine ? GEngine->GetAudioDeviceManager() : nullptr)
	{
		Devices->RegisterSoundClass(MusicClass);
		Devices->RegisterSoundClass(AmbientClass);
		Devices->RegisterSoundClass(VoiceClass);
	}
}

void UTN_GameSettingsSubsystem::ApplyGlobalSettings()
{
	// Clases propias: el dispositivo lee su volumen en cada actualización.
	if (MusicClass) { MusicClass->Properties.Volume = Settings.MusicVolume; }
	if (AmbientClass) { AmbientClass->Properties.Volume = Settings.AmbientVolume; }
	if (VoiceClass) { VoiceClass->Properties.Volume = Settings.VoiceVolume; }
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		ApplyAudioVolumes(GameInstance->GetWorld(), false);
	}

	// Brillo: gamma de salida del motor (la que usa el tonemapper).
	if (GEngine) { GEngine->DisplayGamma = BrightnessToGamma(Settings.Brightness); }

	// Filtro para daltónicos (Slate lo aplica a toda la ventana, juego incluido). Sin filtro no se toca nada (en el editor
	// se respeta la vista previa de daltonismo que tenga puesta).
	const bool bFilterTypeChanged = AppliedColorFilter != Settings.ColorFilter;
	const bool bFilterStrengthChanged = Settings.ColorFilter != 0 && !FMath::IsNearlyEqual(AppliedColorFilterStrength, Settings.ColorFilterStrength);
	if (bFilterTypeChanged || bFilterStrengthChanged)
	{
		AppliedColorFilter = Settings.ColorFilter;
		AppliedColorFilterStrength = Settings.ColorFilterStrength;
		const EColorVisionDeficiency Type = static_cast<EColorVisionDeficiency>(Settings.ColorFilter);
		UWidgetBlueprintLibrary::SetColorVisionDeficiencyType(Type, Type == EColorVisionDeficiency::NormalVision ? 0.f : Settings.ColorFilterStrength,
			Type != EColorVisionDeficiency::NormalVision, false);
	}
}

void UTN_GameSettingsSubsystem::ApplyAudioVolumes(UWorld* World, bool bForce)
{
	if (!World || World->bIsTearingDown)
	{
		return;
	}
	const bool bNewWorld = AudioWorld.Get() != World;
	FAudioDevice* Device = World->GetAudioDeviceRaw();
	if (bNewWorld)
	{
		// Mundo nuevo (viaje, otra partida en el editor): las clases propias en su dispositivo y la mezcla de los efectos.
		if (FAudioDeviceManager* Devices = GEngine ? GEngine->GetAudioDeviceManager() : nullptr)
		{
			if (MusicClass) { Devices->RegisterSoundClass(MusicClass); }
			if (AmbientClass) { Devices->RegisterSoundClass(AmbientClass); }
			if (VoiceClass) { Devices->RegisterSoundClass(VoiceClass); }
		}
		if (EffectsMix) { UGameplayStatics::PushSoundMixModifier(World, EffectsMix); }
		AudioWorld = World;
	}
	if (Device && (bNewWorld || bForce || !FMath::IsNearlyEqual(AppliedMasterVolume, Settings.MasterVolume)))
	{
		Device->SetTransientPrimaryVolume(Settings.MasterVolume);
		AppliedMasterVolume = Settings.MasterVolume;
	}
	if (EffectsMix && (bNewWorld || bForce || !FMath::IsNearlyEqual(AppliedEffectsVolume, Settings.EffectsVolume)))
	{
		// Efectos: todo lo que se queda en la clase por defecto del motor (y sus hijas).
		if (USoundClass* DefaultClass = GetDefault<UAudioSettings>()->GetDefaultSoundClass())
		{
			UGameplayStatics::SetSoundMixClassOverride(World, EffectsMix, DefaultClass, Settings.EffectsVolume, 1.f, 0.05f, true);
		}
		AppliedEffectsVolume = Settings.EffectsVolume;
	}
}

USoundClass* UTN_GameSettingsSubsystem::ClassFor(const UAudioComponent* Component) const
{
	const UObject* Outer = Component ? Component->GetOuter() : nullptr;
	if (!Outer)
	{
		return nullptr;
	}
	// El UAudioComponent de un sintetizador es suyo (USynthComponent lo crea con él de Outer).
	if (Outer->IsA<UTN_MusicSynthComponent>())
	{
		return MusicClass;
	}
	if (Outer->IsA<UTN_AmbientSynthComponent>())
	{
		return AmbientClass;
	}
	// La voz de un compañero: onda procedural del grupo de voz (UProximityVoiceComponent::SetupPlayback).
	const USoundWaveProcedural* Wave = Cast<USoundWaveProcedural>(Component->Sound);
	if (Wave && Wave->SoundGroup == SOUNDGROUP_Voice && !Outer->IsA<USynthComponent>())
	{
		return VoiceClass;
	}
	return nullptr;
}

void UTN_GameSettingsSubsystem::UpdateSounds(UWorld* World)
{
	// Cada fotograma: los sonidos hechos en código (no los assets, que no se tocan) van a su clase, y la voz de cada
	// compañero, a su volumen. El dispositivo lee la clase del sonido en cada actualización, así que vale aunque ya suene.
	ForEachObjectOfClass(UAudioComponent::StaticClass(), [this, World](UObject* Object)
	{
		UAudioComponent* Component = static_cast<UAudioComponent*>(Object);
		if (!IsValid(Component) || Component->GetWorld() != World)
		{
			return;
		}
		USoundBase* Sound = Component->Sound;
		if (!Sound || Sound->IsAsset())
		{
			return;
		}
		USoundClass* Wanted = ClassFor(Component);
		if (!Wanted)
		{
			return;
		}
		if (Sound->SoundClassObject != Wanted)
		{
			Sound->SoundClassObject = Wanted;
		}
		if (Wanted == VoiceClass)
		{
			const APawn* Speaker = Cast<APawn>(Component->GetOwner());
			const UProximityVoiceComponent* Voice = Speaker ? Speaker->FindComponentByClass<UProximityVoiceComponent>() : nullptr;
			if (!Voice)
			{
				return;
			}
			const FString Key = PlayerKey(Speaker->GetPlayerState());
			const float Target = Voice->PlaybackVolume * (IsPlayerMuted(Key) ? 0.f : GetPlayerVoiceVolume(Key));
			if (!FMath::IsNearlyEqual(Component->VolumeMultiplier, Target, 0.001f))
			{
				Component->SetVolumeMultiplier(Target);
			}
		}
	}, true, RF_ClassDefaultObject | RF_ArchetypeObject);
}

// ─────────────────────────────────────────────────────────────────────────────
// Voz
// ─────────────────────────────────────────────────────────────────────────────

FString UTN_GameSettingsSubsystem::PlayerKey(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return FString();
	}
	const FUniqueNetIdRepl& Id = PlayerState->GetUniqueId();
	if (Id.IsValid())
	{
		return Id.ToString();
	}
	return FString(TEXT("Nombre:")) + PlayerState->GetPlayerName();
}

float UTN_GameSettingsSubsystem::GetPlayerVoiceVolume(const FString& Key) const
{
	const float* Found = Key.IsEmpty() ? nullptr : Settings.PlayerVoiceVolumes.Find(Key);
	return Found ? *Found : 1.f;
}

void UTN_GameSettingsSubsystem::SetPlayerVoiceVolume(const FString& Key, float Volume)
{
	if (Key.IsEmpty())
	{
		return;
	}
	EditSettings([&Key, Volume](FTNGameSettings& S)
	{
		if (FMath::IsNearlyEqual(Volume, 1.f)) { S.PlayerVoiceVolumes.Remove(Key); }
		else { S.PlayerVoiceVolumes.Add(Key, Volume); }
	});
}

bool UTN_GameSettingsSubsystem::IsPlayerMuted(const FString& Key) const
{
	return !Key.IsEmpty() && Settings.MutedPlayers.Contains(Key);
}

void UTN_GameSettingsSubsystem::SetPlayerMuted(const FString& Key, bool bMuted)
{
	if (Key.IsEmpty())
	{
		return;
	}
	EditSettings([&Key, bMuted](FTNGameSettings& S)
	{
		if (bMuted) { S.MutedPlayers.AddUnique(Key); }
		else { S.MutedPlayers.Remove(Key); }
	});
}

UProximityVoiceComponent* UTN_GameSettingsSubsystem::GetLocalVoice() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const APlayerController* PC = World ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UProximityVoiceComponent>() : nullptr;
}

float UTN_GameSettingsSubsystem::GetMicLevel() const
{
	const UProximityVoiceComponent* Voice = GetLocalVoice();
	return Voice ? Voice->GetMicLevel() : 0.f;
}

bool UTN_GameSettingsSubsystem::IsMicCapturing() const
{
	const UProximityVoiceComponent* Voice = GetLocalVoice();
	return Voice && Voice->IsCapturing();
}

void UTN_GameSettingsSubsystem::UpdateLocalVoice(APlayerController* PC)
{
	// ¿Sale la voz? Silenciado, no; con pulsar para hablar, solo con la tecla (o el botón del mando) pulsada.
	bool bTalkKeyDown = false;
	if (PC && Settings.bPushToTalk)
	{
		const FKey Key(Settings.PushToTalkKey);
		const FKey PadKey(Settings.PushToTalkPadKey);
		bTalkKeyDown = (Key.IsValid() && PC->IsInputKeyDown(Key)) || (PadKey.IsValid() && PC->IsInputKeyDown(PadKey));
	}
	bTransmitAllowed = !Settings.bMicMuted && (!Settings.bPushToTalk || bTalkKeyDown);

	UProximityVoiceComponent* Voice = GetLocalVoice();
	if (!Voice)
	{
		return;
	}
	Voice->SetTransmitEnabled(bTransmitAllowed);
	Voice->SpeakingThreshold = SensitivityToThreshold(Settings.MicSensitivity);
	Voice->VoiceGain = GetDefault<UProximityVoiceComponent>()->VoiceGain * Settings.MicGain;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cámara y contador de FPS
// ─────────────────────────────────────────────────────────────────────────────

void UTN_GameSettingsSubsystem::UpdateCamera(APlayerController* PC)
{
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// Sensibilidad e inversión: escalas de giro del PlayerController (UInputSettings::bEnableLegacyInputScales está
	// activo en DefaultInput.ini), con los valores del último aparato usado. Valen para la tortuga y para cualquier
	// cámara que gire con AddControllerYawInput/AddControllerPitchInput (el espectador incluido).
	const bool bPad = IsUsingGamepad(PC);
	const APlayerController* Defaults = PC->GetClass()->GetDefaultObject<APlayerController>();
	const float Sensitivity = GetLookSensitivity(bPad);
	PC->InputYawScale_DEPRECATED = Defaults->InputYawScale_DEPRECATED * Sensitivity;
	PC->InputPitchScale_DEPRECATED = Defaults->InputPitchScale_DEPRECATED * Sensitivity * (IsLookYInverted(bPad) ? -1.f : 1.f);

	// Campo de visión: el de la clase de la tortuga más el desplazamiento (en reposo y al correr).
	if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PC->GetPawn()))
	{
		const ATortugaCharacter* TurtleDefaults = Turtle->GetClass()->GetDefaultObject<ATortugaCharacter>();
		Turtle->SetCameraFOVs(FMath::Clamp(TurtleDefaults->GetCameraFOVDefault() + Settings.FieldOfViewOffset, 40.f, 120.f),
			FMath::Clamp(TurtleDefaults->GetCameraFOVSprint() + Settings.FieldOfViewOffset, 40.f, 120.f));
	}

	// Temblor de cámara: los modificadores que tiemblan, apagados (o encendidos otra vez los que se apagaron aquí).
	APlayerCameraManager* Camera = PC->PlayerCameraManager;
	if (!Camera)
	{
		return;
	}
	if (!Settings.bCameraShake)
	{
		Camera->ForEachCameraModifier([this](UCameraModifier* Modifier)
		{
			if (TNGameSettingsDetail::IsShakeModifier(Modifier) && !Modifier->IsDisabled())
			{
				Modifier->DisableModifier(true);
				DisabledShakes.AddUnique(TWeakObjectPtr<UCameraModifier>(Modifier));
			}
			return true;
		});
	}
	else if (DisabledShakes.Num() > 0)
	{
		for (const TWeakObjectPtr<UCameraModifier>& Shake : DisabledShakes)
		{
			if (UCameraModifier* Modifier = Shake.Get()) { Modifier->EnableModifier(); }
		}
		DisabledShakes.Reset();
	}
}

void UTN_GameSettingsSubsystem::UpdateFpsCounter(APlayerController* PC)
{
	if (!Settings.bShowFps)
	{
		if (FpsWidget && FpsWidget->IsInViewport()) { FpsWidget->RemoveFromParent(); }
		return;
	}
	if (!FpsWidget)
	{
		FpsWidget = CreateWidget<UTN_FpsCounterWidget>(GetGameInstance(), UTN_FpsCounterWidget::StaticClass());
	}
	// Tras un viaje el mundo quita todos los widgets: se vuelve a poner.
	if (FpsWidget && !FpsWidget->IsInViewport())
	{
		FpsWidget->AddToViewport(TNGameSettingsDetail::FpsZOrder);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Menú de pausa
// ─────────────────────────────────────────────────────────────────────────────

void UTN_GameSettingsSubsystem::EnsurePauseInput(APlayerController* PC)
{
	// Solo el PlayerController de la partida (no el del menú principal) y el local.
	AMP_GamePlayerController* GamePC = Cast<AMP_GamePlayerController>(PC);
	if (!GamePC || !GamePC->IsLocalController())
	{
		return;
	}
	if (PauseInput && PauseInputOwner.Get() == GamePC)
	{
		// Por si algo vació la pila del PlayerController.
		if (!GamePC->IsInputComponentInStack(PauseInput)) { GamePC->PushInputComponent(PauseInput); }
		return;
	}
	if (APlayerController* Old = PauseInputOwner.Get())
	{
		if (PauseInput) { Old->PopInputComponent(PauseInput); }
	}
	// Un UInputComponent propio en lo alto de la pila del PlayerController: sirve jugando y de espectador, sin tocar el
	// PlayerController. Los viajes sin cortes conservan el PlayerController y su pila; tras uno con corte hay otro y se
	// vuelve a meter.
	PauseInput = NewObject<UInputComponent>(GamePC, UInputComponent::StaticClass(), NAME_None, RF_Transient);
	PauseInput->Priority = TNGameSettingsDetail::PauseInputPriority;
	PauseInput->BindKey(EKeys::Escape, IE_Pressed, this, &UTN_GameSettingsSubsystem::HandlePauseKey);
	PauseInput->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &UTN_GameSettingsSubsystem::HandlePauseKey);
	// En el editor, Escape corta la partida (atajo del editor): ahí se abre con el Tabulador.
	if (GIsEditor)
	{
		PauseInput->BindKey(EKeys::Tab, IE_Pressed, this, &UTN_GameSettingsSubsystem::HandlePauseKey);
	}
	GamePC->PushInputComponent(PauseInput);
	PauseInputOwner = GamePC;
}

void UTN_GameSettingsSubsystem::HandlePauseKey(FKey Key)
{
	if (APlayerController* PC = PauseInputOwner.Get())
	{
		TogglePauseMenu(PC);
	}
}

bool UTN_GameSettingsSubsystem::CanOpenPauseMenu(const APlayerController* PC) const
{
	if (!PC || !PC->IsLocalController() || !PC->IsA<AMP_GamePlayerController>())
	{
		return false;
	}
	const UWorld* World = PC->GetWorld();
	if (!World || World->bIsTearingDown || World->IsInSeamlessTravel())
	{
		return false;
	}
	// Encima de la pantalla de carga (el huevo), no.
	if (const UTN_LoadingScreenSubsystem* Loading = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTN_LoadingScreenSubsystem>() : nullptr)
	{
		if (Loading->IsShowing())
		{
			return false;
		}
	}
	// Otro menú con el ratón a la vista (tienda, probador, general, ruedas, campeón de la carrera...): ese manda y se
	// cierra con su propio Escape.
	return !PC->ShouldShowMouseCursor();
}

void UTN_GameSettingsSubsystem::TogglePauseMenu(APlayerController* PC)
{
	if (IsPauseMenuOpen())
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu(PC);
	}
}

void UTN_GameSettingsSubsystem::OpenPauseMenu(APlayerController* PC)
{
	if (IsPauseMenuOpen() || !CanOpenPauseMenu(PC))
	{
		return;
	}
	UTN_PauseMenuWidget* Menu = CreateWidget<UTN_PauseMenuWidget>(PC, UTN_PauseMenuWidget::StaticClass());
	if (!Menu)
	{
		return;
	}
	PauseMenu = Menu;
	Menu->AddToViewport(TNGameSettingsDetail::PauseMenuZOrder);
	Menu->TakeInput();
	UE_LOG(LogTortunabo, Log, TEXT("[Pausa] Menú abierto (%s)."), *GetNameSafe(PC->GetWorld()));
}

void UTN_GameSettingsSubsystem::ClosePauseMenu()
{
	if (UTN_PauseMenuWidget* Menu = PauseMenu.Get())
	{
		PauseMenu = nullptr;
		// Al quitarse, el menú devuelve la entrada (NativeDestruct) y avisa (NotifyPauseMenuClosed).
		Menu->RemoveFromParent();
	}
}

bool UTN_GameSettingsSubsystem::IsPauseMenuOpen() const
{
	return PauseMenu && PauseMenu->IsInViewport();
}

void UTN_GameSettingsSubsystem::NotifyPauseMenuClosed(UTN_PauseMenuWidget* Menu)
{
	if (PauseMenu == Menu)
	{
		PauseMenu = nullptr;
	}
	// Si el menú se va con una resolución a medio confirmar (un viaje, por ejemplo), se deshace.
	if (bVideoModePending)
	{
		FinishVideoModeChange(false);
	}
	SaveNow();
}
