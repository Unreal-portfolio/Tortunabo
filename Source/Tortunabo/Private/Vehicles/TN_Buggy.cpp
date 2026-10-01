// ATN_Buggy: construcción, física de conducción (fricción, derrape, golpe de rueda, charco, motor cortado),
// enderezado, cámara, tinte y contrato con la carrera. Asientos y tortugas en TN_Buggy_Seats.cpp; input en
// TN_Buggy_Input.cpp; impactos en TN_Buggy_Effects.cpp.

#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyData.h"
#include "Vehicles/TN_BuggyGunnerPawn.h"
#include "Vehicles/TN_BuggyTurretComponent.h"
#include "Vehicles/TN_BuggyWheel.h"
#include "Vehicles/TN_RallyTurretLogic.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

const FName ATN_Buggy::TintSlotName(TEXT("M_PlayerTint"));
const FName ATN_Buggy::TintParameterName(TEXT("Tint"));
const FVector ATN_Buggy::GunnerSeatLocal(-80.f, 0.f, 127.4f);
const FVector ATN_Buggy::DriverSeatLocal(25.f, -35.f, 67.f);

namespace TNBuggyDetail
{
	const TCHAR* const ChassisMeshPath = TEXT("/Game/Vehicles/OffroadCar/SKM_Offroad.SKM_Offroad");
	const TCHAR* const ChassisAnimPath = TEXT("/Game/Vehicles/OffroadCar/Offroad_AnimBP.Offroad_AnimBP_C");
	const TCHAR* const BodyMeshPath = TEXT("/Game/Generated/Meshes/Buggy/SM_BuggyBody.SM_BuggyBody");
	const TCHAR* const TireMeshPath = TEXT("/Game/Generated/Meshes/Buggy/SM_BuggyTire.SM_BuggyTire");
	const TCHAR* const TurtleMeshPath = TEXT("/Game/Meshses/Characters/Player/TotugaDemo_Rig.TotugaDemo_Rig");

	/** Ruedas en el orden de WheelSetups: delanteras 0 y 1, traseras 2 y 3. */
	constexpr int32 FirstRearWheel = 2;
	constexpr int32 WheelCount = 4;

	/** Cámara de persecución de HellYeah: pivote sobre el centro, cabeceo fijo en mundo, se abre con la velocidad. */
	const FVector CameraPivotLocal(0.f, 0.f, 120.f);
	constexpr float CameraPitchDeg = -18.f;
	const FVector CameraSocketOffset(0.f, 0.f, 100.f);
	constexpr float CameraBaseFov = 90.f;
	constexpr float CameraMaxFov = 100.f;
	constexpr float CameraBaseArm = 780.f;
	constexpr float CameraMaxArm = 860.f;
	constexpr float CameraFullSpeed = 3000.f;
	constexpr float CameraLagSpeed = 15.f;
	constexpr float CameraRotationLagSpeed = 6.f;
	constexpr float CameraLagMaxDistance = 100.f;

	/** Curva de par de OffroadCar_TorqueCurve (TP_VehicleAdvBP) en fracciones de MaxRPM. */
	constexpr float TorqueCurveKeys[][2] = {
		{ 0.f, 0.5f }, { 0.1f, 0.8f }, { 0.3f, 1.f }, { 0.55f, 1.f }, { 0.8f, 0.9f }, { 0.9f, 0.4f }, { 1.f, 0.f } };

	void BuildTorqueCurve(FVehicleEngineConfig& Engine, float MaxRPM)
	{
		FRichCurve* Curve = Engine.TorqueCurve.GetRichCurve();
		Curve->Reset();
		for (const auto& Key : TorqueCurveKeys)
		{
			Curve->AddKey(Key[0] * MaxRPM, Key[1]);
		}
	}
}

