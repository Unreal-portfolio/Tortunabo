// PlayerState del Rally: hereda el del juego (nombre de Steam y cosméticos de la tortuga, que pinta el buggy) y añade la
// plaza que ocupa en su buggy.
#pragma once

#include "CoreMinimal.h"
#include "Core/TN_CoopPlayerState.h"
#include "Rally/TN_RallyVehicle.h"
#include "TN_RallyPlayerState.generated.h"

UCLASS()
class TORTUNABO_API ATN_RallyPlayerState : public ATN_CoopPlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor: plaza y equipo (INDEX_NONE = sin buggy, mira la carrera). */
	void SetRallySeat(int32 InTeamIndex, ETNRallySeat InSeat);
	void ClearRallySeat();

	UFUNCTION(BlueprintPure, Category = "Rally")
	bool IsSeated() const { return RallyTeamIndex != INDEX_NONE; }

	UFUNCTION(BlueprintPure, Category = "Rally")
	bool IsGunner() const { return IsSeated() && RallySeat == ETNRallySeat::Gunner; }

	UFUNCTION(BlueprintPure, Category = "Rally")
	int32 GetRallyTeamIndex() const { return RallyTeamIndex; }

	UFUNCTION(BlueprintPure, Category = "Rally")
	ETNRallySeat GetRallySeat() const { return RallySeat; }

private:
	UPROPERTY(Replicated)
	int32 RallyTeamIndex = INDEX_NONE;

	UPROPERTY(Replicated)
	ETNRallySeat RallySeat = ETNRallySeat::Driver;
};
