#include "Player/TN_ShellComponent.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/TN_CarryComponent.h"
#include "Player/TN_InventoryComponent.h"
#include "Player/TN_ShellBody.h"
#include "Player/TN_ShellDecisions.h"
#include "Player/TN_StaminaComponent.h"
#include "Player/TortugaCharacter.h"

namespace TNShellComponentDetail
{
	bool IsCarried(const ATortugaCharacter* Turtle)
	{
		const UTN_CarryComponent* Carry = Turtle ? Turtle->GetCarryComponent() : nullptr;
		return Carry && Carry->IsBeingCarried();
	}
}

UTN_ShellComponent::UTN_ShellComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTN_ShellComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Sin condición: el resto de máquinas necesita el estado para el visual, y un
	// jugador que entra a mitad de partida lo recibe en el bunch inicial.
	DOREPLIFETIME(UTN_ShellComponent, bIsInShell);
	DOREPLIFETIME(UTN_ShellComponent, Body);
}

void UTN_ShellComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// La tortuga se va (jugador que sale, viaje de mapa): su caja no se queda suelta.
	if (ATN_ShellBody* OldBody = Body)
	{
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			OldBody->MarkReleased();
			OldBody->Destroy();
		}
	}
	Body = nullptr;
	LocalBody = nullptr;
	Super::EndPlay(EndPlayReason);
}

ATortugaCharacter* UTN_ShellComponent::GetTurtleOwner() const
{
	return Cast<ATortugaCharacter>(GetOwner());
}

void UTN_ShellComponent::RequestToggleShell()
{
	if (!GetOwner())
	{
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		ServerToggleShell();
		return;
	}

	ServerToggleShell_Implementation();
}

void UTN_ShellComponent::ServerToggleShell_Implementation()
{
	ATortugaCharacter* Turtle = GetTurtleOwner();
	if (!Turtle || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	if (bIsInShell)
	{
		if (bExitLocked || !TNShellLogic::CanExitShell(Now - ShellEnteredServerTime, MinTimeInShellSeconds))
		{
			return;
		}

		SetShellState(false);
		return;
	}

	const UCharacterMovementComponent* Movement = Turtle->GetCharacterMovement();
	const UTN_InventoryComponent* Inventory = Turtle->GetInventoryComponent();

	TNShellLogic::FShellEnterContext Context;
	Context.bOnGround = Movement && Movement->IsMovingOnGround();
	Context.bIsDead = Turtle->IsDead();
	Context.bIsKnockedDown = Turtle->IsKnockedDown();
	Context.bIsDiving = Turtle->IsDiving();
	Context.bHasEquippedItem = Inventory && Inventory->HasEquippedItem();

	if (!TNShellLogic::CanEnterShell(Context))
	{
		return;
	}

	ShellEnteredServerTime = Now;
	SetShellState(true);
}

void UTN_ShellComponent::ForceExitShell()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bExitLocked = false;
	if (!bIsInShell)
	{
		return;
	}

	// Sin comprobar permanencia mínima: esto lo llaman muerte y derribo, donde
	// dejar el caparazón puesto significaría dejar también el speed cap pegado.
	SetShellState(false);
}

void UTN_ShellComponent::ForceEnterShell(bool bPhysics, bool bExitOnRest)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIsInShell)
	{
		return;
	}
	if (GetWorld())
	{
		ShellEnteredServerTime = GetWorld()->GetTimeSeconds();
	}
	SetShellState(true, bPhysics, bExitOnRest);
}

