// Fill out your copyright notice in the Description page of Project Settings.

#include "Ball.h"

#include "BallPlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "DrawDebugHelpers.h"

// Sets default values
ABall::ABall()
{
	PrimaryActorTick.bCanEverTick = true;

	MyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MyMesh"));
	RootComponent = MyMesh;

	MySpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("MySpringArm"));
	MySpringArm->SetupAttachment(RootComponent);

	MyCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MyCamera"));
	MyCamera->SetupAttachment(MySpringArm, USpringArmComponent::SocketName);

	// Trail spawn point (NO hereda rotación)
	TrailSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TrailSpawnPoint"));
	TrailSpawnPoint->SetupAttachment(RootComponent);

	MyMesh->SetSimulatePhysics(true);
	MyMesh->SetMassOverrideInKg(NAME_None, 100.f, true);
}

// Called when the game starts or when spawned
void ABall::BeginPlay()
{
	Super::BeginPlay();

	OnTakeAnyDamage.AddDynamic(this, &ABall::DamageTaken);

	// ================= TRAIL =================
	if (TrailParticles && TrailSpawnPoint)
	{
		TrailPSC = UGameplayStatics::SpawnEmitterAttached(
			TrailParticles,
			TrailSpawnPoint,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false
		);

		if (TrailPSC)
		{
			// 🔒 NO heredar rotación ni escala de la bola
			TrailPSC->SetUsingAbsoluteRotation(true);
			TrailPSC->SetUsingAbsoluteScale(true);

			// Rotación fija (opcional)
			TrailPSC->SetWorldRotation(FRotator::ZeroRotator);
		}
	}
}

// Called every frame
void ABall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Overlaps
void ABall::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!OtherActor) return;

	if (OtherActor->ActorHasTag("Coin"))
	{
		if (BallController)
		{
			BallController->OnCollectable();
		}
		OtherActor->Destroy();
	}

	if (OtherActor->ActorHasTag("DeathZone"))
	{
		if (BallController)
		{
			BallController->OnLoseLive();
		}
		Destroy();
	}
}

// Input binding
void ABall::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABall::Move);
		Input->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ABall::Jump);
	}
}

void ABall::SetBallController(ABallPlayerController* BallPlayerController)
{
	BallController = BallPlayerController;
}

// Damage
void ABall::DamageTaken(
	AActor* DamagedActor,
	float Damage,
	const class UDamageType* DamageType,
	class AController* InstigatedBy,
	AActor* DamageCauser)
{
	if (BallController)
	{
		BallController->OnLoseLive();
	}
	Destroy();
}

// Movement
void ABall::Move(const FInputActionValue& InputActionValue)
{
	const FVector2D InputVector = InputActionValue.Get<FVector2D>();
	MyMesh->AddForce(FVector(InputVector.Y, InputVector.X, 0.f) * Force);
}

void ABall::Jump()
{
	FVector RayEnd = GetActorLocation() + MyMesh->GetCollisionShape().GetExtent().Z * FVector::DownVector;

	DrawDebugLine(GetWorld(), GetActorLocation(), RayEnd, FColor::Red, false, 2.f);

	if (GetWorld()->LineTraceTestByChannel(GetActorLocation(), RayEnd, ECC_GameTraceChannel1))
	{
		MyMesh->AddImpulse(FVector::UpVector * JumpForce);
		UGameplayStatics::PlaySoundAtLocation(this, JumpSound, GetActorLocation());
	}
}