ATN_Buggy::ATN_Buggy()
{
	using namespace TNBuggyDetail;
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	ChassisMeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(ChassisMeshPath));
	ChassisAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(ChassisAnimPath));
	BodyMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(BodyMeshPath));
	TireMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TireMeshPath));
	TurtleMeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TurtleMeshPath));
	GunnerPawnClass = ATN_BuggyGunnerPawn::StaticClass();

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ChassisFinder(ChassisMeshPath);
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(TEXT("/Game/Vehicles/OffroadCar/Offroad_AnimBP"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyFinder(BodyMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TireFinder(TireMeshPath);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TurtleFinder(TurtleMeshPath);

	USkeletalMeshComponent* Chassis = GetMesh();
	Chassis->SetSkeletalMesh(ChassisFinder.Object);
	Chassis->SetAnimInstanceClass(AnimFinder.Class);
	Chassis->SetSimulatePhysics(true);
	// El esqueleto solo aporta física y huesos de rueda: se oculta sin ocultar a sus hijos y refresca huesos siempre.
	Chassis->SetVisibility(false, false);
	Chassis->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Chassis);
	Body->SetStaticMesh(BodyFinder.Object);
	Body->SetCollisionProfileName(TEXT("NoCollision"));

	const TCHAR* const TireSockets[] = { TEXT("VisWheel_FL"), TEXT("VisWheel_FR"), TEXT("VisWheel_BL"), TEXT("VisWheel_BR") };
	for (int32 Index = 0; Index < WheelCount; ++Index)
	{
		const FName Socket(TireSockets[Index]);
		UStaticMeshComponent* Tire = CreateDefaultSubobject<UStaticMeshComponent>(Socket);
		Tire->SetupAttachment(Chassis, Socket);
		Tire->SetStaticMesh(TireFinder.Object);
		Tire->SetCollisionProfileName(TEXT("NoCollision"));
		// Neumáticos derechos girados para que la cara exterior mire afuera.
		if (Index % 2 == 1)
		{
			Tire->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
		}
		Tires.Add(Tire);
	}

	Turret = CreateDefaultSubobject<UTN_BuggyTurretComponent>(TEXT("Turret"));
	Turret->SetupAttachment(Chassis);
	Turret->SetRelativeLocation(GunnerSeatLocal + FVector(0.f, 0.f, UTN_BuggyTurretComponent::PivotAboveSeatCm));

	// Cañón: cilindro básico de 100 cm tumbado sobre X (excepción aceptada: no hay malla de torreta todavía).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BarrelFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	TurretBarrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBarrel"));
	TurretBarrel->SetupAttachment(Turret);
	TurretBarrel->SetStaticMesh(BarrelFinder.Object);
	TurretBarrel->SetRelativeLocationAndRotation(FVector(UTN_BuggyTurretComponent::MuzzleDistanceCm * 0.5f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f));
	TurretBarrel->SetRelativeScale3D(FVector(0.22f, 0.22f, UTN_BuggyTurretComponent::MuzzleDistanceCm / 100.f));
	TurretBarrel->SetCollisionProfileName(TEXT("NoCollision"));
	TurretBarrel->SetCastShadow(false);

	const TCHAR* const SeatNames[] = { TEXT("DriverTurtle"), TEXT("GunnerTurtle") };
	const FVector SeatLocations[] = { DriverSeatLocal, GunnerSeatLocal };
	for (int32 Seat = 0; Seat < 2; ++Seat)
	{
		USkeletalMeshComponent* Turtle = CreateDefaultSubobject<USkeletalMeshComponent>(SeatNames[Seat]);
		Turtle->SetupAttachment(Chassis);
		Turtle->SetRelativeLocation(SeatLocations[Seat]);
		// La malla de la tortuga mira a su +Y (como en BP_TortugaCharacter): -90 la pone mirando al morro.
		Turtle->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		Turtle->SetCollisionProfileName(TEXT("NoCollision"));
		Turtle->SetGenerateOverlapEvents(false);
		Turtle->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		Turtle->SetSkeletalMesh(TurtleFinder.Object);
		Turtle->SetHiddenInGame(true);
		SeatTurtles.Add(Turtle);

		UStaticMeshComponent* Helmet = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("%sHelmet"), SeatNames[Seat]));
		Helmet->SetupAttachment(Turtle);
		Helmet->SetCollisionProfileName(TEXT("NoCollision"));
		SeatHelmets.Add(Helmet);
	}

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Chassis);
	SpringArm->SetRelativeLocationAndRotation(CameraPivotLocal, FRotator(CameraPitchDeg, 0.f, 0.f));
	SpringArm->TargetArmLength = CameraBaseArm;
	SpringArm->SocketOffset = CameraSocketOffset;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = CameraLagSpeed;
	SpringArm->CameraLagMaxDistance = CameraLagMaxDistance;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = CameraRotationLagSpeed;
	SpringArm->bDoCollisionTest = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->SetFieldOfView(CameraBaseFov);

	UChaosWheeledVehicleMovementComponent* Move = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	Move->ChassisHeight = 160.f;
	Move->DragCoefficient = 0.1f;
	Move->bEnableCenterOfMassOverride = true;
	Move->CenterOfMassOverride = FVector(0.f, 0.f, 40.f);
	Move->bLegacyWheelFrictionPosition = false;
	Move->WheelSetups.SetNum(WheelCount);
	const TCHAR* const WheelBones[] = { TEXT("PhysWheel_FL"), TEXT("PhysWheel_FR"), TEXT("PhysWheel_BL"), TEXT("PhysWheel_BR") };
	for (int32 Index = 0; Index < WheelCount; ++Index)
	{
		Move->WheelSetups[Index].WheelClass = Index < FirstRearWheel
			? UTN_BuggyWheelFront::StaticClass()
			: UTN_BuggyWheelRear::StaticClass();
		Move->WheelSetups[Index].BoneName = FName(WheelBones[Index]);
	}
	// Provisional: PostInitializeComponents recrea el motor con el ajuste de Data.
	Move->EngineSetup.MaxTorque = 850.f;
	Move->EngineSetup.MaxRPM = 3400.f;
	BuildTorqueCurve(Move->EngineSetup, Move->EngineSetup.MaxRPM);
	Move->DifferentialSetup.DifferentialType = EVehicleDifferential::RearWheelDrive;
	Move->SteeringSetup.SteeringType = ESteeringType::AngleRatio;
	Move->SteeringSetup.AngleRatio = 0.7f;

	SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);
}

