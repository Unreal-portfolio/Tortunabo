#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_InteractableBase.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UTN_InteractPromptWidget;
class UUserWidget;

UCLASS(Abstract)
class TORTUNABO_API ATN_InteractableBase : public AActor
{
	GENERATED_BODY()

public:
	ATN_InteractableBase();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual bool CanInteract(APawn* Interactor) const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual void Interact(APawn* Interactor);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetInteractionDistance() const { return InteractionDistance; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetPromptText() const { return PromptText; }


protected:
	/**
	 * Mesh del interactuable. Es el componente root del actor.
	 * Ser root (UStaticMeshComponent = UPrimitiveComponent) permite que
	 * UWorld::FindTeleportSpot calcule bounds correctamente al spawnear el actor.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction|UI")
	TObjectPtr<UWidgetComponent> PromptWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float InteractionDistance = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	FText PromptText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	TSubclassOf<UUserWidget> PromptWidgetClass;

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnInteracted(APawn* Interactor);
	virtual void OnInteracted_Implementation(APawn* Interactor);

private:
	UPROPERTY(ReplicatedUsing = OnRep_InteractionEnabled)
	bool bInteractionEnabled = true;

	UFUNCTION()
	void OnRep_InteractionEnabled();

	void ApplyInteractionEnabledState();

protected:
	void SetInteractionEnabled(bool bEnabled);
};

