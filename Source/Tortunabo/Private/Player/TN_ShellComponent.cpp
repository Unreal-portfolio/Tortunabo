#include "Player/TN_ShellComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/TN_InventoryComponent.h"
#include "Player/TN_ShellDecisions.h"
#include "Player/TN_StaminaComponent.h"
#include "Player/TortugaCharacter.h"

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

void UTN_ShellComponent::ForceEnterShell()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIsInShell)
	{
		return;
	}
	if (GetWorld())
	{
		ShellEnteredServerTime = GetWorld()->GetTimeSeconds();
	}
	SetShellState(true);
}

void UTN_ShellComponent::SetShellState(bool bInShell)
{
	if (bIsInShell == bInShell)
	{
		return;
	}

	bIsInShell = bInShell;

	// En un listen server OnRep no dispara en la máquina dueña de la variable,
	// así que aplicamos aquí y OnRep se encarga del resto de máquinas.
	ApplyShellState(bInShell);
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
