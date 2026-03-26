// Fill out your copyright notice in the Description page of Project Settings.


#include "Mecanisms/GoldCube.h"

// Sets default values
AGoldCube::AGoldCube()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MyMesh"));
	RootComponent = MyMesh;

	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	SceneComponent->SetupAttachment(RootComponent);

}

// Called when the game starts or when spawned
void AGoldCube::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AGoldCube::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	FRotator DeltaRotation = FRotator(Rotator.Pitch * DeltaTime, Rotator.Yaw * DeltaTime, Rotator.Roll * DeltaTime);
	AddActorWorldRotation(DeltaRotation);

}
