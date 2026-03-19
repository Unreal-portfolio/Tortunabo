#include "World/TN_InteractableBase.h"
#include "UI/HUD/TN_InteractPromptWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"

ATN_InteractableBase::ATN_InteractableBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	// Mesh es el root: UStaticMeshComponent es UPrimitiveComponent, lo que permite
	// que UWorld::FindTeleportSpot calcule bounds al spawnear el actor dinámicamente.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetIsReplicated(false);
	// ── Collision para el sistema de interacción por proximidad ───────────────
	// ObjectType = WorldDynamic → el OverlapMultiByObjectType del personaje lo detecta.
	// Pawn = Overlap (NO Block) → el jugador atraviesa el pickup sin chocar.
	// WorldStatic = Block → el objeto descansa sobre el suelo correctamente.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);      // traversable
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);     // sin bloquear cámara
	Mesh->SetGenerateOverlapEvents(true);

	PromptWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidgetComponent->SetupAttachment(Mesh);
	PromptWidgetComponent->SetDrawAtDesiredSize(true);
	PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	PromptWidgetComponent->SetVisibility(true);

	PromptText = FText::FromString(TEXT("Interactuar"));
}

void ATN_InteractableBase::BeginPlay()
{
	Super::BeginPlay();

	if (PromptWidgetClass)
	{
		PromptWidgetComponent->SetWidgetClass(PromptWidgetClass);
	}
	else
	{
		PromptWidgetComponent->SetWidgetClass(UTN_InteractPromptWidget::StaticClass());
	}

	if (UTN_InteractPromptWidget* PromptWidget = Cast<UTN_InteractPromptWidget>(PromptWidgetComponent->GetUserWidgetObject()))
	{
		PromptWidget->SetPromptText(PromptText);
	}

	ApplyInteractionEnabledState();
}

void ATN_InteractableBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_InteractableBase, bInteractionEnabled);
}

bool ATN_InteractableBase::CanInteract(APawn* Interactor) const
{
	return bInteractionEnabled && Interactor != nullptr;
}

void ATN_InteractableBase::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}

	OnInteracted(Interactor);
}

void ATN_InteractableBase::OnInteracted_Implementation(APawn* Interactor)
{
}

void ATN_InteractableBase::OnRep_InteractionEnabled()
{
	ApplyInteractionEnabledState();
}

void ATN_InteractableBase::ApplyInteractionEnabledState()
{
	SetActorEnableCollision(bInteractionEnabled);
	if (PromptWidgetComponent)
	{
		PromptWidgetComponent->SetVisibility(bInteractionEnabled);
	}
}

void ATN_InteractableBase::SetInteractionEnabled(bool bEnabled)
{
	if (bInteractionEnabled == bEnabled)
	{
		return;
	}

	bInteractionEnabled = bEnabled;
	ApplyInteractionEnabledState();
}


