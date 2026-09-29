#include "World/Beach/TN_BeachCameraShake.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace TNBeachShakeTuning
{
	/** Lo que baja el golpe por segundo. */
	constexpr float TraumaDecay = 1.3f;
	/** Grados máximos de cabeceo, guiñada y alabeo con la intensidad a 1. */
	constexpr float MaxPitch = 2.6f;
	constexpr float MaxYaw = 1.8f;
	constexpr float MaxRoll = 3.2f;
	/** Desplazamiento máximo de la cámara (cm) con la intensidad a 1. */
	constexpr float MaxOffset = 22.f;
	/** Rapidez del ruido (ciclos por segundo, aprox.). */
	constexpr float NoiseSpeed = 22.f;
}

UTN_BeachCameraShake::UTN_BeachCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Después de los modificadores normales: tiembla la vista ya colocada.
	Priority = 200;
}

bool UTN_BeachCameraShake::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	Clock += DeltaTime;
	Trauma = FMath::Max(0.f, Trauma - DeltaTime * TNBeachShakeTuning::TraumaDecay);
	if (Clock - RumbleStamp > 0.15f)
	{
		RumbleTarget = 0.f;
	}
	RumbleShown = FMath::FInterpTo(RumbleShown, RumbleTarget, DeltaTime, RumbleTarget > RumbleShown ? 5.f : 8.f);
	const float Amount = FMath::Clamp(Trauma * Trauma + 0.8f * RumbleShown * RumbleShown, 0.f, 1.f) * Alpha;
	if (Amount <= 1e-4f)
	{
		return false;
	}
	const float T = Clock * TNBeachShakeTuning::NoiseSpeed;
	const FRotator Wobble(
		Amount * TNBeachShakeTuning::MaxPitch * FMath::PerlinNoise1D(T + 11.3f),
		Amount * TNBeachShakeTuning::MaxYaw * FMath::PerlinNoise1D(T + 37.1f),
		Amount * TNBeachShakeTuning::MaxRoll * FMath::PerlinNoise1D(T + 71.9f));
	const FRotationMatrix Axes(InOutPOV.Rotation);
	const FVector Offset = (Axes.GetScaledAxis(EAxis::Y) * FMath::PerlinNoise1D(T * 1.3f + 5.7f)
		+ Axes.GetScaledAxis(EAxis::Z) * FMath::PerlinNoise1D(T * 1.3f + 23.9f)) * (Amount * TNBeachShakeTuning::MaxOffset);
	InOutPOV.Rotation += Wobble;
	InOutPOV.Location += Offset;
	return false;
}

void UTN_BeachCameraShake::AddTrauma(float Amount)
{
	Trauma = FMath::Clamp(Trauma + FMath::Max(0.f, Amount), 0.f, 1.f);
}

void UTN_BeachCameraShake::SetRumble(float Level)
{
	// Varias fuentes en el mismo fotograma: manda la más fuerte.
	const bool bSameFrame = FMath::IsNearlyEqual(Clock, RumbleStamp);
	RumbleTarget = bSameFrame ? FMath::Max(RumbleTarget, Level) : Level;
	RumbleStamp = Clock;
}

UTN_BeachCameraShake* UTN_BeachCameraShake::GetFor(APlayerController* PC)
{
	APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Cam)
	{
		return nullptr;
	}
	if (UTN_BeachCameraShake* Found = Cast<UTN_BeachCameraShake>(Cam->FindCameraModifierByClass(UTN_BeachCameraShake::StaticClass())))
	{
		return Found;
	}
	return Cast<UTN_BeachCameraShake>(Cam->AddNewCameraModifier(UTN_BeachCameraShake::StaticClass()));
}

float UTN_BeachCameraShake::Falloff(const APlayerController* PC, const FVector& Where, float InnerRadius, float OuterRadius)
{
	FVector From = FVector::ZeroVector;
	if (const APawn* Pawn = PC->GetPawn())
	{
		From = Pawn->GetActorLocation();
	}
	else if (PC->PlayerCameraManager)
	{
		From = PC->PlayerCameraManager->GetCameraLocation();
	}
	else
	{
		return 0.f;
	}
	const float Dist = static_cast<float>(FVector::Dist(From, Where));
	if (Dist <= InnerRadius)
	{
		return 1.f;
	}
	const float Span = FMath::Max(1.f, OuterRadius - InnerRadius);
	return FMath::Clamp(1.f - (Dist - InnerRadius) / Span, 0.f, 1.f);
}

void UTN_BeachCameraShake::Kick(const UObject* WorldContext, const FVector& Where, float Strength, float InnerRadius, float OuterRadius)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || Strength <= 0.f)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController())
		{
			continue;
		}
		const float K = Falloff(PC, Where, InnerRadius, OuterRadius);
		if (K <= 0.f)
		{
			continue;
		}
		if (UTN_BeachCameraShake* Shake = GetFor(PC))
		{
			Shake->AddTrauma(Strength * K);
		}
	}
}

void UTN_BeachCameraShake::Rumble(const UObject* WorldContext, const FVector& Where, float Strength, float InnerRadius, float OuterRadius)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || Strength <= 0.f)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController())
		{
			continue;
		}
		const float K = Falloff(PC, Where, InnerRadius, OuterRadius);
		if (K <= 0.f)
		{
			continue;
		}
		if (UTN_BeachCameraShake* Shake = GetFor(PC))
		{
			Shake->SetRumble(FMath::Clamp(Strength * K, 0.f, 1.f));
		}
	}
}
