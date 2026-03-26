// Fill out your copyright notice in the Description page of Project Settings.

#include "BallPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"


#include "Mecanisms/ChunkManager.h"
#include "RollingBall2/Ball.h"
#include "../EzSaveGame/SaveComponent.h" // include relativo para encontrar el header del componente

void ABallPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Enhanced Input
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			if (ControlsMap)
			{
				Input->AddMappingContext(ControlsMap, 0);
			}
		}
	}

	// Buscar ChunkManager (una vez)
	ChunkManager = Cast<AChunkManager>(UGameplayStatics::GetActorOfClass(this, AChunkManager::StaticClass()));

	// Crear HUD y escribir Score inicial
	InitHUD();
	UpdateScoreUI(Score);
}

void ABallPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ABall* BallPawn = Cast<ABall>(InPawn))
	{
		BallPawn->SetBallController(this);
	}
}

void ABallPlayerController::InitHUD()
{
	if (!HUDWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitHUD: HUDWidgetClass no asignada"));
		return;
	}

	// 1) Intentar encontrar un widget ya creado de esa clase en el viewport
	if (!HUDWidget)
	{
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundWidgets, HUDWidgetClass, /*TopLevelOnly*/ false);

		if (FoundWidgets.Num() > 0)
		{
			HUDWidget = FoundWidgets[0];
		}
	}

	// 2) Si no existe, entonces lo creamos (fallback)
	if (!HUDWidget)
	{
		HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}

	if (!HUDWidget) return;

	// 3) Cachear el TextBlock del score
	ScoreTextBlock = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("ScoreText")));

	if (!ScoreTextBlock)
	{
		UE_LOG(LogTemp, Error, TEXT("InitHUD: No encuentro un TextBlock llamado 'ScoreText' en el HUD."));
	}
}


void ABallPlayerController::UpdateScoreUI(int32 NewScore)
{
	// Si no tenemos el TextBlock, intentamos inicializar
	if (!ScoreTextBlock)
	{
		InitHUD();
	}

	if (!ScoreTextBlock)
		return;

	const FString Str = FString::Printf(TEXT("Score: %d"), NewScore);
	ScoreTextBlock->SetText(FText::FromString(Str));
}

void ABallPlayerController::OnCollectable()
{
	Score += 10;
	UpdateScoreUI(Score);
}

void ABallPlayerController::OnLoseLive()
{
	CurrentLives--;
	UpdateLivesUI(CurrentLives);
	
	if (DeathSound)
	{
		UGameplayStatics::PlaySound2D(this, DeathSound);
	}

	if (CurrentLives <= 0)
	{
		GameOver();
		return;
	}

	UnPossess();

	if (!ChunkManager)
	{
		ChunkManager = Cast<AChunkManager>(UGameplayStatics::GetActorOfClass(this, AChunkManager::StaticClass()));
	}

	if (!ChunkManager)
	{
		UE_LOG(LogTemp, Error, TEXT("OnLoseLive: ChunkManager es NULL. Respawn normal con RestartPlayer."));
		GetWorld()->GetAuthGameMode()->RestartPlayer(this);
		return;
	}

	FTransform RespawnTransform = ChunkManager->GetCurrentRespawnTransform();
	RespawnTransform.AddToTranslation(FVector(0.f, 0.f, 150.f));

	if (RespawnTransform.GetLocation().IsNearlyZero(1.0f))
	{
		UE_LOG(LogTemp, Error, TEXT("OnLoseLive: RespawnTransform es (casi) ZERO. Fallback a RestartPlayer normal."));
		GetWorld()->GetAuthGameMode()->RestartPlayer(this);
		return;
	}

	GetWorld()->GetAuthGameMode()->RestartPlayerAtTransform(this, RespawnTransform);
}

void ABallPlayerController::GameOver()
{
	bShowMouseCursor = true;

	if (GameOverWidgetClass)
	{
		if (UUserWidget* WidgetCreated = CreateWidget(this, GameOverWidgetClass))
		{
			WidgetCreated->AddToViewport();
		}
	}
}

USaveComponent* ABallPlayerController::GetSaveComponent() const
{
	// Intentamos encontrar un componente SaveComponent en el Controller usando la utilidad directa
	if (USaveComponent* SC = FindComponentByClass<USaveComponent>())
	{
		return SC;
	}

	// No encontrado en Controller; intentamos buscar en el Pawn
	if (GetPawn())
	{
		if (USaveComponent* SC = GetPawn()->FindComponentByClass<USaveComponent>())
		{
			return SC;
		}
	}

	return nullptr;
}

void ABallPlayerController::SaveEndGame_BP(int32 ScoreToSave, int32 LivesToSave, const FString& PlayerName, bool bSubmitName)
{
	// Sanitize and limit name to max 5 characters
	FString NameToUse = PlayerName;
	if (NameToUse.Len() > 5)
	{
		NameToUse = NameToUse.Left(5);
	}

	if (USaveComponent* SC = GetSaveComponent())
	{
		SC->SaveEndGameAndPersist(ScoreToSave, LivesToSave, NameToUse, bSubmitName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveEndGame_BP: No SaveComponent found on Controller or Pawn"));
	}
}
