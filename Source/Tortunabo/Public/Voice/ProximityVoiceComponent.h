#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "AudioCaptureCore.h"
#include "ProximityVoiceComponent.generated.h"

class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeakingChanged, bool, bIsSpeaking);

/**
 * @brief Componente de voz por proximidad (VOIP) basado en WASAPI.
 *
 * Captura audio local con FAudioCaptureSynth, lo downsampea y comprime,
 * lo envía al servidor (Server_SendVoiceData) y éste lo multicastea a los
 * clientes cercanos. La atenuación se aplica en el cliente receptor según
 * la distancia entre el emisor y el listener (InnerRadius / OuterRadius).
 *
 * @warning Para detener la captura antes de un ServerTravel hay que llamar a
 *          ShutdownAllCapture(World) ANTES del travel. Hacerlo desde EndPlay
 *          tras teardown causa ACCESS_VIOLATION en WASAPI.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TORTUNABO_API UProximityVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProximityVoiceComponent();

	/** @brief Inicia captura WASAPI si el componente es local-owned y crea el widget indicador. */
	virtual void BeginPlay() override;

	/** @brief Cierra captura y libera recursos de audio respetando el ciclo seguro de WASAPI. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** @brief Hook adicional para asegurar limpieza si el componente se desregistra antes de EndPlay. */
	virtual void OnUnregister() override;

	/** @brief Última red de seguridad: marca bIsShuttingDown antes de la destrucción del UObject. */
	virtual void BeginDestroy() override;

	/** @brief Bombea el buffer de captura, detecta speaking activity y comprime/envía paquetes al servidor. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * @brief Detiene la captura de audio de forma segura mientras WASAPI sigue vivo.
	 *        Debe llamarse ANTES de que ServerTravel inicie el teardown del mundo.
	 *        Después de esta llamada, EndPlay se convierte en un no-op para los recursos de audio.
	 */
	void PrepareForLevelTransition();

	/**
	 * @brief Encuentra todos los ProximityVoiceComponent del mundo y llama a PrepareForLevelTransition() en cada uno.
	 * @param World Mundo cuyos componentes se quieren detener.
	 * @note Llamar desde los GameModes justo ANTES de ServerTravel(). Ver @warning del UCLASS.
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
	 * @brief Reproduce datos de voz remotos recibidos en este componente.
	 * @param CompressedData Payload comprimido tal y como vino del servidor.
	 * @param SenderSampleRate SampleRate original del emisor (para resample si difiere).
	 * @note Llamado desde AMP_GamePlayerController::ClientReceiveVoice tras el filtro de distancia.
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
	/**
	 * @brief RPC al servidor con un paquete de voz comprimido para que lo retransmita.
	 * @param CompressedData Buffer comprimido.
	 * @param SenderSampleRate SampleRate del cliente emisor.
	 */
	UFUNCTION(Server, Unreliable)
	void Server_SendVoiceData(const TArray<uint8>& CompressedData, int32 SenderSampleRate);

	/**
	 * @brief Reenvía un paquete de voz a todos los clientes en multicast.
	 * @param CompressedData Buffer comprimido recibido del cliente emisor.
	 * @param SenderSampleRate SampleRate original del emisor.
	 */
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

	/** @brief Inicializa la SoundWaveProcedural y el AudioComponent de playback con el sample rate dado. */
	void SetupPlayback(int32 InSampleRate = 0);

	/**
	 * @brief Libera AudioCaptureSynth + buffers en orden seguro respetando WASAPI.
	 * @param bForceLeakAudio Si true, no destruye AudioComponent (evita crash si el subsistema ya cerró).
	 */
	void CleanupRuntimeResources(bool bForceLeakAudio = false);

	/** @brief Comprime un array de samples float a uint8 (codec ligero in-house para VOIP). */
	static TArray<uint8> CompressSamples(const TArray<float>& Samples);

	/** @brief Decodifica un array de uint8 vuelta a samples float. */
	static TArray<float> DecompressSamples(const TArray<uint8>& Compressed);

	/** @brief Devuelve true si el componente pertenece al jugador local (autoridad de input/audio). */
	bool IsLocallyOwned() const;

	UPROPERTY()
	TObjectPtr<UUserWidget> VoiceIndicatorWidgetInstance;

	bool bIsShuttingDown = false;
	bool bRuntimeResourcesCleanedUp = false;

	/** Rate limiting server-side para paquetes de voz (evita flooding). */
	float LastVoicePacketServerTime = -1.f;

	/** @brief Crea e inserta el widget VoiceIndicator en el HUD del jugador local. */
	void CreateVoiceIndicatorHUD();
};
