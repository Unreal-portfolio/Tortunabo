// Fill out your copyright notice in the Description page of Project Settings.


#include "Mecanisms/Turret.h"

#include "Components/SphereComponent.h"
#include "Mecanisms/Projectile.h"

// Sets default values
ATurret::ATurret()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	TurretBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBase"));
	TurretTower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretTower"));
	SphereTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("SphereTrigger"));
	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SetRootComponent(TurretBase);
	TurretTower->SetupAttachment(GetRootComponent());
	SphereTrigger->SetupAttachment(GetRootComponent());
	SpawnPoint->SetupAttachment(TurretTower);
	

}

// Called when the game starts or when spawned
void ATurret::BeginPlay()
{
	Super::BeginPlay();

	SphereTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATurret::OnOverlap);
	SphereTrigger->OnComponentEndOverlap.AddDynamic(this, &ATurret::OnEndOverlap);
	
}

void ATurret::SpawnProjetcile()
{
	GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SpawnPoint->GetComponentTransform());
}

// Called every frame
void ATurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Target)
	{
		//FVector TargetLocation = Target->GetActorLocation();
		//FVector TurretLocation = TurretTower->GetComponentLocation();
		//FRotator LookAtRotation = (TargetLocation - TurretLocation).Rotation();
		//FRotator CurrentRotation = TurretTower->GetComponentRotation();
		//FRotator NewRotation = FMath::RInterpTo(CurrentRotation, LookAtRotation, DeltaTime, 2.0f);
		//TurretTower->SetWorldRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));

		//1. Se obtiene direccion a target
		FVector DirectionToTarget = (Target->GetActorLocation() - TurretTower->GetComponentLocation()).GetSafeNormal();

		//2. Se interpola la direccion actual de la torreta a la direccion al target
		FVector InterpVector = FMath::VInterpTo(TurretTower->GetForwardVector(), DirectionToTarget, DeltaTime, RotationSpeed);

		//3. Se obtiene la rotacion de la direccion interpolada y se aplica a la torreta
		FRotator TargetRotation = InterpVector.Rotation();

		//4. Solo se rota en yaw
		TurretTower->SetWorldRotation(FRotator(0.0f, TargetRotation.Yaw, 0.0f));
	}

}

void ATurret::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//Comprobar si el actor que ha entrado en el trigger es el jugador
	if (OtherActor->ActorHasTag("Player"))
	{
		Target = OtherActor;
		GetWorld()->GetTimerManager().SetTimer(ShootTimer, this, &ATurret::SpawnProjetcile, .5f, true);
		
	}
}

void ATurret::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (OtherActor->ActorHasTag("Player"))
	{
		Target = nullptr;
		GetWorld()->GetTimerManager().ClearTimer(ShootTimer);
	}
}