const UTN_BuggyData* ATN_Buggy::GetData() const
{
	return Data ? Data.Get() : GetDefault<UTN_BuggyData>();
}

UChaosWheeledVehicleMovementComponent* ATN_Buggy::GetWheeledMovement() const
{
	return Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
}

double ATN_Buggy::GetServerNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void ATN_Buggy::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Un hijo en Blueprint puede cambiar las rutas: se cargan si no coinciden con las del constructor.
	USkeletalMeshComponent* Chassis = GetMesh();
	if (USkeletalMesh* Wanted = ChassisMeshAsset.LoadSynchronous(); Wanted && Chassis->GetSkeletalMeshAsset() != Wanted)
	{
		Chassis->SetSkeletalMesh(Wanted);
	}
	if (UClass* WantedAnim = ChassisAnimClass.LoadSynchronous(); WantedAnim && Chassis->GetAnimClass() != WantedAnim)
	{
		Chassis->SetAnimInstanceClass(WantedAnim);
	}
	if (UStaticMesh* WantedBody = BodyMeshAsset.LoadSynchronous(); WantedBody && Body->GetStaticMesh() != WantedBody)
	{
		Body->SetStaticMesh(WantedBody);
	}
	if (UStaticMesh* WantedTire = TireMeshAsset.LoadSynchronous())
	{
		for (UStaticMeshComponent* Tire : Tires)
		{
			if (Tire && Tire->GetStaticMesh() != WantedTire)
			{
				Tire->SetStaticMesh(WantedTire);
			}
		}
	}

	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move)
	{
		UE_LOG(LogTNBuggy, Error, TEXT("%s: sin UChaosWheeledVehicleMovementComponent"), *GetName());
		return;
	}
	// El motor se lee al crear la simulación: se recrea con el ajuste de Data.
	const UTN_BuggyData* Tuning = GetData();
	Move->EngineSetup.MaxTorque = Tuning->MaxTorque;
	Move->EngineSetup.MaxRPM = Tuning->MaxRPM;
	TNBuggyDetail::BuildTorqueCurve(Move->EngineSetup, Tuning->MaxRPM);
	Move->TransmissionSetup.FinalRatio = Tuning->FinalDriveRatio;
	Move->RecreatePhysicsState();
	ApplyWheelFriction();
}

void ATN_Buggy::BeginPlay()
{
	Super::BeginPlay();
	ApplyWheelFriction();
	ApplyTint();
	RefreshSeatVisuals(true);
}

void ATN_Buggy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		DestroyGunnerPawn();
	}
	Super::EndPlay(EndPlayReason);
}

