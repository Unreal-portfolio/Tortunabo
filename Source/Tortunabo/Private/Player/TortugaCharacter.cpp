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
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"

// ── CVar de debug ─────────────────────────────────────────────────────────────
// Activar en consola con: TN.Debug.Interaction 1
// Desactivar con: TN.Debug.Interaction 0
static TAutoConsoleVariable<int32> CVarDebugInteraction(
	TEXT("TN.Debug.Interaction"),
	0,
	TEXT("1 = Draw debug lines/spheres para el raycast de interacción y logs detallados. 0 = off."),
	ECVF_Cheat);

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

	// Emote actions: 10 slots (0-9), override paths in BP_TortugaCharacter Class Defaults.
	EmoteActions.SetNum(10);
	for (int32 i = 0; i < 10; i++)
	{
		EmoteActions[i] = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
			FString::Printf(TEXT("/Game/Input/IA_Emote%d.IA_Emote%d"), i, i)));
	}

	InventoryComponent = CreateDefaultSubobject<UTN_InventoryComponent>(TEXT("InventoryComponent"));
	StaminaComponent = CreateDefaultSubobject<UTN_StaminaComponent>(TEXT("StaminaComponent"));

	// Emote sounds: 10 slots (one per emote), assign in BP Class Defaults.
	EmoteSounds.SetNum(10);

	// Make capsule invisible to camera traces → the spring arm won't collide
	// with other players' capsules. Each player's own pawn is already auto-ignored.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void ATortugaCharacter::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] BeginPlay '%s' — LocallyControlled=%s  HasAuthority=%s  Controller=%s"),
		*GetName(),
		IsLocallyControlled() ? TEXT("YES") : TEXT("NO"),
		HasAuthority() ? TEXT("YES") : TEXT("NO"),
		GetController() ? *GetController()->GetName() : TEXT("NULL"));

	CacheInputAssets();
	ApplyInputMappingIfLocal();

	if (IsLocallyControlled() && InteractionScanInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(InteractionScanTimerHandle, this, &ATortugaCharacter::UpdateFocusedInteractable, InteractionScanInterval, true);
		UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] Interaction scan timer started (interval=%.2fs)"), InteractionScanInterval);
	}

	// Cache leg components (added in Blueprint as child SceneComponents).
	// The cube mesh origin must be at the TOP of each cube (the hip pivot).
	// If GetFName() doesn't match, check the component name in the BP Details panel.
	Pata1 = FindChildByName(TEXT("Pata1"));
	Pata2 = FindChildByName(TEXT("Pata2"));

	if (Pata1.IsValid()) { Pata1RestRot = Pata1->GetRelativeRotation(); Pata1RestLoc = Pata1->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Pata1' component not found — add a SceneComponent named exactly 'Pata1' in the Blueprint.")); }

	if (Pata2.IsValid()) { Pata2RestRot = Pata2->GetRelativeRotation(); Pata2RestLoc = Pata2->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Pata2' component not found — add a SceneComponent named exactly 'Pata2' in the Blueprint.")); }

	// Cache arm, tail and head components for the emote system (same pattern as legs).
	Brazo1 = FindChildByName(TEXT("Brazo1"));
	Brazo2 = FindChildByName(TEXT("Brazo2"));
	Cola   = FindChildByName(TEXT("Cola"));
	Cabeza = FindChildByName(TEXT("Cabeza"));

	if (Brazo1.IsValid()) { Brazo1RestRot = Brazo1->GetRelativeRotation(); Brazo1RestLoc = Brazo1->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Brazo1' not found — emotes will be partial. Add a SceneComponent named exactly 'Brazo1' in the Blueprint.")); }

	if (Brazo2.IsValid()) { Brazo2RestRot = Brazo2->GetRelativeRotation(); Brazo2RestLoc = Brazo2->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Brazo2' not found — emotes will be partial.")); }

	if (Cola.IsValid()) { ColaRestRot = Cola->GetRelativeRotation(); ColaRestLoc = Cola->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Cola' not found — emotes will be partial.")); }

	if (Cabeza.IsValid()) { CabezaRestRot = Cabeza->GetRelativeRotation(); CabezaRestLoc = Cabeza->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Cabeza' not found — emotes will be partial. Add a SceneComponent named exactly 'Cabeza' in the Blueprint.")); }

	// Guardar la rotación por defecto del mesh para restaurarla tras knockdown
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshDefaultRelativeRotation = MeshComp->GetRelativeRotation();
	}
}

void ATortugaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickEmote(DeltaTime);          // emote system (overrides leg anim when active)
	TickLegAnimation(DeltaTime);   // normal locomotion (suppressed during emotes)
}

void ATortugaCharacter::TickLegAnimation(float DeltaTime)
{
	// Suppressed while an emote (or its blend-out) controls all 5 components.
	if (ActiveEmoteIndex >= 0 || bEmoteBlendingOut)
	{
		return;
	}

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
	FocusedInteractable = nullptr;
	ActiveEmoteIndex   = -1;
	bEmoteBlendingOut  = false;

	StopEmoteSound();

	GetWorldTimerManager().ClearTimer(InteractionScanTimerHandle);
	GetWorldTimerManager().ClearTimer(KnockdownTimerHandle);
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

	// Load emote actions (10 slots)
	LoadedEmoteActions.SetNum(10);
	for (int32 i = 0; i < EmoteActions.Num() && i < 10; i++)
	{
		LoadedEmoteActions[i] = EmoteActions[i].LoadSynchronous();
	}

	bInputAssetsLoaded = true;

	// ── Log de cada asset para diagnosticar qué falta ─────────────────────────
	auto LogAsset = [](const TCHAR* Name, const UObject* Asset)
	{
		if (Asset)
		{
			UE_LOG(LogTemp, Log, TEXT("[Input] ✓ %s loaded: %s"), Name, *Asset->GetPathName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Input] ✗ %s FAILED TO LOAD — create this asset in /Game/Input/"), Name);
		}
	};

	LogAsset(TEXT("IMC_Player"), LoadedMappingContext);
	LogAsset(TEXT("IA_Move"), LoadedMoveAction);
	LogAsset(TEXT("IA_Look"), LoadedLookAction);
	LogAsset(TEXT("IA_Jump"), LoadedJumpAction);
	LogAsset(TEXT("IA_Interact"), LoadedInteractAction);
	LogAsset(TEXT("IA_RotateInventory"), LoadedRotateInventoryAction);
	LogAsset(TEXT("IA_Sprint"),           LoadedSprintAction);
	LogAsset(TEXT("IA_DropItem"),         LoadedDropItemAction);

	for (int32 i = 0; i < LoadedEmoteActions.Num(); i++)
	{
		const FString EmoteName = FString::Printf(TEXT("IA_Emote%d"), i);
		LogAsset(*EmoteName, LoadedEmoteActions[i]);
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

	UE_LOG(LogTemp, Log, TEXT("[Input] SetupPlayerInputComponent called on '%s' (LocallyControlled=%s)"),
		*GetName(), IsLocallyControlled() ? TEXT("YES") : TEXT("NO"));

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
			UE_LOG(LogTemp, Log, TEXT("[Input] ✓ IA_Interact bound to TryInteract"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Input] ✗ IA_Interact NOT bound — asset is null! Create /Game/Input/IA_Interact"));
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

		// ── Emotes 0–9 ────────────────────────────────────────────────────────
		static void (ATortugaCharacter::* const EmoteHandlers[10])() =
		{
			&ATortugaCharacter::OnEmote0, &ATortugaCharacter::OnEmote1,
			&ATortugaCharacter::OnEmote2, &ATortugaCharacter::OnEmote3,
			&ATortugaCharacter::OnEmote4, &ATortugaCharacter::OnEmote5,
			&ATortugaCharacter::OnEmote6, &ATortugaCharacter::OnEmote7,
			&ATortugaCharacter::OnEmote8, &ATortugaCharacter::OnEmote9,
		};
		for (int32 i = 0; i < LoadedEmoteActions.Num() && i < 10; i++)
		{
			if (LoadedEmoteActions[i])
			{
				EnhancedInput->BindAction(LoadedEmoteActions[i], ETriggerEvent::Started, this, EmoteHandlers[i]);
				UE_LOG(LogTemp, Log, TEXT("[Input] ✓ IA_Emote%d bound"), i);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Input] ✗ PlayerInputComponent is NOT an EnhancedInputComponent! Check DefaultInput.ini uses EnhancedPlayerInput."));
	}
}

void ATortugaCharacter::Move(const FInputActionValue& Value)
{
	// Cancel any active emote the moment the player moves —
	// EXCEPT emotes 5 (Baile Irlandés) and 6 (Superman) which are walkable.
	if (ActiveEmoteIndex >= 0 && ActiveEmoteIndex != 5 && ActiveEmoteIndex != 6) { CancelEmote(); }

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
	const bool bDebug = CVarDebugInteraction.GetValueOnGameThread() != 0;

	if (bDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("[Interact:DEBUG] === E PRESSED === FocusedInteractable: %s"),
			FocusedInteractable.IsValid() ? *FocusedInteractable->GetName() : TEXT("(none)"));
	}

	// Si no hay foco, intentar un scan inmediato
	if (!FocusedInteractable.IsValid())
	{
		UpdateFocusedInteractable();
	}

	// Si tras el scan sigue sin haber interactuable → usar ítem equipado
	if (!FocusedInteractable.IsValid())
	{
		if (bDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact:DEBUG] No interactable in focus → TryUseEquippedItem"));
		}
		TryUseEquippedItem();
		return;
	}

	if (bDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("[Interact:DEBUG] Sending ServerTryInteract → %s (CanInteract client-side: %s)"),
			*FocusedInteractable->GetName(),
			FocusedInteractable->CanInteract(this) ? TEXT("YES") : TEXT("NO"));
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
	if (!GetWorld()) { FocusedInteractable = nullptr; return; }

	const bool bDebug = CVarDebugInteraction.GetValueOnGameThread() != 0;

	// ── Detección por proximidad: esfera alrededor del personaje ─────────────
	// No usa raycast ni cámara — el jugador solo tiene que acercarse al objeto.
	// Busca todos los actores WorldDynamic en el radio y escoge el más cercano
	// que sea un ATN_InteractableBase válido.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TN_InteractionProximity), false);
	QueryParams.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic),
		FCollisionShape::MakeSphere(MaxInteractionDistance),
		QueryParams);

	ATN_InteractableBase* BestCandidate = nullptr;
	float BestDistSq = FLT_MAX;

	for (const FOverlapResult& Result : Overlaps)
	{
		ATN_InteractableBase* Interactable = Cast<ATN_InteractableBase>(Result.GetActor());
		if (!Interactable || !Interactable->CanInteract(this)) { continue; }

		const float DistSq = FVector::DistSquared(GetActorLocation(), Interactable->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestCandidate = Interactable;
		}
	}

	if (FocusedInteractable.Get() != BestCandidate)
	{
		FocusedInteractable = BestCandidate;
		if (bDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact:DEBUG] Focus → %s  (dist=%.0f)"),
				BestCandidate ? *BestCandidate->GetName() : TEXT("(none)"),
				BestCandidate ? FVector::Dist(GetActorLocation(), BestCandidate->GetActorLocation()) : 0.f);
		}
	}

	if (bDebug)
	{
		// Mostrar la esfera de detección
		DrawDebugSphere(GetWorld(), GetActorLocation(), MaxInteractionDistance,
			16, BestCandidate ? FColor::Green : FColor::Silver,
			false, InteractionScanInterval * 1.5f, 0, 0.8f);

		if (BestCandidate)
		{
			DrawDebugLine(GetWorld(), GetActorLocation(), BestCandidate->GetActorLocation(),
				FColor::Cyan, false, InteractionScanInterval * 1.5f, 0, 2.f);
		}
	}
}

