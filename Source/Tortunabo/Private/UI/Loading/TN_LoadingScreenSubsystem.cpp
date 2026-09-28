#include "UI/Loading/TN_LoadingScreenSubsystem.h"

#include "STN_EggLoadingScreen.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "MoviePlayer.h"
#include "Sound/SoundGenerator.h"
#include "UObject/UObjectGlobals.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"
#include <atomic>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Sonido del huevo (hilo de audio: C++ puro, sin UObjects ni asignaciones)
// ─────────────────────────────────────────────────────────────────────────────

namespace TNEggAudio
{
	struct FEggSharedParams
	{
		std::atomic<int32> CrackCount{ 0 };
		std::atomic<float> CrackStrength{ 0.7f };
		std::atomic<int32> PopCount{ 0 };
		std::atomic<float> Volume{ 0.85f };
	};

	constexpr float TwoPi = 6.2831853f;

	class FEggEngine
	{
	public:
		void Init(float InSampleRate)
		{
			SampleRate = FMath::Max(8000.f, InSampleRate);
			// Filtro de estado variable: paso banda alrededor de 2,8 kHz (la cáscara cruje aguda).
			SvfF = 2.f * FMath::Sin(PI * FMath::Min(2800.f, SampleRate * 0.2f) / SampleRate);
		}

		void Render(float* Out, int32 Frames, int32 Channels, FEggSharedParams& Params)
		{
			const int32 Crack = Params.CrackCount.load(std::memory_order_relaxed);
			if (Crack != SeenCrack)
			{
				SeenCrack = Crack;
				StartCrack(Params.CrackStrength.load(std::memory_order_relaxed));
			}
			const int32 Pop = Params.PopCount.load(std::memory_order_relaxed);
			if (Pop != SeenPop)
			{
				SeenPop = Pop;
				PopT = 0.f;
				PopPhase = 0.f;
				bPopActive = true;
			}
			const float Volume = Params.Volume.load(std::memory_order_relaxed);
			const float Dt = 1.f / SampleRate;

			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				float Sample = 0.f;
				if (CrackT >= 0.f)
				{
					// Ruido filtrado con un golpe inicial y tres chasquidos cortos detrás.
					const float N = Noise();
					Low += SvfF * Band;
					const float High = N - Low - 0.8f * Band;
					Band += SvfF * High;
					float Env = FMath::Exp(-CrackT / 0.018f);
					for (int32 k = 0; k < 3; ++k)
					{
						const float Tk = CrackT - ClickAt[k];
						if (Tk >= 0.f)
						{
							Env += ClickAmp[k] * FMath::Exp(-Tk / 0.006f);
						}
					}
					Sample += Band * Env * CrackAmp * 1.7f;
					CrackT += Dt;
					if (CrackT > 0.2f)
					{
						CrackT = -1.f;
					}
				}
				if (bPopActive)
				{
					// «¡Pum!»: golpe grave que cae de tono, soplido y chasquido de arranque.
					const float Freq = 52.f + 140.f * FMath::Exp(-PopT / 0.07f);
					PopPhase += TwoPi * Freq * Dt;
					if (PopPhase > TwoPi)
					{
						PopPhase -= TwoPi;
					}
					const float Attack = 1.f - FMath::Exp(-PopT / 0.0025f);
					const float Body = FMath::Sin(PopPhase) * Attack * FMath::Exp(-PopT / 0.24f);
					const float N = Noise();
					PopLow += (N - PopLow) * 0.12f;
					const float Puff = PopLow * FMath::Exp(-PopT / 0.05f) * 1.4f;
					const float Click = N * FMath::Exp(-PopT / 0.004f) * 0.5f;
					Sample += Body * 0.95f + Puff + Click;
					PopT += Dt;
					if (PopT > 1.f)
					{
						bPopActive = false;
					}
				}
				const float Y = static_cast<float>(std::tanh(Sample * 1.1f)) * Volume * 0.8f;
				for (int32 Ch = 0; Ch < Channels; ++Ch)
				{
					Out[Frame * Channels + Ch] = Y;
				}
			}
		}

