// ATN_Buggy: plazas (conductora y artillera) y tortugas visuales sentadas con el aspecto de cada ocupante.

#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyGunnerPawn.h"
#include "Vehicles/TN_BuggyTurretComponent.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_CosmeticLook.h"
#include "Core/TN_CosmeticsTypes.h"
#include "Player/TN_TurtleAnimInstance.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace TNBuggySeats
{
	constexpr int32 DriverIndex = 0;
	constexpr int32 GunnerIndex = 1;

	FTN_TurtleLook LookOf(const APlayerState* PlayerState)
	{
		FTN_TurtleLook Look;
		if (const ATN_CoopPlayerState* Coop = Cast<ATN_CoopPlayerState>(PlayerState))
		{
			Look.HelmetId = Coop->EquippedHelmetId;
			Look.SkinId = Coop->EquippedSkinId;
			Look.ShellId = Coop->EquippedShellId;
			Look.EyesId = Coop->EquippedEyesId;
		}
		return Look;
	}

	FString LookKey(const FTN_TurtleLook& Look)
	{
		return FString::Printf(TEXT("%s|%s|%s|%s"), *Look.HelmetId.ToString(), *Look.SkinId.ToString(),
			*Look.ShellId.ToString(), *Look.EyesId.ToString());
	}
}

bool ATN_Buggy::SeatController(AController* InController, ETNRallySeat Seat)
{
	if (!HasAuthority() || !InController)
	{
		return false;
	}
	if (GetSeatController(Seat) == InController)
	{
		return true;
	}
	if (!HasFreeSeat(Seat))
	{
		return false;
	}
	// Si ya iba en la otra plaza, la deja (sin pasar a la artillera al volante: se cambia de sitio).
	if (Seat == ETNRallySeat::Driver && GunnerController == InController)
	{
		SetGunnerSeat(nullptr);
	}
	else if (Seat == ETNRallySeat::Gunner && DriverController == InController)
	{
		InController->UnPossess();
		SetDriverSeat(nullptr);
	}

	if (Seat == ETNRallySeat::Driver)
	{
		InController->Possess(this);
		// PossessedBy ya ha llamado a SetDriverSeat; se repite por si el Possess de un controlador propio no lo hace.
		SetDriverSeat(InController);
		return Controller == InController;
	}
	SetGunnerSeat(InController);
	return GunnerController == InController;
}

void ATN_Buggy::UnseatController(AController* InController)
{
	if (!HasAuthority() || !InController)
	{
		return;
	}
	if (InController == GunnerController)
	{
		SetGunnerSeat(nullptr);
		return;
	}
	if (InController != DriverController)
	{
		return;
	}
	InController->UnPossess();
	SetDriverSeat(nullptr);
	// La artillera pasa a conducir.
	if (AController* Promoted = GunnerController)
	{
		SetGunnerSeat(nullptr);
		Promoted->Possess(this);
		SetDriverSeat(Promoted);
		UE_LOG(LogTNBuggy, Log, TEXT("%s: la artillera %s pasa a conducir"), *GetName(), *Promoted->GetName());
	}
}

AController* ATN_Buggy::GetSeatController(ETNRallySeat Seat) const
{
	return Seat == ETNRallySeat::Driver ? DriverController.Get() : GunnerController.Get();
}

bool ATN_Buggy::HasFreeSeat(ETNRallySeat Seat) const
{
	return GetSeatController(Seat) == nullptr;
}

void ATN_Buggy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	SetDriverSeat(NewController);
}

void ATN_Buggy::UnPossessed()
{
	Super::UnPossessed();
	SetDriverSeat(nullptr);
	SteerRequest = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(0.f);
	}
}

void ATN_Buggy::SetDriverSeat(AController* NewDriver)
{
	if (!HasAuthority())
	{
		return;
	}
	DriverController = NewDriver;
	bDriverSeated = NewDriver != nullptr;
	DriverPlayerState = NewDriver ? NewDriver->PlayerState.Get() : nullptr;
	RefreshSeatVisuals(true);
	ForceNetUpdate();
}

void ATN_Buggy::SetGunnerSeat(AController* NewGunner)
{
	if (!HasAuthority())
	{
		return;
	}
	if (GunnerController && GunnerController != NewGunner)
	{
		if (GunnerPawn && GunnerController->GetPawn() == GunnerPawn)
		{
			GunnerController->UnPossess();
		}
		DestroyGunnerPawn();
	}
	GunnerController = NewGunner;
	if (NewGunner)
	{
		if (ATN_BuggyGunnerPawn* Pawn = SpawnGunnerPawn())
		{
			NewGunner->Possess(Pawn);
		}
	}
	bGunnerSeated = NewGunner != nullptr;
	GunnerPlayerState = NewGunner ? NewGunner->PlayerState.Get() : nullptr;
	RefreshSeatVisuals(true);
	ForceNetUpdate();
}