void ATN_Buggy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_Buggy, TeamIndex);
	// La conductora ya conoce su freno de mano: solo lo reciben los demás.
	DOREPLIFETIME_CONDITION(ATN_Buggy, bHandbrakeHeld, COND_SkipOwner);
	DOREPLIFETIME(ATN_Buggy, bDriverSeated);
	DOREPLIFETIME(ATN_Buggy, bGunnerSeated);
	DOREPLIFETIME(ATN_Buggy, DriverPlayerState);
	DOREPLIFETIME(ATN_Buggy, GunnerPlayerState);
	DOREPLIFETIME(ATN_Buggy, GunnerPawn);
	DOREPLIFETIME(ATN_Buggy, bEngineLockedByRace);
	DOREPLIFETIME(ATN_Buggy, bWeaponsLockedByRace);
	DOREPLIFETIME(ATN_Buggy, LockEndServerTime);
	DOREPLIFETIME(ATN_Buggy, bGhost);
	DOREPLIFETIME(ATN_Buggy, ShieldEndServerTime);
	DOREPLIFETIME(ATN_Buggy, InkEndServerTime);
	DOREPLIFETIME(ATN_Buggy, WobbleEndServerTime);
	DOREPLIFETIME(ATN_Buggy, bInPuddle);
}

void ATN_Buggy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FlippedSeconds = TNBuggy::AdvanceFlipped(FlippedSeconds, GetActorUpVector().Z, DeltaSeconds);
	if (HasAuthority())
	{
		UpdateServerTimers();
		UpdateSelfRight(DeltaSeconds);
	}

	// Reintenta si la simulación no estaba lista; la fricción cambia con el freno de mano y el charco.
	const float Grip = TNRallyTurret::PuddleGripMultiplier(bInPuddle);
	if (!bWheelFrictionApplied || bHandbrakeHeld != bHandbrakeFrictionApplied || !FMath::IsNearlyEqual(Grip, AppliedGripMultiplier))
	{
		ApplyWheelFriction();
	}
	if (IsEngineLocked() != bEngineTorqueLockedApplied)
	{
		ApplyEngineTorque();
	}
	if (IsEngineLocked() && (HasAuthority() || IsLocallyControlled()))
	{
		HoldLockedInPlace();
	}
	ApplyBumpKicks();
	ApplyPuddleSpeedCap();
	ApplyAntiRoll();
	if (IsLocallyControlled() || (HasAuthority() && !IsPlayerControlled()))
	{
		ApplySteeringAssist();
	}
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		UpdateCamera(DeltaSeconds);
		if (bSelfRightHeld && TNBuggy::AdvanceHold(RespawnHold, true, DeltaSeconds, GetData()->RespawnHoldSeconds))
		{
			ServerRequestRespawn();
		}
	}

	SeatLookCheckAccumulator += DeltaSeconds;
	if (SeatLookCheckAccumulator >= 0.5f)
	{
		SeatLookCheckAccumulator = 0.f;
		RefreshSeatVisuals(false);
	}
}

void ATN_Buggy::UpdateServerTimers()
{
	const float Now = static_cast<float>(GetServerNow());
	const bool bPuddleNow = PuddleUntilServerTime > Now;
	if (bPuddleNow != bInPuddle)
	{
		bInPuddle = bPuddleNow;
		ForceNetUpdate();
	}
	const bool bGhostNow = GhostEndServerTime > Now;
	if (bGhostNow != bGhost)
	{
		bGhost = bGhostNow;
		ApplyGhost();
		ForceNetUpdate();
	}
}

bool ATN_Buggy::IsEngineLocked() const
{
	return bEngineLockedByRace || LockEndServerTime > GetServerNow();
}

void ATN_Buggy::ApplyWheelFriction()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || !Move->HasValidPhysicsState())
	{
		return;
	}
	const UTN_BuggyData* Tuning = GetData();
	const float Grip = TNRallyTurret::PuddleGripMultiplier(bInPuddle);
	const float RearFriction = bHandbrakeHeld ? Tuning->HandbrakeRearFriction : Tuning->RearFriction;
	const int32 Wheels = FMath::Min(TNBuggyDetail::WheelCount, Move->WheelSetups.Num());
	for (int32 Index = 0; Index < Wheels; ++Index)
	{
		const float Base = Index < TNBuggyDetail::FirstRearWheel ? Tuning->FrontFriction : RearFriction;
		Move->SetWheelFrictionMultiplier(Index, Base * Grip);
	}
	bHandbrakeFrictionApplied = bHandbrakeHeld;
	AppliedGripMultiplier = Grip;
	bWheelFrictionApplied = true;
}

