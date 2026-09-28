#include "World/Beach/TN_BeachStun.h"
#include "World/Beach/TN_BeachStunComponent.h"
#include "World/Beach/TN_BeachSandWorm.h"
#include "Game/TN_BeachRaceGameState.h"
#include "Core/TN_Log.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_CarryComponent.h"
#include "Player/TN_DizzyBirdsComponent.h"
#include "Player/TN_ShellBody.h"
#include "Player/TN_ShellComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace TNBeachStunDetail
{
	/** Reloj del servidor en esta máquina (en el servidor, su propio tiempo de mundo). */
	float ServerNow(const UWorld* World)
	{
		if (!World)
		{
			return 0.f;
		}
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return static_cast<float>(GameState->GetServerWorldTimeSeconds());
		}
		return World->GetTimeSeconds();
	}

	bool IsCarried(const ATortugaCharacter* Turtle)
	{
		const UTN_CarryComponent* Carry = Turtle ? Turtle->GetCarryComponent() : nullptr;
		return Carry && Carry->GetCarrier() != nullptr;
	}

	/** Últimos segundos en los que el temblor se apaga poco a poco. */
	constexpr float TrembleFadeSeconds = 0.6f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Funciones del contrato (TN_BeachStun.h)
// ─────────────────────────────────────────────────────────────────────────────

void TNBeach::StunTurtle(ACharacter* Turtle, float Seconds, const FVector& Launch)
{
	// En la boca de un gusano de arena (al acabar la cuenta atrás) ya no le pasa nada más hasta la ronda siguiente.
	if (!Turtle || Seconds <= 0.f || !Turtle->HasAuthority() || Turtle->IsActorBeingDestroyed() || ATN_BeachSandWorm::IsBeingEaten(Turtle))
	{
		return;
	}
	if (const ATortugaCharacter* TurtleCharacter = Cast<ATortugaCharacter>(Turtle))
	{
		if (TurtleCharacter->IsDead())
		{
			return;
		}
	}
	if (UTN_BeachStunComponent* Stun = UTN_BeachStunComponent::FindOrAddOn(Turtle))
	{
		Stun->StartStun(Seconds, Launch);
	}
}

void TNBeach::KnockDownTurtle(ACharacter* Turtle, float Seconds, const FVector& Impulse)
{
	if (!Turtle || Seconds <= 0.f || !Turtle->HasAuthority() || Turtle->IsActorBeingDestroyed() || ATN_BeachSandWorm::IsBeingEaten(Turtle))
	{
		return;
	}
	ATortugaCharacter* TurtleCharacter = Cast<ATortugaCharacter>(Turtle);
	if (!TurtleCharacter || TurtleCharacter->IsDead())
	{
		return;
	}
	// El derribo de la piel de plátano: ragdoll, mareo y levantarse al acabar.
	TurtleCharacter->ApplyKnockdown(Seconds, Impulse);
}

bool TNBeach::IsTurtleStunned(const ACharacter* Turtle)
{
	const UTN_BeachStunComponent* Stun = UTN_BeachStunComponent::FindOn(Turtle);
	return Stun && Stun->IsStunned();
}

bool TNBeach::IsNoDeathWorld(const UObject* WorldContext)
{
	const UWorld* World = WorldContext && GEngine
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	// El GameState de la carrera en la playa solo lo pone ATN_BeachRaceGameMode y viaja a todas las máquinas.
	return World && Cast<ATN_BeachRaceGameState>(World->GetGameState()) != nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// UTN_BeachStunComponent
// ─────────────────────────────────────────────────────────────────────────────

UTN_BeachStunComponent::UTN_BeachStunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Después de la física: la caja del caparazón coloca la malla en TG_PostPhysics y el temblor va encima.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

void UTN_BeachStunComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTN_BeachStunComponent, bStunned);
	DOREPLIFETIME(UTN_BeachStunComponent, StunEndServerTime);
}

UTN_BeachStunComponent* UTN_BeachStunComponent::FindOn(const AActor* Turtle)
{
	return Turtle ? Turtle->FindComponentByClass<UTN_BeachStunComponent>() : nullptr;
}

