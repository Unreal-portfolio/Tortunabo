#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_InteractPromptWidget.generated.h"

class UTextBlock;

/**
 * @brief Widget de prompt de interacción ("Pulsa E para...").
 *        El texto lo asigna desde C++ ATortugaCharacter al detectar un interactable.
 */
UCLASS()
class TORTUNABO_API UTN_InteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @brief Asigna el texto mostrado en el prompt (ej. "Pulsa E para abrir"). */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetPromptText(const FText& NewText);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptTextBlock;

private:
	UPROPERTY(Transient)
	FText CachedPromptText;
};

