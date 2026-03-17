#pragma once

#include "CoreMinimal.h"
#include "Player/TortugaCharacter.h"
#include "TortugaFirstPersonCharacter.generated.h"

UCLASS()
class TORTUNABO_API ATortugaFirstPersonCharacter : public ATortugaCharacter
{
	GENERATED_BODY()

public:
	ATortugaFirstPersonCharacter();

protected:
	virtual void BeginPlay() override;
};

