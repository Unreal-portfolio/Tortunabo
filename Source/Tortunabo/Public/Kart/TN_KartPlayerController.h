// PlayerController de los karts: el del Rally (HUD, cámaras, voz, cosméticos) y, en cada cliente, el aviso al servidor de
// que ya tiene la pista y la colisión del suelo de la generación del mapa (ATN_KartGameState::IsLocalTrackPlayable): el
// servidor no sienta a nadie hasta que todas lo tienen (ATN_KartGameMode).
#pragma once

#include "CoreMinimal.h"
#include "Rally/TN_RallyPlayerController.h"
#include "TN_KartPlayerController.generated.h"

UCLASS()
class TORTUNABO_API ATN_KartPlayerController : public ATN_RallyPlayerController
{
	GENERATED_BODY()

protected:
	virtual void PlayerTick(float DeltaTime) override;

private:
	/** Cliente → servidor: esta máquina tiene la pista y el suelo de la generación Generation del mapa. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerReportKartTrackReady(int32 Generation);

	/** Última generación avisada (0 = ninguna). */
	int32 ReportedGeneration = 0;
	float ReadyCheckAccumulator = 0.f;
};
