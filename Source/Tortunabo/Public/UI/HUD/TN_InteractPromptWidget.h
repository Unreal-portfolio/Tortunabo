#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_InteractPromptWidget.generated.h"

class UTextBlock;

UCLASS()
class TORTUNABO_API UTN_InteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
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

