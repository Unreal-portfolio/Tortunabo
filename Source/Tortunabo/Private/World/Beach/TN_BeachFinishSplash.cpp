// Chapuzón de la meta del modo carrera: chorro de agua y «¡chof!» sintetizado al entrar en el agua de meta, en cada
// máquina (la corona de gotas, la espuma y las ondas son del generador de la playa).

#include "World/Beach/TN_BeachFinishSplash.h"
#include "World/Beach/TN_BeachLayout.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/Beach/TN_BeachSplashSynthComponent.h"
#include "Core/TN_Log.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "../ProcMap/TN_ProcMapAmbientFX.h"

namespace TNBeachFinishSplashDetail
{
	/** Velocidad de caída (cm/s) que da un chapuzón de tamaño 1: la del salto del acantilado de meta. */
	constexpr float FullSplashFallSpeed = 1600.f;

	/** Cada cuánto se vuelve a buscar el generador de la playa mientras no hay (fuera de la playa, cada 2 s). */
	constexpr float GeneratorLookupSeconds = 2.f;

	FAutoConsoleCommandWithWorldAndArgs CmdSplash(TEXT("TN.Race.Splash"),
		TEXT("Chapuzón de la meta (chorro y sonido) delante de tu tortuga, solo en esta máquina: TN.Race.Splash [tamaño = 1]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UTN_BeachFinishSplashSubsystem* Splashes = World ? World->GetSubsystem<UTN_BeachFinishSplashSubsystem>() : nullptr;
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!Splashes || !Pawn)
			{
				UE_LOG(LogTortunabo, Display, TEXT("[Carrera] TN.Race.Splash: hace falta una tortuga local (y no en un servidor dedicado)."));
				return;
			}
			const float Size = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 1.f;
			Splashes->PlaySplash(Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 400.0, Size);
		}));
}

bool UTN_BeachFinishSplashSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UTN_BeachFinishSplashSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTN_BeachFinishSplashSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTN_BeachFinishSplashSubsystem, STATGROUP_Tickables);
}

void UTN_BeachFinishSplashSubsystem::Deinitialize()
{
	if (FXOwner)
	{
		TNAmbientFX::RemoveOwner(FXOwner);
	}
	FXOwner = nullptr;
	Synth = nullptr;
	Turtles.Reset();
	PendingJets.Reset();
	Super::Deinitialize();
}

void UTN_BeachFinishSplashSubsystem::Tick(float DeltaTime)
{
	using namespace TNBeachFinishSplashDetail;
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Clock += DeltaTime;

	// Solo hay agua de meta en la playa: fuera de ella el generador no existe y esto no hace nada.
	ATN_BeachRaceGenerator* BeachGenerator = Generator.Get();
	if (!BeachGenerator && Clock >= NextGeneratorLookup)
	{
		NextGeneratorLookup = Clock + GeneratorLookupSeconds;
		BeachGenerator = ATN_BeachRaceGenerator::Find(World);
		Generator = BeachGenerator;
	}
	if (BeachGenerator)
	{
		WatchTurtles(*BeachGenerator);
	}

	for (int32 Index = PendingJets.Num() - 1; Index >= 0; --Index)
	{
		if (Clock >= PendingJets[Index].At)
		{
			const FPendingJet Pending = PendingJets[Index];
			PendingJets.RemoveAtSwap(Index);
			LaunchJet(Pending);
		}
	}
	if (IsValid(FXOwner))
	{
		TNAmbientFX::TickOwner(FXOwner, DeltaTime);
	}
}

