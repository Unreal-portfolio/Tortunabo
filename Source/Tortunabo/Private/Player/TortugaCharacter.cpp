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
#include "Player/TN_StaminaComponent.h"
#include "World/TN_InteractableBase.h"
#include "World/TN_PickupInteractableBase.h"
#include "World/TN_ThrowableItemActor.h"
#include "GameFramework/PlayerState.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ATortugaCharacter::ATortugaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;   // needed for leg animation
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	SetNetUpdateFrequency(45.f);
	SetMinNetUpdateFrequency(20.f);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;

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
	SprintAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Sprint.IA_Sprint")));
	DropItemAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_DropItem.IA_DropItem")));

	InventoryComponent = CreateDefaultSubobject<UTN_InventoryComponent>(TEXT("InventoryComponent"));
	StaminaComponent = CreateDefaultSubobject<UTN_StaminaComponent>(TEXT("StaminaComponent"));
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

	// Cache leg components (added in Blueprint as child SceneComponents).
	// The cube mesh origin must be at the TOP of each cube (the hip pivot).
	// If GetFName() doesn't match, check the component name in the BP Details panel.
	Pata1 = FindChildByName(TEXT("Pata1"));
	Pata2 = FindChildByName(TEXT("Pata2"));

	if (Pata1.IsValid()) { Pata1RestRot = Pata1->GetRelativeRotation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Pata1' component not found — add a SceneComponent named exactly 'Pata1' in the Blueprint.")); }

	if (Pata2.IsValid()) { Pata2RestRot = Pata2->GetRelativeRotation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Pata2' component not found — add a SceneComponent named exactly 'Pata2' in the Blueprint.")); }
}

void ATortugaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickLegAnimation(DeltaTime);
}

void ATortugaCharacter::TickLegAnimation(float DeltaTime)
{
	// Bail out early if neither leg is available.
	if (!Pata1.IsValid() && !Pata2.IsValid())
	{
		return;
	}

	// GetVelocity() is replicated by CharacterMovement — works on every machine.
	const float Speed = GetVelocity().Size2D();

	const bool bIsSprinting = StaminaComponent && StaminaComponent->IsSprinting();

	const float TargetAmplitude = bIsSprinting ? LegSprintAmplitudeDeg : LegWalkAmplitudeDeg;
	const float TargetFrequency = bIsSprinting ? LegSprintFrequency    : LegWalkFrequency;

	// Fade the amplitude envelope smoothly when starting/stopping movement.
	const float TargetMult = (Speed > LegMinSpeed) ? 1.f : 0.f;
	LegAmplitudeMultiplier = FMath::FInterpTo(LegAmplitudeMultiplier, TargetMult, DeltaTime, 8.f);

	// Advance phase only while the character is moving (avoids phase pop on stop/resume).
	if (Speed > LegMinSpeed)
	{
		LegPhaseAccumulator += TargetFrequency * DeltaTime;
		LegPhaseAccumulator  = FMath::Fmod(LegPhaseAccumulator, 1.f); // keep in [0,1)
	}

	// Current pendulum angle.
	const float Angle = TargetAmplitude * LegAmplitudeMultiplier
	                  * FMath::Sin(LegPhaseAccumulator * 2.f * PI);

	// Pata1 and Pata2 are 180° out of phase → diagonal trot gait.
	if (Pata1.IsValid()) { ApplyLegAngle(Pata1.Get(), Pata1RestRot,  Angle); }
	if (Pata2.IsValid()) { ApplyLegAngle(Pata2.Get(), Pata2RestRot, -Angle); }
}

void ATortugaCharacter::ApplyLegAngle(USceneComponent* Comp, const FRotator& RestRot, float AngleDeg) const
{
	// Build an incremental rotation around the configured local axis.
	const FQuat SwingQuat(LegSwingAxis.GetSafeNormal(), FMath::DegreesToRadians(AngleDeg));

	// Compose with the rest rotation so the animation is additive.
	const FRotator FinalRot = (FQuat(RestRot) * SwingQuat).Rotator();
	Comp->SetRelativeRotation(FinalRot);
}

USceneComponent* ATortugaCharacter::FindChildByName(FName Name) const
{
	for (UActorComponent* Comp : GetComponents())
	{
		if (Comp && Comp->GetFName() == Name)
		{
			return Cast<USceneComponent>(Comp);
		}
	}
	return nullptr;
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
	LoadedSprintAction = SprintAction.LoadSynchronous();
	LoadedDropItemAction = DropItemAction.LoadSynchronous();
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
			EnhancedInput->BindAction(LoadedMoveAction, ETriggerEvent::Completed, this, &ATortugaCharacter::OnMoveReleased);
			EnhancedInput->BindAction(LoadedMoveAction, ETriggerEvent::Canceled, this, &ATortugaCharacter::OnMoveReleased);
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
		if (LoadedSprintAction)
		{
			EnhancedInput->BindAction(LoadedSprintAction, ETriggerEvent::Started, this, &ATortugaCharacter::StartSprint);
			EnhancedInput->BindAction(LoadedSprintAction, ETriggerEvent::Completed, this, &ATortugaCharacter::StopSprint);
			EnhancedInput->BindAction(LoadedSprintAction, ETriggerEvent::Canceled, this, &ATortugaCharacter::StopSprint);
		}
		if (LoadedDropItemAction)
		{
			EnhancedInput->BindAction(LoadedDropItemAction, ETriggerEvent::Started, this, &ATortugaCharacter::DropEquippedItem);
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

	LastMovementInput = MovementVector;
	RefreshSprintRequest();
}

void ATortugaCharacter::OnMoveReleased()
{
	LastMovementInput = FVector2D::ZeroVector;
	RefreshSprintRequest();
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
		TryUseEquippedItem();
		return;
	}

	ServerTryInteract(FocusedInteractable.Get());
}

