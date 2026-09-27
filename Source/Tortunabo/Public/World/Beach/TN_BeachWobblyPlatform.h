#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "TN_BeachWobblyPlatform.generated.h"

class ACharacter;
class UBoxComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTN_BeachTrapSynthComponent;

/**
 * Plataforma sobre un hoyo de la arena. El hoyo lo trae el elemento en su malla: como el terreno es fijo y no se cava, es
 * un cráter de arena amontonada alrededor (la que sacaron los niños al cavar) con el fondo a ras del suelo (Z = 0) y una
 * brecha en el lado +Y por la que se sale andando. Medidas con SizeScale = 1 (se ajustan a la huella, 900·SizeScale):
 * borde de fuera a 870, cresta de 90 de ancho a ~240 de alto, ladera de fuera a 30° (se sube andando), pared de dentro
 * empinada y hoyo de ~364 de radio arriba.
 *
 * Encima, a lo largo de X (X local = sentido de la carrera), una tabla vieja o una tapa de nevera (según la semilla) de
 * 2·(radio del hoyo + 70) de largo, apoyada en la cresta. Se tambalea con una tortuga (balanceo de lado hasta MaxRollDeg
 * con un muelle poco amortiguado; los aterrizajes la sacuden) y cruje. Con BreakRiders o más tortugas a la vez, se va
 * agrietando (tiembla y cruje cada vez más) y a los CrackSeconds se parte por la mitad: las dos mitades caen al hoyo y
 * se quedan como rampas (~34°) de la cresta al fondo; quien estuviera encima cae al hoyo y sale por la brecha o subiendo
 * por una mitad. No se recompone en la ronda (el generador recoloca todo en la siguiente).
 *
 * Red: la tabla es una base móvil (UBoxComponent con nombre estable) que cada máquina inclina con las tortugas que ve
 * encima (ángulos pequeños, como el puente del lobby); la rotura la decide el servidor (BrokenAt, hora del servidor
 * replicada) y cada máquina anima la caída de las mitades desde esa hora.
 */
UCLASS()
class TORTUNABO_API ATN_BeachWobblyPlatform : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachWobblyPlatform();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsBroken() const { return BrokenAt >= 0.f; }

	/** Tortugas a la vez que la rompen. */
	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "1", ClampMax = "4"))
	int32 BreakRiders = 2;

	/** Segundos con BreakRiders tortugas encima hasta partirse (baja despacio si se bajan). */
	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "0.1"))
	float CrackSeconds = 1.1f;

	/** Balanceo máximo de lado a lado (grados). */
	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "0.0", ClampMax = "15.0"))
	float MaxRollDeg = 7.f;

	/** Volumen de los crujidos. */
	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CreakVolume = 0.9f;

protected:
	virtual void BeginPlay() override;
	virtual void ApplySpec() override;

	UFUNCTION()
	void OnRep_Broken();

	/** Hora del servidor en que se partió (< 0 = entera). */
	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	float BrokenAt = -1.f;

	/** Cráter de arena con la brecha. */
	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UStaticMeshComponent> CraterMesh;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UProceduralMeshComponent> CraterCollision;

	/** Centro de la tabla sobre la cresta: se inclina. */
	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<USceneComponent> BoardPivot;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UStaticMeshComponent> BoardMesh;

	/** Tabla entera: base móvil. */
	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UBoxComponent> BoardBox;

	/** Bisagras de las mitades en el borde de dentro de la cresta (-X y +X; la de +X mira hacia -X). */
	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<USceneComponent> HalfPivotA;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<USceneComponent> HalfPivotB;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UStaticMeshComponent> HalfMeshA;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UStaticMeshComponent> HalfMeshB;

	/** Mitades caídas: rampas del fondo a la cresta (solo con colisión al terminar de caer). */
	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UBoxComponent> HalfBoxA;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UBoxComponent> HalfBoxB;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachTrapSynthComponent> Voice;

private:
	struct FRider
	{
		float LastVz = 0.f;
		float CreakTimer = 0.f;
		bool bOn = false;
		bool bNear = false;
	};

	void TickBoard(float DeltaSeconds, double Now);
	void TickHalves(double Now);

	/** Servidor: la parte. */
	void BreakBoard();

	/** Estado roto en esta máquina (colisiones, mallas y efectos). */
	void ApplyBroken(bool bWithFX);

	void PlayCreak(float Volume, float PitchMul, const FVector& WorldAt);

	double RimHeight = 240.0;
	double PitRadius = 364.0;
	double FloorRadius = 280.0;
	double OuterRadius = 870.0;
	double BoardHalfL = 434.0;
	double BoardHalfW = 110.0;
	double BoardThick = 24.0;
	double DropAngleDeg = 33.0;

	float Roll = 0.f;
	float RollVel = 0.f;
	float Pitch = 0.f;
	float PitchVel = 0.f;
	float Crack = 0.f;
	float CrackCreakTimer = 0.f;
	bool bBrokenApplied = false;
	bool bHalvesLanded = false;
	bool bHalvesSettled = false;

	TMap<TWeakObjectPtr<ACharacter>, FRider> Riders;
	FTNTrapClock Clock;
	FTNTrapBurst Chips;
	FTNTrapBurst Dust;
	FTNTrapPopText Pop;
};
