#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MP_MainMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * @brief Widget del menú principal (botones Host / Find / Quit y log de status).
 *
 * Se suscribe al delegate OnStatusChanged del UMP_GameInstance para reflejar
 * estado de sesión y errores en StatusText.
 */
UCLASS()
class TORTUNABO_API UMP_MainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> FindButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void OnHostClicked();

	UFUNCTION()
	void OnFindClicked();

	UFUNCTION()
	void OnQuitClicked();

	UFUNCTION()
	void OnGameInstanceStatusChanged(const FString& StatusMessage);

	void SetStatus(const FString& Message);
};

