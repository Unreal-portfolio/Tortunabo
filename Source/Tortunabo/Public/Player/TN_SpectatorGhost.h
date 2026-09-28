#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "TN_SpectatorGhost.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class ATN_GhostEgg;
class UInputComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTN_GhostCameraModifier;

/** En qué anda un fantasma espectador. */
UENUM()
enum class ETNGhostStage : uint8
{
	/** Espectador: flota donde está su cámara, mirando a la tortuga que sigue. */
	Spectating,
	/** Volviendo a la vida (TNGhost::ReviveIntoEgg): vuela en U hasta el huevo, se mete de cabeza y espera dentro. */
	Reviving,
	/** Ya no es fantasma (tiene tortuga otra vez o se ha ido): se desvanece y desaparece. */
	Leaving,
};

/** Dónde está la cámara del espectador y a quién sigue: lo manda su dueño unas 12 veces por segundo. */
USTRUCT()
struct FTNGhostView
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<APlayerState> Followed = nullptr;
};

/** Vuelta a la vida en curso: dónde está el huevo y los tiempos (segundos del servidor, GetServerWorldTimeSeconds). */
USTRUCT()
struct FTNGhostRevive
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 EggLocation = FVector::ZeroVector;

	UPROPERTY()
	float EggYaw = 0.f;

	/** Cuándo empezó. */
	UPROPERTY()
	float StartTime = 0.f;

	/** Segundos del vuelo en U (0: sin vuelo, porque aún tenía tortuga y se le quitó). */
	UPROPERTY()
	float FlightSeconds = 0.f;

	/** Cuándo entra en el huevo (empieza a vibrar) y cuándo eclosiona. */
	UPROPERTY()
	float ArriveTime = 0.f;

	UPROPERTY()
	float HatchTime = 0.f;
};

