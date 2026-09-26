#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_PathStorm.generated.h"

class UStaticMeshComponent;
class UPostProcessComponent;
class ATN_ProcMapGenerator;
class APlayerController;

/**
 * Tormenta del mapa procedural (modo Coop). A diferencia de ATN_StormVolume, que
 * es una caja que crece en línea recta, esta avanza A LO LARGO DEL CAMINO: su
 * frente es una distancia sobre el camino principal. Quien quede por detrás del
 * frente (según su progreso proyectado sobre el camino, en 3D para distinguir
 * puentes y cuevas) empieza la cuenta atrás de muerte.
 *
 * Replicada: el servidor avanza FrontProgress; los clientes lo extrapolan con la
 * velocidad para mover el muro visual y activar el post-proceso local.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_PathStorm : public AActor
{
	GENERATED_BODY()

public:
	ATN_PathStorm();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor: arranca la tormenta tras GraceSeconds, a Speed cm/s por el camino. */
	void StartStorm(ATN_ProcMapGenerator* InGenerator, float InSpeed, float GraceSeconds);

	/** Servidor: detiene y reinicia (entre rondas). */
	void StopStorm();

	UFUNCTION(BlueprintPure, Category = "Storm")
	float GetFrontProgress() const { return FrontProgress; }

	UFUNCTION(BlueprintPure, Category = "Storm")
	bool IsStormActive() const { return bActive; }

	/** Fuerza a re-evaluar a un jugador (tras revivir). */
	void ForceCheckPlayer(APlayerController* PC);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm")
	TObjectPtr<USceneComponent> Root;

	/** Muro visual del frente (asignar material de tormenta en el BP). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm")
	TObjectPtr<UStaticMeshComponent> FrontWall;

	/** Post-proceso local de visión degradada (configurar en el BP). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm")
	TObjectPtr<UPostProcessComponent> InsidePostProcess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.1"))
	float SecondsInsideToDie = 5.f;

	/** Margen por detrás del frente antes de contar como dentro (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float InsideMargin = 400.f;

private:
	UPROPERTY(Replicated)
	float FrontProgress = 0.f;

	UPROPERTY(Replicated)
	float Speed = 0.f;

	UPROPERTY(Replicated)
	bool bActive = false;

	UPROPERTY(Replicated)
	TObjectPtr<ATN_ProcMapGenerator> Generator;

	float GraceRemaining = 0.f;
	float CheckAccumulator = 0.f;
	TMap<TWeakObjectPtr<APlayerController>, float> InsideTime;

	void ServerCheckPlayers(float Interval);
	void UpdateVisual();
};
