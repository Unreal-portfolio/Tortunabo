#include "UI/Menu/MP_MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"

namespace TNMainMenuDetail
{
	/** Primer texto dentro del botón (el rótulo del Blueprint, esté directamente o dentro de otros paneles). */
	UTextBlock* FindLabel(UWidget* Widget)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			return Text;
		}
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				if (UTextBlock* Found = FindLabel(Panel->GetChildAt(ChildIndex)))
				{
					return Found;
				}
			}
		}
		return nullptr;
	}

	/** Cambia solo el texto: fuente, color y tamaño siguen siendo los del Blueprint. */
	void SetLabel(UButton* Button, const TCHAR* Label)
	{
		if (UTextBlock* Text = FindLabel(Button))
		{
			Text->SetText(FText::FromString(Label));
		}
	}

	const TCHAR* const IdleStatus = TEXT("Listo. Crea una partida o únete a una.");
	const TCHAR* const ChooseStatus = TEXT("¿A qué jugamos?\nCooperativo: todas juntas, del castillo de arena al mapa procedural.\nCarrera: todas contra todas en la playa; gana quien consigue tres conchas.");
}

void UMP_MainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HostButton)
	{
		HostButton->OnClicked.AddUniqueDynamic(this, &UMP_MainMenuWidget::OnHostClicked);
	}

	if (FindButton)
	{
		FindButton->OnClicked.AddUniqueDynamic(this, &UMP_MainMenuWidget::OnFindClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UMP_MainMenuWidget::OnQuitClicked);
	}

	if (StatusText)
	{
		StatusText->SetAutoWrapText(true);
	}

	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->OnStatusChanged.AddUniqueDynamic(this, &UMP_MainMenuWidget::OnGameInstanceStatusChanged);
	}

	ShowModeChoice(false);
}

void UMP_MainMenuWidget::NativeDestruct()
{
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->OnStatusChanged.RemoveDynamic(this, &UMP_MainMenuWidget::OnGameInstanceStatusChanged);
	}
	Super::NativeDestruct();
}

void UMP_MainMenuWidget::OnHostClicked()
{
	if (!bChoosingMode)
	{
		ShowModeChoice(true);
		return;
	}
	HostWithMode(ETNProcGameMode::Coop);
}

void UMP_MainMenuWidget::OnFindClicked()
{
	if (bChoosingMode)
	{
		HostWithMode(ETNProcGameMode::Race);
		return;
	}
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->FindAndJoinSession();
	}
}

void UMP_MainMenuWidget::OnQuitClicked()
{
	if (bChoosingMode)
	{
		ShowModeChoice(false);
		return;
	}
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

void UMP_MainMenuWidget::ShowModeChoice(bool bChoose)
{
	bChoosingMode = bChoose;
	TNMainMenuDetail::SetLabel(HostButton, bChoose ? TEXT("Cooperativo") : TEXT("Crear partida"));
	TNMainMenuDetail::SetLabel(FindButton, bChoose ? TEXT("Carrera") : TEXT("Unirse"));
	TNMainMenuDetail::SetLabel(QuitButton, bChoose ? TEXT("Volver") : TEXT("Salir"));
	SetStatus(bChoose ? FString(TNMainMenuDetail::ChooseStatus) : BuildIdleStatus());
}

void UMP_MainMenuWidget::HostWithMode(ETNProcGameMode Mode)
{
	// De vuelta al paso principal: si crear la partida falla, el menú no se queda en la elección de modo.
	ShowModeChoice(false);
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->HostSessionWithMode(Mode);
	}
}

FString UMP_MainMenuWidget::BuildIdleStatus() const
{
	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	const FString Existing = GI ? GI->BuildStatusLog() : FString();
	return Existing.IsEmpty() ? FString(TNMainMenuDetail::IdleStatus) : Existing;
}
