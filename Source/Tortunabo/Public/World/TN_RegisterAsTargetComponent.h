#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_RegisterAsTargetComponent.generated.h"

/**
 * Componente que registra al actor dueño en el UTN_LevelTargetSubsystem.
 *
 * Añadir a cualquier actor del nivel (puerta, plataforma, pared) que deba ser
 * referenciable desde un ATN_ButtonInteractable o ATN_PressurePlate dentro de
 * un BP_Chunk spawneado en runtime.
 *
 * Configuración:
 *   1. Añadir el componente al actor del nivel.
 *   2. En Details → "TargetTag", escribir el tag (ej. "GoalDoor1").
 *   3. En el ATN_ButtonInteractable del BP_Chunk, MoveTargetTag con el
 *      mismo nombre — DeferredInit lo resolverá vía el subsystem.
 *
 * Multiplayer:
 *   El componente se registra en BOTH server y clientes en BeginPlay
 *   (cada cual en su propio WorldSubsystem local). No requiere replicación.
 */
UCLASS(ClassGroup = (Tortunabo), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_RegisterAsTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_RegisterAsTargetComponent();

	/** Tag bajo el que este actor se registra. Botones lo buscan con MoveTargetTag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelTarget")
	FName TargetTag = NAME_None;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
