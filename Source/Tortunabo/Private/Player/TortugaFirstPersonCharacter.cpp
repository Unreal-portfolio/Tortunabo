#include "Player/TortugaFirstPersonCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

ATortugaFirstPersonCharacter::ATortugaFirstPersonCharacter()
{
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = 0.f;
		CameraBoom->SocketOffset = FVector(0.f, 0.f, 60.f);
		CameraBoom->bUsePawnControlRotation = true;
	}

	if (FollowCamera)
	{
		FollowCamera->bUsePawnControlRotation = false;
	}

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void ATortugaFirstPersonCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocallyControlled() && GetMesh())
	{
		GetMesh()->SetOwnerNoSee(true);
	}
}

