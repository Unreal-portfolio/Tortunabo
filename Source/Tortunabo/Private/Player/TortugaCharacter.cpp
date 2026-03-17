#include "Player/TortugaCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "Player/TN_InventoryComponent.h"
#include "World/TN_InteractableBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

ATortugaCharacter::ATortugaCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	DefaultMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Player.IMC_Player")));
	MoveAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Move.IA_Move")));
	LookAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Look.IA_Look")));
	JumpAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Jump.IA_Jump")));
	InteractAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Interact.IA_Interact")));
	RotateInventoryAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_RotateInventory.IA_RotateInventory")));

	InventoryComponent = CreateDefaultSubobject<UTN_InventoryComponent>(TEXT("InventoryComponent"));
}

void ATortugaCharacter::BeginPlay()
{
	Super::BeginPlay();
	CacheInputAssets();
	ApplyInputMappingIfLocal();

	if (IsLocallyControlled() && InteractionScanInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(InteractionScanTimerHandle, this, &ATortugaCharacter::UpdateFocusedInteractable, InteractionScanInterval, true);
	}
}

void ATortugaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InteractionScanTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ATortugaCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	CacheInputAssets();
	ApplyInputMappingIfLocal();
}

void ATortugaCharacter::CacheInputAssets()
{
	if (bInputAssetsLoaded)
	{
		return;
	}

	LoadedMappingContext = DefaultMappingContext.LoadSynchronous();
	LoadedMoveAction = MoveAction.LoadSynchronous();
	LoadedLookAction = LookAction.LoadSynchronous();
	LoadedJumpAction = JumpAction.LoadSynchronous();
	LoadedInteractAction = InteractAction.LoadSynchronous();
	LoadedRotateInventoryAction = RotateInventoryAction.LoadSynchronous();
	bInputAssetsLoaded = true;

	if (!LoadedMappingContext || !LoadedMoveAction || !LoadedLookAction || !LoadedJumpAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] Missing IMC/IA assets on TortugaCharacter. Configure /Game/Input/IMC_Player + IA_Move/IA_Look/IA_Jump."));
	}
}

void ATortugaCharacter::ApplyInputMappingIfLocal()
{
	if (!LoadedMappingContext)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSubsystem->ClearAllMappings();
				InputSubsystem->AddMappingContext(LoadedMappingContext, 0);
			}
		}
	}
}

void ATortugaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		CacheInputAssets();
		if (LoadedMoveAction)
		{
			EnhancedInput->BindAction(LoadedMoveAction, ETriggerEvent::Triggered, this, &ATortugaCharacter::Move);
		}
		if (LoadedLookAction)
		{
			EnhancedInput->BindAction(LoadedLookAction, ETriggerEvent::Triggered, this, &ATortugaCharacter::Look);
		}
		if (LoadedJumpAction)
		{
			EnhancedInput->BindAction(LoadedJumpAction, ETriggerEvent::Started, this, &ATortugaCharacter::Jump);
			EnhancedInput->BindAction(LoadedJumpAction, ETriggerEvent::Completed, this, &ATortugaCharacter::StopJumping);
		}
		if (LoadedInteractAction)
		{
			EnhancedInput->BindAction(LoadedInteractAction, ETriggerEvent::Started, this, &ATortugaCharacter::TryInteract);
		}
		if (LoadedRotateInventoryAction)
		{
			EnhancedInput->BindAction(LoadedRotateInventoryAction, ETriggerEvent::Started, this, &ATortugaCharacter::RotateInventory);
		}
	}
}

void ATortugaCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ATortugaCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATortugaCharacter::TryInteract()
{
	if (!FocusedInteractable.IsValid())
	{
		UpdateFocusedInteractable();
	}

	if (!FocusedInteractable.IsValid())
	{
		return;
	}

	if (HasAuthority())
	{
		ServerTryInteract(FocusedInteractable.Get());
	}
	else
	{
		ServerTryInteract(FocusedInteractable.Get());
	}
}

void ATortugaCharacter::RotateInventory()
{
	if (InventoryComponent)
	{
		InventoryComponent->RotateItems();
	}
}

void ATortugaCharacter::UpdateFocusedInteractable()
{
	if (!GetWorld())
	{
		FocusedInteractable = nullptr;
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	ResolveInteractionViewPoint(ViewLocation, ViewDirection);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TN_InteractionTrace), false, this);
	const FVector TraceEnd = ViewLocation + (ViewDirection * MaxInteractionDistance);

	ATN_InteractableBase* NewFocused = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		NewFocused = Cast<ATN_InteractableBase>(Hit.GetActor());
	}

	FocusedInteractable = NewFocused;
}

void ATortugaCharacter::ResolveInteractionViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	if (FollowCamera)
	{
		OutLocation = FollowCamera->GetComponentLocation();
		OutDirection = FollowCamera->GetForwardVector();
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(OutLocation, ViewRotation);
		OutDirection = ViewRotation.Vector();
		return;
	}

	OutLocation = GetActorLocation();
	OutDirection = GetActorForwardVector();
}

void ATortugaCharacter::ServerTryInteract_Implementation(ATN_InteractableBase* Interactable)
{
	if (!Interactable || !Interactable->CanInteract(this))
	{
		return;
	}

	const float MaxDistance = FMath::Max(MaxInteractionDistance, Interactable->GetInteractionDistance());
	if (FVector::DistSquared(GetActorLocation(), Interactable->GetActorLocation()) > FMath::Square(MaxDistance + 100.f))
	{
		return;
	}

	Interactable->Interact(this);
}

