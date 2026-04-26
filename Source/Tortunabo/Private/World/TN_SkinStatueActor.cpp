#include "World/TN_SkinStatueActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_CosmeticsTypes.h"
#include "Player/TortugaCharacter.h"
#include "Player/MP_GamePlayerController.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Engine/DataTable.h"

ATN_SkinStatueActor::ATN_SkinStatueActor()
{
	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(SceneRoot);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetIsReplicated(false);

	// SombreroSocket: punto de anclaje del casco — posiciónalo sobre la cabeza en el BP.
	SombreroSocket = CreateDefaultSubobject<USceneComponent>(TEXT("Sombrero"));
	SombreroSocket->SetupAttachment(PreviewMesh);

	HelmetPreviewComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HelmetPreviewMesh"));
	HelmetPreviewComp->SetupAttachment(SombreroSocket);
	HelmetPreviewComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HelmetPreviewComp->SetIsReplicated(false);
	HelmetPreviewComp->SetHiddenInGame(true);

	PromptText = FText::FromString(TEXT("Equipar cosmético"));
	CooldownSeconds = 0.5f;
}

void ATN_SkinStatueActor::BeginPlay()
{
	Super::BeginPlay();

	// Statues tipo Helmet no asignan SkeletalMesh al PreviewMesh → el componente
	// spamea "GetSocketByName(None): No SkeletalMesh". Si está vacío, deshabilitamos
	// tick y lo ocultamos — nadie lo usa, pero sin esto el warning llena el log.
	if (PreviewMesh && !PreviewMesh->GetSkeletalMeshAsset())
	{
		PreviewMesh->SetComponentTickEnabled(false);
		PreviewMesh->SetVisibility(false);
	}

	ApplyPreviewCosmetic();
}

void ATN_SkinStatueActor::ApplyPreviewCosmetic()
{
	if (CosmeticId == NAME_None)
	{
		return;
	}

	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	switch (CosmeticType)
	{
		case ETNCosmeticType::Skin:
		{
			const UDataTable* SkinDT = GI->GetSkinDataTable();
			if (!SkinDT) { break; }

			const FTN_SkinData* Row = SkinDT->FindRow<FTN_SkinData>(CosmeticId, TEXT("StatuePreview"));
			if (!Row) { break; }

			// Mapeo de la estatua (componentes legacy con nombres) hacia los slots
			// del nuevo sistema de 5 materiales:
			//   "Body"             → ShellMaterial (caparazón)
			//   "Body1"            → BellyMaterial (panza, fallback Shell)
			//   "Mesh1..5","Mesh13"→ SkinMaterial (cabeza/patas/extremidades)
			// Los slots EyeShineMaterial y EyesMouthMaterial son específicos del
			// SkM unificado del personaje y no tienen target en la estatua vieja.
			static const TArray<FName> ShellNames = { TEXT("Body") };
			static const TArray<FName> BellyNames = { TEXT("Body1") };
			static const TArray<FName> SkinNames = {
				TEXT("Mesh1"), TEXT("Mesh2"), TEXT("Mesh3"),
				TEXT("Mesh4"), TEXT("Mesh5"), TEXT("Mesh13")
			};

			// Iterar todos los StaticMeshComponents hijos de la estatua y aplicar
			// el material correcto según el nombre del componente.
			TArray<UActorComponent*> AllComps;
			GetComponents(AllComps);
			for (UActorComponent* ActComp : AllComps)
			{
				UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(ActComp);
				if (!SMC || SMC == HelmetPreviewComp) { continue; }

				const FName CompName = SMC->GetFName();

				UMaterialInterface* MatToApply = nullptr;
				if (ShellNames.Contains(CompName) && Row->ShellMaterial)
				{
					MatToApply = Row->ShellMaterial;
				}
				else if (BellyNames.Contains(CompName))
				{
					MatToApply = Row->BellyMaterial ? Row->BellyMaterial : Row->ShellMaterial;
				}
				else if (SkinNames.Contains(CompName) && Row->SkinMaterial)
				{
					MatToApply = Row->SkinMaterial;
				}

				if (!MatToApply) { continue; }

				const int32 N = SMC->GetNumMaterials();
				for (int32 i = 0; i < N; ++i)
				{
					SMC->SetMaterial(i, MatToApply);
				}
			}
			break;
		}

		case ETNCosmeticType::Helmet:
		{
			const UDataTable* HelmDT = GI->GetHelmetDataTable();
			if (!HelmDT) { break; }

			const FTN_HelmetData* Row = HelmDT->FindRow<FTN_HelmetData>(CosmeticId, TEXT("StatuePreview"));
			if (!Row || !Row->DisplayMesh) { break; }

			HelmetPreviewComp->SetStaticMesh(Row->DisplayMesh);
			HelmetPreviewComp->SetRelativeScale3D(Row->MeshScale.IsNearlyZero() ? FVector::OneVector : Row->MeshScale);
			HelmetPreviewComp->SetRelativeLocation(Row->MeshOffset);
			HelmetPreviewComp->SetRelativeRotation(Row->MeshRotation);
			HelmetPreviewComp->SetHiddenInGame(false);
			break;
		}
	}
}

void ATN_SkinStatueActor::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor) || !Interactor)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Interactor->GetController());
	ATN_CoopPlayerState* TNPS = PC ? PC->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
	ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(Interactor);
	AMP_GamePlayerController* TNPC = Cast<AMP_GamePlayerController>(PC);

	if (!TNPS)
	{
		return;
	}

	switch (CosmeticType)
	{
		case ETNCosmeticType::Skin:
		{
			const FName NewSkin = (TNPS->EquippedSkinId == CosmeticId) ? NAME_None : CosmeticId;
			TNPS->EquippedSkinId = NewSkin;
			TNPS->ForceNetUpdate();

			if (TurtleChar) { TurtleChar->UpdateSkinVisual(NewSkin); }
			if (TNPC)       { TNPC->NotifySkinEquipped(NewSkin); }

			UE_LOG(LogTemp, Log, TEXT("[CosmeticStatue] SKIN '%s' → '%s'"),
				*GetNameSafe(Interactor),
				NewSkin == NAME_None ? TEXT("(ninguno)") : *NewSkin.ToString());
			break;
		}

		case ETNCosmeticType::Helmet:
		{
			const FName NewHelmet = (TNPS->EquippedHelmetId == CosmeticId) ? NAME_None : CosmeticId;
			TNPS->EquippedHelmetId = NewHelmet;
			TNPS->ForceNetUpdate();

			if (TurtleChar) { TurtleChar->UpdateHelmetMesh(NewHelmet); }
			if (TNPC)       { TNPC->NotifyHelmetEquipped(NewHelmet); }

			UE_LOG(LogTemp, Log, TEXT("[CosmeticStatue] HELMET '%s' → '%s'"),
				*GetNameSafe(Interactor),
				NewHelmet == NAME_None ? TEXT("(ninguno)") : *NewHelmet.ToString());
			break;
		}
	}

	Super::Interact(Interactor);
}
