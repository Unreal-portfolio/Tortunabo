#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_BeachRoundSyncComponent.generated.h"

class AController;
class APlayerController;
class ATN_BeachRaceGenerator;

/**
 * Aviso de cada cliente de que tiene montada la ronda de la playa (Docs/Modo_Carrera.md, «Seguridad: nunca bajo el
 * mapa»). Los asientos del terreno y el decorado local (~4000 piezas con colisión) los monta cada máquina por su cuenta y
 * en varios fotogramas: si la salida se diera antes de que un cliente acabara, su movimiento predicho atravesaría (o
 * chocaría con) lo que en el servidor es distinto, con correcciones, tortugas metidas en el decorado o bajo la arena.
 *
 * ATN_BeachRaceGameMode lo añade en ejecución (replicado) al PlayerController de cada jugador. En el cliente dueño mira
 * cuatro veces por segundo el generador y, cuando ATN_BeachRaceGenerator::IsRoundReady de la ronda actual, se lo dice al
 * servidor (ServerReportRoundReady, fiable) una vez por ronda. El GameMode espera a que todos los clientes que corren lo
 * hayan dicho (con tope, ClientRoundReadyTimeoutSeconds) antes de soltar a las tortugas.
 */
UCLASS(ClassGroup = (Custom))
class TORTUNABO_API UTN_BeachRoundSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_BeachRoundSyncComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** El del jugador, si ya lo tiene (cualquier máquina). */
	static UTN_BeachRoundSyncComponent* FindOn(const AController* Controller);

	/** Servidor: el del jugador, creándolo y registrándolo (replicado) si aún no lo tiene. */
	static UTN_BeachRoundSyncComponent* FindOrAddOn(APlayerController* Controller);

	/** Servidor: la última ronda (ATN_BeachRaceGenerator::GetRoundNumber) que este cliente ha dicho tener montada; 0 = ninguna. */
	int32 GetReadyRound() const { return ReadyRound; }

protected:
	virtual void BeginPlay() override;

	/** Cliente dueño → servidor: la ronda Round ya está montada en esta máquina (terreno con sus asientos y decorado local). */
	UFUNCTION(Server, Reliable)
	void ServerReportRoundReady(int32 Round);

private:
	/** Servidor: lo último que ha dicho el cliente. */
	int32 ReadyRound = 0;

	/** Cliente dueño: lo último que ha dicho (una vez por ronda). */
	int32 ReportedRound = 0;

	/** Cliente dueño: el generador de la playa de este mundo (se busca una vez). */
	TWeakObjectPtr<ATN_BeachRaceGenerator> Generator;
};
