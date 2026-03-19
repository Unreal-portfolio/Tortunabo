#include "Voice/ProximityVoiceComponent.h"
#include "UI/Voice/VoiceIndicatorWidget.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"
#include "EngineUtils.h"

UProximityVoiceComponent::UProximityVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	// VoiceIndicatorWidgetClass is assigned via EditDefaultsOnly in the owning Blueprint
	// (e.g. BP_GamePlayerController or the Character BP that hosts this component).
}

void UProximityVoiceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UProximityVoiceComponent, bIsSpeaking);
	DOREPLIFETIME(UProximityVoiceComponent, VoiceSampleRate);
}

bool UProximityVoiceComponent::IsLocallyOwned() const
{
	if (const AActor* Owner = GetOwner())
	{
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			return Pawn->IsLocallyControlled();
		}
		return Owner->HasLocalNetOwner();
	}
	return false;
}

void UProximityVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	bIsShuttingDown = false;
	bRuntimeResourcesCleanedUp = false;

	if (IsLocallyOwned())
	{
		AudioCaptureSynth = MakeUnique<Audio::FAudioCaptureSynth>();
		if (AudioCaptureSynth->OpenDefaultStream())
		{
			AudioCaptureSynth->StartCapturing();
			Audio::FCaptureDeviceInfo DeviceInfo;
			if (AudioCaptureSynth->GetDefaultCaptureDeviceInfo(DeviceInfo))
			{
				VoiceSampleRate = DeviceInfo.PreferredSampleRate;
				CaptureNumChannels = FMath::Max(1, DeviceInfo.InputChannels);
			}
		}
		else
		{
			AudioCaptureSynth.Reset();
		}

	}
}

void UProximityVoiceComponent::PrepareForLevelTransition()
{
	if (bRuntimeResourcesCleanedUp || bIsShuttingDown)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Voice] PrepareForLevelTransition on %s — orphaning capture, cleaning playback/UI."),
		*GetNameSafe(GetOwner()));

	bIsShuttingDown = true;
	SetComponentTickEnabled(false);

	// bForceLeakAudio = false → playback component and UI are cleaned up properly
	// (world is still valid at this point). The capture synth is ALWAYS leaked regardless.
	CleanupRuntimeResources(/* bForceLeakAudio */ false);
}

void UProximityVoiceComponent::ShutdownAllCapture(const UWorld* World)
{
	if (!World)
	{
		return;
	}

	for (TObjectIterator<UProximityVoiceComponent> It; It; ++It)
	{
		UProximityVoiceComponent* Comp = *It;
		if (Comp && Comp->GetWorld() == World)
		{
			Comp->PrepareForLevelTransition();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Voice] ShutdownAllCapture: all voice components in world cleaned up before travel."));
}

void UProximityVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bIsShuttingDown = true;
	SetComponentTickEnabled(false);

	// If PrepareForLevelTransition already ran, bRuntimeResourcesCleanedUp is true
	// and CleanupRuntimeResources will early-out (no-op).
	// For EndPlay(Destroyed) during gameplay (e.g. player death), the capture synth
	// is still leaked (see comment in CleanupRuntimeResources), but playback/UI are
	// cleaned up normally.
	const bool bForceLeakPlayback = (EndPlayReason == EEndPlayReason::LevelTransition)
		|| IsEngineExitRequested();

	CleanupRuntimeResources(bForceLeakPlayback);
	Super::EndPlay(EndPlayReason);
}

void UProximityVoiceComponent::OnUnregister()
{
	bIsShuttingDown = true;
	SetComponentTickEnabled(false);
	CleanupRuntimeResources(IsEngineExitRequested());
	Super::OnUnregister();
}

void UProximityVoiceComponent::BeginDestroy()
{
	bIsShuttingDown = true;
	SetComponentTickEnabled(false);
	CleanupRuntimeResources(IsEngineExitRequested());
	Super::BeginDestroy();
}