void UTN_ShellComponent::SetShellState(bool bInShell, bool bPhysics, bool bExitOnRest)
{
	if (bIsInShell == bInShell)
	{
		return;
	}

	bIsInShell = bInShell;

	// En un listen server OnRep no dispara en la máquina dueña de la variable,
	// así que aplicamos aquí y OnRep se encarga del resto de máquinas.
	ApplyShellState(bInShell);

	// Física propia (solo servidor; la caja y Body se replican solos).
	ATortugaCharacter* Turtle = GetTurtleOwner();
	if (bInShell)
	{
		if (bPhysics && Turtle && !TNShellComponentDetail::IsCarried(Turtle))
		{
			StartBody(Turtle->GetVelocity(), false, bExitOnRest);
		}
	}
	else
	{
		StopBody();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Caparazón con física propia
// ─────────────────────────────────────────────────────────────────────────────

void UTN_ShellComponent::StartBody(const FVector& Velocity, bool bLaunched, bool bExitOnRest)
{
	ATortugaCharacter* Turtle = GetTurtleOwner();
	UWorld* World = GetWorld();
	if (!Turtle || !World || !Turtle->HasAuthority() || !bIsInShell || TNShellComponentDetail::IsCarried(Turtle))
	{
		return;
	}
	// Una caja cada vez.
	if (Body)
	{
		StopBody();
	}

	const FVector Location = Turtle->GetActorLocation();
	const bool bFast = Velocity.SizeSquared2D() > FMath::Square(150.0);
	const float Yaw = (bLaunched && bFast) ? static_cast<float>(Velocity.Rotation().Yaw) : static_cast<float>(Turtle->GetActorRotation().Yaw);

	// A mano: la caja nace de pie en el tronco (su +X, la cabeza, hacia arriba) y se vuelca hacia delante sobre la
	// tripa. Lanzada o soltada: nace tumbada donde está el actor.
	const FRotator Rotation = bLaunched ? FRotator(0.f, Yaw, 0.f) : FRotator(90.f, Yaw, 0.f);
	const FVector Center = bLaunched ? Location : Location - FVector(0.0, 0.0, 2.5);

	FActorSpawnParameters Params;
	Params.Owner = Turtle;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATN_ShellBody* NewBody = World->SpawnActor<ATN_ShellBody>(ATN_ShellBody::StaticClass(), FTransform(Rotation, Center), Params);
	if (!NewBody || !NewBody->GetBox())
	{
		return;
	}
	NewBody->InitBody(Turtle, bExitOnRest);

	// Antes del primer paso de física: la cápsula deja de chocar para que la caja no nazca empujada por ella.
	Body = NewBody;
	AdoptBody(NewBody);

	UBoxComponent* BoxComp = NewBody->GetBox();
	const FVector Right = FRotator(0.f, Yaw, 0.f).RotateVector(FVector::RightVector);
	BoxComp->SetPhysicsLinearVelocity(Velocity);
	if (!bLaunched)
	{
		BoxComp->SetPhysicsAngularVelocityInRadians(Right * 3.0);
	}
	else if (bFast)
	{
		BoxComp->SetPhysicsAngularVelocityInRadians(Right * 7.0);
	}
	Turtle->ForceNetUpdate();
}

void UTN_ShellComponent::StopBody()
{
	ATortugaCharacter* Turtle = GetTurtleOwner();
	if (!Turtle || !Turtle->HasAuthority())
	{
		return;
	}
	ATN_ShellBody* OldBody = Body;
	if (!OldBody && !bBodyLocalApplied)
	{
		return;
	}

	if (!TNShellComponentDetail::IsCarried(Turtle))
	{
		if (OldBody && OldBody->GetBox())
		{
			const UBoxComponent* BoxComp = OldBody->GetBox();
			PlaceStandingFromBox(BoxComp->GetComponentTransform(), ATN_ShellBody::IsInWater(*BoxComp), OldBody);
		}
		else if (bHasLastBox)
		{
			PlaceStandingFromBox(LastBoxTransform, false, nullptr);
		}
	}

	Body = nullptr;
	LocalBody = nullptr;
	ApplyBodyLocalState(false);
	if (OldBody)
	{
		OldBody->MarkReleased();
		OldBody->Destroy();
	}
	Turtle->ForceNetUpdate();
}

void UTN_ShellComponent::OnRep_Body()
{
	if (Body)
	{
		AdoptBody(Body);
	}
	else
	{
		ReleaseLocalBody();
	}
}

void UTN_ShellComponent::AdoptBody(ATN_ShellBody* InBody)
{
	if (!InBody || InBody->GetTurtle() != GetTurtleOwner())
	{
		return;
	}
	// En los clientes solo la caja que el servidor dice que es la actual (o la que llega antes que la referencia).
	if (GetOwner() && !GetOwner()->HasAuthority() && Body && Body != InBody)
	{
		return;
	}
	if (TNShellComponentDetail::IsCarried(GetTurtleOwner()))
	{
		return;
	}
	LocalBody = InBody;
	bHasLastBox = false;
	ApplyBodyLocalState(true);
}

void UTN_ShellComponent::DropLocalBody()
{
	if (!bBodyLocalApplied)
	{
		return;
	}
	LocalBody = nullptr;
	ApplyBodyLocalState(false);
}

void UTN_ShellComponent::ReleaseLocalBody()
{
	if (!bBodyLocalApplied)
	{
		LocalBody = nullptr;
		return;
	}
	ATN_ShellBody* OldBody = LocalBody.Get();
	if (!TNShellComponentDetail::IsCarried(GetTurtleOwner()))
	{
		if (OldBody && OldBody->GetBox())
		{
			const UBoxComponent* BoxComp = OldBody->GetBox();
			PlaceStandingFromBox(BoxComp->GetComponentTransform(), ATN_ShellBody::IsInWater(*BoxComp), OldBody);
		}
		else if (bHasLastBox)
		{
			PlaceStandingFromBox(LastBoxTransform, false, nullptr);
		}
	}
	LocalBody = nullptr;
	ApplyBodyLocalState(false);
}

void UTN_ShellComponent::FollowBody(ATN_ShellBody* InBody)
{
	ATortugaCharacter* Turtle = GetTurtleOwner();
	if (!Turtle || !InBody || !bBodyLocalApplied || LocalBody.Get() != InBody || !InBody->GetBox())
	{
		return;
	}
	// Si ya la han cogido (el enganche al portador llega antes que la caja se destruya), no se toca.
	if (TNShellComponentDetail::IsCarried(Turtle))
	{
		return;
	}
	LastBoxTransform = InBody->GetBox()->GetComponentTransform();
	bHasLastBox = true;
	Turtle->PlaceOnShellBody(LastBoxTransform);
}

void UTN_ShellComponent::NotifyBodyAtRest()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	// ForceExitShell desbloquea la salida y, vía SetShellState, pone a la tortuga de pie donde quedó la caja.
	ForceExitShell();
}

void UTN_ShellComponent::NotifyBodyInWater()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	ForceExitShell();
}