ATN_BuggyGunnerPawn* ATN_Buggy::SpawnGunnerPawn()
{
	if (GunnerPawn)
	{
		return GunnerPawn;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* PawnClass = GunnerPawnClass ? GunnerPawnClass.Get() : ATN_BuggyGunnerPawn::StaticClass();
	GunnerPawn = World->SpawnActor<ATN_BuggyGunnerPawn>(PawnClass, GetActorTransform(), Params);
	if (GunnerPawn)
	{
		GunnerPawn->SetBuggy(this);
	}
	else
	{
		UE_LOG(LogTNBuggy, Error, TEXT("%s: no se ha podido crear el peón de la artillera"), *GetName());
	}
	return GunnerPawn;
}

void ATN_Buggy::DestroyGunnerPawn()
{
	if (GunnerPawn)
	{
		GunnerPawn->Destroy();
		GunnerPawn = nullptr;
	}
}

void ATN_Buggy::OnRep_Seats()
{
	RefreshSeatVisuals(true);
}

void ATN_Buggy::RefreshSeatVisuals(bool bForce)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	ApplySeatLook(TNBuggySeats::DriverIndex, bForce);
	ApplySeatLook(TNBuggySeats::GunnerIndex, bForce);
}

void ATN_Buggy::ApplySeatLook(int32 SeatIndex, bool bForce)
{
	if (!SeatTurtles.IsValidIndex(SeatIndex) || !SeatHelmets.IsValidIndex(SeatIndex))
	{
		return;
	}
	USkeletalMeshComponent* Turtle = SeatTurtles[SeatIndex];
	UStaticMeshComponent* Helmet = SeatHelmets[SeatIndex];
	const bool bDriver = SeatIndex == TNBuggySeats::DriverIndex;
	const bool bSeated = bDriver ? bDriverSeated : bGunnerSeated;
	if (!Turtle || !Helmet)
	{
		return;
	}
	Turtle->SetHiddenInGame(!bSeated, true);
	if (!bSeated)
	{
		return;
	}
	if (!Turtle->GetAnimInstance() || !Turtle->GetAnimInstance()->IsA<UTN_TurtleAnimInstance>())
	{
		if (USkeletalMesh* Wanted = TurtleMeshAsset.LoadSynchronous(); Wanted && Turtle->GetSkeletalMeshAsset() != Wanted)
		{
			Turtle->SetSkeletalMesh(Wanted);
		}
		Turtle->SetAnimInstanceClass(UTN_TurtleAnimInstance::StaticClass());
		FitTurtle(Turtle);
		bForce = true;
	}
	const FTN_TurtleLook Look = TNBuggySeats::LookOf(bDriver ? DriverPlayerState.Get() : GunnerPlayerState.Get());
	const FString Key = TNBuggySeats::LookKey(Look);
	AppliedLookKeys.SetNum(2);
	if (!bForce && AppliedLookKeys[SeatIndex] == Key)
	{
		return;
	}
	AppliedLookKeys[SeatIndex] = Key;
	TArray<TObjectPtr<UMaterialInterface>>& Defaults = bDriver ? DriverTurtleDefaults : GunnerTurtleDefaults;
	UTN_CosmeticLook::ApplyLook(this, Turtle, Helmet, Look, Defaults);
}

void ATN_Buggy::FitTurtle(USkeletalMeshComponent* Turtle) const
{
	const USkeletalMesh* TurtleMesh = Turtle ? Turtle->GetSkeletalMeshAsset() : nullptr;
	if (!TurtleMesh)
	{
		return;
	}
	const float Height = static_cast<float>(TurtleMesh->GetBounds().BoxExtent.Z) * 2.f;
	if (Height > KINDA_SMALL_NUMBER)
	{
		Turtle->SetRelativeScale3D(FVector(SeatedTurtleHeightCm / Height));
	}
}

bool ATN_Buggy::IsOccupiedByLocalPlayer() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Local = World ? World->GetFirstPlayerController() : nullptr;
	if (!Local || !Local->IsLocalController())
	{
		return false;
	}
	const APawn* Pawn = Local->GetPawn();
	return Pawn == this || (Pawn && Pawn == GunnerPawn);
}
