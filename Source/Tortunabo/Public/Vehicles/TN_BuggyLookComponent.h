// Aspecto del buggy del Rally (#297, #114): monta la carrocería de tortuga por piezas, la pinta con la pintura elegida
// (M_BuggyPaint) y el color del equipo, y mueve la antena con los acelerones. Lo usan ATN_Buggy (colgado del chasis,
// con sus ruedas y su cañón) y el escaparate de la tienda (con ruedas propias). Solo visual: no tiene colisión ni red.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Core/TN_CosmeticsTypes.h"
#include "TN_BuggyLookComponent.generated.h"

class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup = (Rally), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_BuggyLookComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTN_BuggyLookComponent();

	/**
	 * Ruedas de otro componente (las del ATN_Buggy, en los huesos VisWheel_*; las derechas giradas 180 grados): solo se
	 * les pone la malla y el material. Sin llamarlo, el componente crea sus cuatro ruedas en su sitio (escaparate).
	 */
	void UseExternalWheels(const TArray<UStaticMeshComponent*>& InWheels);

	/** Cañón de la torreta: se le pone la malla del modelo y se le quita el ajuste del cilindro provisional. */
	void UseExternalCannon(UStaticMeshComponent* InCannon);

	/** Escaparate: las piezas solo salen en las capturas y las ilumina el canal de luz del estudio. */
	void SetStudio(const FLightingChannels& Channels);

	/** Con artillera, ella sujeta el cañón; sin ella, el cañón va en su poste sobre el sillín. */
	void SetGunnerSeated(bool bSeated);

	/** Viste el buggy (TeamIndex < 0 = sin equipo: banderín y iris blancos). Con bForce lo rehace aunque no cambie. */
	void ApplyLook(const FTN_BuggyLook& InLook, int32 InTeamIndex, bool bForce = false);

	const FTN_BuggyLook& GetLook() const { return Look; }

	/** Ya hay carrocería de tortuga puesta (para esconder la prestada). */
	bool HasBuiltLook() const { return !AppliedKey.IsEmpty(); }

	/** Todas las piezas visibles (carrocería, ruedas propias, antena; el cañón externo no). */
	void GetPrimitives(TArray<UPrimitiveComponent*>& Out) const;

	/** Pieza de carrocería por índice (TNBuggyArt::EPiece, de Chassis a Extras): para engancharle arte (#319). */
	UStaticMeshComponent* GetBodyPiece(int32 Index) const { return BodyPieces.IsValidIndex(Index) ? BodyPieces[Index].Get() : nullptr; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void EnsureParts();
	UStaticMeshComponent* MakePart(const TCHAR* Name, USceneComponent* Parent);
	void SetupPart(UStaticMeshComponent* Part) const;
	void UpdateAntenna(float DeltaTime);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BodyPieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Wheels;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Cannon;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> AntennaPivot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Antenna;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TurretPost;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PaintMID;

	FTN_BuggyLook Look;
	int32 TeamIndex = INDEX_NONE;
	FString AppliedKey;
	bool bExternalWheels = false;
	bool bExternalCannon = false;
	bool bStudio = false;
	bool bGunnerSeated = false;
	FLightingChannels StudioChannels;

	// Antena: muelle amortiguado que se inclina con la aceleración del chasis.
	FVector LastLocation = FVector::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;
	FVector2D Lean = FVector2D::ZeroVector;
	FVector2D LeanSpeed = FVector2D::ZeroVector;
	bool bHasLastLocation = false;
	float AntennaTime = 0.f;
};
