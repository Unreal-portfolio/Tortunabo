#include "Audio/TN_RaceMusicComponent.h"
#include "TN_RaceMusicDSP.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Sound/SoundGenerator.h"

namespace
{
	/**
	 * Generador del hilo de render de audio: solo C++ puro (TNRaceMusic::FEngine) y los parámetros atómicos, que comparte
	 * con el componente por un puntero compartido: si el componente se destruye mientras suena, no queda nada colgando.
	 * Se crea con MakeShared (el motor lleva la sala y las voces: cientos de KB que no deben ir a la pila).
	 */
	class FTNRaceMusicGenerator final : public ISoundGenerator
	{
	public:
		FTNRaceMusicGenerator(float InSampleRate, int32 InNumChannels, const TSharedPtr<TNRaceMusic::FRaceMusicParams, ESPMode::ThreadSafe>& InParams)
			: Params(InParams)
			, OutChannels(FMath::Max(1, InNumChannels))
		{
			DspEngine.Init(InSampleRate);
		}

		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
		{
			// Estéreo intercalado (música 2D).
			const int32 Frames = NumSamples / OutChannels;
			DspEngine.Render(OutAudio, Frames, OutChannels, *Params);
			for (int32 i = Frames * OutChannels; i < NumSamples; ++i)
			{
				OutAudio[i] = 0.f;
			}
			return NumSamples;
		}

	private:
		TSharedPtr<TNRaceMusic::FRaceMusicParams, ESPMode::ThreadSafe> Params;
		int32 OutChannels = 2;
		TNRaceMusic::FEngine DspEngine;
	};
}

// ─────────────────────────────────────────────────────────────────────────────
// UTN_RaceMusicComponent
// ─────────────────────────────────────────────────────────────────────────────

UTN_RaceMusicComponent::UTN_RaceMusicComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 2D y sin tick (el de la base solo sirve a las fuentes 3D). Empieza sin autoactivarse: lo arranca AttachRaceMusic.
	bSpatial = false;
	NumChannels = 2;
	RaceParams = MakeShared<TNRaceMusic::FRaceMusicParams, ESPMode::ThreadSafe>();
}

ISoundGeneratorPtr UTN_RaceMusicComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	return MakeShared<FTNRaceMusicGenerator, ESPMode::ThreadSafe>(InParams.SampleRate, InParams.NumChannels, RaceParams);
}

UTN_RaceMusicComponent* UTN_RaceMusicComponent::AttachRaceMusic(AActor* InOwner)
{
	if (!InOwner) { return nullptr; }
	const UWorld* OwnerWorld = InOwner->GetWorld();
	if (!OwnerWorld || !OwnerWorld->IsGameWorld() || OwnerWorld->GetNetMode() == NM_DedicatedServer || !FApp::CanEverRenderAudio())
	{
		return nullptr;
	}

	UTN_RaceMusicComponent* Comp = NewObject<UTN_RaceMusicComponent>(InOwner, NAME_None, RF_Transient);
	if (USceneComponent* RootComp = InOwner->GetRootComponent())
	{
		Comp->SetupAttachment(RootComp);
	}
	Comp->RegisterComponent();
	InOwner->AddInstanceComponent(Comp);
	if (!Comp->IsPlaying())
	{
		Comp->Start();
	}
	return Comp;
}

void UTN_RaceMusicComponent::SetRacePlaying(bool bInPlay, float InFadeSeconds)
{
	if (!RaceParams.IsValid()) { return; }
	// El fundido primero: el hilo de audio lo lee en el mismo bloque en que ve el cambio de bPlay.
	TNRaceMusic::FRaceMusicParams::Set(RaceParams->FadeSeconds, FMath::Max(0.05f, InFadeSeconds));
	RaceParams->bPlay.store(bInPlay, std::memory_order_release);
	// Tras un viaje sin cortes el componente llega registrado pero parado (el motor lo para al cambiar de nivel).
	if (bInPlay && IsRegistered() && !IsPlaying())
	{
		Start();
	}
}

void UTN_RaceMusicComponent::SetRaceTension(float InTension)
{
	if (RaceParams.IsValid()) { TNRaceMusic::FRaceMusicParams::Set(RaceParams->Tension, FMath::Clamp(InTension, 0.f, 1.f)); }
}

void UTN_RaceMusicComponent::SetRaceDuck(float InDuck)
{
	if (RaceParams.IsValid()) { TNRaceMusic::FRaceMusicParams::Set(RaceParams->Duck, FMath::Clamp(InDuck, 0.f, 1.f)); }
}

void UTN_RaceMusicComponent::SetRaceVolume(float InVolume)
{
	if (RaceParams.IsValid()) { TNRaceMusic::FRaceMusicParams::Set(RaceParams->Volume, FMath::Clamp(InVolume, 0.f, 1.5f)); }
}

void UTN_RaceMusicComponent::SetRaceLayerMask(int32 InMask)
{
	if (RaceParams.IsValid())
	{
		RaceParams->LayerMask.store(static_cast<uint32>(InMask) & TNRaceMusic::ELayer::All, std::memory_order_relaxed);
	}
}

void UTN_RaceMusicComponent::RestartRacePiece()
{
	if (RaceParams.IsValid()) { RaceParams->RestartSerial.fetch_add(1u, std::memory_order_relaxed); }
}

FTNRaceMusicDebugInfo UTN_RaceMusicComponent::GetRaceDebugInfo() const
{
	FTNRaceMusicDebugInfo Info;
	if (RaceParams.IsValid())
	{
		Info.bRunning = RaceParams->bDebugRunning.load(std::memory_order_relaxed);
		Info.Bar = RaceParams->DebugBar.load(std::memory_order_relaxed);
		Info.Pass = RaceParams->DebugPass.load(std::memory_order_relaxed);
		Info.Tension = TNRaceMusic::FRaceMusicParams::Get(RaceParams->DebugTension);
		Info.Duck = TNRaceMusic::FRaceMusicParams::Get(RaceParams->DebugDuck);
	}
	return Info;
}
