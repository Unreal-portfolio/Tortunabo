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
	TSoftClassPtr<UUserWidget> CoopFlowWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UUserWidget> CosmeticsWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> VoiceIndicatorWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> CoopFlowWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> CosmeticsWidget;

	UFUNCTION(Server, Reliable)
	void ServerSyncUnlockedHelmets(const TArray<FName>& UnlockedHelmetIds);

	UFUNCTION(Server, Reliable)
	void ServerSetEquippedHelmet(FName HelmetId);

	void SyncCosmeticsToServer();

	void ApplyGameplayInputMode();
	void CreateVoiceHUD();
	void CreateCoopFlowHUD();
	void SpectateByDirection(int32 Direction);

	TSet<FName> ServerUnlockedHelmets;
};