FVector ATortugaCharacter::FindGroundBelow(const FVector& WorldLocation) const
{
	if (!GetWorld()) { return WorldLocation; }

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TN_GroundTrace), false, this);
	const FVector Start = WorldLocation + FVector(0.f, 0.f, 30.f);
	const FVector End   = WorldLocation - FVector(0.f, 0.f, 1500.f);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, 5.f);
	}
	return WorldLocation;
}


void ATortugaCharacter::ServerTryInteract_Implementation(ATN_InteractableBase* Interactable)
{
	const bool bDebug = CVarDebugInteraction.GetValueOnGameThread() != 0;

	if (!Interactable)
	{
		if (bDebug) { UE_LOG(LogTemp, Warning, TEXT("[Interact:SERVER] Interactable is NULL — client sent invalid reference")); }
		return;
	}

	if (!Interactable->CanInteract(this))
	{
		if (bDebug) { UE_LOG(LogTemp, Warning, TEXT("[Interact:SERVER] CanInteract=FALSE for '%s' — already taken? disabled? no inventory space?"), *Interactable->GetName()); }
		return;
	}

	const float MaxDistance = FMath::Max(MaxInteractionDistance, Interactable->GetInteractionDistance());
	float PingDistanceAllowance = 0.f;
	if (const APlayerState* PS = GetPlayerState())
	{
		PingDistanceAllowance = FMath::Clamp(PS->ExactPing * 0.25f, 0.f, MaxLagCompensationDistance);
	}

	const float TotalAllowed = MaxDistance + 100.f + PingDistanceAllowance;
	const float ActualDist = FVector::Dist(GetActorLocation(), Interactable->GetActorLocation());

	if (ActualDist > TotalAllowed)
	{
		if (bDebug) { UE_LOG(LogTemp, Warning, TEXT("[Interact:SERVER] TOO FAR — dist=%.1f  allowed=%.1f  (MaxDist=%.1f + 100 + ping=%.1f)"), ActualDist, TotalAllowed, MaxDistance, PingDistanceAllowance); }
		return;
	}

	if (bDebug) { UE_LOG(LogTemp, Log, TEXT("[Interact:SERVER] ✓ Calling Interact on '%s' — dist=%.1f"), *Interactable->GetName(), ActualDist); }

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

		// Añadir ángulo hacia arriba para que el throwable haga arco
		const FVector RightVec = FVector::CrossProduct(FVector::UpVector, ThrowDirection).GetSafeNormal();
		const FQuat UpTilt(RightVec, FMath::DegreesToRadians(ThrowUpAngleDeg));
		const FVector ArcedDirection = UpTilt.RotateVector(ThrowDirection).GetSafeNormal();

		const FVector LaunchVelocity = ArcedDirection * FMath::Max(EquippedItem.ThrowSpeed, 0.0f);

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
			// SourceItem lleva PickupActorClass para que el throwable sepa
			// qué pickup spawnear cuando aterrice o impacte (se convierte en recogible)
			ThrowableActor->SetSourceItem(ConsumedItem);
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
	if (!InventoryComponent) { return; }

	FTN_InventoryItem DroppedItem;
	if (!InventoryComponent->TryExtractEquippedItem(DroppedItem) || !DroppedItem.IsValid() || !DroppedItem.PickupActorClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Siempre spawnear en el suelo aunque el personaje esté en el aire
	const FVector DropPoint = FindGroundBelow(GetItemSpawnLocation());

	if (ATN_PickupInteractableBase* PickupActor = GetWorld()->SpawnActor<ATN_PickupInteractableBase>(
		DroppedItem.PickupActorClass, DropPoint, FRotator::ZeroRotator, SpawnParams))
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

// ── Replicación ───────────────────────────────────────────────────────────────

void ATortugaCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Replicar a todos los clientes para que el visual sea visible en todos
	DOREPLIFETIME(ATortugaCharacter, bIsKnockedDown);
	// SkipOwner: el owner ya arranca el emote localmente en TriggerEmote/CancelEmote.
	DOREPLIFETIME_CONDITION(ATortugaCharacter, ReplicatedEmoteIndex, COND_SkipOwner);
}

// ── Knockdown ─────────────────────────────────────────────────────────────────

void ATortugaCharacter::ApplyKnockdown(float Duration)
{
	if (!HasAuthority())
	{
		return;
	}

	// Evitar solapar knockdowns
	if (bIsKnockedDown)
	{
		// Si ya está en knockdown, reiniciar el timer con la nueva duración
		GetWorldTimerManager().SetTimer(KnockdownTimerHandle, this,
		                                &ATortugaCharacter::RecoverFromKnockdown, Duration, false);
		return;
	}

	bIsKnockedDown = true;

	// Servidor (listen server) también necesita aplicar visual y bloqueo:
	// OnRep_IsKnockedDown no se dispara en el propietario de la variable.
	ApplyKnockdownVisual(true);
	if (UCharacterMovementComponent* MC = GetCharacterMovement())
	{
		MC->DisableMovement();
	}

	GetWorldTimerManager().SetTimer(KnockdownTimerHandle, this,
	                                &ATortugaCharacter::RecoverFromKnockdown, Duration, false);
}

void ATortugaCharacter::RecoverFromKnockdown()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsKnockedDown = false;

	// Listen-server aplica recuperación localmente (OnRep no se dispara aquí)
	ApplyKnockdownVisual(false);
	if (UCharacterMovementComponent* MC = GetCharacterMovement())
	{
		MC->SetMovementMode(MOVE_Walking);
	}
}