UTN_BeachStunComponent* UTN_BeachStunComponent::FindOrAddOn(ACharacter* Turtle)
{
	if (UTN_BeachStunComponent* Existing = FindOn(Turtle))
	{
		return Existing;
	}
	if (!Turtle || !Turtle->HasAuthority())
	{
		return nullptr;
	}
	// Componente dinámico replicado: el servidor lo crea y cada cliente recibe el suyo (OnCreatedFromReplication).
	UTN_BeachStunComponent* Stun = NewObject<UTN_BeachStunComponent>(Turtle, UTN_BeachStunComponent::StaticClass(), TEXT("BeachStun"));
	if (!Stun)
	{
		return nullptr;
	}
	Turtle->AddInstanceComponent(Stun);
	Stun->RegisterComponent();
	return Stun;
}

void UTN_BeachStunComponent::StartStun(float Seconds, const FVector& Launch)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Owner->HasAuthority() || Seconds <= 0.f)
	{
		return;
	}

	const float NewEnd = World->GetTimeSeconds() + Seconds;
	StunEndServerTime = bStunned ? FMath::Max(StunEndServerTime, NewEnd) : NewEnd;

	if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Owner))
	{
		UTN_CarryComponent* Carry = Turtle->GetCarryComponent();
		const bool bCarried = TNBeachStunDetail::IsCarried(Turtle);
		// Suelta a quien lleve; si la lleva otra, sigue en sus brazos (ya va metida en el caparazón).
		if (Carry && Carry->IsCarrying())
		{
			Carry->ForceRelease(false);
		}
		if (Turtle->IsKnockedDown())
		{
			Turtle->RecoverFromKnockdown();
		}
		if (UTN_ShellComponent* Shell = Turtle->GetShellComponent())
		{
			if (!Shell->IsInShell())
			{
				// Sin cuerpo aquí: la caja se suelta abajo como lanzada (tumbada donde está o con la velocidad pedida).
				Shell->ForceEnterShell(false, false);
			}
			if (!bCarried)
			{
				if (ATN_ShellBody* Body = Shell->GetBody())
				{
					// Ya rodaba (se metió a mano o la habían aturdido): que no salga al pararse y, si hay golpe, otro empujón.
					Body->InitBody(Turtle, false);
					UBoxComponent* Box = Body->GetBox();
					if (Box && !Launch.IsNearlyZero())
					{
						Box->SetPhysicsLinearVelocity(Launch);
					}
				}
				else
				{
					Shell->StartBody(Launch, true, false);
				}
			}
			Shell->SetExitLocked(true);
		}
	}

	const bool bWasStunned = bStunned;
	bStunned = true;
	if (!bWasStunned)
	{
		// En un servidor escucha OnRep no corre en el anfitrión.
		ApplyLocalVisuals(true);
	}
	SetComponentTickEnabled(true);
	Owner->ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] %s aturdida %.1f s%s."), *GetNameSafe(Owner), StunEndServerTime - World->GetTimeSeconds(),
		bWasStunned ? TEXT(" (alargado)") : TEXT(""));
}

void UTN_BeachStunComponent::EndStun(bool bExitShellNow)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bStunned)
	{
		return;
	}
	bStunned = false;
	StunEndServerTime = 0.f;

	ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Owner);
	UTN_ShellComponent* Shell = Turtle ? Turtle->GetShellComponent() : nullptr;
	if (Shell)
	{
		Shell->SetExitLocked(false);
		// Si la lleva otra tortuga, sale cuando la suelte (lo de siempre del caparazón).
		if (Shell->IsInShell() && !TNBeachStunDetail::IsCarried(Turtle))
		{
			ATN_ShellBody* Body = Shell->GetBody();
			if (Body && !bExitShellNow)
			{
				// Sale en cuanto la bola se para (si ya está quieta, en un momento).
				Body->InitBody(Turtle, true);
			}
			else
			{
				Shell->ForceExitShell();
			}
		}
	}

	ApplyLocalVisuals(false);
	Owner->ForceNetUpdate();
}

float UTN_BeachStunComponent::GetSecondsLeft() const
{
	if (!bStunned)
	{
		return 0.f;
	}
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	const float Now = (Owner && Owner->HasAuthority() && World) ? World->GetTimeSeconds() : TNBeachStunDetail::ServerNow(World);
	return FMath::Max(0.f, StunEndServerTime - Now);
}

void UTN_BeachStunComponent::OnRep_Stunned()
{
	ApplyLocalVisuals(bStunned);
}