void UTN_ShellComponent::NotifyBodyEnded(ATN_ShellBody* InBody)
{
	if (!InBody)
	{
		return;
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (Body != InBody)
		{
			return;
		}
		Body = nullptr;
		ReleaseLocalBody();
		ForceExitShell();
		return;
	}
	// Cliente: la caja enganchada se ha ido antes que la réplica de Body.
	if (LocalBody.Get() == InBody)
	{
		ReleaseLocalBody();
	}
}

void UTN_ShellComponent::PlaceStandingFromBox(const FTransform& BoxWorld, bool bInWater, const AActor* IgnoreActor)
{
	ATortugaCharacter* Turtle = GetTurtleOwner();
	UWorld* World = GetWorld();
	if (!Turtle || !World)
	{
		return;
	}
	const UCapsuleComponent* Capsule = Turtle->GetCapsuleComponent();
	const double HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 70.0;
	const FVector Center = BoxWorld.GetLocation();

	// Mira hacia donde apunta la cabeza (+X de la caja); si la caja está de pie, hacia donde mira la tripa (-Z).
	FVector Heading = BoxWorld.GetUnitAxis(EAxis::X);
	Heading.Z = 0.0;
	if (Heading.SizeSquared() < 0.04)
	{
		Heading = -BoxWorld.GetUnitAxis(EAxis::Z);
		Heading.Z = 0.0;
	}
	const float Yaw = Heading.IsNearlyZero() ? static_cast<float>(Turtle->GetActorRotation().Yaw) : static_cast<float>(Heading.Rotation().Yaw);

	// De pie sobre el suelo que hay debajo del caparazón; en el agua, en su centro (el movimiento pasa a nadar).
	FVector Stand = bInWater ? Center : Center + FVector(0.0, 0.0, HalfHeight - ATN_ShellBody::BoxHalfExtent().Z);
	if (!bInWater)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(TNShellStandUp), false, Turtle);
		if (IgnoreActor)
		{
			Query.AddIgnoredActor(IgnoreActor);
		}
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Center + FVector(0.0, 0.0, 40.0), Center - FVector(0.0, 0.0, 140.0), ECC_WorldStatic, Query))
		{
			Stand = Hit.ImpactPoint + FVector(0.0, 0.0, HalfHeight + 2.0);
		}
	}
	Turtle->SetActorLocationAndRotation(Stand, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
}