void ATortugaCharacter::OnRep_IsKnockedDown()
{
	// Aplicar visual en todos los clientes (propietario y observadores)
	ApplyKnockdownVisual(bIsKnockedDown);

	// El cliente propietario también necesita que su movimiento quede bloqueado
	// para que no envíe inputs al servidor durante el knockdown
	if (IsLocallyControlled())
	{
		if (UCharacterMovementComponent* MC = GetCharacterMovement())
		{
			if (bIsKnockedDown)
			{
				MC->DisableMovement();
			}
			else
			{
				MC->SetMovementMode(MOVE_Walking);
			}
		}
	}
}

void ATortugaCharacter::ApplyKnockdownVisual(bool bKnocked)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	if (bKnocked)
	{
		// Use RELATIVE rotation so CharacterMovementComponent replication
		// doesn't overwrite the visual on remote clients.
		FRotator KnockedRot = MeshDefaultRelativeRotation;
		KnockedRot.Pitch -= 100.0f;
		MeshComp->SetRelativeRotation(KnockedRot);
	}
	else
	{
		MeshComp->SetRelativeRotation(MeshDefaultRelativeRotation);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// EMOTE SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::TriggerEmote(int32 Index)
{
	if (!IsLocallyControlled() || Index < 0 || Index > 9) { return; }

	// Start locally for immediate visual feedback
	StartEmoteLocally(Index);

	// Replicate to server → other clients
	ServerSetEmote(Index);
}

void ATortugaCharacter::StartEmoteLocally(int32 Index)
{
	// Reset all components to rest pose (clears stale location offsets)
	if (Brazo1.IsValid()) { Brazo1->SetRelativeRotation(Brazo1RestRot); Brazo1->SetRelativeLocation(Brazo1RestLoc); }
	if (Brazo2.IsValid()) { Brazo2->SetRelativeRotation(Brazo2RestRot); Brazo2->SetRelativeLocation(Brazo2RestLoc); }
	if (Pata1.IsValid())  { Pata1->SetRelativeRotation(Pata1RestRot);  Pata1->SetRelativeLocation(Pata1RestLoc);  }
	if (Pata2.IsValid())  { Pata2->SetRelativeRotation(Pata2RestRot);  Pata2->SetRelativeLocation(Pata2RestLoc);  }
	if (Cola.IsValid())   { Cola->SetRelativeRotation(ColaRestRot);    Cola->SetRelativeLocation(ColaRestLoc);    }
	if (Cabeza.IsValid()) { Cabeza->SetRelativeRotation(CabezaRestRot); Cabeza->SetRelativeLocation(CabezaRestLoc); }

	// (Re)start — pressing the same emote while it plays restarts from the top.
	ActiveEmoteIndex   = Index;
	EmoteTime          = 0.f;
	bEmoteBlendingOut  = false;
	EmoteBlendOutTimer = 0.f;

	// Start looping audio for this emote (proximity-attenuated).
	PlayEmoteSound(Index);
}

void ATortugaCharacter::ServerSetEmote_Implementation(int32 Index)
{
	ReplicatedEmoteIndex = Index;

	// Listen server: OnRep no dispara en la máquina que posee la variable,
	// así que arrancamos/cancelamos el emote directamente para pawns remotos.
	if (!IsLocallyControlled())
	{
		if (Index >= 0)
		{
			StartEmoteLocally(Index);
		}
		else
		{
			CancelEmote();
		}
	}
}

void ATortugaCharacter::OnRep_ReplicatedEmoteIndex()
{
	// Clientes remotos: arrancar o cancelar el emote en este pawn.
	// Saltar si es el pawn local (ya lo arrancó en TriggerEmote).
	if (!IsLocallyControlled())
	{
		if (ReplicatedEmoteIndex >= 0)
		{
			StartEmoteLocally(ReplicatedEmoteIndex);
		}
		else
		{
			CancelEmote();
		}
	}
}

void ATortugaCharacter::CancelEmote()
{
	if (ActiveEmoteIndex < 0 && !bEmoteBlendingOut) { return; }

	// Snapshot current component rotations so we can blend back to rest.
	if (Brazo1.IsValid()) { SnapshotBrazo1 = Brazo1->GetRelativeRotation(); SnapshotBrazo1Loc = Brazo1->GetRelativeLocation(); }
	if (Brazo2.IsValid()) { SnapshotBrazo2 = Brazo2->GetRelativeRotation(); SnapshotBrazo2Loc = Brazo2->GetRelativeLocation(); }
	if (Pata1.IsValid())  { SnapshotPata1  = Pata1->GetRelativeRotation();  SnapshotPata1Loc  = Pata1->GetRelativeLocation();  }
	if (Pata2.IsValid())  { SnapshotPata2  = Pata2->GetRelativeRotation();  SnapshotPata2Loc  = Pata2->GetRelativeLocation();  }
	if (Cola.IsValid())   { SnapshotCola   = Cola->GetRelativeRotation();   SnapshotColaLoc   = Cola->GetRelativeLocation();   }
	if (Cabeza.IsValid()) { SnapshotCabeza = Cabeza->GetRelativeRotation(); SnapshotCabezaLoc = Cabeza->GetRelativeLocation(); }

	ActiveEmoteIndex   = -1;
	bEmoteBlendingOut  = true;
	EmoteBlendOutTimer = 0.f;

	// Stop looping emote audio.
	StopEmoteSound();

	// Replicar cancelación a otros clientes (solo desde el jugador que controla)
	if (IsLocallyControlled())
	{
		ServerSetEmote(-1);
	}
}

void ATortugaCharacter::ApplyEmoteAngle(USceneComponent* Comp, const FRotator& Rest,
                                        float AngleDeg, const FVector& Axis) const
{
	// Swing en ESPACIO DEL PADRE: Swing * Rest.
	// Los ejes AX/AY/AZ son del padre (raíz del personaje), no del componente.
	// Así AX produce el mismo efecto visual en Brazo1 y Brazo2 aunque tengan rests espejados.
	const FQuat SwingQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(AngleDeg));
	Comp->SetRelativeRotation((SwingQuat * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::ApplyEmoteAngles2(USceneComponent* Comp, const FRotator& Rest,
                                          float A1, const FVector& Ax1,
                                          float A2, const FVector& Ax2) const
{
	const FQuat Q1(Ax1.GetSafeNormal(), FMath::DegreesToRadians(A1));
	const FQuat Q2(Ax2.GetSafeNormal(), FMath::DegreesToRadians(A2));
	Comp->SetRelativeRotation((Q2 * Q1 * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::ApplyEmoteAngles3(USceneComponent* Comp, const FRotator& Rest,
                                          float A1, const FVector& Ax1,
                                          float A2, const FVector& Ax2,
                                          float A3, const FVector& Ax3) const
{
	const FQuat Q1(Ax1.GetSafeNormal(), FMath::DegreesToRadians(A1));
	const FQuat Q2(Ax2.GetSafeNormal(), FMath::DegreesToRadians(A2));
	const FQuat Q3(Ax3.GetSafeNormal(), FMath::DegreesToRadians(A3));
	Comp->SetRelativeRotation((Q3 * Q2 * Q1 * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::TickEmote(float DeltaTime)
{
	// ── Blend-out: lerp all components back to rest rotations & locations ─────
	if (bEmoteBlendingOut)
	{
		EmoteBlendOutTimer += DeltaTime;
		const float Alpha = FMath::Min(EmoteBlendOutTimer / FMath::Max(EmoteBlendOutDuration, KINDA_SMALL_NUMBER), 1.f);

		auto Blend = [Alpha](TWeakObjectPtr<USceneComponent>& Comp, const FRotator& SnapR, const FRotator& RestR,
		                     const FVector& SnapL, const FVector& RestL)
		{
			if (Comp.IsValid())
			{
				Comp->SetRelativeRotation(FMath::Lerp(SnapR, RestR, Alpha));
				Comp->SetRelativeLocation(FMath::Lerp(SnapL, RestL, Alpha));
			}
		};

		Blend(Brazo1, SnapshotBrazo1, Brazo1RestRot, SnapshotBrazo1Loc, Brazo1RestLoc);
		Blend(Brazo2, SnapshotBrazo2, Brazo2RestRot, SnapshotBrazo2Loc, Brazo2RestLoc);
		Blend(Pata1,  SnapshotPata1,  Pata1RestRot,  SnapshotPata1Loc,  Pata1RestLoc);
		Blend(Pata2,  SnapshotPata2,  Pata2RestRot,  SnapshotPata2Loc,  Pata2RestLoc);
		Blend(Cola,   SnapshotCola,   ColaRestRot,   SnapshotColaLoc,   ColaRestLoc);
		Blend(Cabeza, SnapshotCabeza, CabezaRestRot, SnapshotCabezaLoc, CabezaRestLoc);

		if (Alpha >= 1.f) { bEmoteBlendingOut = false; }
		return;
	}

	if (ActiveEmoteIndex < 0) { return; }

	EmoteTime += DeltaTime;
	const float T = EmoteTime;

	// Trig helpers (frequency in Hz)
	auto S    = [](float Hz, float t) { return FMath::Sin(2.f * PI * Hz * t); };
	auto Cos  = [](float Hz, float t) { return FMath::Cos(2.f * PI * Hz * t); };
	auto Sat  = [](float v)           { return FMath::Clamp(v, 0.f, 1.f); };

	// ── Ejes de referencia (espacio del PADRE — T-Pose) ─────────────────────
	// Setup: SceneComponents a rot (0,0,0), brazos extendidos por ±Y (T-Pose).
	//
	// BRAZOS (mesh por ±Y desde el joint):
	//   AX (1,0,0) adelante  → sube/baja visto de frente  (+AX = arriba)
	//   AY (0,1,0) lateral   → roll sobre eje largo del brazo (casi invisible en cubos)
	//   AZ (0,0,1) arriba    → adelante/atrás             (+AZ = backward, −AZ = forward)
	//
	// CABEZA (mesh por +X desde el joint):
	//   AX = roll (rara vez útil)
	//   AY = cabeceo arriba/abajo (nod)
	//   AZ = girar izquierda/derecha (shake)
	//
	// PATAS (depende de orientación del mesh — si cuelgan por −Z):
	//   LX (1,0,0) = splay lateral (abrir/cerrar piernas)
	//   LY (0,1,0) = stride adelante/atrás  ← LegSwingAxis configurable
	//   LZ (0,0,1) = twist/giro
	//   Si las patas están en T-Pose (±Y), cambiar LegSwingAxis a (0,0,1).
	//
	// COLA (mesh por −X desde el joint):
	//   TY = arriba/abajo   TZ = lado a lado   TX = roll cola
	const FVector AX(1.f, 0.f, 0.f);
	const FVector AY(0.f, 1.f, 0.f);
	const FVector AZ(0.f, 0.f, 1.f);

	// Patas
	const FVector LX(1.f, 0.f, 0.f);       // splay lateral
	const FVector LY = LegSwingAxis;         // stride adelante/atrás (configurable)
	const FVector LZ(0.f, 0.f, 1.f);       // giro pata

	// Cola
	const FVector TY = TailUpDownAxis;
	const FVector TZ = TailSideAxis;
	const FVector TX(1.f, 0.f, 0.f);       // roll cola

	// ── Helpers de aplicación (rotación) ──────────────────────────────────────
	auto Ap = [&](TWeakObjectPtr<USceneComponent>& Comp, const FRotator& Rest,
	              float Angle, const FVector& Axis)
	{
		if (Comp.IsValid()) { ApplyEmoteAngle(Comp.Get(), Rest, Angle, Axis); }
	};

	auto Ap2 = [&](TWeakObjectPtr<USceneComponent>& Comp, const FRotator& Rest,
	               float A1, const FVector& Ax1, float A2, const FVector& Ax2)
	{
		if (Comp.IsValid()) { ApplyEmoteAngles2(Comp.Get(), Rest, A1, Ax1, A2, Ax2); }
	};

	auto Ap3 = [&](TWeakObjectPtr<USceneComponent>& Comp, const FRotator& Rest,
	               float A1, const FVector& Ax1,
	               float A2, const FVector& Ax2,
	               float A3, const FVector& Ax3)
	{
		if (Comp.IsValid()) { ApplyEmoteAngles3(Comp.Get(), Rest, A1, Ax1, A2, Ax2, A3, Ax3); }
	};

	// ── Helper de aplicación (traslación) — para Explosivo y Modo Loco 2 ────
	auto SetLoc = [&](TWeakObjectPtr<USceneComponent>& Comp, const FVector& RestLoc, const FVector& Offset)
	{
		if (Comp.IsValid()) { Comp->SetRelativeLocation(RestLoc + Offset); }
	};

	bool bEnded = false;

	switch (ActiveEmoteIndex)
	{
	// ──────────────────────────────────────────────────────────────────────────
	// 0  SALUDAR — Brazo1 (dcha): AY 90° posiciona + AX ±75° aleteo.
	//              Brazo2 (izq) baja.  2.5 s
	// ──────────────────────────────────────────────────────────────────────────
	case 0:
	{
		const float setup   = Sat(T / 0.3f);
		const float fadeOut = (T > 2.0f) ? Sat((T - 2.0f) / 0.5f) : 0.f;
		const float env     = setup * (1.f - fadeOut);
		const float wave    = (T > 0.3f) ? S(5.f, T) : 0.f;

		// Brazo1 (dcha): AY 90° posiciona brazo, AX ±75° oscila (aleteo)
		Ap2(Brazo1, Brazo1RestRot,
		    env * 90.f,              AY,
		    env * 75.f * wave,       AX);

		// Brazo2 (izq): baja relajado
		Ap(Brazo2, Brazo2RestRot, env * 80.f, AX);

		// Cabeza
		Ap(Cabeza, CabezaRestRot, env * 8.f * S(2.f, T), AY);

		// Cola
		Ap(Cola, ColaRestRot, env * 10.f * S(1.5f, T), TZ);

		bEnded = (T >= 2.5f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 1  APLAUSO — Brazos en V cerrada, palmaditas rápidas.             3 s
	// ──────────────────────────────────────────────────────────────────────────
	case 1:
	{
		const float rise    = Sat(T / 0.4f);
		const float fadeOut = (T > 2.5f) ? Sat((T - 2.5f) / 0.5f) : 0.f;
		const float env     = rise * (1.f - fadeOut);
		const float bob     = (T > 0.4f) ? 12.f * S(4.f, T) : 0.f;   // 4 Hz, ±12°

		// Brazo1 (dcha): arriba +AX 50° (más cerrado) + palma al frente AY 90°
		Ap2(Brazo1, Brazo1RestRot,
		    env * 110.f + bob * env,   AX,
		    env * 90.f,               AY);
		// Brazo2 (izq): arriba −AX 50° + palma al frente AY +90°
		Ap2(Brazo2, Brazo2RestRot,
		    -(env * 110.f + bob * env), AX,
		    env * 90.f,                AY);

		// Cabeza
		Ap(Cabeza, CabezaRestRot, env * (-15.f), AY);

		// Patas
		Ap(Pata1, Pata1RestRot,  10.f * S(2.f, T) * env, LY);
		Ap(Pata2, Pata2RestRot, -10.f * S(2.f, T) * env, LY);

		// Cola
		Ap(Cola, ColaRestRot, env * 20.f * S(3.f, T), TZ);

		bEnded = (T >= 3.f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 2  HELICÓPTERO — Brazos giran solo en AZ a tope.                LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 2:
	{
		// Spin continuo: 8 vueltas/seg en AZ, solo rotación vertical
		const float spin = 360.f * T * 8.f;

		// Brazo1 (dcha): solo spin AZ
		Ap(Brazo1, Brazo1RestRot, spin, AZ);
		// Brazo2 (izq): spin opuesto AZ
		Ap(Brazo2, Brazo2RestRot, -spin, AZ);

		// Cabeza
		Ap(Cabeza, CabezaRestRot, 8.f * S(2.f, T), AY);

		// Patas marchan
		Ap(Pata1, Pata1RestRot,  25.f * S(2.f, T), LY);
		Ap(Pata2, Pata2RestRot, -25.f * S(2.f, T), LY);

		// Cola
		Ap(Cola, ColaRestRot, 12.f * S(1.f, T), TZ);

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 3  PALMADA POTENTE — Ambos brazos: AX 90° + AZ 90° posición,
	//     luego AY oscila −90° rápido abajo, lento arriba.            LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 3:
	{
		// 0.3s para que los brazos lleguen a la posición de partida
		const float setup = Sat(T / 0.3f);

		// Palmada asimétrica: baja rápido (25% ciclo), sube normal (50% ciclo), pausa (25%)
		const float clapT     = FMath::Max(T - 0.3f, 0.f);
		const float cycleTime = 0.5f;   // 2 palmadas/segundo
		float palmada = 0.f;
		if (clapT > 0.f)
		{
			const float phase = FMath::Fmod(clapT, cycleTime) / cycleTime;
			if (phase < 0.25f)
			{
				// Fast down: 0 → −90° en 25% del ciclo
				palmada = -90.f * (phase / 0.25f);
			}
			else if (phase < 0.75f)
			{
				// Normal up: −90° → 0 en 50% del ciclo
				palmada = -90.f * (1.f - (phase - 0.25f) / 0.50f);
			}
			// else: 25% pausa en reposo (brazos arriba antes de la siguiente palmada)
		}

		// Brazo1 (dcha): AX 90° sube + AZ 90° centra + AY palmada
		Ap3(Brazo1, Brazo1RestRot,
		    setup *  -90.f,  AX,
		    setup *  -90.f,  AZ,
		    palmada,        AY);
		// Brazo2 (izq): espejo (−AX, −AZ, misma AY)
		Ap3(Brazo2, Brazo2RestRot,
		    setup * 90.f,  AX,
		    setup * 90.f,  AZ,
		    palmada,        AY);

		// Cabeza asiente con cada palmada
		Ap(Cabeza, CabezaRestRot, 8.f * (palmada / -90.f), AY);

		// Cola menea
		Ap(Cola, ColaRestRot, 15.f * S(2.f, T), TZ);

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 4  APLAUDIR — Brazo1 (dcha): AX 120° + AY (20° + ±10° clap).
	//              Brazo2 (izq): reposo abajo. Cola: TY 180° rápido.   2 s
	// ──────────────────────────────────────────────────────────────────────────
	case 4:
	{
		const float setup   = Sat(T / 0.3f);
		const float clap    = S(6.f, T);
		const float colSnap = Sat(T / 0.2f); // cola rápido a posición

		// Brazo1 (dcha): AX 120° sube + AY 20° orienta + AY ±10° palmada rápida
		Ap2(Brazo1, Brazo1RestRot,
		    setup * -120.f,                      AX,
		    setup * (-20.f - 10.f * clap),       AY);

		// Brazo2 (izq): reposo abajo
		Ap(Brazo2, Brazo2RestRot, setup * 80.f, AX);

		// Cabeza
		Ap(Cabeza, CabezaRestRot, 5.f * S(6.f, T), AY);

		// Cola: entra rápido por −TY (0.2s), sale por el mismo lado (0.4s a partir de T=1.5)
		// — la vuelta es explícita para que no tome el arco opuesto en el blend-out.
		const float colOut = T > 2.5f ? Sat((T - 2.5f) / 0.1f) : 0.f;
		const float colEnv = colSnap * (1.f - colOut);
		Ap(Cola, ColaRestRot, colEnv * -175.f, TY);

		bEnded = (T >= 2.f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 5  BAILE IRLANDÉS — Ambos brazos se juntan por DETRÁS.         LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 5:
	{
		const float setup = Sat(T / 0.4f);

		// Brazo1 (dcha): atrás (+AZ), baja un poco (−AX)
		Ap2(Brazo1, Brazo1RestRot,
		    setup * (-10.f),    AX,
		    setup *  80.f,      AZ);
		// Brazo2 (izq):  atrás (−AZ), baja un poco (+AX = abajo para izq)
		Ap2(Brazo2, Brazo2RestRot,
		    setup *  10.f,      AX,
		    setup * (-80.f),    AZ);

		// Cabeza
		Ap(Cabeza, CabezaRestRot, 5.f * S(2.5f, T), AZ);

		// Patas
		Ap2(Pata1, Pata1RestRot,
		     30.f * S(2.5f, T),   LY,
		     15.f * S(1.25f, T),  LX);
		Ap2(Pata2, Pata2RestRot,
		    -30.f * S(2.5f, T),   LY,
		    -15.f * S(1.25f, T),  LX);

		// Cola
		Ap(Cola, ColaRestRot, 10.f * S(2.5f, T), TZ);

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 6  FLOTAR (SUPERMAN) — bien                                       5 s
	// ──────────────────────────────────────────────────────────────────────────
	case 6:
	{
		const float fadeIn  = Sat(T / 1.f);
		const float fadeOut = (T > 4.f) ? Sat((T - 4.f) / 1.f) : 0.f;
		const float env     = fadeIn * (1.f - fadeOut);
		const float drift   = 3.f * S(0.3f, T);

		Ap2(Brazo1, Brazo1RestRot,
		    env * 50.f + drift,    AX,
		    env * (-80.f),         AZ);
		Ap2(Brazo2, Brazo2RestRot,
		    -(env * 50.f) + drift, AX,
		    env * 80.f,            AZ);

		Ap(Cabeza, CabezaRestRot, env * (-20.f) + drift, AY);

		Ap(Pata1, Pata1RestRot, (env * 80.f) + drift, LY);
		Ap(Pata2, Pata2RestRot, (env * 80.f) + drift, LY);

		Ap2(Cola, ColaRestRot,
		    env * 20.f + drift,  TY,
		    drift * 2.f,         TZ);

		bEnded = (T >= 5.f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 7  SEÑALAR — Brazo1 apunta, Brazo2 baja reposo.                  1.2 s
	// ──────────────────────────────────────────────────────────────────────────
	case 7:
	{
		const float rise    = Sat(T / 0.1f);
		const float fadeOut = (T > 0.8f) ? Sat((T - 0.8f) / 0.4f) : 0.f;
		const float env     = rise * (1.f - fadeOut);

		// Brazo1 (dcha): apunta adelante
		Ap2(Brazo1, Brazo1RestRot,
		    env * 25.f,        AX,
		    env * (-80.f),     AZ);

		// Brazo2 (izq): reposo abajo
		Ap(Brazo2, Brazo2RestRot, env * 80.f, AX);

		Ap(Cabeza, CabezaRestRot, env * 10.f, AY);
		Ap(Cola, ColaRestRot, env * 8.f * S(1.5f, T), TZ);

		bEnded = (T >= 1.2f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 8  MODO LOCO 2 — Caos TOTAL: todos los componentes rotan y se
	//     trasladan a lo bestia (incluye Brazo2).                     LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 8:
	{
		// Rotación salvaje en TODOS los componentes
		Ap3(Brazo1, Brazo1RestRot,
		     50.f * S(3.0f, T),          AX,
		     40.f * S(2.1f, T),          AY,
		     20.f * S(4.7f, T),          AZ);
		Ap3(Brazo2, Brazo2RestRot,
		    -55.f * S(2.8f, T + 0.1f),   AX,
		    -40.f * S(3.3f, T + 0.2f),   AY,
		     25.f * S(4.1f, T),           AZ);
		Ap3(Pata1, Pata1RestRot,
		     35.f * S(4.0f, T),          LY,
		     20.f * S(2.7f, T),          LX,
		     10.f * S(5.0f, T),          LZ);
		Ap3(Pata2, Pata2RestRot,
		    -35.f * S(4.0f, T + 0.08f),  LY,
		    -20.f * S(2.7f, T + 0.15f),  LX,
		    -10.f * S(5.0f, T + 0.05f),  LZ);
		Ap3(Cola, ColaRestRot,
		    15.f * Cos(5.f, T),           TY,
		    25.f * S(7.f, T),             TZ,
		    10.f * S(3.f, T + 0.1f),      TX);
		Ap3(Cabeza, CabezaRestRot,
		    10.f * S(3.7f, T),            AY,
		    15.f * S(2.3f, T + 0.07f),    AZ,
		     5.f * S(5.5f, T),            AX);

		// Traslación: HIPER-EXAGERADA tipo explosión — ±250 cm
		const float locAmp = 250.f;
		SetLoc(Brazo1, Brazo1RestLoc, FVector(
		    locAmp * S(1.7f, T),  locAmp * S(2.3f, T + 0.1f),  locAmp * S(3.1f, T)));
		SetLoc(Brazo2, Brazo2RestLoc, FVector(
		    locAmp * S(2.1f, T + 0.05f), -locAmp * S(1.9f, T),  locAmp * S(3.5f, T + 0.1f)));
		SetLoc(Pata1, Pata1RestLoc, FVector(
		    locAmp * S(2.9f, T), -locAmp * S(1.3f, T + 0.15f), locAmp * S(3.7f, T)));
		SetLoc(Pata2, Pata2RestLoc, FVector(
		   -locAmp * S(2.3f, T + 0.1f),  locAmp * S(1.7f, T),  locAmp * Cos(3.3f, T)));
		SetLoc(Cola, ColaRestLoc, FVector(
		   -locAmp * S(1.1f, T), locAmp * S(2.9f, T + 0.05f), locAmp * S(4.1f, T)));
		SetLoc(Cabeza, CabezaRestLoc, FVector(
		    locAmp * 0.5f * S(2.5f, T), locAmp * 0.3f * S(3.1f, T + 0.12f), locAmp * 0.4f * S(4.3f, T)));

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 9  FIESTA (MODO LOCO) — Caos puro solo rotación, sin separar.     LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 9:
		Ap3(Brazo1, Brazo1RestRot,
		     50.f * S(3.0f, T),          AX,
		     40.f * S(2.1f, T),          AY,
		     20.f * S(4.7f, T),          AZ);
		Ap3(Brazo2, Brazo2RestRot,
		    -55.f * S(2.3f, T + 0.13f),  AX,
		    -35.f * S(3.1f, T),          AY,
		     25.f * S(1.7f, T + 0.08f),  AZ);
		Ap3(Pata1, Pata1RestRot,
		     35.f * S(4.0f, T),          LY,
		     20.f * S(2.7f, T),          LX,
		     10.f * S(5.0f, T),          LZ);
		Ap3(Pata2, Pata2RestRot,
		    -35.f * S(4.0f, T + 0.08f),  LY,
		    -20.f * S(2.7f, T + 0.15f),  LX,
		    -10.f * S(5.0f, T + 0.05f),  LZ);
		Ap3(Cola, ColaRestRot,
		    15.f * Cos(5.f, T),           TY,
		    25.f * S(7.f, T),             TZ,
		    10.f * S(3.f, T + 0.1f),      TX);
		Ap3(Cabeza, CabezaRestRot,
		    10.f * S(3.7f, T),            AY,
		    15.f * S(2.3f, T + 0.07f),    AZ,
		     5.f * S(5.5f, T),            AX);
		break; // LOOP ∞

	default:
		bEnded = true;
		break;
	}

	if (bEnded) { CancelEmote(); }
}

// ── Emote Audio ───────────────────────────────────────────────────────────────

void ATortugaCharacter::PlayEmoteSound(int32 Index)
{
	// Stop any previous emote sound first.
	StopEmoteSound();

	if (!EmoteSounds.IsValidIndex(Index) || !EmoteSounds[Index])
	{
		return;
	}

	// Lazily create the audio component on first use (attached to root, spatialized).
	if (!EmoteAudioComponent)
	{
		EmoteAudioComponent = NewObject<UAudioComponent>(this, TEXT("EmoteAudio"));
		if (!EmoteAudioComponent)
		{
			return;
		}

		EmoteAudioComponent->SetupAttachment(GetRootComponent());
		EmoteAudioComponent->bAutoActivate = false;
		EmoteAudioComponent->bAlwaysPlay = false;

		// Proximity attenuation matching voice chat range.
		EmoteAudioComponent->bAllowSpatialization = true;
		EmoteAudioComponent->bOverrideAttenuation = true;
		EmoteAudioComponent->AttenuationOverrides.bAttenuate = true;
		EmoteAudioComponent->AttenuationOverrides.bSpatialize = true;
		EmoteAudioComponent->AttenuationOverrides.FalloffDistance = FMath::Max(EmoteAudioOuterRadius - EmoteAudioInnerRadius, 100.f);
		EmoteAudioComponent->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		EmoteAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(EmoteAudioInnerRadius);
		EmoteAudioComponent->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

		// Auto-restart when sound finishes → forced loop while emote is active.
		EmoteAudioComponent->OnAudioFinished.AddDynamic(this, &ATortugaCharacter::OnEmoteAudioFinished);

		EmoteAudioComponent->RegisterComponent();
	}

	EmoteAudioComponent->SetSound(EmoteSounds[Index]);
	EmoteAudioComponent->Play();
}

void ATortugaCharacter::StopEmoteSound()
{
	if (EmoteAudioComponent && EmoteAudioComponent->IsPlaying())
	{
		EmoteAudioComponent->Stop();
	}
}

void ATortugaCharacter::OnEmoteAudioFinished()
{
	// If an emote is still active, restart the sound for seamless looping.
	if (ActiveEmoteIndex >= 0 && EmoteAudioComponent && EmoteAudioComponent->Sound)
	{
		EmoteAudioComponent->Play();
	}
}

// ── Per-emote input handlers ──────────────────────────────────────────────────
void ATortugaCharacter::OnEmote0() { TriggerEmote(0); }
void ATortugaCharacter::OnEmote1() { TriggerEmote(1); }
void ATortugaCharacter::OnEmote2() { TriggerEmote(2); }
void ATortugaCharacter::OnEmote3() { TriggerEmote(3); }
void ATortugaCharacter::OnEmote4() { TriggerEmote(4); }
void ATortugaCharacter::OnEmote5() { TriggerEmote(5); }
void ATortugaCharacter::OnEmote6() { TriggerEmote(6); }
void ATortugaCharacter::OnEmote7() { TriggerEmote(7); }
void ATortugaCharacter::OnEmote8() { TriggerEmote(8); }
void ATortugaCharacter::OnEmote9() { TriggerEmote(9); }

