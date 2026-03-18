#include "World/TN_StaminaBoostPickup.h"

ATN_StaminaBoostPickup::ATN_StaminaBoostPickup()
{
	PromptText = FText::FromString(TEXT("Recoger boost de stamina"));
	PickupItem.ItemId = TEXT("Item_StaminaBoost");
	PickupItem.UseType = ETN_ItemUseType::SelfStaminaBoost;
	PickupItem.StaminaUnlimitedDurationSeconds = 4.0f;
}

