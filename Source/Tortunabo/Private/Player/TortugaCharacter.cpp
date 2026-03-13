#include "Player/TortugaCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "GameFramework/PlayerController.h"

ATortugaCharacter::ATortugaCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

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

	DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("DefaultMappingContext"));
	MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveAction"));
	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("LookAction"));
	JumpAction = CreateDefaultSubobject<UInputAction>(TEXT("JumpAction"));

	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction->ValueType = EInputActionValueType::Axis2D;
	JumpAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);
	DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_Right2D);
	DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
	DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);

	FEnhancedActionKeyMapping& WKey = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
	UInputModifierSwizzleAxis* WSwizzleModifier = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveW_Swizzle"));
	WSwizzleModifier->Order = EInputAxisSwizzle::YXZ;
	WKey.Modifiers.Add(WSwizzleModifier);

	FEnhancedActionKeyMapping& SKey = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
	UInputModifierSwizzleAxis* SSwizzleModifier = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveS_Swizzle"));
	SSwizzleModifier->Order = EInputAxisSwizzle::YXZ;
	SKey.Modifiers.Add(SSwizzleModifier);
	SKey.Modifiers.Add(CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveS_Negate")));

	FEnhancedActionKeyMapping& AKey = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
	AKey.Modifiers.Add(CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveA_Negate")));

	DefaultMappingContext->MapKey(MoveAction, EKeys::D);
}

void ATortugaCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSubsystem->ClearAllMappings();
				InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ATortugaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATortugaCharacter::Move);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATortugaCharacter::Look);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ATortugaCharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATortugaCharacter::StopJumping);
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

