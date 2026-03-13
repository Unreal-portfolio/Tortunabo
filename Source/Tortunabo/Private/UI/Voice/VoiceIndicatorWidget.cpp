#include "UI/Voice/VoiceIndicatorWidget.h"
#include "Voice/ProximityVoiceComponent.h"
#include "Components/Image.h"
#include "GameFramework/PlayerController.h"

void UVoiceIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SpeakerIcon)
	{
		SpeakerIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	TryBindVoiceComponent();
}

void UVoiceIndicatorWidget::NativeDestruct()
{
	if (LocalVoiceComponent)
	{
		LocalVoiceComponent->OnSpeakingChanged.RemoveDynamic(this, &UVoiceIndicatorWidget::OnSpeakingChanged);
	}

	Super::NativeDestruct();
}

void UVoiceIndicatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBound)
	{
		RetryTimer += InDeltaTime;
		if (RetryTimer >= 0.5f)
		{
			RetryTimer = 0.f;
			TryBindVoiceComponent();
		}
	}
}

void UVoiceIndicatorWidget::TryBindVoiceComponent()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}

	UProximityVoiceComponent* VoiceComp = Pawn->FindComponentByClass<UProximityVoiceComponent>();
	if (!VoiceComp)
	{
		return;
	}

	LocalVoiceComponent = VoiceComp;
	LocalVoiceComponent->OnSpeakingChanged.AddDynamic(this, &UVoiceIndicatorWidget::OnSpeakingChanged);
	bBound = true;

	if (SpeakerIcon)
	{
		SpeakerIcon->SetVisibility(LocalVoiceComponent->bIsSpeaking ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UVoiceIndicatorWidget::OnSpeakingChanged(bool bIsSpeaking)
{
	if (SpeakerIcon)
	{
		SpeakerIcon->SetVisibility(bIsSpeaking ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

