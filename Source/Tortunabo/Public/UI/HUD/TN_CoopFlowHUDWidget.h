#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_CoopFlowHUDWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class ATN_CoopGameState;

UCLASS()
class TORTUNABO_API UTN_CoopFlowHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RootContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PrimaryText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SecondaryText;

private:
	void EnsureRuntimeWidgets();
	void RefreshTexts();

	FText BuildPrimaryText(const ATN_CoopGameState* GameState) const;
	FText BuildSecondaryText(const ATN_CoopGameState* GameState) const;
	bool ShouldBeVisible(ETNMatchFlowState State) const;

	float RefreshAccumulator = 0.f;
	float RefreshInterval = 0.1f;
};

