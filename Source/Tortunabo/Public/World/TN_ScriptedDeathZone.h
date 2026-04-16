#pragma once

#include "CoreMinimal.h"
#include "World/TN_DeathZoneVolume.h"
#include "TN_ScriptedDeathZone.generated.h"

/**
 * Tipo de acción que puede ejecutar TN_ScriptedDeathZone sobre actores del mundo.
 */
UENUM(BlueprintType)
enum class ETN_WorldActionType : uint8
{
	/** Mueve el actor sumando LocationOffset a su posición actual. */
	MoveActor UMETA(DisplayName = "Move Actor"),
	/** Rota el actor sumando RotationOffset a su rotación actual. */
	RotateActor UMETA(DisplayName = "Rotate Actor"),
	/** Oculta o muestra el actor (HideActor=true → ocultar). */
	SetVisibility UMETA(DisplayName = "Set Visibility"),
	/** Destruye el actor. */
	DestroyActor UMETA(DisplayName = "Destroy Actor"),
};

/**
 * Acción scripted sobre un actor del mundo.
 * Configurable en el Blueprint hijo de TN_ScriptedDeathZone.
 */
USTRUCT(BlueprintType)
struct FTN_WorldAction
{
	GENERATED_BODY()

	/** Actor sobre el que se ejecuta la acción. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldAction")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** Tipo de acción. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldAction")
	ETN_WorldActionType ActionType = ETN_WorldActionType::MoveActor;

	/** Offset de posición (para MoveActor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldAction")
	FVector LocationOffset = FVector::ZeroVector;

	/** Offset de rotación (para RotateActor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldAction")
	FRotator RotationOffset = FRotator::ZeroRotator;

	/** Para SetVisibility: true = ocultar actor, false = mostrar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldAction")
	bool bHide = true;
};

/**
 * Zona de muerte scriptada (#14).
 *
 * Hereda de TN_DeathZoneVolume y añade un array de FTN_WorldAction
 * que se ejecutan en eventos configurables (al activar la zona, al matar a alguien).
 *
 * Eventos:
 *   - OnActivate: primer jugador entra en la zona.
 *   - OnFirstKill: primer jugador muere dentro.
 *
 * Los OnActivate/OnFirstKill son BlueprintImplementableEvents para VFX/audio adicionales.
 *
 * Uso:
 *   1. Crear BP hijo.
 *   2. Configurar OnActivateActions y/o OnFirstKillActions.
 *   3. Override OnScriptedActivate / OnScriptedFirstKill en BP para efectos.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ScriptedDeathZone : public ATN_DeathZoneVolume
{
	GENERATED_BODY()

public:
	ATN_ScriptedDeathZone();

protected:
	/**
	 * Acciones ejecutadas en el servidor cuando el primer jugador entra en la zona.
	 * Solo se dispara una vez (bOnActivateFired).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scripted")
	TArray<FTN_WorldAction> OnActivateActions;

	/**
	 * Acciones ejecutadas en el servidor cuando el primer jugador muere en la zona.
	 * Solo se dispara una vez (bOnFirstKillFired).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scripted")
	TArray<FTN_WorldAction> OnFirstKillActions;

	/** Override en BP para VFX/audio cuando la zona se activa por primera vez. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Scripted")
	void OnScriptedActivate();

	/** Override en BP para VFX/audio cuando muere el primer jugador. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Scripted")
	void OnScriptedFirstKill();

protected:
	virtual void OnPlayerEnteredZone(APawn* Pawn) override;
	virtual void OnPlayerDiedInZone(APlayerController* PC) override;

private:
	bool bOnActivateFired = false;
	bool bOnFirstKillFired = false;

	void ExecuteActions(const TArray<FTN_WorldAction>& Actions);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotifyActivate();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotifyFirstKill();
};
