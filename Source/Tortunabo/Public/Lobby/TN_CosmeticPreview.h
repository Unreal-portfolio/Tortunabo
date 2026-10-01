#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TN_CosmeticsTypes.h"
#include "TN_CosmeticPreview.generated.h"

class UAnimationAsset;
class UMaterialInterface;
class UPointLightComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
class UTN_BuggyLookComponent;

/**
 * Escaparate de la tienda y del probador: la tortuga del jugador sobre una peana de arena que gira despacio, con luces
 * de estudio (key, relleno y contra) y una cámara que la pinta en una textura; la UI la enseña con M_UI_Preview.
 *
 * Vive lejos, en el cielo, y solo lo ven sus capturas (bVisibleInSceneCaptureOnly): las capturas no usan el sol ni el
 * cielo del nivel, así que se ve igual en cualquier mapa. También hace las miniaturas del catálogo (un cosmético sobre
 * la tortuga de serie). Es local, no se replica: cada jugador tiene el suyo (Get).
 *
 * Modo buggy (pestaña y página del buggy del Rally): sobre la peana, el buggy de tortuga en miniatura con la tortuga
 * del jugador al volante; las miniaturas de modelos y pinturas salen del mismo buggy.
 */
UCLASS(NotPlaceable, Transient)
class TORTUNABO_API ATN_CosmeticPreview : public AActor
{
	GENERATED_BODY()

public:
	ATN_CosmeticPreview();

	/** El escaparate de este mundo (lo crea la primera vez). */
	static ATN_CosmeticPreview* Get(UWorld* World);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Viste a la tortuga del escaparate. */
	void SetLook(const FTN_TurtleLook& InLook);
	const FTN_TurtleLook& GetLook() const { return Look; }

	/** Enseña el buggy (con la tortuga al volante) en vez de la tortuga sola. */
	void SetBuggyMode(bool bInBuggyMode);
	bool IsBuggyMode() const { return bBuggyMode; }

	/** Modelo y pintura del buggy del escaparate. */
	void SetBuggyLook(const FTN_BuggyLook& InLook);
	const FTN_BuggyLook& GetBuggyLook() const { return BuggyLookState; }

#if !UE_BUILD_SHIPPING
	/** Pruebas (TN.Buggy.Photos): captura el escaparate, girado Yaw grados, en un PNG de Size píxeles. */
	bool DebugSavePhoto(const FString& File, int32 Size, float Yaw);
#endif

	/** Pose: saludo al elegir algo; con bCelebrate, el grito de alegría de una compra. Luego vuelve a la espera. */
	void PlayPose(bool bCelebrate);

	/** Giro manual (arrastrar con el ratón); el giro automático vuelve a los pocos segundos. */
	void AddSpin(float Degrees);

	/** Captura en vivo (se pinta cada fotograma solo mientras hay un menú abierto: SetLiveCapture). */
	UTextureRenderTarget2D* GetRenderTarget() const { return Target; }
	void SetLiveCapture(bool bEnabled);

	/** Miniatura de un cosmético: la textura se devuelve al momento y se pinta en los fotogramas siguientes. */
	UTextureRenderTarget2D* GetThumbnail(ETNCosmeticCategory Category, FName Id);

private:
	struct FThumbRequest
	{
		ETNCosmeticCategory Category = ETNCosmeticCategory::Helmet;
		FName Id = NAME_None;
		TObjectPtr<UTextureRenderTarget2D> Target;
	};

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USceneComponent> StageRoot;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USceneComponent> Turntable;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UStaticMeshComponent> Pedestal;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USkeletalMeshComponent> Turtle;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UStaticMeshComponent> Helmet;

	/** Buggy en miniatura (escala BuggyScale) sobre la peana. */
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USceneComponent> BuggyRoot;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UTN_BuggyLookComponent> Buggy;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USceneCaptureComponent2D> ThumbCapture;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UPointLightComponent> RimLight;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Target;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTextureRenderTarget2D>> Thumbnails;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> DefaultMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> SaluteAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> CheerAnim;

	TArray<FThumbRequest> PendingThumbs;
	FTN_TurtleLook Look;
	FTN_BuggyLook BuggyLookState;
	bool bBuggyMode = false;
	float SpinDeg = 0.f;
	float ManualSpinHold = 0.f;
	float PoseTimeLeft = -1.f;
	bool bLive = false;

	void BuildPedestal();
	void ApplyLookNow(const FTN_TurtleLook& InLook);
	/** Coloca la tortuga en la peana o al volante, la cámara y lo que ven las capturas según el modo. */
	void ApplyMode(bool bBuggy);
	/** Primitivas del buggy (para las listas de las capturas). */
	void BuggyPrimitives(TArray<UPrimitiveComponent*>& Out) const;
	void CaptureThumbnail(const FThumbRequest& Request);
};
