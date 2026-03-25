#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "AudioCaptureCore.h"
#include "ProximityVoiceComponent.generated.h"

class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeakingChanged, bool, bIsSpeaking);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TORTUNABO_API UProximityVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProximityVoiceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;
	virtual void BeginDestroy() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Cleanly stop audio capture while the audio subsystem (WASAPI) is still alive.
	 * Must be called BEFORE ServerTravel initiates world teardown.
	 * After this call, EndPlay becomes a safe no-op for audio resources.
	 */
	void PrepareForLevelTransition();

	/**
	 * Find every ProximityVoiceComponent in the given world and call
	 * PrepareForLevelTransition() on each one. Call this from GameModes
	 * right before ServerTravel().
	 */
	static void ShutdownAllCapture(const UWorld* World);

	UPROPERTY(BlueprintAssignable, Category = "Voice")
	FOnSpeakingChanged OnSpeakingChanged;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Voice")
	bool bIsSpeaking = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Attenuation")
	float InnerRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Attenuation")
	float OuterRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Detection")
	float SpeakingThreshold = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Voice|Audio")
	int32 VoiceSampleRate = 48000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Audio")
	int32 VoiceNumChannels = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Network")
	float SendInterval = 0.08f;

	/**
	 * Factor de downsampling antes de comprimir y enviar.
	 * 2 = 48kHz → 24kHz (reduce paquete a 1/2; box filter evita aliasing).
	 * 1 = sin downsampling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Network", meta = (ClampMin = "1", ClampMax = "6"))
	int32 VoiceDownsampleFactor = 2;

	/**
	 * Reproduce datos de voz remotos en este componente.
	 * Llamado desde AMP_GamePlayerController::ClientReceiveVoice tras el filtro de distancia.
	 */
	void PlayRemoteVoice(const TArray<uint8>& CompressedData, int32 SenderSampleRate);

	/**
	 * Segundos de silencio continuo antes de marcar bIsSpeaking = false.
	 * Evita que el flag rebote rápidamente generando tráfico de replicación.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Detection", meta = (ClampMin = "0.0"))
	float SilenceHoldOffSeconds = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Audio")
	float VoiceGain = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Audio")
	float PlaybackVolume = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|UI")
	TSubclassOf<UUserWidget> VoiceIndicatorWidgetClass;

protected:
	UFUNCTION(Server, Unreliable)
	void Server_SendVoiceData(const TArray<uint8>& CompressedData, int32 SenderSampleRate);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ReceiveVoiceData(const TArray<uint8>& CompressedData, int32 SenderSampleRate);

private:
	TUniquePtr<Audio::FAudioCaptureSynth> AudioCaptureSynth;
	TArray<float> CaptureBuffer;
	FCriticalSection CaptureBufferLock;
	float SendTimer = 0.f;
	float SilenceHoldOffTimer = 0.f;
	int32 CaptureNumChannels = 1;

	UPROPERTY()
	TObjectPtr<UAudioComponent> PlaybackAudioComponent;

	UPROPERTY()
	TObjectPtr<USoundWaveProcedural> ProceduralSoundWave;

	void SetupPlayback(int32 InSampleRate = 0);
	void CleanupRuntimeResources(bool bForceLeakAudio = false);

	static TArray<uint8> CompressSamples(const TArray<float>& Samples);
	static TArray<float> DecompressSamples(const TArray<uint8>& Compressed);

	bool IsLocallyOwned() const;

	UPROPERTY()
	TObjectPtr<UUserWidget> VoiceIndicatorWidgetInstance;

	bool bIsShuttingDown = false;
	bool bRuntimeResourcesCleanedUp = false;

	void CreateVoiceIndicatorHUD();
};

