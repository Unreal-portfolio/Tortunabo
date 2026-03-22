#include "UI/HUD/TN_CosmeticsMenuWidget.h"
#include "Player/MP_GamePlayerController.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Engine/DataTable.h"
#include "Framework/Application/SlateApplication.h"

// ── NativeConstruct / Destruct ────────────────────────────────────────────────

void UTN_CosmeticsMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Vincular botones opcionales
	if (UnequipButton)
	{
		UnequipButton->OnClicked.AddDynamic(this, &UTN_CosmeticsMenuWidget::OnUnequipClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UTN_CosmeticsMenuWidget::OnCloseClicked);
	}

	// Refrescar el grid con los cascos desbloqueados del jugador
	RefreshHelmetGrid();
}

void UTN_CosmeticsMenuWidget::NativeDestruct()
{
	if (UnequipButton)
	{
		UnequipButton->OnClicked.RemoveDynamic(this, &UTN_CosmeticsMenuWidget::OnUnequipClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UTN_CosmeticsMenuWidget::OnCloseClicked);
	}

	Super::NativeDestruct();
}

// ── API pública ───────────────────────────────────────────────────────────────

void UTN_CosmeticsMenuWidget::RefreshHelmetGrid()
{
	const UMP_GameInstance* GI = GetOwningGI();
	if (!GI)
	{
		return;
	}

	const TArray<FName> UnlockedIds = GI->GetUnlockedHelmetIds();
	const FName CurrentEquipped    = GetEquippedHelmetId();

	// Disparar el evento BP para que construya visualmente el grid
	OnHelmetGridRefreshed(UnlockedIds, CurrentEquipped);
}

void UTN_CosmeticsMenuWidget::RequestEquip(FName HelmetId)
{
	if (HelmetId == NAME_None)
	{
		RequestUnequip();
		return;
	}

	AMP_GamePlayerController* PC = GetOwningPC();
	if (!PC)
	{
		return;
	}

	if (PC->RequestEquipHelmet(HelmetId))
	{
		// Feedback inmediato local (sin esperar OnRep)
		OnHelmetEquipped(HelmetId);
	}
}

void UTN_CosmeticsMenuWidget::RequestUnequip()
{
	AMP_GamePlayerController* PC = GetOwningPC();
	if (PC)
	{
		PC->RequestUnequipHelmet();
		OnHelmetEquipped(NAME_None);
	}
}

void UTN_CosmeticsMenuWidget::CloseMenu()
{
	SetVisibility(ESlateVisibility::Hidden);

	// Restaurar modo de input de gameplay (sin cursor)
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}

	if (GEngine && GEngine->GameViewport)
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

bool UTN_CosmeticsMenuWidget::GetHelmetData(FName HelmetId, FTN_HelmetData& OutData) const
{
	if (HelmetId == NAME_None)
	{
		return false;
	}

	const UMP_GameInstance* GI = GetOwningGI();
	if (!GI)
	{
		return false;
	}

	const UDataTable* HelmDT = GI->GetHelmetDataTable();
	if (!HelmDT)
	{
		return false;
	}

	const FTN_HelmetData* Row = HelmDT->FindRow<FTN_HelmetData>(HelmetId, TEXT("GetHelmetData"));
	if (!Row)
	{
		return false;
	}

	OutData = *Row;
	return true;
}

FName UTN_CosmeticsMenuWidget::GetEquippedHelmetId() const
{
	const UMP_GameInstance* GI = GetOwningGI();
	return GI ? GI->GetEquippedHelmetId() : NAME_None;
}

// ── Callbacks internos ────────────────────────────────────────────────────────

void UTN_CosmeticsMenuWidget::OnUnequipClicked()
{
	RequestUnequip();
}

void UTN_CosmeticsMenuWidget::OnCloseClicked()
{
	CloseMenu();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

AMP_GamePlayerController* UTN_CosmeticsMenuWidget::GetOwningPC() const
{
	return Cast<AMP_GamePlayerController>(GetOwningPlayer());
}

UMP_GameInstance* UTN_CosmeticsMenuWidget::GetOwningGI() const
{
	return GetWorld() ? Cast<UMP_GameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

