// Modo VR de la tortuga (Docs/Modo_VR.md): primera persona con gafas o simulada, giro con la cabeza y puntería con la
// aleta. El resto del modo VR (aletas, panel de la interfaz, mandos) vive en ATN_VRRig y UTN_VRSubsystem.

#include "Player/TortugaCharacter.h"
#include "Core/TN_Log.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

void ATortugaCharacter::SetVRView(bool bOn, bool bHeadset)
{
	if (!IsLocallyControlled())
	{
		bOn = false;
	}
	if (!bOn)
	{
		bHeadset = false;
	}
	if (bOn == bVRViewActive && bHeadset == bVRHeadsetView)
	{
		return;
	}
	bVRViewActive = bOn;
	bVRHeadsetView = bHeadset;

	if (bOn)
	{
		if (!VROrigin)
		{
			VROrigin = NewObject<USceneComponent>(this, TEXT("VROrigin"), RF_Transient);
			VROrigin->SetupAttachment(GetCapsuleComponent());
			VROrigin->RegisterComponent();
		}
		if (!VRCamera)
		{
			VRCamera = NewObject<UCameraComponent>(this, TEXT("VRCamera"), RF_Transient);
			VRCamera->SetupAttachment(VROrigin);
			VRCamera->RegisterComponent();
		}
		VROrigin->SetRelativeLocation(VREyeOffset);
		// Con gafas el origen no gira con la cápsula: lo gira el stick (y la cabeza gira la cámara dentro de él).
		VROrigin->SetUsingAbsoluteRotation(bHeadset);
		VRYaw = Controller ? static_cast<float>(Controller->GetControlRotation().Yaw) : static_cast<float>(GetActorRotation().Yaw);
		if (bHeadset)
		{
			VROrigin->SetWorldRotation(FRotator(0.0, VRYaw, 0.0));
		}
		else
		{
			VROrigin->SetRelativeRotation(FRotator::ZeroRotator);
		}
		VRCamera->bUsePawnControlRotation = !bHeadset;
		VRCamera->bLockToHmd = bHeadset;
		VRCamera->SetRelativeTransform(FTransform::Identity);
		VRCamera->SetFieldOfView(90.f);
		// La cámara activa es la que ve el juego (AActor::CalcCamera coge la primera activa).
		if (FollowCamera)
		{
			FollowCamera->SetActive(false);
		}
		VRCamera->SetActive(true);
		bVRControlYawValid = false;
	}
	else
	{
		if (VRCamera)
		{
			VRCamera->SetActive(false);
		}
		if (FollowCamera)
		{
			FollowCamera->SetActive(true);
		}
		bLocalVRAimValid = false;
	}

	// Por dentro uno no se ve: la malla y el casco solo para los demás (la sombra propia sí se ve).
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetOwnerNoSee(bOn);
		Body->bCastHiddenShadow = bOn;
		Body->MarkRenderStateDirty();
	}
	if (HelmetMeshComp)
	{
		HelmetMeshComp->SetOwnerNoSee(bOn);
	}

	// La tortuga mira hacia donde mira la cabeza: aquí al momento y en el servidor (y de ahí a los demás).
	if (bVRPlayer != bOn)
	{
		bVRPlayer = bOn;
		ApplyVRRotationMode();
		if (!HasAuthority())
		{
			ServerSetVRPlayer(bOn);
		}
	}
	UE_LOG(LogTortunabo, Log, TEXT("[VR] %s: primera persona %s%s."), *GetName(), bOn ? TEXT("encendida") : TEXT("apagada"),
		bOn ? (bHeadset ? TEXT(" (gafas)") : TEXT(" (simulada)")) : TEXT(""));
}

void ATortugaCharacter::AddVRYaw(float DeltaYaw)
{
	VRYaw = static_cast<float>(FRotator::NormalizeAxis(static_cast<double>(VRYaw + DeltaYaw)));
	if (VROrigin && bVRHeadsetView)
	{
		VROrigin->SetWorldRotation(FRotator(0.0, VRYaw, 0.0));
	}
}

void ATortugaCharacter::SetLocalVRAim(const FRotator& Aim, bool bValid)
{
	LocalVRAim = Aim;
	bLocalVRAimValid = bValid && bVRViewActive;
}

FRotator ATortugaCharacter::GetTurtleAimRotation() const
{
	// El dueño en VR: la aleta de ahora mismo.
	if (bVRViewActive && bLocalVRAimValid && IsLocallyControlled())
	{
		return LocalVRAim;
	}
	// El servidor, con un dueño en VR: la última aleta que mandó (si es reciente).
	const UWorld* World = GetWorld();
	if (bVRPlayer && ServerVRAimTime >= 0.0 && World && World->GetTimeSeconds() - ServerVRAimTime < 2.0)
	{
		return ServerVRAim;
	}
	return Controller ? Controller->GetControlRotation() : GetActorRotation();
}

void ATortugaCharacter::SendVRAimToServer()
{
	if (bVRViewActive && bLocalVRAimValid && !HasAuthority())
	{
		ServerSetVRAim(LocalVRAim);
	}
}

void ATortugaCharacter::ServerSetVRAim_Implementation(FRotator Aim)
{
	ServerVRAim = Aim;
	ServerVRAimTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void ATortugaCharacter::ServerSetVRPlayer_Implementation(bool bOn)
{
	bVRPlayer = bOn;
	ApplyVRRotationMode();
}

void ATortugaCharacter::OnRep_VRPlayer()
{
	ApplyVRRotationMode();
}

void ATortugaCharacter::ApplyVRRotationMode()
{
	bUseControllerRotationYaw = bVRPlayer;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = !bVRPlayer;
	}
}

void ATortugaCharacter::TickVRView(float DeltaTime)
{
	if (!bVRViewActive || !bVRHeadsetView || !VRCamera || !VROrigin || !Controller)
	{
		return;
	}
	VROrigin->SetWorldRotation(FRotator(0.0, VRYaw, 0.0));
	const FRotator Head = VRCamera->GetComponentRotation();
	float TargetYaw = static_cast<float>(Head.Yaw);
	if (bVRControlYawValid)
	{
		// Algo giró la vista a propósito (reaparecer mirando al frente, ClientSetRotation...): el seguimiento gira con ella
		// para que la cabeza mire hacia allí. Los giros pequeños de cada fotograma (ratón, animaciones) no cuentan.
		const float Wanted = static_cast<float>(Controller->GetControlRotation().Yaw);
		if (FMath::Abs(static_cast<float>(FRotator::NormalizeAxis(static_cast<double>(Wanted - VRLastControlYaw)))) > 20.f)
		{
			AddVRYaw(static_cast<float>(FRotator::NormalizeAxis(static_cast<double>(Wanted - TargetYaw))));
			TargetYaw = Wanted;
		}
	}
	const float Pitch = FMath::Clamp(static_cast<float>(FRotator::NormalizeAxis(Head.Pitch)), -80.f, 80.f);
	Controller->SetControlRotation(FRotator(Pitch, TargetYaw, 0.f));
	VRLastControlYaw = TargetYaw;
	bVRControlYawValid = true;
}