void UProximityVoiceComponent::CleanupRuntimeResources(bool bForceLeakAudio)
{
	if (bRuntimeResourcesCleanedUp)
	{
		return;
	}

	bRuntimeResourcesCleanedUp = true;
	OnSpeakingChanged.Clear();

	// --- Audio Capture cleanup ---
	// ALWAYS orphan (Release) the FAudioCaptureSynth — NEVER call StopCapturing()
	// or Reset()/destructor. In UE 5.6, the WASAPI capture handle inside the synth
	// can become INVALID_HANDLE_VALUE at any time (the world's FAudioDevice may
	// invalidate it during teardown, or Windows may reclaim it). Both StopCapturing()
	// and the destructor attempt to use that handle and cause:
	//   - StopCapturing(): ACCESS_VIOLATION reading 0xFFFFFFFFFFFFFFFF
	//   - Destructor:      ACCESS_VIOLATION writing 0x0000000000000024
	//
	// Release() detaches our TUniquePtr without touching the synth internals.
	// The synth object leaks (~KB) — its capture callback continues harmlessly
	// writing to an internal buffer that nobody reads. The OS reclaims the memory
	// when the process exits.
	if (AudioCaptureSynth)
	{
		(void)AudioCaptureSynth.Release();
	}

	{
		FScopeLock Lock(&CaptureBufferLock);
		CaptureBuffer.Reset();
	}

	SendTimer = 0.f;
	bIsSpeaking = false;

	// --- Playback component cleanup ---
	// bForceLeakAudio controls whether we touch the playback component.
	// When called proactively (PrepareForLevelTransition, bForceLeakAudio=false),
	// the world is still valid and we can Stop/Deactivate/Destroy properly.
	// During EndPlay(LevelTransition) or engine exit, objects may already be
	// partially destroyed — skip interactive cleanup.
	if (UAudioComponent* AudioComponent = PlaybackAudioComponent.Get())
	{
		PlaybackAudioComponent = nullptr;

		const bool bSafeToTouch = !bForceLeakAudio && !AudioComponent->IsBeingDestroyed();

		if (bSafeToTouch)
		{
			AudioComponent->Stop();
			AudioComponent->Deactivate();
			AudioComponent->SetSound(nullptr);
		}

		if (bSafeToTouch && AudioComponent->IsRegistered())
		{
			AudioComponent->UnregisterComponent();
		}

		if (bSafeToTouch)
		{
			AudioComponent->DestroyComponent();
		}
	}

	ProceduralSoundWave = nullptr;

	if (!bForceLeakAudio && IsValid(VoiceIndicatorWidgetInstance))
	{
		VoiceIndicatorWidgetInstance->RemoveFromParent();
		VoiceIndicatorWidgetInstance = nullptr;
	}
	else
	{
		VoiceIndicatorWidgetInstance = nullptr;
	}
}

void UProximityVoiceComponent::CreateVoiceIndicatorHUD()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UClass* WidgetClass = VoiceIndicatorWidgetClass
		? VoiceIndicatorWidgetClass.Get()
		: UVoiceIndicatorWidget::StaticClass();

	VoiceIndicatorWidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (VoiceIndicatorWidgetInstance)
	{
		VoiceIndicatorWidgetInstance->AddToViewport(10);
	}
}

void UProximityVoiceComponent::SetupPlayback(int32 InSampleRate)
{
	if (bIsShuttingDown || (GetWorld() && GetWorld()->bIsTearingDown))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const int32 ActualSampleRate = (InSampleRate > 0) ? InSampleRate : VoiceSampleRate;

	ProceduralSoundWave = NewObject<USoundWaveProcedural>(this);
	if (!ProceduralSoundWave)
	{
		return;
	}

	ProceduralSoundWave->SetSampleRate(ActualSampleRate);
	ProceduralSoundWave->NumChannels = VoiceNumChannels;
	ProceduralSoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
	ProceduralSoundWave->SoundGroup = SOUNDGROUP_Voice;
	ProceduralSoundWave->bLooping = false;
	ProceduralSoundWave->bProcedural = true;
	ProceduralSoundWave->Volume = PlaybackVolume;

	PlaybackAudioComponent = NewObject<UAudioComponent>(Owner);
	if (!PlaybackAudioComponent)
	{
		return;
	}

	PlaybackAudioComponent->SetupAttachment(Owner->GetRootComponent());
	PlaybackAudioComponent->bAutoActivate = false;
	PlaybackAudioComponent->bAlwaysPlay = false; // Allow attenuation to cull distant voices

	// Configure spatial attenuation for proximity voice:
	// Full volume within InnerRadius (default 300cm = 3m),
	// fades to silence at OuterRadius (default 2500cm = 25m).
	PlaybackAudioComponent->bAllowSpatialization = true;
	PlaybackAudioComponent->bOverrideAttenuation = true;
	PlaybackAudioComponent->AttenuationOverrides.bAttenuate = true;
	PlaybackAudioComponent->AttenuationOverrides.bSpatialize = true;
	PlaybackAudioComponent->AttenuationOverrides.FalloffDistance = FMath::Max(OuterRadius - InnerRadius, 100.f);
	PlaybackAudioComponent->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	PlaybackAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(InnerRadius);
	PlaybackAudioComponent->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

	PlaybackAudioComponent->RegisterComponent();
	PlaybackAudioComponent->SetVolumeMultiplier(PlaybackVolume);
	PlaybackAudioComponent->SetSound(ProceduralSoundWave);
	PlaybackAudioComponent->Play();
}

void UProximityVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsShuttingDown || (GetWorld() && GetWorld()->bIsTearingDown) || !IsLocallyOwned() || !AudioCaptureSynth)
	{
		return;
	}

	TArray<float> NewAudioData;
	if (AudioCaptureSynth->GetAudioData(NewAudioData) && NewAudioData.Num() > 0)
	{
		TArray<float> MonoData;
		if (CaptureNumChannels > 1)
		{
			const int32 NumFrames = NewAudioData.Num() / CaptureNumChannels;
			MonoData.SetNumUninitialized(NumFrames);
			for (int32 Frame = 0; Frame < NumFrames; ++Frame)
			{
				float Sum = 0.f;
				for (int32 Ch = 0; Ch < CaptureNumChannels; ++Ch)
				{
					Sum += NewAudioData[Frame * CaptureNumChannels + Ch];
				}
				MonoData[Frame] = Sum / CaptureNumChannels;
			}
		}
		else
		{
			MonoData = MoveTemp(NewAudioData);
		}

		for (float& Sample : MonoData)
		{
			Sample = FMath::Clamp(Sample * VoiceGain, -1.0f, 1.0f);
		}

		FScopeLock Lock(&CaptureBufferLock);
		CaptureBuffer.Append(MonoData);
	}

	{
		FScopeLock Lock(&CaptureBufferLock);
		if (CaptureBuffer.Num() > 0)
		{
			float SumSquares = 0.f;
			for (const float Sample : CaptureBuffer)
			{
				SumSquares += Sample * Sample;
			}
			const float RMS = FMath::Sqrt(SumSquares / CaptureBuffer.Num());
			const bool bNewSpeaking = RMS > SpeakingThreshold;
			if (bNewSpeaking != bIsSpeaking)
			{
				bIsSpeaking = bNewSpeaking;
				OnSpeakingChanged.Broadcast(bIsSpeaking);
			}
		}
		else if (bIsSpeaking)
		{
			bIsSpeaking = false;
			OnSpeakingChanged.Broadcast(false);
		}
	}

	SendTimer += DeltaTime;
	if (SendTimer >= SendInterval)
	{
		SendTimer = 0.f;

		TArray<float> SamplesToSend;
		{
			FScopeLock Lock(&CaptureBufferLock);
			if (CaptureBuffer.Num() > 0)
			{
				SamplesToSend = MoveTemp(CaptureBuffer);
				CaptureBuffer.Reset();
			}
		}

		if (SamplesToSend.Num() > 0 && bIsSpeaking)
		{
			TArray<uint8> Compressed = CompressSamples(SamplesToSend);
			if (Compressed.Num() > 0)
			{
				Server_SendVoiceData(Compressed, VoiceSampleRate);
			}
		}
	}
}

void UProximityVoiceComponent::Server_SendVoiceData_Implementation(const TArray<uint8>& CompressedData, int32 SenderSampleRate)
{
	Multicast_ReceiveVoiceData(CompressedData, SenderSampleRate);
}

void UProximityVoiceComponent::Multicast_ReceiveVoiceData_Implementation(const TArray<uint8>& CompressedData, int32 SenderSampleRate)
{
	if (bIsShuttingDown || (GetWorld() && GetWorld()->bIsTearingDown) || IsLocallyOwned())
	{
		return;
	}

	if (SenderSampleRate <= 0)
	{
		SenderSampleRate = 48000;
	}

	if (!ProceduralSoundWave || !PlaybackAudioComponent)
	{
		SetupPlayback(SenderSampleRate);
	}

	if (!ProceduralSoundWave || !PlaybackAudioComponent)
	{
		return;
	}

	const TArray<float> Samples = DecompressSamples(CompressedData);
	if (Samples.Num() == 0)
	{
		return;
	}

	TArray<uint8> PCMData;
	PCMData.SetNumUninitialized(Samples.Num() * sizeof(int16));
	int16* OutPtr = reinterpret_cast<int16*>(PCMData.GetData());
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		OutPtr[i] = static_cast<int16>(FMath::Clamp(Samples[i], -1.f, 1.f) * 32767.f);
	}

	ProceduralSoundWave->QueueAudio(PCMData.GetData(), PCMData.Num());
	if (!PlaybackAudioComponent->IsPlaying())
	{
		PlaybackAudioComponent->Play();
	}
}

static const float MU_LAW_MU = 255.f;

TArray<uint8> UProximityVoiceComponent::CompressSamples(const TArray<float>& Samples)
{
	TArray<uint8> Compressed;
	Compressed.SetNumUninitialized(Samples.Num());

	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const float Sample = FMath::Clamp(Samples[i], -1.f, 1.f);
		const float Sign = Sample < 0.f ? -1.f : 1.f;
		const float Magnitude = FMath::Loge(1.f + MU_LAW_MU * FMath::Abs(Sample)) / FMath::Loge(1.f + MU_LAW_MU);
		const float Encoded = Sign * Magnitude;
		Compressed[i] = static_cast<uint8>(FMath::Clamp((Encoded + 1.f) * 0.5f * 255.f, 0.f, 255.f));
	}

	return Compressed;
}

TArray<float> UProximityVoiceComponent::DecompressSamples(const TArray<uint8>& Compressed)
{
	TArray<float> Samples;
	Samples.SetNumUninitialized(Compressed.Num());

	for (int32 i = 0; i < Compressed.Num(); ++i)
	{
		const float Encoded = (static_cast<float>(Compressed[i]) / 255.f) * 2.f - 1.f;
		const float Sign = Encoded < 0.f ? -1.f : 1.f;
		const float Decoded = Sign * (1.f / MU_LAW_MU) * (FMath::Pow(1.f + MU_LAW_MU, FMath::Abs(Encoded)) - 1.f);
		Samples[i] = FMath::Clamp(Decoded, -1.f, 1.f);
	}

	return Samples;
}