void ATortugaCharacter::RotateInventory()
{
	if (InventoryComponent)
	{
		InventoryComponent->RotateItems();
	}
}

void ATortugaCharacter::StartSprint()
{
	bSprintHeld = true;
	RefreshSprintRequest();
}

void ATortugaCharacter::StopSprint()
{
	bSprintHeld = false;
	RefreshSprintRequest();
}

void ATortugaCharacter::DropEquippedItem()
{
	ServerDropEquippedItem();
}

void ATortugaCharacter::TryUseEquippedItem()
{
	ServerUseEquippedItem();
}

void ATortugaCharacter::RefreshSprintRequest()
{
	if (!StaminaComponent)
	{
		return;
	}

	const bool bHasForwardInput = LastMovementInput.Y > 0.25f;
	StaminaComponent->SetSprintRequested(bSprintHeld && bHasForwardInput);
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
	float PingDistanceAllowance = 0.f;
	if (const APlayerState* PS = GetPlayerState())
	{
		PingDistanceAllowance = FMath::Clamp(PS->ExactPing * 0.25f, 0.f, MaxLagCompensationDistance);
	}

	if (FVector::DistSquared(GetActorLocation(), Interactable->GetActorLocation()) > FMath::Square(MaxDistance + 100.f + PingDistanceAllowance))
	{
		return;
	}

	Interactable->Interact(this);
}

void ATortugaCharacter::ServerUseEquippedItem_Implementation()
{
	if (!InventoryComponent || !StaminaComponent)
	{
		return;
	}

	if (!InventoryComponent->HasEquippedItem())
	{
		return;
	}

	const FTN_InventoryItem EquippedItem = InventoryComponent->GetEquippedItem();
	if (!EquippedItem.IsValid())
	{
		return;
	}

	if (EquippedItem.UseType == ETN_ItemUseType::SelfStaminaBoost)
	{
		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem))
		{
			return;
		}

		GrantInfiniteStamina(EquippedItem.StaminaUnlimitedDurationSeconds);
		return;
	}

	if (EquippedItem.UseType == ETN_ItemUseType::Throwable && EquippedItem.ThrowableActorClass)
	{
		const FVector SpawnLocation = GetItemSpawnLocation();
		const FVector ThrowDirection = GetItemForwardDirection();
		const FVector LaunchVelocity = ThrowDirection * FMath::Max(EquippedItem.ThrowSpeed, 0.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem))
		{
			return;
		}

		if (ATN_ThrowableItemActor* ThrowableActor = GetWorld()->SpawnActor<ATN_ThrowableItemActor>(EquippedItem.ThrowableActorClass, SpawnLocation, ThrowDirection.Rotation(), SpawnParams))
		{
			ThrowableActor->InitializeThrow(SpawnLocation, LaunchVelocity);
		}
		else
		{
			InventoryComponent->TryAddOrReplaceEquipped(ConsumedItem, true);
		}
		return;
	}
}

void ATortugaCharacter::ServerDropEquippedItem_Implementation()
{
	if (!InventoryComponent)
	{
		return;
	}

	FTN_InventoryItem DroppedItem;
	if (!InventoryComponent->TryExtractEquippedItem(DroppedItem) || !DroppedItem.IsValid() || !DroppedItem.PickupActorClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FVector SpawnLocation = GetItemSpawnLocation();
	if (ATN_PickupInteractableBase* PickupActor = GetWorld()->SpawnActor<ATN_PickupInteractableBase>(DroppedItem.PickupActorClass, SpawnLocation, GetActorRotation(), SpawnParams))
	{
		PickupActor->InitializeFromInventoryItem(DroppedItem);
	}
}

FVector ATortugaCharacter::GetItemSpawnLocation() const
{
	return GetActorLocation() + (GetActorForwardVector() * 120.0f) + FVector(0.0f, 0.0f, 40.0f);
}

FVector ATortugaCharacter::GetItemForwardDirection() const
{
	if (Controller)
	{
		const FRotator ViewRotation = Controller->GetControlRotation();
		return ViewRotation.Vector().GetSafeNormal();
	}

	return GetActorForwardVector();
}

void ATortugaCharacter::GrantInfiniteStamina(float DurationSeconds)
{
	if (!StaminaComponent)
	{
		return;
	}

	StaminaComponent->GrantUnlimitedStamina(DurationSeconds);
}

