// Fill out your copyright notice in the Description page of Project Settings.


#include "Mecanisms/PushingCylinder.h"

// Sets default values
APushingCylinder::APushingCylinder()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	CylinderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CylinderMesh"));
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	
	SetRootComponent(Root);
	CylinderMesh->SetupAttachment(Root);

	CylinderMesh->SetSimulatePhysics(true);
	CylinderMesh->SetMassOverrideInKg(NAME_None, 99999999999999999.f);
	CylinderMesh->SetEnableGravity(false);

}

// Called when the game starts or when spawned
void APushingCylinder::BeginPlay()
{
	Super::BeginPlay();
	CylinderMesh->AddAngularImpulseInDegrees(FVector::UpVector * ImpulseForce, NAME_None, true);
	
}

// Called every frame
void APushingCylinder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