void ATN_Buggy::ApplyEngineTorque()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || !Move->HasValidPhysicsState())
	{
		return;
	}
	const bool bLocked = IsEngineLocked();
	// El par es parte de la simulación (no de la entrada): así el corte vale también en el servidor.
	Move->SetMaxEngineTorque(bLocked ? 0.f : GetData()->MaxTorque);
	bEngineTorqueLockedApplied = bLocked;
}

void ATN_Buggy::HoldLockedInPlace()
{
	// Solo la reaparición (LockEndServerTime) inmoviliza; el semáforo y la salida anticipada solo cortan el motor.
	if (LockEndServerTime <= GetServerNow())
	{
		return;
	}
	USkeletalMeshComponent* Chassis = GetMesh();
	if (Chassis->IsSimulatingPhysics())
	{
		Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

void ATN_Buggy::ApplySteeringAssist()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move)
	{
		return;
	}
	const UTN_BuggyData* Tuning = GetData();
	const float Slip = TNBuggy::SlipAngleDeg(GetActorForwardVector(), GetVelocity());
	const float WobbleLeft = WobbleEndServerTime - static_cast<float>(GetServerNow());
	const float Wobble = TNBuggy::SteerWobble(WobbleLeft, TNRallyTurret::CocoWobbleSeconds, Tuning->WobbleAmplitude, Tuning->WobbleFrequency);
	Move->SetSteeringInput(FMath::Clamp(
		TNBuggy::AssistSteer(SteerRequest, Slip, Tuning->CounterSteerAssist, Tuning->MaxAssistAngleDeg) + Wobble, -1.f, 1.f));
}

void ATN_Buggy::ApplyBumpKicks()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Chassis = GetMesh();
	if (!Move || !Move->HasValidPhysicsState() || !Chassis->IsSimulatingPhysics())
	{
		return;
	}
	const int32 Wheels = FMath::Min(Move->Wheels.Num(), Move->WheelSetups.Num());
	PrevContactPoint.SetNumZeroed(Wheels);
	bPrevWheelContact.SetNumZeroed(Wheels);
	BumpTracks.SetNum(Wheels);
	const UTN_BuggyData* BuggyData = GetData();
	TNBuggy::FBumpTuning Tuning;
	Tuning.MinStepCm = BuggyData->BumpMinStepCm;
	Tuning.Scale = BuggyData->BumpKickScale;
	Tuning.MaxKick = BuggyData->BumpKickMax;
	const float Mass = Chassis->GetMass();
	const float ForwardSpeed = Move->GetForwardSpeed();
	for (int32 Index = 0; Index < Wheels; ++Index)
	{
		const UChaosVehicleWheel* Wheel = Move->Wheels[Index];
		if (!Wheel)
		{
			continue;
		}
		const FWheelStatus& State = Move->GetWheelState(Index);
		const FVector Delta = State.ContactPoint - PrevContactPoint[Index];
		const bool bBothInContact = bPrevWheelContact[Index] && State.bInContact;
		// El golpe de un escalón se aplica un frame después, al comprobar que no era una rampa.
		const TNBuggy::FBumpStep Step = TNBuggy::BumpStep(BumpTracks[Index], Delta.Z, ForwardSpeed, Wheel->GetWheelRadius(), bBothInContact, Tuning);
		BumpTracks[Index] = Step.Track;
		if (Step.Kick > 0.f)
		{
			const FVector WheelLocation = Chassis->GetSocketLocation(Move->WheelSetups[Index].BoneName);
			Chassis->AddImpulseAtLocation(GetActorUpVector() * Step.Kick * Mass, WheelLocation);
		}
		PrevContactPoint[Index] = State.ContactPoint;
		bPrevWheelContact[Index] = State.bInContact;
	}
}

void ATN_Buggy::ApplyPuddleSpeedCap()
{
	if (!bInPuddle)
	{
		return;
	}
	USkeletalMeshComponent* Chassis = GetMesh();
	if (!Chassis->IsSimulatingPhysics())
	{
		return;
	}
	const FVector Velocity = GetVelocity();
	const FVector Flat(Velocity.X, Velocity.Y, 0.f);
	const float Decel = TNBuggy::SpeedCapDecel(Flat.Size(), TNRallyTurret::PuddleSpeedCapCms(true), GetData()->PuddleBrakeGain);
	if (Decel > 0.f)
	{
		// Como aceleración (bAccelChange) en el centro de masas, en todas las máquinas que simulan el chasis.
		Chassis->AddForce(-Flat.GetSafeNormal() * Decel, NAME_None, true);
	}
}

