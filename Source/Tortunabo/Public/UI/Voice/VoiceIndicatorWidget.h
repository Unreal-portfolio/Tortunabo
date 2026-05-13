#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VoiceIndicatorWidget.generated.h"

class UImage;
class UProximityVoiceComponent;

/**
 * @brief Indicador HUD del estado VOIP del jugador local (icono de altavoz).
 *
 * Se bindea al UProximityVoiceComponent del propio pawn (con retry hasta que esté disponible)
 * y muestra/oculta el icono según el delegate OnSpeakingChanged.
 */
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

