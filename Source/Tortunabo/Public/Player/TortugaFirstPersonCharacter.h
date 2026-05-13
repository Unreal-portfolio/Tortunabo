#pragma once

#include "CoreMinimal.h"
#include "Player/TortugaCharacter.h"
#include "TortugaFirstPersonCharacter.generated.h"

/**
 * @brief Variante en primera persona de ATortugaCharacter usada en cinemáticas.
 *
 * Hereda toda la lógica de ATortugaCharacter pero fuerza el SpringArm a longitud 0
 * en BeginPlay para colocar la cámara en la cabeza. Sólo se usa en escenas
 * cinemáticas — el gameplay normal usa la tercera persona.
 */
UCLASS()
class TORTUNABO_API ATortugaFirstPersonCharacter : public ATortugaCharacter
{
	GENERATED_BODY()

public:
	ATortugaFirstPersonCharacter();

protected:
	/** @brief Aplica longitud 0 al SpringArm para anclar la cámara en la cabeza. */
	virtual void BeginPlay() override;
};
