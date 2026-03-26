#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BallPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UTextBlock;
class AChunkManager;
class USaveComponent; // forward

UCLASS()
class ROLLINGBALL2_API ABallPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Vidas siguen por Blueprint (como ya lo tenías funcionando)
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void UpdateLivesUI(int32 NewLives);

	// Lógica gameplay
	void OnLoseLive();
	void OnCollectable();

	// Blueprint accessors for Score/Lives
	UFUNCTION(BlueprintPure, Category = "Data")
	int32 GetScore() const { return Score; }

	UFUNCTION(BlueprintPure, Category = "Data")
	int32 GetCurrentLives() const { return CurrentLives; }

	// Return the SaveComponent instance attached to this controller (if any)
	UFUNCTION(BlueprintPure, Category = "Save")
	USaveComponent* GetSaveComponent() const;

	// Wrapper callable from Blueprints to save end-game via the SaveComponent
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveEndGame_BP(int32 ScoreToSave, int32 LivesToSave, const FString& PlayerName, bool bSubmitName);

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	/* ========= Inputs ========= */
	UPROPERTY(EditAnywhere, Category = "Inputs")
	UInputMappingContext* ControlsMap = nullptr;

	/* ========= Data ========= */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	int32 Score = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	int32 CurrentLives = 3;
	
	UPROPERTY(EditAnywhere, Category="Audio")
	USoundBase* DeathSound = nullptr;

	/* ========= UI ========= */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> GameOverWidgetClass;

	UPROPERTY()
	UUserWidget* HUDWidget = nullptr;

	UPROPERTY()
	UTextBlock* ScoreTextBlock = nullptr;

	void InitHUD();
	void UpdateScoreUI(int32 NewScore);

	/* ========= Respawn / Chunks ========= */
	UPROPERTY()
	AChunkManager* ChunkManager = nullptr;

	void GameOver();
};
