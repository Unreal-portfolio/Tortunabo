#include "World/TN_RescuePickup.h"
#include "Game/TN_RunGameMode.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ATN_RescuePickup::ATN_RescuePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	NetDormancy = DORM_Awake;
	SetNetUpdateFrequency(15.f);
	SetMinNetUpdateFrequency(5.f);
	PromptText = FText::FromString(TEXT("Rescatar"));
	InteractionDistance = 300.f;
}

void ATN_RescuePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !FollowedDeadPawn.IsValid())
	{
		return;
	}

	const FVector FollowLocation = ResolveFollowLocation();
	SetActorLocation(FollowLocation + FVector(0.f, 0.f, 35.f), false, nullptr, ETeleportType::TeleportPhysics);
}

void ATN_RescuePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_RescuePickup, DeadPlayerId);
}

void ATN_RescuePickup::SetDeadPlayerId(int32 InPlayerId)
{
	DeadPlayerId = InPlayerId;
}

void ATN_RescuePickup::FollowDeadPawn(APawn* InDeadPawn, FName InTrackingBone)
{
	FollowedDeadPawn = InDeadPawn;
	TrackingBone = InTrackingBone.IsNone() ? TEXT("pelvis") : InTrackingBone;

	if (HasAuthority())
	{
		FlushNetDormancy();
		SetActorLocation(ResolveFollowLocation() + FVector(0.f, 0.f, 35.f), false, nullptr, ETeleportType::TeleportPhysics);
		ForceNetUpdate();
	}
}

FVector ATN_RescuePickup::ResolveFollowLocation() const
{
	APawn* DeadPawn = FollowedDeadPawn.Get();
	if (!DeadPawn)
	{
		return GetActorLocation();
	}

	if (const ATortugaCharacter* Character = Cast<ATortugaCharacter>(DeadPawn))
	{
		if (USkeletalMeshComponent* RagdollMesh = Character->GetMesh())
		{
			const int32 BoneIdx = RagdollMesh->GetBoneIndex(TrackingBone);
			if (BoneIdx != INDEX_NONE)
			{
				return RagdollMesh->GetBoneLocation(TrackingBone, EBoneSpaces::WorldSpace);
			}
		}
	}

	return DeadPawn->GetActorLocation();
}

bool ATN_RescuePickup::CanInteract(APawn* Interactor) const
{
	if (!Super::CanInteract(Interactor))
	{
		return false;
	}

	// No puede rescatarse a sí mismo ni interactuar si el jugador muerto ya no existe
	if (DeadPlayerId < 0)
	{
		return false;
	}

	// No dejar que el propio jugador muerto (si de alguna forma tiene pawn) se auto-rescue
	if (Interactor)
	{
		if (APlayerController* InteractorPC = Cast<APlayerController>(Interactor->GetController()))
		{
			if (APlayerState* PS = InteractorPC->GetPlayerState<APlayerState>())
			{
				if (PS->GetPlayerId() == DeadPlayerId)
				{
					return false;
				}
			}
		}
	}

	return true;
}

void ATN_RescuePickup::Interact(APawn* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	// Buscar el PlayerController del jugador muerto por PlayerId
	APlayerController* DeadPC = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }
		if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			if (PS->GetPlayerId() == DeadPlayerId)
			{
				DeadPC = PC;
				break;
			}
		}
	}

	if (!DeadPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RescuePickup] Could not find PlayerController for DeadPlayerId=%d"), DeadPlayerId);
		Destroy();
		return;
	}

	// Llamar a RevivePlayer del GameMode
	if (ATN_RunGameMode* RunGM = Cast<ATN_RunGameMode>(GetWorld()->GetAuthGameMode()))
	{
		RunGM->RevivePlayer(DeadPC);

		UE_LOG(LogTemp, Log, TEXT("[RescuePickup] Revived player (Id=%d) at (%.0f,%.0f,%.0f)"),
			DeadPlayerId, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RescuePickup] No TN_RunGameMode found — cannot revive"));
	}

	// Llamar OnInteracted para hooks BP
	OnInteracted(Interactor);

	Destroy();
}

