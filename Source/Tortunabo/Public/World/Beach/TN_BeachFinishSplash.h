#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TN_BeachFinishSplash.generated.h"

class ACharacter;
class ATN_BeachRaceGenerator;
class UTN_BeachSplashSynthComponent;

/**
 * @brief Chapuzón de la meta del modo carrera en cada máquina con jugador (Docs/Modo_Carrera.md, «Salto final al agua»).
 *
 * El generador de la playa ya pinta, al entrar los pies de una tortuga en el agua de meta, la corona de gotas, la espuma
 * y las ondas (ATN_BeachRaceGenerator::Splash). Esto añade lo que faltaba, mirando lo mismo (el movimiento replicado de
 * cada tortuga, sin RPC): el chorro de agua que sube un instante después (gotas y una columna de espuma blanca) y el
 * «¡chof!» sintetizado (UTN_BeachSplashSynthComponent). El tamaño sale de la velocidad de caída al tocar el agua: el
 * salto del acantilado es el grande. Efectos de TNAmbientFX en un actor local sin réplica; nada en servidor dedicado.
 *
 * Además, con la ronda de la carrera en juego, la tortuga que entra en el agua de meta se queda con la postura de la
 * zambullida (bPauseAnims de su malla, solo cosmético y en cada máquina) hasta que la meta la oculta: nadie la ve ponerse de
 * pie en el agua mientras en su pantalla se cierra el huevo negro del puesto (UTN_RaceScreensSubsystem). Si sale del agua
 * sin llegar (la sacan de ahí), o sigue a la vista 2,5 s después (no era una llegada), recupera la animación.
 *
 * Consola (solo en la máquina que lo escribe): TN.Race.Splash [tamaño] hace un chapuzón donde está tu tortuga.
 */
UCLASS()
class TORTUNABO_API UTN_BeachFinishSplashSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Chapuzón en WorldLocation (a ras del agua) de tamaño Size (1 = el salto del acantilado), solo en esta máquina. */
	void PlaySplash(const FVector& WorldLocation, float Size);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Lo que se recuerda de cada tortuga entre fotogramas. */
	struct FTurtleWater
	{
		bool bWet = false;
		/** Velocidad vertical del fotograma anterior (al tocar el agua, la de la caída; el agua ya la ha frenado). */
		float VelocityZ = 0.f;
		/** Su postura se ha congelado aquí al llegar (bPauseAnims): solo esta la descongela. */
		bool bPoseFrozen = false;
		/** Cuándo se congeló (reloj del subsistema): si sigue a la vista mucho después, no era una llegada y se descongela. */
		float FrozenAt = 0.f;
	};

	/** Chorro que sube un momento después del golpe (cuando se cierra el hueco que abre la tortuga). */
	struct FPendingJet
	{
		float At = 0.f;
		FVector Location = FVector::ZeroVector;
		float Size = 1.f;
	};

	/** Actor local (sin réplica) dueño de los efectos y del sonido. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> FXOwner;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachSplashSynthComponent> Synth;

	TWeakObjectPtr<ATN_BeachRaceGenerator> Generator;
	TMap<TWeakObjectPtr<ACharacter>, FTurtleWater> Turtles;
	TArray<FPendingJet> PendingJets;
	int32 JetDropsFX = INDEX_NONE;
	int32 JetMistFX = INDEX_NONE;
	float Clock = 0.f;
	float NextGeneratorLookup = 0.f;

	/** Crea el actor de los efectos, sus emisores y el sintetizador (la primera vez). false si no se puede. */
	bool EnsureFX(const FVector& Near);
	void WatchTurtles(ATN_BeachRaceGenerator& InGenerator);
	void LaunchJet(const FPendingJet& Pending);
	/** Deja a la tortuga con la postura de la zambullida (o se la devuelve) sin tocar lo que haya pausado otro (el derribo). */
	void FreezePose(ACharacter& Turtle, FTurtleWater& Water) const;
	static void ThawPose(ACharacter& Turtle, FTurtleWater& Water);
};
