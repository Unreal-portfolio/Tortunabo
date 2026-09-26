// ─────────────────────────────────────────────────────────────────────────────
// TortugaCharacter — Cosméticos (casco, caparazón y color).
//
// El PlayerState replica los tres por separado (EquippedHelmetId, EquippedShellId, EquippedSkinId) y cada OnRep llama
// aquí; el personaje guarda el conjunto en CosmeticLook y lo aplica entero con UTN_CosmeticLook, lo mismo que usan el
// tendero y las vistas previas de la tienda y el probador.
// ─────────────────────────────────────────────────────────────────────────────

#include "Player/TortugaCharacter.h"
#include "Core/TN_Log.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TN_CosmeticLook.h"
#include "Core/TN_CosmeticsTypes.h"
#include "Core/TN_CoopPlayerState.h"

// ── Cosmetics ─────────────────────────────────────────────────────────────────

void ATortugaCharacter::UpdateHelmetMesh(FName HelmetId)
{
	CosmeticLook.HelmetId = HelmetId;
	RefreshCosmeticLook();
	UE_LOG(LogTortunabo, Log, TEXT("[TortugaCharacter] '%s' casco '%s'."), *GetName(), *HelmetId.ToString());
}

void ATortugaCharacter::UpdateSkinVisual(FName SkinId)
{
	CosmeticLook.SkinId = SkinId;
	if (const ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		CosmeticLook.ShellId = TNPS->EquippedShellId;
	}
	RefreshCosmeticLook();
	UE_LOG(LogTortunabo, Log, TEXT("[TortugaCharacter] '%s' color '%s', caparazón '%s'."), *GetName(), *SkinId.ToString(), *CosmeticLook.ShellId.ToString());
}

void ATortugaCharacter::ApplyShellSlot()
{
	if (const ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		CosmeticLook.ShellId = TNPS->EquippedShellId;
	}
	RefreshCosmeticLook();
}

void ATortugaCharacter::RefreshCosmeticLook()
{
	UTN_CosmeticLook::ApplyLook(this, GetMesh(), HelmetMeshComp, CosmeticLook, DefaultSkelMeshMaterials);
}

bool ATortugaCharacter::ApplyCosmeticsFromPlayerState()
{
	if (const ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		CosmeticLook.HelmetId = TNPS->EquippedHelmetId;
		CosmeticLook.SkinId = TNPS->EquippedSkinId;
		CosmeticLook.ShellId = TNPS->EquippedShellId;
		RefreshCosmeticLook();
		return true;
	}
	return false;
}
