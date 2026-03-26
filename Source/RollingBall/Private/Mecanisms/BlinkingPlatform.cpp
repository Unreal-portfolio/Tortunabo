// Fill out your copyright notice in the Description page of Project Settings.


#include "Mecanisms/BlinkingPlatform.h"
#include "Math/UnrealMathUtility.h" // para FMath::RandBool / FRandRange


// Sets default values
ABlinkingPlatform::ABlinkingPlatform()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
 	PrimaryActorTick.bCanEverTick = true;
 	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
 	SetRootComponent(PlatformMesh);

}

// Called when the game starts or when spawned
void ABlinkingPlatform::BeginPlay()
{
	Super::BeginPlay();
	// Inicializamos el estado inicial aleatoriamente para que algunas plataformas empiecen visibles y otras no
	bIsVisible = FMath::RandBool();
	if (bIsVisible)
	{
		PlatformMesh->SetVisibility(true);
		PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		PlatformMesh->SetVisibility(false);
		PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Retardo inicial aleatorio para desfasar el parpadeo entre plataformas (0..2 segundos)
	float InitialDelay = FMath::FRandRange(0.0f, 2.0f);
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ABlinkingPlatform::Blink, 2.0f, true, InitialDelay);
	
	
}

// Called every frame
void ABlinkingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	//Para matar al timer
	//GetWorldTimerManager().ClearTimer(TimerHandle);

}

void ABlinkingPlatform::Blink()
{
	//PlatformMesh->SetHiddenInGame(true);
	//PlatformMesh->SetActive(false);

	if (!bIsVisible)
	{
		PlatformMesh->SetVisibility(true);
		PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		bIsVisible = true;
	}else
	{
		PlatformMesh->SetVisibility(false);
		PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		bIsVisible = false;
	}
		
}
