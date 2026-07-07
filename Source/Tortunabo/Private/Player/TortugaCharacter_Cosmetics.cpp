// ─────────────────────────────────────────────────────────────────────────────
// TortugaCharacter — Cosméticos (casco y skin).
//
// Definiciones extraídas de TortugaCharacter.cpp para mejorar la legibilidad.
// Misma clase ATortugaCharacter en otra unidad de traducción: sin cambios de
// lógica ni de replicación, solo organización.
// ─────────────────────────────────────────────────────────────────────────────

#include "Player/TortugaCharacter.h"
#include "Core/TN_Log.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/DataTable.h"
#include "Core/TN_CosmeticsTypes.h"
#include "Multiplayer/MP_GameInstance.h"

// ── Cosmetics ─────────────────────────────────────────────────────────────────

void ATortugaCharacter::UpdateHelmetMesh(FName HelmetId)
{
	if (!HelmetMeshComp)
	{
		return;
	}

	// NAME_None = desequipar
	if (HelmetId == NAME_None)
	{
		HelmetMeshComp->SetStaticMesh(nullptr);
		HelmetMeshComp->SetHiddenInGame(true);
		return;
	}

	// Obtener DataTable desde GameInstance
	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[TortugaCharacter] UpdateHelmetMesh: GameInstance no es UMP_GameInstance."));
		return;
	}

	const UDataTable* HelmDT = GI->GetHelmetDataTable();
	if (!HelmDT)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[TortugaCharacter] UpdateHelmetMesh: HelmetDataTable no asignado en BP_GameInstance."));
		return;
	}

	const FTN_HelmetData* Row = HelmDT->FindRow<FTN_HelmetData>(HelmetId, TEXT("UpdateHelmetMesh"));
	if (!Row)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[TortugaCharacter] UpdateHelmetMesh: HelmetId '%s' no encontrado en DT_Helmets."), *HelmetId.ToString());
		HelmetMeshComp->SetStaticMesh(nullptr);
		HelmetMeshComp->SetHiddenInGame(true);
		return;
	}

	if (!Row->DisplayMesh)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[TortugaCharacter] UpdateHelmetMesh: HelmetId '%s' sin DisplayMesh asignado."), *HelmetId.ToString());
		HelmetMeshComp->SetHiddenInGame(true);
		return;
	}

	HelmetMeshComp->SetStaticMesh(Row->DisplayMesh);
	HelmetMeshComp->SetRelativeScale3D(Row->MeshScale.IsNearlyZero() ? FVector::OneVector : Row->MeshScale);
	HelmetMeshComp->SetRelativeLocation(Row->MeshOffset);
	HelmetMeshComp->SetRelativeRotation(Row->MeshRotation);
	HelmetMeshComp->SetHiddenInGame(false);

	UE_LOG(LogTortunabo, Log, TEXT("[TortugaCharacter] '%s' equipa casco '%s'."), *GetName(), *HelmetId.ToString());
}

void ATortugaCharacter::UpdateSkinVisual(FName SkinId)
{
	USkeletalMeshComponent* SKM = GetMesh();
	if (!SKM) { return; }

	// Sin skin equipado → restaurar materiales originales cacheados en BeginPlay.
	if (SkinId == NAME_None)
	{
		for (int32 i = 0; i < DefaultSkelMeshMaterials.Num(); ++i)
		{
			SKM->SetMaterial(i, DefaultSkelMeshMaterials[i]);
		}
		return;
	}

	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	const UDataTable* SkinDT = GI ? GI->GetSkinDataTable() : nullptr;
	if (!SkinDT)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[TortugaCharacter] UpdateSkinVisual: SkinDataTable no asignado en BP_GameInstance."));
		return;
	}

	const FTN_SkinData* Row = SkinDT->FindRow<FTN_SkinData>(SkinId, TEXT("UpdateSkinVisual"));
	if (!Row)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[TortugaCharacter] UpdateSkinVisual: SkinId '%s' no encontrado en DT_Skins."), *SkinId.ToString());
		return;
	}

	// Validar que el SkM unificado tiene los 5 slots esperados (Belly/EyeShine/
	// EyesMouth/Skin/Shell). Si tiene menos, SetMaterial(3/4,...) crea
	// OverrideMaterials fantasma sin warning y la skin parece aplicada pero no
	// se ve cambio en esos slots.
	const int32 NumMaterials = SKM->GetNumMaterials();
	if (NumMaterials < 5)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[SKIN-CONFIG] '%s' SkM tiene solo %d slots (esperados 5: Belly/EyeShine/EyesMouth/Skin/Shell). Reordenar slots o reimportar mesh."),
			*GetName(), NumMaterials);
	}

	// Mapeo de slots del SkM unificado:
	//   Slot 0 → Barriga                         (BellyMaterial)
	//   Slot 1 → Brillo de los ojos              (EyeShineMaterial)
	//   Slot 2 → Ojos y boca                     (EyesMouthMaterial)
	//   Slot 3 → Cabeza, patas, brazos, cola     (SkinMaterial)
	//   Slot 4 → Caparazón principal             (ShellMaterial)
	//
	// Si la skin no aporta material para un slot, se mantiene el material por
	// defecto cacheado en BeginPlay → permite skins parciales (solo caparazón,
	// solo ojos, etc.) sin tener que repetir todos los materiales.
	auto AssignSlot = [&](int32 SlotIdx, UMaterialInterface* Mat)
	{
		if (Mat)
		{
			SKM->SetMaterial(SlotIdx, Mat);
		}
		else if (DefaultSkelMeshMaterials.IsValidIndex(SlotIdx))
		{
			SKM->SetMaterial(SlotIdx, DefaultSkelMeshMaterials[SlotIdx]);
		}
	};

	AssignSlot(0, Row->BellyMaterial);
	AssignSlot(1, Row->EyeShineMaterial);
	AssignSlot(2, Row->EyesMouthMaterial);
	AssignSlot(3, Row->SkinMaterial);
	AssignSlot(4, Row->ShellMaterial);

	// Forzar refresh del render state. Sin esto, SetMaterial puede no actualizar
	// visualmente con el SKM unificado en algunas configuraciones (overrideMaterials cache).
	SKM->MarkRenderStateDirty();

	UE_LOG(LogTortunabo, Log, TEXT("[SKIN-DEBUG] '%s' skin '%s' aplicado · slots=[0:%s 1:%s 2:%s 3:%s 4:%s]"),
		*GetName(), *SkinId.ToString(),
		*GetNameSafe(Row->BellyMaterial),
		*GetNameSafe(Row->EyeShineMaterial),
		*GetNameSafe(Row->EyesMouthMaterial),
		*GetNameSafe(Row->SkinMaterial),
		*GetNameSafe(Row->ShellMaterial));
}

