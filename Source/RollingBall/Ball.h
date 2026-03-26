// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "GameFramework/Pawn.h"
#include "Ball.generated.h"

class ABallPlayerController;
class UParticleSystem;
class UParticleSystemComponent;
class USoundBase;

UCLASS()
class ROLLINGBALL2_API ABall : public APawn
{
	GENERATED_BODY()

public:
	ABall();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

public:
	void Move(const FInputActionValue& InputActionValue);
	void Jump();
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	void SetBallController(ABallPlayerController* BallPlayerController);

private:
#pragma region Movement
	UPROPERTY(EditDefaultsOnly, Category="My Data")
	float Speed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category="My Data")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditDefaultsOnly, Category="My Data")
	float Force = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category="My Data")
	UStaticMeshComponent* MyMesh;

	UPROPERTY(EditDefaultsOnly, Category="My Data")
	class USpringArmComponent* MySpringArm;

	UPROPERTY(EditDefaultsOnly, Category="My Data")
	class UCameraComponent* MyCamera;

	UPROPERTY(EditDefaultsOnly, Category="Inputs")
	class UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, Category="Inputs")
	UInputAction* JumpAction;
#pragma endregion

	UPROPERTY(EditDefaultsOnly, Category="My Data")
	float JumpForce;

	UPROPERTY(EditDefaultsOnly, Category="My Data")
	USoundBase* JumpSound;

	UPROPERTY()
	ABallPlayerController* BallController;

	/* ================= TRAIL ================= */

	UPROPERTY(EditDefaultsOnly, Category="VFX")
	UParticleSystem* TrailParticles = nullptr;

	UPROPERTY(VisibleAnywhere, Category="VFX")
	USceneComponent* TrailSpawnPoint = nullptr;

	UPROPERTY()
	UParticleSystemComponent* TrailPSC = nullptr;

	/* ================= DAMAGE ================= */

	UFUNCTION()
	void DamageTaken(
		AActor* DamagedActor,
		float Damage,
		const class UDamageType* DamageType,
		class AController* InstigatedBy,
		AActor* DamageCauser
	);
};