	private:
		float Noise()
		{
			RandState ^= RandState << 13;
			RandState ^= RandState >> 17;
			RandState ^= RandState << 5;
			return static_cast<float>(RandState & 0xFFFFFF) / 8388607.5f - 1.f;
		}

		void StartCrack(float Strength)
		{
			CrackT = 0.f;
			CrackAmp = FMath::Clamp(Strength, 0.f, 1.f);
			const float R0 = 0.5f + 0.5f * Noise();
			const float R1 = 0.5f + 0.5f * Noise();
			const float R2 = 0.5f + 0.5f * Noise();
			ClickAt[0] = 0.012f + 0.01f * R0;
			ClickAt[1] = 0.03f + 0.02f * R1;
			ClickAt[2] = 0.055f + 0.02f * R2;
			ClickAmp[0] = 0.5f + 0.3f * R1;
			ClickAmp[1] = 0.35f + 0.25f * R2;
			ClickAmp[2] = 0.25f + 0.2f * R0;
		}

		float SampleRate = 48000.f;
		float SvfF = 0.3f;
		float Low = 0.f;
		float Band = 0.f;
		float CrackT = -1.f;
		float CrackAmp = 0.f;
		float ClickAt[3] = { 0.f, 0.f, 0.f };
		float ClickAmp[3] = { 0.f, 0.f, 0.f };
		bool bPopActive = false;
		float PopT = 0.f;
		float PopPhase = 0.f;
		float PopLow = 0.f;
		int32 SeenCrack = 0;
		int32 SeenPop = 0;
		uint32 RandState = 0x9E3779B9u;
	};

	class FTNEggSynthGenerator final : public ISoundGenerator
	{
	public:
		FTNEggSynthGenerator(float InSampleRate, int32 InNumChannels, const TSharedPtr<FEggSharedParams, ESPMode::ThreadSafe>& InParams)
			: Params(InParams)
			, OutChannels(FMath::Max(1, InNumChannels))
		{
			DspEngine.Init(InSampleRate);
		}

		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
		{
			const int32 Frames = NumSamples / OutChannels;
			DspEngine.Render(OutAudio, Frames, OutChannels, *Params);
			for (int32 i = Frames * OutChannels; i < NumSamples; ++i)
			{
				OutAudio[i] = 0.f;
			}
			return NumSamples;
		}

	private:
		TSharedPtr<FEggSharedParams, ESPMode::ThreadSafe> Params;
		int32 OutChannels = 1;
		FEggEngine DspEngine;
	};
}

UTN_EggSynthComponent::UTN_EggSynthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = false;
	NumChannels = 1;
	bAllowSpatialization = false;
	SharedParams = MakeShared<TNEggAudio::FEggSharedParams, ESPMode::ThreadSafe>();
}

void UTN_EggSynthComponent::PlayCrack(float Strength)
{
	if (SharedParams.IsValid())
	{
		SharedParams->CrackStrength.store(FMath::Clamp(Strength, 0.f, 1.f), std::memory_order_relaxed);
		SharedParams->CrackCount.fetch_add(1, std::memory_order_relaxed);
	}
}

void UTN_EggSynthComponent::PlayPop()
{
	if (SharedParams.IsValid())
	{
		SharedParams->PopCount.fetch_add(1, std::memory_order_relaxed);
	}
}

bool UTN_EggSynthComponent::Init(int32& /*SampleRate*/)
{
	NumChannels = 1;
	return true;
}

ISoundGeneratorPtr UTN_EggSynthComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	return MakeShared<TNEggAudio::FTNEggSynthGenerator, ESPMode::ThreadSafe>(InParams.SampleRate, InParams.NumChannels, SharedParams);
}

// ─────────────────────────────────────────────────────────────────────────────
// Consola
// ─────────────────────────────────────────────────────────────────────────────

