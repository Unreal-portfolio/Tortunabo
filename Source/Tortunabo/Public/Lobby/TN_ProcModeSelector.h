#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_ProcModeSelector.generated.h"

class UTextRenderComponent;
class USoundBase;

/** Qué elige un selector del lobby. */
UENUM(BlueprintType)
enum class ETNProcSelectorKind : uint8
{
	Mode       UMETA(DisplayName = "Modo de juego"),
	Difficulty UMETA(DisplayName = "Dificultad")
};

/**
 * @brief Interactuable del lobby (LVL_HQ) para elegir modo y dificultad de la partida.
 *
 * Cada interacción pasa a la siguiente opción: Clásico → Coop → Carrera → 2vs2
 * (solo con 4 jugadores en el lobby) o Fácil → Normal → Difícil. La elección vive
 * en la GameInstance del host (UMP_GameInstance::SelectedProcMode/Difficulty), que
 * ATN_HQGameMode lee al viajar: Clásico va a LVL_Run y el resto a LVL_ProcMap.
 * La etiqueta 3D se replica para que todos vean lo elegido.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcModeSelector : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_ProcModeSelector();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void OnInteracted_Implementation(APawn* Interactor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap")
	ETNProcSelectorKind Kind = ETNProcSelectorKind::Mode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcMap")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ProcMap")
	TObjectPtr<USoundBase> ChangeSound;

	/** Se dispara en todas las máquinas al cambiar la opción (para efectos en el BP). */
	UFUNCTION(BlueprintImplementableEvent, Category = "ProcMap")
	void OnSelectionChanged(const FText& SelectionText);

private:
	/** Valor del enum elegido (ETNProcGameMode o ETNProcDifficulty según Kind). */
	UPROPERTY(ReplicatedUsing = OnRep_Selection)
	uint8 Selection = 0;

	UFUNCTION()
	void OnRep_Selection();

	void RefreshLabel();
	FText DescribeSelection() const;
};
