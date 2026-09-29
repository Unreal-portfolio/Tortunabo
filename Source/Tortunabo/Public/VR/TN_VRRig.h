#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VR/TN_VRMode.h"
#include "TN_VRRig.generated.h"

class APlayerController;
class ATortugaCharacter;
class SWidget;
class UCameraComponent;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UMotionControllerComponent;
class UStaticMeshComponent;
class UTN_VRScreenWidget;
class UUserWidget;
class UWidgetComponent;
class UWidgetInteractionComponent;

/**
 * El jugador local en VR (Docs/Modo_VR.md): lo crea UTN_VRSubsystem en cada mundo de juego mientras hay VR. Solo existe en
 * la máquina del jugador (no se replica).
 *
 * - Sigue a la vista: con gafas, su raíz es el origen del seguimiento de la cámara que se esté viendo (el de la tortuga,
 *   ATortugaCharacter::GetVROrigin); simulado, la propia cámara. Sin peón (menú principal), la vista es su cámara.
 * - Aletas: dos mandos con seguimiento (MotionSource LeftGrip/RightGrip) con una aleta de tortuga en cada uno; simulado,
 *   quietas delante de la cámara.
 * - Pantalla: UTN_VRScreenWidget en un panel del mundo. Jugando, el HUD flota delante y sigue a la cabeza con retraso;
 *   con un menú (el juego enseña el cursor), el panel se queda quieto delante y la aleta derecha apunta con un láser
 *   (gatillo = clic). Simulado, apunta el ratón. El panel se acerca si hay una pared en medio.
 * - Mandos jugando: los añade como mapeo propio sobre las acciones de siempre (IA_Move, IA_Jump...), gira por pasos con el
 *   stick derecho y recentra con su clic.
 */
UCLASS(NotBlueprintable, Transient)
class TORTUNABO_API ATN_VRRig : public AActor
{
	GENERATED_BODY()

public:
	ATN_VRRig();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ── Pantalla ─────────────────────────────────────────────────────────────

	bool HostWidget(UUserWidget* Widget, int32 ZOrder);
	bool IsHosting(const UUserWidget* Widget) const;
	bool HostSlate(const TSharedRef<SWidget>& Widget, int32 ZOrder);
	bool UnhostSlate(const TSharedRef<SWidget>& Widget);

	/** Al apagar el modo VR: lo que había en el panel vuelve al viewport. */
	void ReleaseScreenToViewport();

	/** ¿Un menú delante (panel quieto y puntero)? */
	bool IsMenuMode() const { return bMenuMode; }

	/** Vuelve a poner el panel delante de la cabeza. */
	void RecenterPanel();

	// ── Puntero (lo llama el procesador de entrada VR) ───────────────────────

	void PointerPress();
	void PointerRelease();
	void PointerScroll(float Delta);

	/** Aleta derecha o izquierda (para enganchar el objeto que se lleva en la mano). */
	USceneComponent* GetHand(bool bRight) const;

	/** El modo cambió entre gafas y simulado. */
	void OnModeChanged(ETNVRMode NewMode);

protected:
	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<USceneComponent> RigRoot;

	/** Vista propia cuando no hay peón (menú principal). */
	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UCameraComponent> RigCamera;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UMotionControllerComponent> LeftGrip;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UMotionControllerComponent> RightGrip;

	/** Pose de apuntar del mando derecho (el láser). */
	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UMotionControllerComponent> RightAim;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<USceneComponent> LeftHand;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<USceneComponent> RightHand;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UStaticMeshComponent> LeftFlipper;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UStaticMeshComponent> RightFlipper;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UStaticMeshComponent> LaserBeam;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UStaticMeshComponent> LaserDot;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UWidgetInteractionComponent> Pointer;

	UPROPERTY(VisibleAnywhere, Category = "VR")
	TObjectPtr<UWidgetComponent> ScreenPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTN_VRScreenWidget> Screen;

	/** Mandos VR sobre las acciones del juego (solo con gafas). */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> VRMapping;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LaserMaterial;

private:
	APlayerController* GetLocalPC() const;
	void EnsureScreen();
	void BuildHands();
	void BuildLaser();

	/** Raíz del rig en el origen de la vista que se esté viendo (ver arriba). */
	void UpdateViewAttachment(APlayerController* PC, ATortugaCharacter* Turtle);
	void UpdateHands(float DeltaSeconds);
	void UpdatePanel(APlayerController* PC, float DeltaSeconds);
	void UpdatePointer(APlayerController* PC);
	void UpdateInput(APlayerController* PC, ATortugaCharacter* Turtle, float DeltaSeconds);
	void EnsureVRMapping(APlayerController* PC);
	void RemoveVRMapping();

	/** Cámara (de este fotograma o del anterior) desde la que se ve. */
	bool GetViewPoint(APlayerController* PC, FVector& OutLocation, FRotator& OutRotation) const;
	/** Distancia a la que cabe el panel delante de la vista sin meterse en una pared. */
	float FitDistance(const FVector& From, const FVector& Dir, float Desired) const;
	void PlacePanel(const FVector& ViewLocation, float Yaw, float Distance, float HorizontalFov);

	/** Rayo del puntero: el mando derecho (gafas) o el ratón (simulado). */
	bool GetPointerRay(APlayerController* PC, FVector& OutOrigin, FVector& OutDir) const;

	ETNVRMode Mode = ETNVRMode::Off;
	bool bMenuMode = false;
	bool bPanelPlaced = false;
	bool bHudFollowing = false;
	bool bSnapLatched = false;
	bool bRecenterHeld = false;
	bool bPointerDown = false;
	bool bOwnsViewTarget = false;
	float HudYaw = 0.f;
	float PanelDistanceSmoothed = 0.f;
	FVector MenuPlacedFrom = FVector::ZeroVector;

	TWeakObjectPtr<USceneComponent> AttachedBase;
	FName AttachedSocket = NAME_None;
	TWeakObjectPtr<ATortugaCharacter> ViewTurtle;
	TWeakObjectPtr<APlayerController> MappedPC;
};
