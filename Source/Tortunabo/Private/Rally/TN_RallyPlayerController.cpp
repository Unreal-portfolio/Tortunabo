#include "Rally/TN_RallyPlayerController.h"

#include "Rally/TN_RallyHUDWidget.h"

ATN_RallyPlayerController::ATN_RallyPlayerController()
{
	HUDWidgetClass = UTN_RallyHUDWidget::StaticClass();
}

void ATN_RallyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	if (HUDWidgetClass && !RallyHUD)
	{
		RallyHUD = CreateWidget<UTN_RallyHUDWidget>(this, HUDWidgetClass);
		if (RallyHUD)
		{
			RallyHUD->AddToViewport(0);
		}
	}
}

void ATN_RallyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RallyHUD)
	{
		RallyHUD->RemoveFromParent();
		RallyHUD = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
