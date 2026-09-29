#include "Multiplayer/TN_RoomInfo.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Core/TN_Log.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ATN_RoomInfo::ATN_RoomInfo()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	// Cambia poco (cerrar, abrir, expulsar) y cada cambio fuerza su envío.
	SetNetUpdateFrequency(1.f);
}

void ATN_RoomInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_RoomInfo, RoomNameId);
	DOREPLIFETIME(ATN_RoomInfo, RoomCode);
	DOREPLIFETIME(ATN_RoomInfo, HostName);
	DOREPLIFETIME(ATN_RoomInfo, bPrivate);
	DOREPLIFETIME(ATN_RoomInfo, bLocked);
	DOREPLIFETIME(ATN_RoomInfo, MaxPlayers);
	DOREPLIFETIME(ATN_RoomInfo, KickedPlayerIds);
}

ATN_RoomInfo* ATN_RoomInfo::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ATN_RoomInfo> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

void ATN_RoomInfo::ServerApply(const FTNRoomConfig& Room, const FString& InHostName)
{
	if (!HasAuthority())
	{
		return;
	}
	const bool bChanged = RoomNameId != Room.NameId || RoomCode != Room.Code || HostName != InHostName || bPrivate != Room.bPrivate
		|| bLocked != Room.bLocked || MaxPlayers != Room.MaxPlayers;
	if (!bChanged)
	{
		return;
	}
	RoomNameId = Room.NameId;
	RoomCode = Room.Code;
	HostName = InHostName;
	bPrivate = Room.bPrivate;
	bLocked = Room.bLocked;
	MaxPlayers = Room.MaxPlayers;
	ForceNetUpdate();
}

void ATN_RoomInfo::ServerMarkKicked(int32 PlayerId)
{
	if (!HasAuthority())
	{
		return;
	}
	KickedPlayerIds.AddUnique(PlayerId);
	ForceNetUpdate();
}

void ATN_RoomInfo::OnRep_KickedPlayerIds()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APlayerState* PS = PC && PC->IsLocalController() ? PC->PlayerState.Get() : nullptr;
		if (PS && KickedPlayerIds.Contains(PS->GetPlayerId()))
		{
			UE_LOG(LogTortunabo, Log, TEXT("[Salas] El anfitrión me ha expulsado de la sala (PlayerId %d)."), PS->GetPlayerId());
			if (UMP_GameInstance* GameInstance = Cast<UMP_GameInstance>(World->GetGameInstance()))
			{
				GameInstance->HandleKickedFromRoom(RoomNameId);
			}
			return;
		}
	}
}
