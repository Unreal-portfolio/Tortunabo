#include "Rally/TN_RallyPlayerController.h"

#include "Core/TN_CoopPlayerState.h"
#include "GameFramework/Pawn.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Rally/TN_RallyHUDWidget.h"
#include "Rally/TN_RallyLogic.h"

ATN_RallyPlayerController::ATN_RallyPlayerController()
{
	HUDWidgetClass = UTN_RallyHUDWidget::StaticClass();
}

void ATN_RallyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	if (HUDWidgetClass && !RallyHUD)
	{
		RallyHUD = CreateWidget<UTN_RallyHUDWidget>(this, HUDWidgetClass);
		if (RallyHUD)
		{
			RallyHUD->AddToViewport(0);
		}
	}
	SyncCosmeticsToServer();
}

void ATN_RallyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RallyHUD)
	{
		RallyHUD->RemoveFromParent();
		RallyHUD = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ATN_RallyPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	const UEnum* Roles = StaticEnum<ENetRole>();
	UE_LOG(LogTNRally, Log, TEXT("[RallyPC] %s posee %s (%s): rol local %s, remoto %s"), *GetNameSafe(this), *GetNameSafe(InPawn),
		*GetNameSafe(InPawn ? InPawn->GetClass() : nullptr),
		InPawn ? *Roles->GetNameStringByValue(InPawn->GetLocalRole()) : TEXT("-"),
		InPawn ? *Roles->GetNameStringByValue(InPawn->GetRemoteRole()) : TEXT("-"));
}

void ATN_RallyPlayerController::SyncCosmeticsToServer()
{
	const UMP_GameInstance* GameInstance = Cast<UMP_GameInstance>(GetGameInstance());
	if (!IsLocalController() || !GameInstance)
	{
		return;
	}
	ServerSyncCosmetics(TNCosmeticsSync::ReadLocalLoadout(*GameInstance));
}

bool ATN_RallyPlayerController::ServerSyncCosmetics_Validate(const FTNCosmeticLoadout& Loadout)
{
	return TNCosmeticsSync::IsLoadoutWithinRpcCaps(Loadout);
}

void ATN_RallyPlayerController::ServerSyncCosmetics_Implementation(const FTNCosmeticLoadout& Loadout)
{
	ATN_CoopPlayerState* CoopState = GetPlayerState<ATN_CoopPlayerState>();
	if (!CoopState)
	{
		UE_LOG(LogTNRally, Warning, TEXT("[RallyPC] %s manda cosméticos sin PlayerState: se ignoran."), *GetNameSafe(this));
		return;
	}
	const int32 Applied = TNCosmeticsSync::ApplyLoadoutOnServer(Cast<UMP_GameInstance>(GetGameInstance()), *CoopState, Loadout);
	UE_LOG(LogTNRally, Log, TEXT("[RallyPC] cosméticos de %s: casco=%s color=%s caparazón=%s ojos=%s (%d/4 aplicados)"),
		*CoopState->GetPlayerName(), *CoopState->EquippedHelmetId.ToString(), *CoopState->EquippedSkinId.ToString(),
		*CoopState->EquippedShellId.ToString(), *CoopState->EquippedEyesId.ToString(), Applied);
}
