#include "UI/HUD/TN_LoadingScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"

void UTN_LoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureRuntimeLayout();
	SetStatusMessage(FText::FromString(TEXT("Cargando...")));
}

void UTN_LoadingScreenWidget::SetStatusMessage(const FText& NewMessage)
{
	if (StatusText)
	{
		StatusText->SetText(NewMessage);
	}
}

void UTN_LoadingScreenWidget::EnsureRuntimeLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!RootOverlay)
	{
		RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
		if (RootOverlay && !WidgetTree->RootWidget)
		{
			WidgetTree->RootWidget = RootOverlay;
		}
	}

	if (!StatusText && RootOverlay)
	{
		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
		if (StatusText)
		{
			StatusText->SetAutoWrapText(true);
			StatusText->SetJustification(ETextJustify::Center);
			StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			if (UOverlaySlot* OverlaySlot = RootOverlay->AddChildToOverlay(StatusText))
			{
				OverlaySlot->SetHorizontalAlignment(HAlign_Center);
				OverlaySlot->SetVerticalAlignment(VAlign_Center);
				OverlaySlot->SetPadding(FMargin(20.f));
			}
		}
	}
}

