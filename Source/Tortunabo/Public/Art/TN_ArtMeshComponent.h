#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "TN_ArtMeshComponent.generated.h"

/**
 * Malla de arte puesta por TNArt (Docs/Arte_Assets.md) en lugar de una pieza generada desde C++: hija del componente
 * generado (o del que tiene la malla combinada), con una instancia por cada copia de la pieza. Es transitoria (no se guarda
 * con el nivel ni se duplica al jugar: cada construcción la vuelve a poner) y se esconde y se enseña con su padre.
 */
UCLASS(ClassGroup = (Tortunavy), meta = (BlueprintSpawnableComponent = false))
class TORTUNABO_API UTN_ArtMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UTN_ArtMeshComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Pieza que dibuja («Lobby.Castle.Tower»). */
	FName Slot;

	/** Construcción que la puso (FPieceLog::Group) o NAME_None si sustituye a un componente suelto. */
	FName Group;

	/**
	 * Cómo estaba el componente generado antes de dejar de dibujarse (modo componente suelto): al quitar el sustituto se
	 * deja como estaba.
	 */
	struct FHiddenState
	{
		bool bValid = false;
		bool bRenderInMainPass = true;
		bool bRenderInDepthPass = true;
		bool bCastShadow = true;
		bool bVisibleInRayTracing = true;
		bool bVisibleInReflectionCaptures = true;
		bool bVisibleInRealTimeSkyCaptures = true;
		bool bHiddenInSceneCapture = false;
		bool bAffectDistanceFieldLighting = true;
		bool bAffectDynamicIndirectLighting = true;
		bool bRenderCustomDepth = false;
		/** La colisión del generado se pasó a esta malla (bUseArtCollision): la que tenía. */
		bool bTookCollision = false;
		TEnumAsByte<ECollisionEnabled::Type> Collision = ECollisionEnabled::NoCollision;
	};
	FHiddenState Hidden;
};