void UTN_BeachFinishSplashSubsystem::WatchTurtles(ATN_BeachRaceGenerator& InGenerator)
{
	using namespace TNBeachFinishSplashDetail;
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	// Lo mismo que mira el generador para su corona de gotas: los pies de cada tortuga al entrar en el agua de meta.
	const FTransform BeachTransform = InGenerator.GetActorTransform();
	for (APlayerState* PS : GS->PlayerArray)
	{
		ACharacter* Turtle = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
		if (!Turtle)
		{
			continue;
		}
		const UCapsuleComponent* Capsule = Turtle->GetCapsuleComponent();
		const double HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 50.0;
		const FVector Feet = Turtle->GetActorLocation() - FVector(0.0, 0.0, HalfHeight);
		const bool bWet = InGenerator.IsFinishWater(Feet);
		FTurtleWater& Water = Turtles.FindOrAdd(Turtle);
		if (bWet && !Water.bWet && !Turtle->IsHidden())
		{
			// El tamaño, por la velocidad de caída justo antes de tocar el agua (el agua ya la ha frenado este fotograma).
			const float FallSpeed = FMath::Max(0.f, -Water.VelocityZ);
			const float Size = FMath::Clamp(FallSpeed / FullSplashFallSpeed, 0.35f, 1.4f);
			const FVector Local = BeachTransform.InverseTransformPosition(Feet);
			PlaySplash(BeachTransform.TransformPosition(FVector(Local.X, Local.Y, TNBeachLayout::WaterZ)), Size);
		}
		Water.bWet = bWet;
		Water.VelocityZ = static_cast<float>(Turtle->GetVelocity().Z);
	}
	// Fuera las tortugas que ya no existen (cada ronda se crean nuevas).
	if (Turtles.Num() > 2 * GS->PlayerArray.Num() + 4)
	{
		for (auto It = Turtles.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
}

void UTN_BeachFinishSplashSubsystem::PlaySplash(const FVector& WorldLocation, float Size)
{
	if (!EnsureFX(WorldLocation))
	{
		return;
	}
	const float Scaled = FMath::Clamp(Size, 0.2f, 1.6f);
	if (Synth)
	{
		Synth->PlaySplashAt(WorldLocation, Scaled, FMath::FRandRange(0.94f, 1.06f));
	}
	// El chorro sale un instante después del golpe, cuando se cierra el hueco que abre la tortuga en el agua.
	FPendingJet& Pending = PendingJets.AddDefaulted_GetRef();
	Pending.At = Clock + 0.08f + 0.04f * Scaled;
	Pending.Location = WorldLocation;
	Pending.Size = Scaled;
}

void UTN_BeachFinishSplashSubsystem::LaunchJet(const FPendingJet& Pending)
{
	if (!IsValid(FXOwner))
	{
		return;
	}
	// Chorro de gotas hacia arriba (sube deprisa: se ve entero mientras la tortuga sigue en el agua) y una columna de
	// espuma blanca que se abre al subir.
	if (TNAmbientFX::FEmitter* Drops = TNAmbientFX::GetEmitter(FXOwner, JetDropsFX))
	{
		Drops->Origin = Pending.Location + FVector(0.0, 0.0, 20.0);
		Drops->Desc.Speed = 1350.f + 350.f * Pending.Size;
		TNAmbientFX::Burst(*Drops, FMath::RoundToInt(10.f + 24.f * Pending.Size));
	}
	if (TNAmbientFX::FEmitter* Mist = TNAmbientFX::GetEmitter(FXOwner, JetMistFX))
	{
		Mist->Origin = Pending.Location + FVector(0.0, 0.0, 30.0);
		Mist->Desc.Speed = 800.f + 350.f * Pending.Size;
		TNAmbientFX::Burst(*Mist, FMath::RoundToInt(5.f + 10.f * Pending.Size));
	}
}

bool UTN_BeachFinishSplashSubsystem::EnsureFX(const FVector& Near)
{
	if (IsValid(FXOwner))
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// Un actor local sin réplica: los efectos de TNAmbientFX van a nombre de un actor y el sonido cuelga de su raíz.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AActor* SplashActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Near), Params);
	if (!SplashActor)
	{
		return false;
	}
	USceneComponent* SplashRoot = NewObject<USceneComponent>(SplashActor, TEXT("SplashRoot"), RF_Transient);
	SplashActor->SetRootComponent(SplashRoot);
	SplashRoot->RegisterComponent();
	SplashActor->SetActorLocation(Near);
	FXOwner = SplashActor;
	Synth = UTN_BeachSplashSynthComponent::AttachTo(SplashActor);

	{
		// Gotas del chorro: casi rectas hacia arriba y con gravedad fuerte, para que suban y caigan en un segundo.
		TNAmbientFX::FEmitterDesc Drops;
		Drops.Shape = TNAmbientFX::EShape::Drop;
		Drops.Color = FLinearColor(0.82f, 0.94f, 1.f);
		Drops.MaxParticles = 90;
		Drops.Rate = 0.f;
		Drops.SpawnRadius = 45.f;
		Drops.SpawnHeight = 40.f;
		Drops.Direction = FVector::UpVector;
		Drops.Speed = 1700.f;
		Drops.SpeedJitter = 0.35f;
		Drops.Spread = 0.16f;
		Drops.Gravity = -1900.f;
		Drops.Drag = 0.05f;
		Drops.LifeMin = 1.1f;
		Drops.LifeMax = 1.7f;
		Drops.SizeStart = 60.f;
		Drops.SizeEnd = 30.f;
		Drops.WakeDistance = 60000.f;
		JetDropsFX = TNAmbientFX::AddEmitter(SplashActor, Drops, Near);
	}
	{
		// Columna de espuma: bocanadas blancas que suben con el chorro, se abren y se deshacen.
		TNAmbientFX::FEmitterDesc Mist;
		Mist.Shape = TNAmbientFX::EShape::Puff;
		Mist.bSoft = true;
		Mist.bCloud = true;
		Mist.Color = FLinearColor(0.93f, 0.97f, 1.f);
		Mist.Alpha = 0.75f;
		Mist.MaxParticles = 40;
		Mist.Rate = 0.f;
		Mist.SpawnRadius = 70.f;
		Mist.SpawnHeight = 60.f;
		Mist.Direction = FVector::UpVector;
		Mist.Speed = 1150.f;
		Mist.SpeedJitter = 0.4f;
		Mist.Spread = 0.22f;
		Mist.Gravity = -900.f;
		Mist.Drag = 1.2f;
		Mist.LifeMin = 0.55f;
		Mist.LifeMax = 1.f;
		Mist.SizeStart = 150.f;
		Mist.SizeEnd = 340.f;
		Mist.WakeDistance = 60000.f;
		JetMistFX = TNAmbientFX::AddEmitter(SplashActor, Mist, Near);
	}
	return true;
}