/**
 * Fantasma espectador (Docs/Fantasma_Espectador.md). Uno por jugador espectador, en todos los modos: lo crea el servidor
 * cuando el jugador pasa a espectador (AMP_GamePlayerController::EnterSpectateMode) con su PlayerController de dueño, y
 * se desvanece cuando vuelve a tener tortuga (OnPossess).
 *
 * - Los demás lo ven como un fantasmita de tortuga (cabeza, caparazón, aletas y una colita ondulante en lugar de patas
 *   traseras; translúcido, blanco azulado, con ojitos) flotando justo donde está la cámara del espectador y mirando a la
 *   tortuga que sigue. El dueño manda la posición de su cámara al servidor ~12 veces por segundo (ServerUpdateView) y
 *   las demás máquinas la suavizan. Sin colisión; se desvanece si la cámara local lo atraviesa y no se aleja más de
 *   MaxDistanceToTurtle de la tortuga. El dueño no lo ve (salvo en su vuelo al huevo).
 * - En la máquina del dueño pone las cámaras (UTN_GhostCameraModifier: libre que orbita o fija a la vista del jugador
 *   seguido), los controles (un UInputComponent propio en la pila del PlayerController) y el cartel del espectador
 *   (UTN_GhostHUDWidget). La tortuga seguida es el ViewTarget del PlayerController, como antes; el servidor la sigue
 *   también para que la vista fija tenga la rotación de cámara replicada del jugador (TargetViewRotation).
 * - ReviveIntoEgg: vuelo en U hasta el huevo (todas las máquinas) y, en la del dueño, la transición de la cáscara
 *   oscura (UTN_GhostHatchWidget).
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API ATN_SpectatorGhost : public AActor
{
	GENERATED_BODY()

public:
	ATN_SpectatorGhost();

	/** Distancia máxima (cm) del fantasma a la tortuga que sigue. */
	static constexpr float MaxDistanceToTurtle = 1100.f;

	// ── Búsqueda (cualquier máquina) ─────────────────────────────────────────

	/** El fantasma de PC (el que no se está yendo, si hay dos). Solo donde existe PC: servidor y su dueño. */
	static ATN_SpectatorGhost* FindFor(const APlayerController* PC);

	/** El fantasma del jugador de PlayerState (cualquier máquina); null si no es fantasma o ya se está yendo. */
	static ATN_SpectatorGhost* FindForPlayerState(const APlayerState* PlayerState);

	/** Fantasmas activos del mundo (sin los que se están yendo). */
	static void GetActiveGhosts(const UWorld* World, TArray<ATN_SpectatorGhost*>& OutGhosts);

	// ── Servidor ─────────────────────────────────────────────────────────────

	/** Justo después de crearlo: de quién es y la tortuga que ha dejado (oculta o panza arriba; puede ser null). */
	void InitGhost(APlayerController* InOwner, APawn* InLeftBody);

	/** Empieza la vuelta a la vida hacia Egg (tiempos en segundos del servidor). FlightSeconds 0 = sin vuelo. */
	void BeginRevive(ATN_GhostEgg* Egg, float FlightSeconds, float StartTime, float ArriveTime, float HatchTime);

	/** La vuelta a la vida no ha podido acabar (no salió la tortuga): vuelve a ser espectador. */
	void CancelRevive();

	/** Ya no es fantasma: se desvanece y desaparece. */
	void EndGhost();

	APawn* GetLeftBody() const { return LeftBody.Get(); }
	void SetLeftBody(APawn* InBody) { LeftBody = InBody; }
	void ForgetLeftBody() { LeftBody.Reset(); }

	// ── Consultas (cualquier máquina) ────────────────────────────────────────

	APlayerState* GetGhostPlayerState() const { return GhostPlayerState; }
	APlayerState* GetFollowedPlayerState() const;
	ETNGhostStage GetStage() const { return Stage; }
	bool IsActiveGhost() const { return Stage != ETNGhostStage::Leaving; }
	bool IsReviving() const { return Stage == ETNGhostStage::Reviving; }
	const FTNGhostRevive& GetReviveInfo() const { return Revive; }

	/** true en la máquina del jugador de este fantasma. */
	bool IsLocallyOwned() const;

	/** El PlayerController dueño (solo en el servidor y en su máquina). */
	APlayerController* GetOwnerController() const;

	/** Dónde se dibuja ahora el fantasma (en su vuelo al huevo, el punto del vuelo). */
	FVector GetVisualLocation() const { return DisplayLocation; }

	/** Dueño: cámara libre (true) o fija a la vista del jugador seguido. */
	bool IsFreeCamera() const;

	/** Segundos del servidor ahora (reloj común para los tiempos de la vuelta a la vida). */
	static float ServerNow(const UWorld* World);

	/** Consola en un cliente: pide al servidor una orden de prueba (TNGhostInternal::EDebugCommand). */
	void RequestDebugCommand(uint8 Command, int32 PlayerIndex);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> GhostPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_Stage)
	ETNGhostStage Stage = ETNGhostStage::Spectating;

	/** Cámara del dueño (no se le manda a él). */
	UPROPERTY(Replicated)
	FTNGhostView View;

	UPROPERTY(ReplicatedUsing = OnRep_Revive)
	FTNGhostRevive Revive;

	UFUNCTION()
	void OnRep_Stage();

	UFUNCTION()
	void OnRep_Revive();

	/** Dueño → servidor: dónde está su cámara y a quién sigue. */
	UFUNCTION(Server, Unreliable)
	void ServerUpdateView(FVector_NetQuantize10 CameraLocation, APlayerState* Followed);

	/** Dueño → servidor: órdenes de prueba de la consola (no en Shipping). */
	UFUNCTION(Server, Reliable)
	void ServerRunDebug(uint8 Command, int32 PlayerIndex);

	// ── Malla (cada máquina con pantalla; RF_Transient, se construye en BeginPlay) ──

	UPROPERTY(VisibleAnywhere, Category = "Ghost")
	TObjectPtr<USceneComponent> GhostRoot;

	/** Lo que flota y se mece (cabeza, caparazón, aletas, ojos y cola cuelgan de aquí). */
	UPROPERTY(VisibleAnywhere, Category = "Ghost")
	TObjectPtr<USceneComponent> FloatRoot;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Ghost")
	TObjectPtr<UProceduralMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Ghost")
	TObjectPtr<UProceduralMeshComponent> TailMesh;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Ghost")
	TObjectPtr<UProceduralMeshComponent> FlipperLeft;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Ghost")
	TObjectPtr<UProceduralMeshComponent> FlipperRight;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Ghost")
	TObjectPtr<UProceduralMeshComponent> EyesMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EyesMaterial;

	// ── Dueño ────────────────────────────────────────────────────────────────

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> GhostInput;

	UPROPERTY(Transient)
	TObjectPtr<UTN_GhostCameraModifier> CameraModifier;

	/** Servidor: la tortuga que dejó al hacerse fantasma y el huevo de su vuelta a la vida. */
	TWeakObjectPtr<APawn> LeftBody;
	TWeakObjectPtr<ATN_GhostEgg> ReviveEgg;
	/** Servidor: a quién sigue su PlayerController (para no repetir SetViewTarget). */
	TWeakObjectPtr<APawn> ServerViewPawn;

	bool bVisualsBuilt = false;
	bool bOwnerSetUp = false;
	bool bLocalReviveStarted = false;
	/** Dueño: ya se ha puesto la transición de la cáscara en su pantalla. */
	bool bHatchScreenShown = false;
	bool bHasDisplay = false;
	bool bVisible = false;
	FVector DisplayLocation = FVector::ZeroVector;
	FRotator DisplayRotation = FRotator::ZeroRotator;
	float LocalTime = 0.f;
	float SpawnFade = 0.f;
	float LeaveFade = 1.f;
	float ShownOpacity = -1.f;
	/** Vuelo al huevo en esta máquina: desde dónde y cuándo empezó (reloj del servidor). */
	FVector FlightFrom = FVector::ZeroVector;
	bool bFlightFromSet = false;
	/** Dueño: envíos de la vista al servidor y cambios de tortuga. */
	float SendTimer = 0.f;
	float KeepAliveTimer = 0.f;
	FVector LastSentLocation = FVector::ZeroVector;
	TWeakObjectPtr<APlayerState> LastSentFollowed;
	float InvalidTargetTime = 0.f;
	float SwitchRetryTimer = 0.f;

	void BuildVisuals();
	void UpdateVisuals(float DeltaSeconds);
	void UpdateTail();
	void SetGhostVisible(bool bInVisible);
	void ApplyOpacity(float Opacity);
	/** Punto del vuelo en U hacia el huevo para el avance U (0-1) y su dirección. */
	FVector FlightPoint(float U, FVector* OutTangent) const;

	void SetUpOwner();
	void CleanUpOwner();
	void TickOwner(float DeltaSeconds);
	void TickServer(float DeltaSeconds);
	void StartLocalRevive();
	/** Servidor: el PlayerController dueño sigue también a esa tortuga (rotación de cámara replicada para la vista fija). */
	void ApplyServerFollow(APlayerState* Followed);

	/** Tortuga que se puede seguir (viva, visible, de otro jugador y que no sea fantasma). */
	bool IsWatchable(const APawn* Pawn) const;

	// Controles del dueño.
	void HandleMouseX(float Value);
	void HandleMouseY(float Value);
	void HandlePadX(float Value);
	void HandlePadY(float Value);
	void HandleZoomInAxis(float Value);
	void HandleZoomOutAxis(float Value);
	void HandleWheelUp();
	void HandleWheelDown();
	void HandleNext();
	void HandlePrevious();
	void HandleToggleCamera();
	void AddLook(float X, float Y, bool bGamepad);
};

namespace TNGhost
{
	/**
	 * Tortuga cuya interfaz enseña el HUD de PC: la suya o, si es un fantasma, la que sigue (su ViewTarget) con su
	 * PlayerState. Un fantasma que aún no sigue a nadie devuelve su propio PlayerState y ninguna tortuga.
	 */
	TORTUNABO_API void GetHUDSubject(const APlayerController* PC, APawn*& OutPawn, APlayerState*& OutPlayerState);

	/** true si el jugador de PlayerState es ahora un fantasma (en cualquier máquina; volviendo a la vida incluido). */
	TORTUNABO_API bool IsGhostPlayer(const APlayerState* PlayerState);
}
