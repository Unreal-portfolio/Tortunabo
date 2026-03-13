#include "Voice/ProximityVoiceComponent.h"
#include "UI/Voice/VoiceIndicatorWidget.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"

UProximityVoiceComponent::UProximityVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	// Optional project widget path; if not present, the component only logs a warning.
	VoiceIndicatorWidgetClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Game/UI/WBP_VoiceIndicator.WBP_VoiceIndicator_C")));
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

		CreateVoiceIndicatorHUD();
	}
}

void UProximityVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AudioCaptureSynth)
	{
		AudioCaptureSynth->StopCapturing();
		AudioCaptureSynth.Reset();
	}

	if (PlaybackAudioComponent)
	{
		PlaybackAudioComponent->Stop();
		PlaybackAudioComponent->DestroyComponent();
		PlaybackAudioComponent = nullptr;
	}

	if (VoiceIndicatorWidgetInstance)
	{
		VoiceIndicatorWidgetInstance->RemoveFromParent();
		VoiceIndicatorWidgetInstance = nullptr;
	}

	Super::EndPlay(EndPlayReason);
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

	UClass* WidgetClass = nullptr;
	if (!VoiceIndicatorWidgetClass.IsNull())
	{
		WidgetClass = VoiceIndicatorWidgetClass.LoadSynchronous();
	}

	if (!WidgetClass)
	{
		WidgetClass = UVoiceIndicatorWidget::StaticClass();
	}

	VoiceIndicatorWidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (VoiceIndicatorWidgetInstance)
	{
		VoiceIndicatorWidgetInstance->AddToViewport(10);
	}
}

void UProximityVoiceComponent::SetupPlayback(int32 InSampleRate)
{
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

	PlaybackAudioComponent->RegisterComponent();
	PlaybackAudioComponent->SetupAttachment(Owner->GetRootComponent());
	PlaybackAudioComponent->bAutoActivate = false;
	PlaybackAudioComponent->bAlwaysPlay = true;
	PlaybackAudioComponent->SetVolumeMultiplier(PlaybackVolume);
	PlaybackAudioComponent->SetSound(ProceduralSoundWave);
	PlaybackAudioComponent->Play();
}

void UProximityVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsLocallyOwned() || !AudioCaptureSynth)
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
	if (IsLocallyOwned())
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

