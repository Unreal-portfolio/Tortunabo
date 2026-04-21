#include "World/TN_UmbrellaInteractable.h"
#include "Player/TortugaCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "UObject/ConstructorHelpers.h"

ATN_UmbrellaInteractable::ATN_UmbrellaInteractable()
{
	bReplicates = true;
	PromptText = FText::FromString(TEXT("Abrir sombrilla"));
	CooldownSeconds = ReuseDelaySecs;

	// ── Meshes por defecto ────────────────────────────────────────────────────
	// Cylinder = sombrilla plegada (mango fino vertical).
	// Cone     = sombrilla desplegada (cúpula abierta).
	// El BP hijo puede sobreescribir ambas con assets propios.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		MeshClosed = CylinderAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(
		TEXT("/Engine/BasicShapes/Cone"));
	if (ConeAsset.Succeeded())
	{
		MeshOpen = ConeAsset.Object;
	}

	// Aplicar estado inicial cerrado en el componente heredado.
	// Mesh se creó en TN_InteractableBase::ATN_InteractableBase().
	if (Mesh && MeshClosed)
	{
		Mesh->SetStaticMesh(MeshClosed);
	}
}

void ATN_UmbrellaInteractable::BeginPlay()
{
	Super::BeginPlay();

	// Garantizar estado cerrado en TODAS las máquinas al entrar en juego.
	// Importante para clientes que se unan tarde (JIP) o spawns en chunks.
	ApplyUmbrellaMesh(MeshClosed);
}

void ATN_UmbrellaInteractable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UmbrellaActiveTimerHandle);

	// Si destruido mientras la protección está activa, limpiar
	if (ATortugaCharacter* Char = ProtectedCharacter.Get())
	{
		Char->SetUmbrellaProtection(false);
	}

	Super::EndPlay(EndPlayReason);
}

void ATN_UmbrellaInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !Interactor) { return; }

	ATortugaCharacter* Char = Cast<ATortugaCharacter>(Interactor);
	if (!Char) { return; }

	// Actualizar cooldown de la base
	LastInteractionServerTime = GetWorld()->GetTimeSeconds();
	CooldownSeconds = ReuseDelaySecs;

	// Activar protección
	Char->SetUmbrellaProtection(true);
	ProtectedCharacter = Char;

	UE_LOG(LogTemp, Log, TEXT("[Umbrella] %s abrió sombrilla (%.1fs)"), *GetNameSafe(Char), UmbrellaDurationSeconds);

	MulticastOnUmbrellaOpened(Interactor);

	// Timer para cerrar la sombrilla — CreateUObject, no lambda
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &ATN_UmbrellaInteractable::HandleUmbrellaExpired);
	GetWorldTimerManager().SetTimer(UmbrellaActiveTimerHandle, Del, UmbrellaDurationSeconds, false);
}

void ATN_UmbrellaInteractable::HandleUmbrellaExpired()
{
	if (ATortugaCharacter* Char = ProtectedCharacter.Get())
	{
		Char->SetUmbrellaProtection(false);
	}
	ProtectedCharacter.Reset();
	MulticastOnUmbrellaClosed();
}

void ATN_UmbrellaInteractable::ApplyUmbrellaMesh(UStaticMesh* NewMesh)
{
	if (Mesh && NewMesh)
	{
		Mesh->SetStaticMesh(NewMesh);
	}
}

void ATN_UmbrellaInteractable::MulticastOnUmbrellaOpened_Implementation(APawn* User)
{
	// Swap al mesh abierto en todas las máquinas
	ApplyUmbrellaMesh(MeshOpen);

	if (SoundOpen)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoundOpen, GetActorLocation());
	}
	if (VFXOpen)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), VFXOpen, GetActorLocation());
	}
	OnUmbrellaOpened(User);
}

void ATN_UmbrellaInteractable::MulticastOnUmbrellaClosed_Implementation()
{
	// Volver al mesh cerrado en todas las máquinas
	ApplyUmbrellaMesh(MeshClosed);

	if (SoundClose)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoundClose, GetActorLocation());
	}
	if (VFXClose)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), VFXClose, GetActorLocation());
	}
	OnUmbrellaClosed();
}