namespace TNLoadingConsole
{
	UTN_LoadingScreenSubsystem* Find(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UTN_LoadingScreenSubsystem>() : nullptr;
	}

	void Test(UWorld* World)
	{
		if (UTN_LoadingScreenSubsystem* Loading = Find(World))
		{
			Loading->SetHold(false);
			Loading->BeginLoading(TEXT("Prueba de carga"));
			Loading->BreakAfter(3.f);
		}
	}

	void Hold(UWorld* World)
	{
		if (UTN_LoadingScreenSubsystem* Loading = Find(World))
		{
			Loading->SetHold(true);
			Loading->BeginLoading(TEXT("Prueba de carga (esperando a TN.Loading.Test.Break)"));
		}
	}

	void Break(UWorld* World)
	{
		if (UTN_LoadingScreenSubsystem* Loading = Find(World))
		{
			Loading->SetHold(false);
			Loading->BreakNow();
		}
	}

	FAutoConsoleCommandWithWorld TestCommand(TEXT("TN.Loading.Test"),
		TEXT("Enseña la pantalla de carga del huevo y la rompe a los 3 s."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Test));
	FAutoConsoleCommandWithWorld HoldCommand(TEXT("TN.Loading.Test.Hold"),
		TEXT("Enseña la pantalla de carga del huevo y la deja cargando hasta TN.Loading.Test.Break."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Hold));
	FAutoConsoleCommandWithWorld BreakCommand(TEXT("TN.Loading.Test.Break"),
		TEXT("Rompe el huevo de la pantalla de carga que esté a la vista."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Break));
}

// ─────────────────────────────────────────────────────────────────────────────
// Subsistema
// ─────────────────────────────────────────────────────────────────────────────

bool UTN_LoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UTN_LoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreLoadHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UTN_LoadingScreenSubsystem::HandlePreLoadMap);
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTN_LoadingScreenSubsystem::HandlePostLoadMap);
}

void UTN_LoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
	RemoveFromViewport();
	Screen.Reset();
	Super::Deinitialize();
}

ETickableTickType UTN_LoadingScreenSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UTN_LoadingScreenSubsystem::IsTickable() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance && GameInstance->GetGameViewportClient();
}

TStatId UTN_LoadingScreenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTN_LoadingScreenSubsystem, STATGROUP_Tickables);
}

UWorld* UTN_LoadingScreenSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

FString UTN_LoadingScreenSubsystem::FriendlyStatusForMap(const FString& MapName)
{
	if (MapName.Contains(TEXT("Menu")))
	{
		return TEXT("Volviendo al menú");
	}
	if (MapName.Contains(TEXT("HQ")) || MapName.Contains(TEXT("Lobby")))
	{
		return TEXT("Rumbo al cuartel");
	}
	if (MapName.Contains(TEXT("ProcMap")) || MapName.Contains(TEXT("LVL_Run")))
	{
		return TEXT("Incubando la partida");
	}
	return TEXT("Cargando");
}

void UTN_LoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}
	const UWorld* World = WorldContext.World();
	const bool bSeamless = World && World->IsInSeamlessTravel();
	const FString Status = FriendlyStatusForMap(MapName);
	LoadingMapName = MapName;
	if (!bSeamless)
	{
		bInHardLoad = true;
		HardLoadStartTime = FPlatformTime::Seconds();
	}
	// Carga bloqueante: el viewport no se va a repintar hasta que acabe, así que el huevo sale ya cerrado.
	BeginLoading(Status, !bSeamless);

	// Fuera del editor MoviePlayer pinta la misma escena en su hilo mientras dura el LoadMap.
	if (!bSeamless && !GIsEditor && IsMoviePlayerEnabled())
	{
		FLoadingScreenAttributes Attributes;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		Attributes.bMoviesAreSkippable = false;
		Attributes.bWaitForManualStop = false;
		Attributes.MinimumLoadingScreenDisplayTime = 0.f;
		Attributes.WidgetLoadingScreen = SNew(STN_EggLoadingScreen).StartClosed(true).Status(FText::FromString(Status));
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
	}
}

