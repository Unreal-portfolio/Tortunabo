// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Turret.generated.h"

UCLASS()
class ROLLINGBALL2_API ATurret : public AActor
{
	GENERATED_BODY()

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:
	// Sets default values for this actor's properties
	ATurret();

protected:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
private:
	UPROPERTY(EditAnywhere, Category = "Compponents")
	UStaticMeshComponent* TurretBase;

	UPROPERTY(EditAnywhere, Category = "Compponents")
	UStaticMeshComponent* TurretTower;

	UPROPERTY(EditAnywhere, Category = "Compponents")
	class USphereComponent* SphereTrigger;

	UPROPERTY(EditAnywhere, Category = "Components")
	USceneComponent* SpawnPoint;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	TSubclassOf<class AProjectile> ProjectileClass;

	FTimerHandle ShootTimer;

	UPROPERTY()
	AActor* Target;

	UPROPERTY(EditAnywhere, Category = "Data")
	float RotationSpeed = 2.0f;

	void SpawnProjetcile();
};
