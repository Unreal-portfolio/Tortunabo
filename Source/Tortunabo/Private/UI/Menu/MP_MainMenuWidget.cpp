#include "UI/Menu/MP_MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"

void UMP_MainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UMP_MainMenuWidget::OnHostClicked);
	}

	if (FindButton)
	{
		FindButton->OnClicked.AddDynamic(this, &UMP_MainMenuWidget::OnFindClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UMP_MainMenuWidget::OnQuitClicked);
	}

	if (StatusText)
	{
		StatusText->SetAutoWrapText(true);
	}

	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->OnStatusChanged.AddDynamic(this, &UMP_MainMenuWidget::OnGameInstanceStatusChanged);
		const FString Existing = GI->BuildStatusLog();
		SetStatus(Existing.IsEmpty() ? TEXT("Ready. Host or Find a game.") : Existing);
	}
	else
	{
		SetStatus(TEXT("Ready. Host or Find a game."));
	}
}

void UMP_MainMenuWidget::OnHostClicked()
{
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->HostSession();
	}
}

void UMP_MainMenuWidget::OnFindClicked()
{
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->FindAndJoinSession();
	}
}

void UMP_MainMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, true);
}

void UMP_MainMenuWidget::OnGameInstanceStatusChanged(const FString& StatusMessage)
{
	SetStatus(StatusMessage);
}

void UMP_MainMenuWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

