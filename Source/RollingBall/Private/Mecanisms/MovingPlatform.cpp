// Fill out your copyright notice in the Description page of Project Settings.


#include "Mecanisms/MovingPlatform.h"

#include "Components/SplineComponent.h"

// Sets default values
AMovingPlatform::AMovingPlatform()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));

	SetRootComponent(Root);
	PlatformMesh->SetupAttachment(Root);
	Spline->SetupAttachment(Root);

	PlatformMesh->SetSimulatePhysics(true);
	PlatformMesh->SetEnableGravity(false);

}

// Called when the game starts or when spawned
void AMovingPlatform::BeginPlay()
{
	Super::BeginPlay();
	PointsCount = Spline->GetNumberOfSplinePoints();
	if (PointsCount > 0)
	{
		CalculateNewDestination();
		//Esto es como rb.linearVelocity en Unity
		PlatformMesh->SetPhysicsLinearVelocity(CurrentDirection * Speed);
	}
	
}

// Called every frame
void AMovingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(FVector::Dist(PlatformMesh->GetComponentLocation(), CurrentDestination) < 10.f)
	{
		CalculateNewDestination();
		PlatformMesh->SetPhysicsLinearVelocity(CurrentDirection * Speed);
	}

}

void AMovingPlatform::CalculateNewDestination()
{
	if (PointsCount == 0) return; // Evitar división por cero
	
	CurrentPointIndex = (CurrentPointIndex + 1) % PointsCount;
	CurrentDestination = Spline->GetLocationAtSplinePoint(CurrentPointIndex, ESplineCoordinateSpace::World);
	CurrentDirection = (CurrentDestination - PlatformMesh->GetComponentLocation()).GetSafeNormal();
}