void UTN_ShellComponent::ApplyBodyLocalState(bool bOn)
{
	ATortugaCharacter* Turtle = GetTurtleOwner();
	if (!Turtle || bBodyLocalApplied == bOn)
	{
		return;
	}
	bBodyLocalApplied = bOn;
	UCharacterMovementComponent* Move = Turtle->GetCharacterMovement();
	UCapsuleComponent* Capsule = Turtle->GetCapsuleComponent();

	if (bOn)
	{
		if (Move)
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
			SavedSmoothingMode = static_cast<uint8>(Move->NetworkSmoothingMode);
			Move->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
			Move->SetComponentTickEnabled(false);
		}
		if (Capsule)
		{
			// La cápsula ya no choca con nada (el cuerpo es la caja), pero sigue solapando: zonas, agua y disparadores
			// la siguen viendo.
			SavedCapsuleProfile = Capsule->GetCollisionProfileName();
			SavedCapsuleEnabled = Capsule->GetCollisionEnabled();
			SavedCapsuleResponses = Capsule->GetCollisionResponseToChannels();
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Capsule->SetCollisionResponseToAllChannels(ECR_Overlap);
			Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
		// Cada máquina sigue localmente a la caja, que ya se replica sola.
		if (Turtle->HasAuthority())
		{
			Turtle->SetReplicateMovement(false);
		}
		return;
	}

	if (Capsule)
	{
		if (SavedCapsuleProfile != NAME_None && SavedCapsuleProfile != UCollisionProfile::CustomCollisionProfileName)
		{
			Capsule->SetCollisionProfileName(SavedCapsuleProfile);
		}
		else
		{
			Capsule->SetCollisionEnabled(SavedCapsuleEnabled);
			Capsule->SetCollisionResponseToChannels(SavedCapsuleResponses);
		}
	}
	Turtle->ResetMeshTransform();
	if (Move)
	{
		Move->NetworkSmoothingMode = static_cast<ENetworkSmoothingMode>(SavedSmoothingMode);
		Move->SetComponentTickEnabled(true);
		if (!TNShellComponentDetail::IsCarried(Turtle))
		{
			Move->SetMovementMode(MOVE_Falling);
		}
	}
	if (Turtle->HasAuthority())
	{
		Turtle->SetReplicateMovement(true);
	}
}

void UTN_ShellComponent::OnRep_IsInShell()
{
	ApplyShellState(bIsInShell);
}

void UTN_ShellComponent::ApplyShellState(bool bInShell)
{
	ATortugaCharacter* Turtle = GetTurtleOwner();
	if (!Turtle)
	{
		return;
	}

	if (UTN_StaminaComponent* Stamina = Turtle->GetStaminaComponent())
	{
		if (bInShell)
		{
			// El freno entra por el speed cap del componente de stamina, único punto
			// del proyecto que escribe MaxWalkSpeed. Cap 0 = el personaje no se desplaza.
			Stamina->SetSprintRequested(false);
			Stamina->SetSpeedCap(0.f);
		}
		else
		{
			Stamina->ClearSpeedCap();
		}
	}

	// El personaje se encarga de lo suyo (cancelar emote, visual del caparazón):
	// esta función corre en todas las máquinas, así que el reparto es el mismo.
	Turtle->OnShellStateChanged(bInShell);

	// Sonido local en cada máquina — no multicast: ApplyShellState ya se ejecuta
	// en todas, y un multicast encima duplicaría el disparo.
	if (USoundBase* Sound = bInShell ? EnterShellSound : ExitShellSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(Turtle, Sound, Turtle->GetActorLocation());
	}
}
