#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TN_LevelTargetSubsystem.generated.h"

/**
 * Registry global de actores del nivel referenciables por tag desde
 * actores spawneados en runtime (chunks, etc.).
 *
 * Patrón canónico Epic recomendado por Gemini 2.5-pro debate:
 *   - True singleton por UWorld (auto-creado/destruido por el motor).
 *   - O(1) lookup vía TMap<FName, TWeakObjectPtr<AActor>>.
 *   - TWeakObjectPtr evita memory leaks si un target es destruido.
 *   - No requiere actor "manager" colocado en el nivel.
 *
 * Uso:
 *   1. Actor del nivel añade UTN_RegisterAsTargetComponent + asigna
 *      TargetTag (ej. "GoalDoor1") en el Class Defaults o Details.
 *   2. ATN_ButtonInteractable dentro de un BP_Chunk usa MoveTargetTag con el
 *      mismo nombre — DeferredInit lo resuelve via Get(World)->FindTarget(Tag).
 *
 * Nota: usamos FName por compatibilidad con el flow actual. Upgrade futuro
 * a FGameplayTag requiere añadir "GameplayTags" a Tortunabo.Build.cs y
 * configurar los tags en Project Settings → Project → GameplayTags.
 */
UCLASS()
class TORTUNABO_API UTN_LevelTargetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Helper: obtener el subsystem desde cualquier WorldContext. */
	static UTN_LevelTargetSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Registra un actor bajo un tag. Si el tag ya existe, sobreescribe (el
	 * último que se registra gana). Llamar desde el componente de registro.
	 * Solo tiene efecto en el servidor — los clientes resuelven sus targets
	 * por el mismo flow (su WorldSubsystem local también los registra).
	 */
	UFUNCTION(BlueprintCallable, Category = "LevelTargets")
	void RegisterTarget(FName Tag, AActor* Actor);

	/** Quita el registro (por unregistro explícito o destrucción). */
	UFUNCTION(BlueprintCallable, Category = "LevelTargets")
	void UnregisterTarget(FName Tag);

	/** Devuelve el actor registrado con ese tag, o nullptr si no hay o ya murió. */
	UFUNCTION(BlueprintPure, Category = "LevelTargets")
	AActor* FindTarget(FName Tag) const;

	/** Debug: cantidad de targets registrados. */
	UFUNCTION(BlueprintPure, Category = "LevelTargets")
	int32 GetRegisteredCount() const { return RegisteredTargets.Num(); }

protected:
	virtual void Deinitialize() override;

private:
	UPROPERTY()
	TMap<FName, TWeakObjectPtr<AActor>> RegisteredTargets;
};