void UTN_BeachStunComponent::ApplyLocalVisuals(bool bOn)
{
	if (const AActor* Owner = GetOwner())
	{
		if (UTN_DizzyBirdsComponent* Birds = Owner->FindComponentByClass<UTN_DizzyBirdsComponent>())
		{
			// Al acabar, los pájaros siguen si la tortuga está derribada (son del derribo).
			const ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Owner);
			Birds->SetDizzy(bOn || (Turtle && Turtle->IsKnockedDown()));
		}
	}
	TrembleTime = 0.f;
	if (!bOn)
	{
		if (AActor* After = TrembleAfter.Get())
		{
			PrimaryComponentTick.RemovePrerequisite(After, After->PrimaryActorTick);
		}
		TrembleAfter.Reset();
	}
	// El servidor necesita el tick para acabar el aturdimiento; los demás, para el temblor.
	SetComponentTickEnabled(bOn);
}

void UTN_BeachStunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !bStunned)
	{
		return;
	}

	if (Owner->HasAuthority())
	{
		if (World->GetTimeSeconds() >= StunEndServerTime)
		{
			EndStun(false);
			return;
		}
		// Lanzada y rebotando (la suelta quien la llevaba), el caparazón desbloquea la salida: aturdida no se sale.
		if (const ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Owner))
		{
			UTN_ShellComponent* Shell = Turtle->GetShellComponent();
			if (Shell && Shell->IsInShell() && !Shell->IsExitLocked())
			{
				Shell->SetExitLocked(true);
			}
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		// Los pájaros son también del derribo: si el derribo acaba mientras sigue aturdida, se vuelven a encender.
		if (UTN_DizzyBirdsComponent* Birds = Owner->FindComponentByClass<UTN_DizzyBirdsComponent>())
		{
			if (!Birds->IsDizzy())
			{
				Birds->SetDizzy(true);
			}
		}
		TickTremble(DeltaTime);
	}
}

void UTN_BeachStunComponent::TickTremble(float DeltaTime)
{
	ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(GetOwner());
	UTN_ShellComponent* Shell = Turtle ? Turtle->GetShellComponent() : nullptr;
	ATN_ShellBody* Body = Shell ? Shell->GetBody() : nullptr;
	USkeletalMeshComponent* SkelMesh = Turtle ? Turtle->GetMesh() : nullptr;
	if (!Body || !SkelMesh || !Shell->HasLocalBody())
	{
		return;
	}
	// La caja coloca la malla cada fotograma (FollowBody): el temblor va después y nunca se acumula.
	if (TrembleAfter.Get() != Body)
	{
		if (AActor* Old = TrembleAfter.Get())
		{
			PrimaryComponentTick.RemovePrerequisite(Old, Old->PrimaryActorTick);
		}
		PrimaryComponentTick.AddPrerequisite(Body, Body->PrimaryActorTick);
		TrembleAfter = Body;
	}

	TrembleTime += DeltaTime;
	const float T = TrembleTime;
	const float Strength = FMath::Clamp(GetSecondsLeft() / TNBeachStunDetail::TrembleFadeSeconds, 0.f, 1.f);
	if (Strength <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Offset(
		FMath::Sin(T * 57.f) * 0.6f + FMath::Sin(T * 83.f + 1.1f) * 0.4f,
		FMath::Sin(T * 61.f + 2.f) * 0.6f + FMath::Sin(T * 97.f + 0.3f) * 0.4f,
		FMath::Abs(FMath::Sin(T * 44.f)) * 0.5f);
	const FRotator Wobble(
		FMath::Sin(T * 49.f + 0.5f) * TrembleDegrees,
		FMath::Sin(T * 38.f) * TrembleDegrees * 0.5f,
		FMath::Sin(T * 53.f + 1.7f) * TrembleDegrees);
	SkelMesh->AddWorldOffset(Offset * (TrembleAmplitude * Strength), false, nullptr, ETeleportType::TeleportPhysics);
	SkelMesh->AddLocalRotation(Wobble * Strength, false, nullptr, ETeleportType::TeleportPhysics);
}

void UTN_BeachStunComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* After = TrembleAfter.Get())
	{
		PrimaryComponentTick.RemovePrerequisite(After, After->PrimaryActorTick);
	}
	TrembleAfter.Reset();
	Super::EndPlay(EndPlayReason);
}
