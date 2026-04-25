#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"

ATN_CoopPlayerState::ATN_CoopPlayerState()
{
	// Frecuencia de replicación alta para que cambios de estado (helmet, alive, DBNO)
	// lleguen rápido a todos los clientes. Default de APlayerState es ~1-2 Hz.
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(15.f);
}

bool ATN_CoopPlayerState::CanServerSendQuickChat(float Now, float CooldownSeconds) const
{
	return (Now - ServerLastQuickChatTime) >= CooldownSeconds;
}

void ATN_CoopPlayerState::MarkServerQuickChatSent(float Now)
{
	ServerLastQuickChatTime = Now;
}

bool ATN_CoopPlayerState::CanServerPlayEmote(uint8 EmoteID, float Now, float CooldownSeconds) const
{
	const float* LastUse = ServerLastEmoteTimes.Find(EmoteID);
	return !LastUse || ((Now - *LastUse) >= CooldownSeconds);
}

void ATN_CoopPlayerState::MarkServerEmotePlayed(uint8 EmoteID, float Now)
{
	ServerLastEmoteTimes.FindOrAdd(EmoteID) = Now;
}

void ATN_CoopPlayerState::OnRep_EquippedHelmetId()
{
	// Actualiza el mesh del casco en el personaje local cuando el servidor replica el cambio.
	// GetPawn() puede ser null si el PlayerState se recibe antes de la posesión del pawn.
	if (ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(GetPawn()))
	{
		TurtleChar->UpdateHelmetMesh(EquippedHelmetId);
	}
}

void ATN_CoopPlayerState::MulticastForceApplyHelmet_Implementation(FName HelmId)
{
	EquippedHelmetId = HelmId;

	if (ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(GetPawn()))
	{
		TurtleChar->UpdateHelmetMesh(HelmId);
	}
	else if (UWorld* World = GetWorld())
	{
		// Pawn no está disponible aún (race condition post-seamless-travel).
		// Reintentamos con margen amplio: 15 intentos × 0.2s = 3s de ventana.
		TWeakObjectPtr<ATN_CoopPlayerState> WeakThis(this);
		struct FRetryState { int32 Remaining = 15; };
		TSharedPtr<FRetryState> Retry = MakeShared<FRetryState>();
		TSharedPtr<FTimerHandle> RetryHandle = MakeShared<FTimerHandle>();
		World->GetTimerManager().SetTimer(*RetryHandle, [WeakThis, HelmId, Retry, RetryHandle, World]()
		{
			if (!WeakThis.IsValid())
			{
				World->GetTimerManager().ClearTimer(*RetryHandle);
				return;
			}
			if (ATortugaCharacter* TurtleChar2 = Cast<ATortugaCharacter>(WeakThis->GetPawn()))
			{
				TurtleChar2->UpdateHelmetMesh(HelmId);
				World->GetTimerManager().ClearTimer(*RetryHandle);
				return;
			}
			if (--Retry->Remaining <= 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CoopPlayerState] MulticastForceApplyHelmet: agotados reintentos para '%s'."),
					*WeakThis->GetPlayerName());
				World->GetTimerManager().ClearTimer(*RetryHandle);
			}
		}, 0.2f, true);
	}
}

void ATN_CoopPlayerState::OnRep_EquippedSkinId()
{
	if (ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(GetPawn()))
	{
		TurtleChar->UpdateSkinVisual(EquippedSkinId);
	}
}

void ATN_CoopPlayerState::MulticastForceApplySkin_Implementation(FName SkinId)
{
	EquippedSkinId = SkinId;

	if (ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(GetPawn()))
	{
		TurtleChar->UpdateSkinVisual(SkinId);
	}
	else if (UWorld* World = GetWorld())
	{
		// Pawn no está disponible aún (race condition post-seamless-travel).
		// Reintentamos con margen amplio: 15 intentos × 0.2s = 3s de ventana.
		TWeakObjectPtr<ATN_CoopPlayerState> WeakThis(this);
		struct FRetryState { int32 Remaining = 15; };
		TSharedPtr<FRetryState> Retry = MakeShared<FRetryState>();
		TSharedPtr<FTimerHandle> RetryHandle = MakeShared<FTimerHandle>();
		World->GetTimerManager().SetTimer(*RetryHandle, [WeakThis, SkinId, Retry, RetryHandle, World]()
		{
			if (!WeakThis.IsValid())
			{
				World->GetTimerManager().ClearTimer(*RetryHandle);
				return;
			}
			if (ATortugaCharacter* TurtleChar2 = Cast<ATortugaCharacter>(WeakThis->GetPawn()))
			{
				TurtleChar2->UpdateSkinVisual(SkinId);
				World->GetTimerManager().ClearTimer(*RetryHandle);
				return;
			}
			if (--Retry->Remaining <= 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CoopPlayerState] MulticastForceApplySkin: agotados reintentos para '%s'."),
					*WeakThis->GetPlayerName());
				World->GetTimerManager().ClearTimer(*RetryHandle);
			}
		}, 0.2f, true);
	}
}

void ATN_CoopPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATN_CoopPlayerState, bIsInReadyZone);
	DOREPLIFETIME(ATN_CoopPlayerState, bHasFinishedRun);
	DOREPLIFETIME(ATN_CoopPlayerState, bIsAlive);
	DOREPLIFETIME(ATN_CoopPlayerState, bIsDBNO);
	DOREPLIFETIME_CONDITION(ATN_CoopPlayerState, DBNOBleedoutTimeRemaining, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATN_CoopPlayerState, DeathZoneTimeRemaining, COND_OwnerOnly);
	DOREPLIFETIME(ATN_CoopPlayerState, EquippedHelmetId);
	DOREPLIFETIME(ATN_CoopPlayerState, EquippedSkinId);
	DOREPLIFETIME(ATN_CoopPlayerState, FinishTimeSeconds);
	DOREPLIFETIME(ATN_CoopPlayerState, FinishRank);
	DOREPLIFETIME(ATN_CoopPlayerState, bIsEliminated);
	DOREPLIFETIME(ATN_CoopPlayerState, RaceScore);
}

void ATN_CoopPlayerState::OnRep_RaceScore()
{
	OnRaceScoreChanged.Broadcast(RaceScore);
}

