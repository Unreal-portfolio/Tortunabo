#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VoiceIndicatorWidget.generated.h"

class UImage;
class UProximityVoiceComponent;

UCLASS()
class TORTUNABO_API UVoiceIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SpeakerIcon;

private:
	UPROPERTY()
	TObjectPtr<UProximityVoiceComponent> LocalVoiceComponent;

	UFUNCTION()
	void OnSpeakingChanged(bool bIsSpeaking);

	void TryBindVoiceComponent();

	bool bBound = false;
	float RetryTimer = 0.f;
};

