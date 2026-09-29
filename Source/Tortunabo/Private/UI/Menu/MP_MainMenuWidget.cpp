#include "UI/Menu/MP_MainMenuWidget.h"
#include "UI/Menu/TN_RoomMenuWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
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
	void SetLabel(UButton* Button, const FText& Label)
	{
		if (UTextBlock* Text = FindLabel(Button))
		{
			Text->SetText(Label);
		}
	}

	const TCHAR* const IdleStatus = TEXT("Listo. Crea una partida o únete a una.");

	/** Pantallas de salas: por encima de este menú. */
	constexpr int32 RoomMenuZOrder = 10;
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

	UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	if (GI)
	{
		GI->OnStatusChanged.AddUniqueDynamic(this, &UMP_MainMenuWidget::OnGameInstanceStatusChanged);
	}

	TNMainMenuDetail::SetLabel(HostButton, NSLOCTEXT("TNRooms", "MenuCreate", "Crear partida"));
	TNMainMenuDetail::SetLabel(FindButton, NSLOCTEXT("TNRooms", "MenuJoin", "Unirse"));
	TNMainMenuDetail::SetLabel(QuitButton, NSLOCTEXT("TNRooms", "MenuQuit", "Salir"));
	SetStatus(BuildIdleStatus());

	// Pantallas de salas: su propio widget a pantalla completa, montado y en pantalla antes de enseñar nada.
	if (!RoomMenu)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			RoomMenu = CreateWidget<UTN_RoomMenuWidget>(PC, UTN_RoomMenuWidget::StaticClass());
		}
		if (RoomMenu)
		{
			TWeakObjectPtr<UMP_MainMenuWidget> WeakThis(this);
			RoomMenu->OnOpenChanged = [WeakThis](bool bOpen)
			{
				if (UMP_MainMenuWidget* Menu = WeakThis.Get()) { Menu->HandleRoomsOpenChanged(bOpen); }
			};
			RoomMenu->AddToViewport(TNMainMenuDetail::RoomMenuZOrder);
		}
	}

	// Aviso que dejó la GameInstance al volver aquí (expulsado, sala cerrada o llena, el anfitrión se fue...).
	if (GI && RoomMenu)
	{
		const FTNMenuNotice Notice = GI->ConsumeMenuNotice();
		if (!Notice.Text.IsEmpty())
		{
			if (Notice.bOpenJoin)
			{
				OpenRooms(ETNRoomMenuPage::Join);
			}
			RoomMenu->ShowNotice(Notice.Text, Notice.bError, 9.f);
		}
	}

	// Con el mando, el foco empieza en «Crear partida».
	if (HostButton && !(RoomMenu && RoomMenu->IsOpen()))
	{
		HostButton->SetKeyboardFocus();
	}
}

void UMP_MainMenuWidget::NativeDestruct()
{
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->OnStatusChanged.RemoveDynamic(this, &UMP_MainMenuWidget::OnGameInstanceStatusChanged);
	}
	if (RoomMenu)
	{
		RoomMenu->OnOpenChanged = nullptr;
		RoomMenu->RemoveFromParent();
		RoomMenu = nullptr;
	}
	Super::NativeDestruct();
}

void UMP_MainMenuWidget::OnHostClicked()
{
	RoomsOpener = HostButton.Get();
	OpenRooms(ETNRoomMenuPage::Create);
}

void UMP_MainMenuWidget::OnFindClicked()
{
	RoomsOpener = FindButton.Get();
	OpenRooms(ETNRoomMenuPage::Join);
}

void UMP_MainMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, true);
}

void UMP_MainMenuWidget::OpenRooms(ETNRoomMenuPage Page)
{
	if (RoomMenu)
	{
		RoomMenu->Open(Page);
		return;
	}
	// Sin pantalla de salas (no debería pasar): lo de siempre, crear una pública o unirse a la primera.
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		if (Page == ETNRoomMenuPage::Create)
		{
			GI->HostSessionWithMode(GI->SelectedProcMode);
		}
		else
		{
			GI->FindAndJoinSession();
		}
	}
}

void UMP_MainMenuWidget::HandleRoomsOpenChanged(bool bOpen)
{
	if (bOpen)
	{
		// Escondido mientras tanto: así el mando no se escapa a estos botones y no se ven dos menús.
		if (GetVisibility() != ESlateVisibility::Collapsed)
		{
			VisibilityBeforeRooms = GetVisibility();
		}
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(VisibilityBeforeRooms);
	SetStatus(BuildIdleStatus());
	UButton* Back = RoomsOpener.Get();
	if (!Back) { Back = HostButton.Get(); }
	if (Back) { Back->SetKeyboardFocus(); }
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

FString UMP_MainMenuWidget::BuildIdleStatus() const
{
	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	const FString Existing = GI ? GI->BuildStatusLog() : FString();
	return Existing.IsEmpty() ? FString(TNMainMenuDetail::IdleStatus) : Existing;
}
