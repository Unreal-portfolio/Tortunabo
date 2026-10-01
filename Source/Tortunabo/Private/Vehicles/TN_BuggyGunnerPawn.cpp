#include "Vehicles/TN_BuggyGunnerPawn.h"
#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyData.h"
#include "Vehicles/TN_BuggyInput.h"
#include "Vehicles/TN_BuggyTurretComponent.h"
#include "Vehicles/TN_RallyTurretLogic.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

namespace TNGunnerDetail
{
	/** Cámara detrás de la torreta: brazo, altura y cabeceo base (grados) más una fracción del de la torreta. */
	constexpr float ArmLengthCm = 520.f;
	const FVector SocketOffset(0.f, 0.f, 90.f);
	constexpr float BasePitchDeg = -12.f;
	constexpr float PitchFollow = 0.6f;
}

ATN_BuggyGunnerPawn::ATN_BuggyGunnerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// La posición la da el enganche al buggy en cada máquina (OnRep_Buggy), no el movimiento replicado.
	SetReplicatingMovement(false);
	bAlwaysRelevant = false;
	AutoPossessAI = EAutoPossessAI::Disabled;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, UTN_BuggyTurretComponent::PivotAboveSeatCm));
	SpringArm->TargetArmLength = TNGunnerDetail::ArmLengthCm;
	SpringArm->SocketOffset = TNGunnerDetail::SocketOffset;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->SetUsingAbsoluteRotation(true);
	SpringArm->bEnableCameraLag = false;
	SpringArm->bDoCollisionTest = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->SetFieldOfView(90.f);
}

void ATN_BuggyGunnerPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BuggyGunnerPawn, Buggy);
}

void ATN_BuggyGunnerPawn::BeginPlay()
{
	Super::BeginPlay();
	AttachToBuggy();
}

void ATN_BuggyGunnerPawn::SetBuggy(ATN_Buggy* InBuggy)
{
	if (!HasAuthority())
	{
		return;
	}
	Buggy = InBuggy;
	AttachToBuggy();
	ForceNetUpdate();
}

void ATN_BuggyGunnerPawn::OnRep_Buggy()
{
	AttachToBuggy();
}

void ATN_BuggyGunnerPawn::AttachToBuggy()
{
	if (!Buggy || GetAttachParentActor() == Buggy)
	{
		return;
	}
	AttachToComponent(Buggy->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(ATN_Buggy::GunnerSeatLocal);
	SetActorRelativeRotation(FRotator::ZeroRotator);
}

bool ATN_BuggyGunnerPawn::IsSeatedGunner() const
{
	return Buggy && Controller && Buggy->GetSeatController(ETNRallySeat::Gunner) == Controller && Buggy->GetGunnerPawn() == this;
}

void ATN_BuggyGunnerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsLocallyControlled() || !Buggy)
	{
		return;
	}
	UpdateCamera();
	AimSendAccumulator += DeltaSeconds;
	if (AimSendAccumulator >= 1.f / FMath::Max(AimSendRate, 1.f) && !LocalAim.Equals(LastSentAim, 0.5f))
	{
		AimSendAccumulator = 0.f;
		LastSentAim = LocalAim;
		ServerSetAim(static_cast<float>(LocalAim.Yaw), static_cast<float>(LocalAim.Pitch));
	}
	if (bSelfRightHeld && TNBuggy::AdvanceHold(RespawnHold, true, DeltaSeconds, Buggy->GetData()->RespawnHoldSeconds))
	{
		ServerRequestRespawn();
	}
}

void ATN_BuggyGunnerPawn::UpdateCamera()
{
	// Detrás de la torreta: guiñada del buggy más la del apuntado, sin alabeo, con parte del cabeceo de la torreta.
	const float Yaw = static_cast<float>(Buggy->GetActorRotation().Yaw + LocalAim.Yaw);
	const float Pitch = TNGunnerDetail::BasePitchDeg + TNGunnerDetail::PitchFollow * static_cast<float>(LocalAim.Pitch);
	SpringArm->SetWorldRotation(FRotator(Pitch, Yaw, 0.f));
}

void ATN_BuggyGunnerPawn::AddAim(float DeltaYaw, float DeltaPitch)
{
	LocalAim = TNRallyTurret::ClampAim(FRotator(LocalAim.Pitch + DeltaPitch, LocalAim.Yaw + DeltaYaw, 0.f));
}

UTN_BuggyInputSet* ATN_BuggyGunnerPawn::GetInputSet()
{
	if (!InputSet)
	{
		InputSet = UTN_BuggyInputSet::Create(this);
	}
	return InputSet;
}