void UTN_LoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	bInHardLoad = false;
	if (Screen.IsValid())
	{
		// LoadMap puede haber vaciado el viewport: se vuelve a poner encima.
		RemoveFromViewport();
		AddToViewport();
	}
}

void UTN_LoadingScreenSubsystem::BeginLoading(const FString& InStatus, bool bStartClosed)
{
	if (IsRunningDedicatedServer() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	if (Screen.IsValid() && !Screen->IsBreaking())
	{
		if (!InStatus.IsEmpty())
		{
			Screen->SetStatus(FText::FromString(InStatus));
		}
		return;
	}
	bool bClosed = bStartClosed;
	if (Screen.IsValid())
	{
		// Se estaba rompiendo y empieza otra carga: huevo nuevo, ya cerrado.
		RemoveFromViewport();
		Screen.Reset();
		bClosed = true;
	}
	Screen = SNew(STN_EggLoadingScreen).StartClosed(bClosed).Status(FText::FromString(InStatus));
	ShownTime = FPlatformTime::Seconds();
	LoadDoneTime = -1.0;
	BreakAtTime = -1.0;
	FiredSounds = 0;
	AddToViewport();
}

void UTN_LoadingScreenSubsystem::SetStatus(const FString& InStatus)
{
	// Los puntos suspensivos los anima la pantalla.
	FString Clean = InStatus.TrimStartAndEnd();
	while (Clean.EndsWith(TEXT(".")) || Clean.EndsWith(TEXT("…")))
	{
		Clean.LeftChopInline(1);
	}
	if (Screen.IsValid() && !Clean.IsEmpty())
	{
		Screen->SetStatus(FText::FromString(Clean));
	}
}

void UTN_LoadingScreenSubsystem::BreakNow()
{
	if (Screen.IsValid() && !Screen->IsBreaking())
	{
		BreakAtTime = -1.0;
		FiredSounds = 0;
		Screen->StartBreak();
		EnsureSynth();
	}
}

void UTN_LoadingScreenSubsystem::BreakAfter(float Seconds)
{
	BreakAtTime = FPlatformTime::Seconds() + FMath::Max(0.f, Seconds);
}

void UTN_LoadingScreenSubsystem::AddToViewport()
{
	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport || !Screen.IsValid())
	{
		return;
	}
	if (ScreenInViewport.IsValid() && ViewportUsed.Get() == Viewport)
	{
		return;
	}
	RemoveFromViewport();
	// Por encima del HUD y por debajo de la pantalla de texto del GameInstance (que se aparta si el huevo está).
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), 20000);
	ScreenInViewport = Screen;
	ViewportUsed = Viewport;
}

void UTN_LoadingScreenSubsystem::RemoveFromViewport()
{
	if (ScreenInViewport.IsValid())
	{
		if (UGameViewportClient* Viewport = ViewportUsed.Get())
		{
			Viewport->RemoveViewportWidgetContent(ScreenInViewport.ToSharedRef());
		}
	}
	ScreenInViewport.Reset();
	ViewportUsed = nullptr;
}

void UTN_LoadingScreenSubsystem::Hide()
{
	RemoveFromViewport();
	Screen.Reset();
	bHold = false;
	BreakAtTime = -1.0;
	LoadDoneTime = -1.0;
}

