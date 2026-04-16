#include "World/TN_ScriptedDeathZone.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ATN_ScriptedDeathZone::ATN_ScriptedDeathZone()
{
}

// ── Virtual hooks ─────────────────────────────────────────────────────────────

void ATN_ScriptedDeathZone::OnPlayerEnteredZone(APawn* Pawn)
{
	if (!HasAuthority() || bOnActivateFired) { return; }

	bOnActivateFired = true;
	ExecuteActions(OnActivateActions);
	MulticastNotifyActivate();
}

void ATN_ScriptedDeathZone::OnPlayerDiedInZone(APlayerController* PC)
{
	if (!HasAuthority() || bOnFirstKillFired) { return; }

	bOnFirstKillFired = true;
	ExecuteActions(OnFirstKillActions);
	MulticastNotifyFirstKill();
}

// ── Action execution ──────────────────────────────────────────────────────────

void ATN_ScriptedDeathZone::ExecuteActions(const TArray<FTN_WorldAction>& Actions)
{
	for (const FTN_WorldAction& Action : Actions)
	{
		AActor* Target = Action.TargetActor;
		if (!Target) { continue; }

		switch (Action.ActionType)
		{
		case ETN_WorldActionType::MoveActor:
			Target->SetActorLocation(Target->GetActorLocation() + Action.LocationOffset);
			break;

		case ETN_WorldActionType::RotateActor:
			Target->SetActorRotation(Target->GetActorRotation() + Action.RotationOffset);
			break;

		case ETN_WorldActionType::SetVisibility:
			Target->SetActorHiddenInGame(Action.bHide);
			Target->SetActorEnableCollision(!Action.bHide);
			break;

		case ETN_WorldActionType::DestroyActor:
			Target->Destroy();
			break;
		}
	}
}

// ── Multicast cosmético ───────────────────────────────────────────────────────

void ATN_ScriptedDeathZone::MulticastNotifyActivate_Implementation()
{
	OnScriptedActivate();
}

void ATN_ScriptedDeathZone::MulticastNotifyFirstKill_Implementation()
{
	OnScriptedFirstKill();
}
