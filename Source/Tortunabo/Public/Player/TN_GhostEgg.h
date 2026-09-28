#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_GhostEgg.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTN_EggSynthComponent;

/**
 * Huevo de TNGhost::ReviveIntoEgg (Docs/Fantasma_Espectador.md): el mismo huevo que los de la salida del mapa procedural,
 * del lobby y de la carrera (base y tapa de TNCastleKit, con el color de acento de cada jugador). Sale del suelo de un
 * saltito, espera al fantasma (que se mete de cabeza), se ilumina por dentro y vibra «pum, pum, pum»; al eclosionar la
 * tapa salta dando vueltas como en la salida y la tortuga sale de un saltito (lo lanzan a la vez el servidor y su
 * dueño, como en ATN_ProcStartStructure). Después la base se hunde en la arena y el huevo desaparece.
 *
 * Lo crea el servidor; se replica siempre (bAlwaysRelevant): viajan quién vuelve, los tiempos y la tortuga que sale, y
 * cada máquina construye y anima sus mallas y sus sonidos (el «pum» sintetizado de la pantalla de carga del huevo).
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API ATN_GhostEgg : public AActor
{
	GENERATED_BODY()

public:
	ATN_GhostEgg();

	/** Salto con el que sale la tortuga (cm/s): el de los huevos de la salida del mapa procedural. */
	static constexpr float HopSpeedForward = 380.f;
	static constexpr float HopSpeedUp = 620.f;

	/** Servidor, antes de FinishSpawning: quién vuelve y los tiempos (segundos del servidor). */
	void InitEgg(APlayerController* InReviver, float InAppearTime, float InArriveTime, float InHatchTime);

	/** Servidor: el huevo eclosiona con la tortuga Pawn dentro (null: sale vacío). */
	void Hatch(APawn* Pawn);

	APlayerController* GetReviver() const { return Reviver.Get(); }
	APlayerState* GetReviverState() const { return ReviverState; }
	float GetHatchTime() const { return HatchTime; }
	bool HasHatched() const { return bHatched; }

	/** Velocidad del saltito de salida (hacia su X y hacia arriba). */
	FVector GetHopVelocity() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> ReviverState;

	UPROPERTY(Replicated)
	float AppearTime = 0.f;

	UPROPERTY(Replicated)
	float ArriveTime = 0.f;

	UPROPERTY(Replicated)
	float HatchTime = 0.f;

	/** Color del huevo (el acento de los huevos de la pila, TNCastleKit::EggAccent). */
	UPROPERTY(Replicated)
	uint8 AccentIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Hatched)
	bool bHatched = false;

	/** La tortuga que ha salido (su dueño la lanza también). */
	UPROPERTY(Replicated)
	TObjectPtr<APawn> HatchedPawn;

	UFUNCTION()
	void OnRep_Hatched();

	UPROPERTY(VisibleAnywhere, Category = "Egg")
	TObjectPtr<USceneComponent> EggRoot;

	/** Lo que se estira, se aplasta y se inclina (base y tapa cuelgan de aquí). */
	UPROPERTY(VisibleAnywhere, Category = "Egg")
	TObjectPtr<USceneComponent> WobbleRoot;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Egg")
	TObjectPtr<UProceduralMeshComponent> CupMesh;

	UPROPERTY(VisibleAnywhere, Category = "Egg")
	TObjectPtr<UStaticMeshComponent> Lid;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> LidStaticMesh;

	/** Luz de dentro mientras el fantasma está en el huevo. */
	UPROPERTY(VisibleAnywhere, Category = "Egg")
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY(Transient)
	TObjectPtr<UTN_EggSynthComponent> Synth;

	/** Servidor: quién vuelve a la vida. */
	TWeakObjectPtr<APlayerController> Reviver;

	bool bBuilt = false;
	bool bHatchRequested = false;
	bool bArrivalCued = false;
	bool bAppearCued = false;
	int32 KnocksPlayed = 0;
	/** Instante local (s del mundo) en que esta máquina vio eclosionar el huevo (< 0: todavía no). */
	float LocalHatchSeen = -1.f;
	bool bLocalHopDone = false;
	float LidSide = 1.f;
	float KnockTilt = 0.f;
	float KnockTiltSide = 1.f;

	void BuildMeshes();
	void UpdateLook(float DeltaSeconds);
	void StartLocalHatch();
	void TryLocalHop();
	bool ShouldPlayWorldSound() const;
};
