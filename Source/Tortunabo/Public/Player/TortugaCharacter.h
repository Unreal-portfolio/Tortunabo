#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TortugaCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;

UCLASS()
class TORTUNABO_API ATortugaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATortugaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> JumpAction;

private:
	void CacheInputAssets();
	void ApplyInputMappingIfLocal();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LoadedMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedMoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedLookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedJumpAction;
	bool bInputAssetsLoaded = false;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
};

