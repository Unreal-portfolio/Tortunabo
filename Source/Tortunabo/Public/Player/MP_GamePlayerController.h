#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MP_GamePlayerController.generated.h"

class UUserWidget;

UCLASS()
class TORTUNABO_API AMP_GamePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMP_GamePlayerController();

	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void EnterSpectateMode();

	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void SpectateNextPlayer();

	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void SpectatePreviousPlayer();

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void OpenCosmeticsMenu();

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool RequestEquipHelmet(FName HelmetId);

	/** Desequipa el casco actual. Actualiza PlayerState en el servidor. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void RequestUnequipHelmet();

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	FName OpenHelmetCrate();

	UFUNCTION(Client, Reliable)
	void ClientOpenCosmeticsMenu();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> VoiceIndicatorWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> CoopFlowWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> CosmeticsWidgetClass;

	/**
	 * HUD principal del jugador (barra de stamina, etc.).
	 * Asigna WBP_PlayerHUD en BP_GamePlayerController → Class Defaults.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> PlayerHUDWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> VoiceIndicatorWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> CoopFlowWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> CosmeticsWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> PlayerHUDWidget;

	UFUNCTION(Server, Reliable)
	void ServerSyncUnlockedHelmets(const TArray<FName>& UnlockedHelmetIds);

	UFUNCTION(Server, Reliable)
	void ServerSetEquippedHelmet(FName HelmetId);

	void SyncCosmeticsToServer();

	void ApplyGameplayInputMode();
	void CreateVoiceHUD();
	void CreateCoopFlowHUD();
	void CreatePlayerHUD();
	void SpectateByDirection(int32 Direction);

	TSet<FName> ServerUnlockedHelmets;
};
