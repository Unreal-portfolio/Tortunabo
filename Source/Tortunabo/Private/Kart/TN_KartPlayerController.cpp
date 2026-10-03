#include "Kart/TN_KartPlayerController.h"

#include "Engine/World.h"
#include "Kart/TN_KartGameMode.h"
#include "Kart/TN_KartGameState.h"

namespace TNKartPlayer
{
	/** Cada cuánto mira un cliente si ya tiene la pista y el suelo (s). */
	constexpr float ReadyCheckSeconds = 0.5f;
	/** Tope de la generación que acepta el servidor (una carrera no llega ni de lejos). */
	constexpr int32 MaxGeneration = 1000000;
}

void ATN_KartPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// El anfitrión usa la pista del servidor: solo avisan los clientes.
	if (HasAuthority() || !IsLocalController())
	{
		return;
	}
	ReadyCheckAccumulator += DeltaTime;
	if (ReadyCheckAccumulator < TNKartPlayer::ReadyCheckSeconds)
	{
		return;
	}
	ReadyCheckAccumulator = 0.f;
	const ATN_KartGameState* KartState = GetWorld() ? GetWorld()->GetGameState<ATN_KartGameState>() : nullptr;
	if (KartState && KartState->MapGeneration != ReportedGeneration && KartState->IsLocalTrackPlayable())
	{
		ReportedGeneration = KartState->MapGeneration;
		ServerReportKartTrackReady(ReportedGeneration);
	}
}

bool ATN_KartPlayerController::ServerReportKartTrackReady_Validate(int32 Generation)
{
	return Generation > 0 && Generation < TNKartPlayer::MaxGeneration;
}

void ATN_KartPlayerController::ServerReportKartTrackReady_Implementation(int32 Generation)
{
	if (ATN_KartGameMode* KartMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATN_KartGameMode>() : nullptr)
	{
		KartMode->NotifyClientTrackReady(this, Generation);
	}
}
