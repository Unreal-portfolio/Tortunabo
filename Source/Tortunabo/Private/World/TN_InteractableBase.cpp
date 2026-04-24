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

	// ── Optimización de red: los interactuables cambian raramente ──────────
	NetDormancy = DORM_DormantAll;
	SetNetUpdateFrequency(4.f);
	SetMinNetUpdateFrequency(2.f);

	// ── SceneRoot: componente raíz invisible ─────────────────────────────────
	// Al ser la raíz, su posición es la posición REAL del actor en el mundo.
	// Mesh es hijo de SceneRoot: Mesh->SetRelativeLocation() solo mueve el visual
	// sin teletransportar el actor al origen.
	// BUG ANTERIOR: Mesh era root → SetRelativeLocation(FVector(0,0,HalfHeight))
	// movía el actor completo a (0,0,HalfHeight) al llamar InitializeFromInventoryItem.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetIsReplicated(false);
	// ObjectType = WorldDynamic → detectado por OverlapMultiByObjectType del personaje.
	// Pawn = Overlap → el jugador atraviesa el pickup sin chocar.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
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

	// Aplicar offset de suelo: evita que el mesh clipe con el suelo cuando el pivote
	// está en el centro. Se aplica solo si no se ha sobreescrito en InitializeFromInventoryItem.
	if (Mesh && !FMath::IsNearlyZero(MeshFloorOffset))
	{
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, MeshFloorOffset));
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

void ATN_InteractableBase::HideInteractableMesh()
{
	if (Mesh) { Mesh->SetVisibility(false); }
}

void ATN_InteractableBase::SetInteractionEnabled(bool bEnabled)
{
	if (bInteractionEnabled == bEnabled)
	{
		return;
	}

	bInteractionEnabled = bEnabled;
	ApplyInteractionEnabledState();

	// Despertar al actor dormido para que el cambio se replique inmediatamente
	FlushNetDormancy();
}