void ATN_Buggy::ApplyAntiRoll()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Chassis = GetMesh();
	if (!Move || !Move->HasValidPhysicsState() || !Chassis->IsSimulatingPhysics())
	{
		return;
	}
	bool bAnyContact = false;
	for (int32 Index = 0; Index < Move->Wheels.Num() && !bAnyContact; ++Index)
	{
		bAnyContact = Move->GetWheelState(Index).bInContact;
	}
	const UTN_BuggyData* Tuning = GetData();
	TNBuggy::FAntiRollTuning AntiRoll;
	AntiRoll.GroundFreeRollDeg = Tuning->AntiRollGroundFreeRollDeg;
	AntiRoll.GroundFreePitchDeg = Tuning->AntiRollGroundFreePitchDeg;
	AntiRoll.Stiffness = Tuning->AntiRollStiffness;
	AntiRoll.Damping = Tuning->AntiRollDamping;
	AntiRoll.MaxAccel = Tuning->AntiRollMaxAccel;
	const FVector Accel = TNBuggy::AntiRollAccel(GetActorForwardVector(), GetActorUpVector(),
		Chassis->GetPhysicsAngularVelocityInRadians(), !bAnyContact, AntiRoll);
	if (!Accel.IsNearlyZero())
	{
		// Como aceleración (bAccelChange): igual para cualquier masa e inercia del chasis.
		Chassis->AddTorqueInRadians(Accel, NAME_None, true);
	}
}

void ATN_Buggy::UpdateSelfRight(float DeltaSeconds)
{
	const UTN_BuggyData* Tuning = GetData();
	const TNBuggy::ESelfRight Decision = TNBuggy::DecideSelfRight(FlippedSeconds, bSelfRightRequested,
		Tuning->SelfRightManualDelay, Tuning->SelfRightAutoDelay);
	bSelfRightRequested = false;
	if (Decision != TNBuggy::ESelfRight::None)
	{
		UE_LOG(LogTNBuggy, Log, TEXT("%s: enderezado %s tras %.1f s volcado"), *GetName(),
			Decision == TNBuggy::ESelfRight::Auto ? TEXT("automático") : TEXT("pedido"), FlippedSeconds);
		DoSelfRight();
	}
}

void ATN_Buggy::DoSelfRight()
{
	const FTransform Target = TNBuggy::SelfRightTransform(GetActorTransform(), GetData()->SelfRightLiftCm);
	SetActorLocationAndRotation(Target.GetLocation(), Target.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	FlippedSeconds = 0.f;
	ForceNetUpdate();
}

bool ATN_Buggy::ServerSelfRight_Validate()
{
	return true;
}

void ATN_Buggy::ServerSelfRight_Implementation()
{
	// Revalidado en UpdateSelfRight: el cliente no decide cuándo se puede enderezar.
	bSelfRightRequested = true;
}

bool ATN_Buggy::ServerRequestRespawn_Validate()
{
	return true;
}

void ATN_Buggy::ServerRequestRespawn_Implementation()
{
	UE_LOG(LogTNBuggy, Log, TEXT("%s: piden reaparecer"), *GetName());
	bRespawnRequested = true;
}

bool ATN_Buggy::ConsumeRespawnRequest()
{
	const bool bWas = bRespawnRequested;
	bRespawnRequested = false;
	return bWas;
}

bool ATN_Buggy::ConsumeFellOutOfWorld()
{
	const bool bWas = bFellOutOfWorld;
	bFellOutOfWorld = false;
	return bWas;
}

void ATN_Buggy::FellOutOfWorld(const UDamageType& DmgType)
{
	// AActor::FellOutOfWorld destruiría el buggy (y con él el peón de la artillera) y la carrera perdería el equipo. Chaos lo
	// llama en cada paso mientras siga bajo el KillZ: se frena la caída y la carrera lo devuelve a la pista.
	if (!HasAuthority())
	{
		return;
	}
	if (!bFellOutOfWorld)
	{
		UE_LOG(LogTNBuggy, Warning, TEXT("%s: bajo el KillZ (Z %.0f); pide volver a la pista"), *GetName(), GetActorLocation().Z);
	}
	bFellOutOfWorld = true;
	if (USkeletalMeshComponent* Chassis = GetMesh(); Chassis && Chassis->IsSimulatingPhysics())
	{
		Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

void ATN_Buggy::UpdateCamera(float DeltaSeconds)
{
	using namespace TNBuggyDetail;
	const float Alpha = FMath::Clamp(GetVelocity().Size() / CameraFullSpeed, 0.f, 1.f);
	Camera->SetFieldOfView(FMath::Lerp(CameraBaseFov, CameraMaxFov, Alpha));
	SpringArm->TargetArmLength = FMath::Lerp(CameraBaseArm, CameraMaxArm, Alpha);
}

void ATN_Buggy::SetRallyTeamIndex(int32 Index)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTNBuggy, Warning, TEXT("%s: SetRallyTeamIndex solo se aplica en el servidor"), *GetName());
		return;
	}
	TeamIndex = Index;
	ApplyTint();
	ForceNetUpdate();
}

