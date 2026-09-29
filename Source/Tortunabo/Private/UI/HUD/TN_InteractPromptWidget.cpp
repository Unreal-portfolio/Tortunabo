#include "UI/HUD/TN_InteractPromptWidget.h"
#include "Components/TextBlock.h"

void UTN_InteractPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PromptTextBlock)
	{
		PromptTextBlock->SetText(CachedPromptText.IsEmpty() ? NSLOCTEXT("Tortunabo", "InteractPrompt", "Interactuar") : CachedPromptText);
	}
}

void UTN_InteractPromptWidget::SetPromptText(const FText& NewText)
{
	CachedPromptText = NewText;
	if (PromptTextBlock)
	{
		PromptTextBlock->SetText(CachedPromptText);
	}
}