void ATN_BuggyGunnerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTNBuggy, Error, TEXT("%s: la artillera necesita un UEnhancedInputComponent"), *GetName());
		return;
	}
	const UTN_BuggyInputSet* Set = GetInputSet();
	Input->BindAction(Set->AimMouse, ETriggerEvent::Triggered, this, &ATN_BuggyGunnerPawn::OnAimMouse);
	Input->BindAction(Set->AimStick, ETriggerEvent::Triggered, this, &ATN_BuggyGunnerPawn::OnAimStick);
	Input->BindAction(Set->FireCoco, ETriggerEvent::Triggered, this, &ATN_BuggyGunnerPawn::OnFireCoco);
	Input->BindAction(Set->FireSpecial, ETriggerEvent::Started, this, &ATN_BuggyGunnerPawn::OnFireSpecial);
	Input->BindAction(Set->SelfRight, ETriggerEvent::Started, this, &ATN_BuggyGunnerPawn::OnSelfRightPressed);
	Input->BindAction(Set->SelfRight, ETriggerEvent::Completed, this, &ATN_BuggyGunnerPawn::OnSelfRightReleased);
}

void ATN_BuggyGunnerPawn::NotifyControllerChanged()
{
	if (InputSet)
	{
		UTN_BuggyInputSet::RemoveContext(Cast<APlayerController>(PreviousController), InputSet->GunnerContext);
	}
	if (const APlayerController* PC = Cast<APlayerController>(Controller); PC && PC->IsLocalController())
	{
		UTN_BuggyInputSet::AddContext(PC, GetInputSet()->GunnerContext);
	}
	bSelfRightHeld = false;
	RespawnHold = TNBuggy::FHold();
	Super::NotifyControllerChanged();
}

void ATN_BuggyGunnerPawn::OnAimMouse(const FInputActionValue& Value)
{
	const FVector2D Delta = Value.Get<FVector2D>();
	AddAim(Delta.X * MouseDegreesPerUnit, Delta.Y * MouseDegreesPerUnit);
}

void ATN_BuggyGunnerPawn::OnAimStick(const FInputActionValue& Value)
{
	const FVector2D Rate = Value.Get<FVector2D>();
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	AddAim(Rate.X * StickDegreesPerSecond * Dt, Rate.Y * StickDegreesPerSecond * Dt);
}

void ATN_BuggyGunnerPawn::OnFireCoco(const FInputActionValue& Value)
{
	// Mantener el botón pide a la cadencia del coco; el servidor la vuelve a comprobar.
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireRequest >= TNRallyTurret::SpecFor(ETNRallyAmmo::Coco).FireInterval)
	{
		LastFireRequest = Now;
		ServerFire(false, static_cast<float>(LocalAim.Yaw), static_cast<float>(LocalAim.Pitch));
	}
}

void ATN_BuggyGunnerPawn::OnFireSpecial(const FInputActionValue& Value)
{
	ServerFire(true, static_cast<float>(LocalAim.Yaw), static_cast<float>(LocalAim.Pitch));
}

void ATN_BuggyGunnerPawn::OnSelfRightPressed(const FInputActionValue& Value)
{
	bSelfRightHeld = true;
	RespawnHold = TNBuggy::FHold();
	ServerSelfRight();
}

void ATN_BuggyGunnerPawn::OnSelfRightReleased(const FInputActionValue& Value)
{
	bSelfRightHeld = false;
	RespawnHold = TNBuggy::FHold();
}

bool ATN_BuggyGunnerPawn::ServerSetAim_Validate(float Yaw, float Pitch)
{
	return TNRallyTurret::IsAimFinite(Yaw, Pitch);
}

void ATN_BuggyGunnerPawn::ServerSetAim_Implementation(float Yaw, float Pitch)
{
	if (IsSeatedGunner())
	{
		// SetAimRelative limita el cabeceo a -10..+45: un cliente no puede apuntar fuera.
		Buggy->GetTurret()->SetAimRelative(FRotator(Pitch, Yaw, 0.f));
	}
}

bool ATN_BuggyGunnerPawn::ServerFire_Validate(bool bSpecial, float Yaw, float Pitch)
{
	return TNRallyTurret::IsAimFinite(Yaw, Pitch);
}

void ATN_BuggyGunnerPawn::ServerFire_Implementation(bool bSpecial, float Yaw, float Pitch)
{
	if (!IsSeatedGunner())
	{
		return;
	}
	UTN_BuggyTurretComponent* Turret = Buggy->GetTurret();
	Turret->SetAimRelative(FRotator(Pitch, Yaw, 0.f));
	Turret->TryFire(bSpecial, Turret->GetAimWorldDirection());
}

void ATN_BuggyGunnerPawn::ServerSelfRight_Implementation()
{
	if (IsSeatedGunner())
	{
		// Llamada en el servidor: la RPC del buggy se ejecuta aquí mismo.
		Buggy->ServerSelfRight();
	}
}

void ATN_BuggyGunnerPawn::ServerRequestRespawn_Implementation()
{
	if (IsSeatedGunner())
	{
		Buggy->ServerRequestRespawn();
	}
}
