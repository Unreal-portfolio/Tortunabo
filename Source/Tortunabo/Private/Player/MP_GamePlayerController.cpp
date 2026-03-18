#include "Player/MP_GamePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Voice/ProximityVoiceComponent.h"
#include "UI/HUD/TN_CoopFlowHUDWidget.h"
#include "Multiplayer/MP_GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Core/TN_CoopPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"

AMP_GamePlayerController::AMP_GamePlayerController()
{
	CoopFlowWidgetClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Game/UI/HUD/WBP_CoopFlowHUD.WBP_CoopFlowHUD_C")));
	CosmeticsWidgetClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Game/UI/Menu/WBP_CosmeticsMenu.WBP_CosmeticsMenu_C")));
}

void AMP_GamePlayerController::BeginPlay()
{
	Super::BeginPlay();

	ApplyGameplayInputMode();

	if (IsLocalController() && GetPawn())
	{
		CreateVoiceHUD();
	}

	if (IsLocalController())
	{
		CreateCoopFlowHUD();
		SyncCosmeticsToServer();
	}
}

void AMP_GamePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AMP_GamePlayerController::SpectateNextPlayer);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AMP_GamePlayerController::SpectatePreviousPlayer);
		InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &AMP_GamePlayerController::SpectateNextPlayer);
		InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &AMP_GamePlayerController::SpectatePreviousPlayer);
	}
}

void AMP_GamePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ApplyGameplayInputMode();

	if (!InPawn)
	{
		return;
	}

	UProximityVoiceComponent* ExistingVoice = InPawn->FindComponentByClass<UProximityVoiceComponent>();
	if (!ExistingVoice)
	{
		UProximityVoiceComponent* VoiceComp = NewObject<UProximityVoiceComponent>(InPawn, TEXT("ProximityVoice"));
		if (VoiceComp)
		{
			VoiceComp->RegisterComponent();
		}
	}

	if (IsLocalController())
	{
		CreateVoiceHUD();
		CreateCoopFlowHUD();
		SyncCosmeticsToServer();
	}
}

void AMP_GamePlayerController::ApplyGameplayInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);

	if (GEngine && GEngine->GameViewport)
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

void AMP_GamePlayerController::EnterSpectateMode()
{
	ChangeState(NAME_Spectating);
	StartSpectatingOnly();
	SpectateNextPlayer();
}

void AMP_GamePlayerController::SpectateNextPlayer()
{
	SpectateByDirection(1);
}

void AMP_GamePlayerController::SpectatePreviousPlayer()
{
	SpectateByDirection(-1);
}

void AMP_GamePlayerController::SpectateByDirection(int32 Direction)
{
	if (Direction == 0 || !GetWorld() || !PlayerState)
	{
		return;
	}

	TArray<APlayerState*> Candidates;
	for (APlayerState* PS : GetWorld()->GetGameState()->PlayerArray)
	{
		ATN_CoopPlayerState* CoopPS = Cast<ATN_CoopPlayerState>(PS);
		if (!CoopPS || CoopPS == PlayerState || CoopPS->bHasFinishedRun)
		{
			continue;
		}
		if (CoopPS->GetPawn())
		{
			Candidates.Add(CoopPS);
		}
	}

	if (Candidates.Num() == 0)
	{
		return;
	}

	Candidates.Sort([](const APlayerState& A, const APlayerState& B)
	{
		return A.GetPlayerId() < B.GetPlayerId();
	});

	int32 CurrentIndex = INDEX_NONE;
	AActor* CurrentViewTarget = GetViewTarget();
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		if (Candidates[i]->GetPawn() == CurrentViewTarget)
		{
			CurrentIndex = i;
			break;
		}
	}

	int32 NextIndex = 0;
	if (CurrentIndex != INDEX_NONE)
	{
		NextIndex = (CurrentIndex + Direction + Candidates.Num()) % Candidates.Num();
	}
	else if (Direction < 0)
	{
		NextIndex = Candidates.Num() - 1;
	}

	SetViewTargetWithBlend(Candidates[NextIndex]->GetPawn(), 0.25f);
}

void AMP_GamePlayerController::CreateVoiceHUD()
{
	if (VoiceIndicatorWidget || !VoiceIndicatorWidgetClass || !IsLocalController())
	{
		return;
	}

	VoiceIndicatorWidget = CreateWidget<UUserWidget>(this, VoiceIndicatorWidgetClass);
	if (VoiceIndicatorWidget)
	{
		VoiceIndicatorWidget->AddToViewport(10);
	}
}

void AMP_GamePlayerController::CreateCoopFlowHUD()
{
	if (CoopFlowWidget || !IsLocalController())
	{
		return;
	}

	UClass* WidgetClass = nullptr;
	if (!CoopFlowWidgetClass.IsNull())
	{
		WidgetClass = CoopFlowWidgetClass.LoadSynchronous();
	}

	if (!WidgetClass)
	{
		WidgetClass = UTN_CoopFlowHUDWidget::StaticClass();
	}

	CoopFlowWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (CoopFlowWidget)
	{
		CoopFlowWidget->AddToViewport(5);
	}
}

void AMP_GamePlayerController::OpenCosmeticsMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	if (CosmeticsWidget)
	{
		CosmeticsWidget->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	UClass* WidgetClass = CosmeticsWidgetClass.IsNull() ? nullptr : CosmeticsWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		return;
	}

	CosmeticsWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (!CosmeticsWidget)
	{
		return;
	}

	CosmeticsWidget->AddToViewport(40);

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

bool AMP_GamePlayerController::RequestEquipHelmet(FName HelmetId)
{
	if (HelmetId == NAME_None)
	{
		return false;
	}

	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		if (!GI->EquipHelmet(HelmetId))
		{
			return false;
		}
	}

	ServerSetEquippedHelmet(HelmetId);
	return true;
}

FName AMP_GamePlayerController::OpenHelmetCrate()
{
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		const FName Result = GI->OpenHelmetCrate();
		SyncCosmeticsToServer();
		if (Result != NAME_None)
		{
			ServerSetEquippedHelmet(Result);
		}
		return Result;
	}

	return NAME_None;
}

void AMP_GamePlayerController::ClientOpenCosmeticsMenu_Implementation()
{
	OpenCosmeticsMenu();
}

void AMP_GamePlayerController::ServerSyncUnlockedHelmets_Implementation(const TArray<FName>& UnlockedHelmetIds)
{
	ServerUnlockedHelmets.Reset();
	for (const FName HelmetId : UnlockedHelmetIds)
	{
		if (HelmetId != NAME_None)
		{
			ServerUnlockedHelmets.Add(HelmetId);
		}
	}

	if (ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		if (TNPS->EquippedHelmetId == NAME_None && ServerUnlockedHelmets.Num() > 0)
		{
			for (const FName HelmetId : ServerUnlockedHelmets)
			{
				TNPS->EquippedHelmetId = HelmetId;
				break;
			}
		}
	}
}

void AMP_GamePlayerController::ServerSetEquippedHelmet_Implementation(FName HelmetId)
{
	if (HelmetId == NAME_None || !ServerUnlockedHelmets.Contains(HelmetId))
	{
		return;
	}

	if (ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->EquippedHelmetId = HelmetId;
	}
}

void AMP_GamePlayerController::SyncCosmeticsToServer()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		ServerSyncUnlockedHelmets(GI->GetUnlockedHelmetIds());
		const FName EquippedHelmetId = GI->GetEquippedHelmetId();
		if (EquippedHelmetId != NAME_None)
		{
			ServerSetEquippedHelmet(EquippedHelmetId);
		}
	}
}