bool UTN_LoadingScreenSubsystem::IsWorldReady(UWorld* World) const
{
	if (!World || !World->HasBegunPlay())
	{
		return false;
	}
	const double SinceLoad = FPlatformTime::Seconds() - LoadDoneTime;
	const FString MapName = World->GetMapName();
	// En el menú no hay tortuga que esperar.
	if (MapName.Contains(TEXT("Menu")))
	{
		return true;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* PC = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	if (!(PC && PC->GetPawn()) && SinceLoad < 6.0)
	{
		return false;
	}
	// Mapa procedural: el terreno tiene que estar generado en esta máquina.
	bool bFoundGenerator = false;
	for (TActorIterator<ATN_ProcMapGenerator> It(World); It; ++It)
	{
		bFoundGenerator = true;
		if (!It->IsMapReady())
		{
			return false;
		}
	}
	if (!bFoundGenerator && MapName.Contains(TEXT("ProcMap")) && SinceLoad < 12.0)
	{
		return false;
	}
	return true;
}

UTN_EggSynthComponent* UTN_LoadingScreenSubsystem::EnsureSynth()
{
	if (IsValid(Synth) && Synth->IsRegistered())
	{
		return Synth;
	}
	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* PC = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	if (!PC || !PC->GetWorld() || !FApp::CanEverRenderAudio())
	{
		return nullptr;
	}
	Synth = NewObject<UTN_EggSynthComponent>(PC, NAME_None, RF_Transient);
	if (USceneComponent* RootComp = PC->GetRootComponent())
	{
		Synth->SetupAttachment(RootComp);
	}
	Synth->RegisterComponent();
	PC->AddInstanceComponent(Synth);
	Synth->Start();
	return Synth;
}

void UTN_LoadingScreenSubsystem::TickBreakSounds()
{
	if (!Screen.IsValid())
	{
		return;
	}
	const float Elapsed = Screen->GetBreakElapsed();
	static const float CrackTimes[] = { 0.04f, 0.3f, 0.55f };
	const int32 NumCracks = UE_ARRAY_COUNT(CrackTimes);
	for (int32 i = 0; i < NumCracks; ++i)
	{
		const int32 Bit = 1 << i;
		if (Elapsed >= CrackTimes[i] && !(FiredSounds & Bit))
		{
			FiredSounds |= Bit;
			if (UTN_EggSynthComponent* EggSynth = EnsureSynth())
			{
				EggSynth->PlayCrack(0.6f + 0.2f * i);
			}
		}
	}
	if (Elapsed >= FTNEggTimeline::PopAt && !(FiredSounds & 8))
	{
		FiredSounds |= 8;
		if (UTN_EggSynthComponent* EggSynth = EnsureSynth())
		{
			EggSynth->PlayPop();
		}
	}
}

void UTN_LoadingScreenSubsystem::Tick(float DeltaTime)
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const double Now = FPlatformTime::Seconds();

	// Viaje sin cortes: sale aunque no haya pasado por PreLoadMap.
	if (!Screen.IsValid())
	{
		if (World && World->IsInSeamlessTravel())
		{
			const FString From = World->GetMapName();
			BeginLoading((From.Contains(TEXT("ProcMap")) || From.Contains(TEXT("LVL_Run"))) ? TEXT("Rumbo al cuartel") : TEXT("Incubando la partida"));
		}
		return;
	}

	AddToViewport();

	if (Screen->IsBreaking())
	{
		TickBreakSounds();
		if (Screen->IsBreakFinished())
		{
			Hide();
		}
		return;
	}

	// Un LoadMap que no avisó de su final (fallo de conexión, por ejemplo) no deja el huevo puesto para siempre.
	if (bInHardLoad && Now - HardLoadStartTime > 60.0)
	{
		bInHardLoad = false;
	}
	const bool bTravelling = bInHardLoad || !World || World->IsInSeamlessTravel();
	if (bTravelling)
	{
		LoadDoneTime = -1.0;
		return;
	}
	if (LoadDoneTime < 0.0)
	{
		LoadDoneTime = Now;
	}

	// Pruebas: rotura programada o espera manual.
	if (BreakAtTime >= 0.0)
	{
		if (Now >= BreakAtTime)
		{
			BreakNow();
		}
		return;
	}
	if (bHold)
	{
		return;
	}

	const bool bShownLongEnough = Now - ShownTime > 1.2;
	const bool bTimedOut = Now - LoadDoneTime > 30.0;
	if (bShownLongEnough && (bTimedOut || IsWorldReady(World)))
	{
		BreakNow();
	}
}