void ATN_Buggy::OnRep_TeamIndex()
{
	ApplyTint();
}

void ATN_Buggy::ApplyTint()
{
	if (!TintMaterial)
	{
		const int32 Slot = Body->GetMaterialIndex(TintSlotName);
		if (Slot == INDEX_NONE)
		{
			UE_LOG(LogTNBuggy, Warning, TEXT("%s: la carrocería no tiene el slot %s"), *GetName(), *TintSlotName.ToString());
			return;
		}
		TintMaterial = Body->CreateAndSetMaterialInstanceDynamic(Slot);
	}
	if (TintMaterial)
	{
		TintMaterial->SetVectorParameterValue(TintParameterName, TNBuggy::TeamColor(TeamIndex));
	}
}

void ATN_Buggy::OnRep_Ghost()
{
	ApplyGhost();
}

void ATN_Buggy::ApplyGhost()
{
	// Fantasma: no choca con otros vehículos (la respuesta Ignore de un lado basta para el par).
	GetMesh()->SetCollisionResponseToChannel(ECC_Vehicle, bGhost ? ECR_Ignore : ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, bGhost ? ECR_Ignore : ECR_Block);
}

void ATN_Buggy::RallyTeleport(const FTransform& Where, float LockSeconds, float GhostSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	// TeleportPhysics: con ResetPhysics la física de Chaos devuelve el chasis a donde estaba en el siguiente paso (medido con
	// TN.Rally.DebugTeleport en UE 5.6) y la reaparición no movía el buggy.
	SetActorLocationAndRotation(Where.GetLocation(), Where.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	USkeletalMeshComponent* Chassis = GetMesh();
	Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->StopMovementImmediately();
	}
	const float Now = static_cast<float>(GetServerNow());
	LockEndServerTime = LockSeconds > 0.f ? Now + LockSeconds : 0.f;
	GhostEndServerTime = GhostSeconds > 0.f ? Now + GhostSeconds : 0.f;
	FlippedSeconds = 0.f;
	UpdateServerTimers();
	ApplyEngineTorque();
	ForceNetUpdate();
}

void ATN_Buggy::SetEngineLocked(bool bLocked)
{
	if (!HasAuthority())
	{
		return;
	}
	bEngineLockedByRace = bLocked;
	ApplyEngineTorque();
	ForceNetUpdate();
}

void ATN_Buggy::SetWeaponsLocked(bool bLocked)
{
	if (!HasAuthority() || bWeaponsLockedByRace == bLocked)
	{
		return;
	}
	bWeaponsLockedByRace = bLocked;
	ForceNetUpdate();
}

float ATN_Buggy::GetForwardSpeedCms() const
{
	const UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	return Move ? Move->GetForwardSpeed() : 0.f;
}

bool ATN_Buggy::IsFlipped() const
{
	return TNBuggy::IsFlipped(GetActorUpVector().Z);
}

void ATN_Buggy::SetAIDriveInput(float Throttle, float Brake, float Steer, bool bHandbrake)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move)
	{
		return;
	}
	// Sin controlador local, Chaos solo procesa las entradas con esto desactivado.
	Move->SetRequiresControllerForInputs(false);
	const bool bLocked = IsEngineLocked();
	Move->SetThrottleInput(bLocked ? 0.f : FMath::Clamp(Throttle, 0.f, 1.f));
	Move->SetBrakeInput(FMath::Clamp(Brake, 0.f, 1.f));
	SteerRequest = FMath::Clamp(Steer, -1.f, 1.f);
	if (bHandbrake != bHandbrakeHeld)
	{
		SetHandbrakeHeld(bHandbrake);
	}
	ApplySteeringAssist();
}
