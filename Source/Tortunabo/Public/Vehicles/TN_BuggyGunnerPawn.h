// Peón de la artillera del buggy biplaza: sin movimiento, sujeto al asiento trasero (Seat_Gunner), con cámara propia
// detrás de la torreta y apuntado replicado por RPC validada.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicles/TN_BuggyMath.h"
#include "TN_BuggyGunnerPawn.generated.h"

class ATN_Buggy;
class UCameraComponent;
class USpringArmComponent;
class UTN_BuggyInputSet;
struct FInputActionValue;

UCLASS()
class TORTUNABO_API ATN_BuggyGunnerPawn : public APawn
{
	GENERATED_BODY()

public:
	ATN_BuggyGunnerPawn();

	/** Solo servidor: el buggy al que va sujeta (lo replica y cada máquina lo engancha al asiento). */
	void SetBuggy(ATN_Buggy* InBuggy);

	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	ATN_Buggy* GetBuggy() const { return Buggy; }

	/** Apuntado relativo al buggy que lleva la artillera en esta máquina (cabeceo ya limitado). */
	FRotator GetLocalAim() const { return LocalAim; }

	/** Artillera local: pide al servidor un disparo con el apuntado de esta máquina (lo usan la entrada y TN.Rally.LocalFire). */
	void RequestFire(bool bSpecial);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Sensibilidad del ratón (grados por unidad de delta) y velocidad del stick (grados por segundo). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Torreta")
	float MouseDegreesPerUnit = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Torreta")
	float StickDegreesPerSecond = 160.f;

	/** Envíos del apuntado al servidor por segundo. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Torreta")
	float AimSendRate = 15.f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Buggy();
	void AttachToBuggy();
	void UpdateCamera();

	UTN_BuggyInputSet* GetInputSet();
	void OnAimMouse(const FInputActionValue& Value);
	void OnAimStick(const FInputActionValue& Value);
	void OnFireCoco(const FInputActionValue& Value);
	void OnFireSpecial(const FInputActionValue& Value);
	void OnSelfRightPressed(const FInputActionValue& Value);
	void OnSelfRightReleased(const FInputActionValue& Value);
	void AddAim(float DeltaYaw, float DeltaPitch);

	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerSetAim(float Yaw, float Pitch);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFire(bool bSpecial, float Yaw, float Pitch);

	UFUNCTION(Server, Reliable)
	void ServerSelfRight();

	UFUNCTION(Server, Reliable)
	void ServerRequestRespawn();

	/** Solo servidor: la que habla es de verdad la artillera de este buggy. */
	bool IsSeatedGunner() const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(ReplicatedUsing = OnRep_Buggy)
	TObjectPtr<ATN_Buggy> Buggy;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BuggyInputSet> InputSet;

	FRotator LocalAim = FRotator::ZeroRotator;
	FRotator LastSentAim = FRotator(1000.f, 0.f, 0.f);
	float AimSendAccumulator = 0.f;
	double LastFireRequest = -1000.0;
	bool bSelfRightHeld = false;
	TNBuggy::FHold RespawnHold;
};
