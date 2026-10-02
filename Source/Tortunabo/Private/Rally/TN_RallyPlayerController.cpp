#include "Rally/TN_RallyPlayerController.h"

#include "Containers/Ticker.h"
#include "Core/TN_CoopPlayerState.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Pawn.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Rally/TN_RallyHUDWidget.h"
#include "Rally/TN_RallyLogic.h"
#include "Rally/TN_RallyPlayerState.h"
#include "Rally/UI/TN_RallyCopilotTablet.h"
#include "Voice/ProximityVoiceComponent.h"
#include "Rally/UI/TN_RallyDashboard.h"
#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyGunnerPawn.h"
#include "VR/TN_VRMode.h"

namespace TNRallyPC
{
	/** Cada cuánto se comprueba que el buggy local lleva el salpicadero y el cartel del arco (s). */
	constexpr float DashboardCheckSeconds = 0.5f;
}

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
			// En VR, al panel del mundo como el resto de la interfaz.
			TNVR::AddToScreen(RallyHUD, 0);
		}
	}
	// Tableta de copiloto: grande para la artillera, compacta para la conductora sola; elige sola por la plaza.
	UTN_RallyCopilotTablet::FindOrCreateFor(this);
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

void ATN_RallyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController())
	{
		return;
	}
	DashboardCheckAccumulator += DeltaTime;
	if (DashboardCheckAccumulator >= TNRallyPC::DashboardCheckSeconds)
	{
		DashboardCheckAccumulator = 0.f;
		UTN_RallyDashboardComponent::AttachTo(FindLocalBuggy(), this);
	}
}

ATN_Buggy* ATN_RallyPlayerController::FindLocalBuggy() const
{
	APawn* MyPawn = GetPawn();
	if (ATN_Buggy* Driven = Cast<ATN_Buggy>(MyPawn))
	{
		return Driven;
	}
	const ATN_BuggyGunnerPawn* Gunner = Cast<ATN_BuggyGunnerPawn>(MyPawn);
	return Gunner ? Gunner->GetBuggy() : nullptr;
}

void ATN_RallyPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UProximityVoiceComponent::EnsureOn(InPawn);
}

void ATN_RallyPlayerController::SendVoiceToOwningClient(const TArray<uint8>& CompressedData, int32 SenderSampleRate,
	AActor* SpeakerActor, bool bIntercom)
{
	ClientReceiveVoice(CompressedData, SenderSampleRate, SpeakerActor, bIntercom);
}

void ATN_RallyPlayerController::ClientReceiveVoice_Implementation(const TArray<uint8>& CompressedData, int32 SenderSampleRate,
	AActor* SpeakerActor, bool bIntercom)
{
	if (UProximityVoiceComponent* Voice = SpeakerActor ? SpeakerActor->FindComponentByClass<UProximityVoiceComponent>() : nullptr)
	{
		Voice->PlayRemoteVoice(CompressedData, SenderSampleRate, bIntercom);
	}
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

void ATN_RallyPlayerController::PawnLeavingGame()
{
	const ATN_RallyPlayerState* RallyPlayer = GetPlayerState<ATN_RallyPlayerState>();
	if (RallyPlayer && RallyPlayer->IsSeated())
	{
		UE_LOG(LogTNRally, Log, TEXT("[RallyPC] %s se desconecta sentada en el equipo %d: su peón %s queda para Logout."),
			*GetNameSafe(this), RallyPlayer->GetRallyTeamIndex(), *GetNameSafe(GetPawn()));
		return;
	}
	Super::PawnLeavingGame();
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

#if !UE_BUILD_SHIPPING
void ATN_RallyPlayerController::DebugSendCosmetics(FName SkinId, FName ShellId, FName EyesId)
{
	const UMP_GameInstance* GameInstance = Cast<UMP_GameInstance>(GetGameInstance());
	if (!IsLocalController() || !GameInstance)
	{
		return;
	}
	FTNCosmeticLoadout Loadout = TNCosmeticsSync::ReadLocalLoadout(*GameInstance);
	for (const TPair<FName*, FName>& Override : { TPair<FName*, FName>(&Loadout.SkinId, SkinId),
		TPair<FName*, FName>(&Loadout.ShellId, ShellId), TPair<FName*, FName>(&Loadout.EyesId, EyesId) })
	{
		if (!Override.Value.IsNone())
		{
			*Override.Key = Override.Value;
			Loadout.UnlockedSkinIds.AddUnique(Override.Value);
		}
	}
	UE_LOG(LogTNRally, Log, TEXT("[RallyPC] %s manda aspecto de prueba: color=%s caparazón=%s ojos=%s"), *GetNameSafe(this),
		*Loadout.SkinId.ToString(), *Loadout.ShellId.ToString(), *Loadout.EyesId.ToString());
	ServerSyncCosmetics(Loadout);
}

static FAutoConsoleCommandWithWorldAndArgs GTNRallyDebugCosmeticsCommand(
	TEXT("TN.Rally.DebugCosmetics"),
	TEXT("Rally: TN.Rally.DebugCosmetics <color> [caparazón] [ojos] [espera]: la jugadora local manda ese aspecto al servidor (filas de DT_Skins; no toca el save)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
	{
		const auto Arg = [&Args](int32 Index) { return Args.IsValidIndex(Index) && Args[Index] != TEXT("-") ? FName(*Args[Index]) : NAME_None; };
		const FName Skin = Arg(0);
		const FName Shell = Arg(1);
		const FName Eyes = Arg(2);
		const float Wait = Args.IsValidIndex(3) ? FCString::Atof(*Args[3]) : 0.f;
		// Con el ticker del motor: en un cliente, -ExecCmds corre antes de llegar al mapa del servidor.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Skin, Shell, Eyes](float)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				ATN_RallyPlayerController* PC = World && Context.WorldType == EWorldType::Game
					? Cast<ATN_RallyPlayerController>(World->GetFirstPlayerController()) : nullptr;
				if (PC)
				{
					PC->DebugSendCosmetics(Skin, Shell, Eyes);
					return false;
				}
			}
			UE_LOG(LogTNRally, Warning, TEXT("TN.Rally.DebugCosmetics: no hay un PlayerController del Rally"));
			return false;
		}), FMath::Max(Wait, 0.01f));
	}));
#endif

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
