#include "Player/MP_GamePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Voice/ProximityVoiceComponent.h"
#include "GameFramework/Pawn.h"
#include "Core/TN_CoopPlayerState.h"
#include "GameFramework/GameStateBase.h"

AMP_GamePlayerController::AMP_GamePlayerController()
{
}

void AMP_GamePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController() && GetPawn())
	{
		CreateVoiceHUD();
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
		if (APawn* CandidatePawn = CoopPS->GetPawn())
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
