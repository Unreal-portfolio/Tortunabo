#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/TN_TurtleSurface.h"
#include "TN_TurtleMovementComponent.generated.h"

class ATortugaCharacter;
class ATN_ProcMapGenerator;

/** Fase del panzazo en el suelo. La simulan igual el cliente dueño (predicha y guardada en sus movimientos) y el servidor. */
enum class ETNBellyPhase : uint8
{
	/** Normal, o el panzazo aún en el aire. */
	None = 0,
	/** Arrastrándose sobre la tripa: poco rozamiento, sigue las pendientes y rebota contra lo que encuentra. */
	Slide = 1,
	/** Ya parada pero sin sitio encima para ponerse de pie (algo bajo encima): repta despacio hasta que lo haya. */
	Rest = 2,
	/** De pie otra vez (cápsula entera): unos instantes con la velocidad recortada mientras se levanta. */
	GetUp = 3,
};

/**
 * Movimiento de la tortuga: el de UCharacterMovementComponent más el arrastre del panzazo.
 *
 * Al caer de tripa (ATortugaCharacter::IsDiving, número de panzazo nuevo), en vez de quedarse tiesa se arrastra: conserva
 * la inercia a lo largo del suelo (en una bajada, parte de la caída se convierte en arrastre), frena según la superficie
 * (TNTurtleSurface: arena mucho, agua y fango poco; entre 0,6 y 1,2 s), sigue las pendientes (cuesta abajo se desliza
 * más; en las suaves se queda quieta) y rebota un poco contra paredes y obstáculos. Casi parada, se levanta: la cápsula
 * vuelve a su altura sin subir por dentro de nada (si no cabe, repta sobre la tripa hasta que quepa) y unos instantes
 * anda más despacio. Saltando o moviéndose por debajo de BellyExitSpeed sale antes (el salto es un brinco).
 *
 * Todo ocurre dentro de la simulación del movimiento, con el estado (fase, tiempo y número de panzazo) guardado en cada
 * movimiento del cliente (FTNSavedMove_Turtle): el cliente dueño lo predice igual que el servidor y, si hay corrección,
 * lo repite desde el estado de entonces. No hay física de verdad (desincronizaría y atravesaría paredes al volver a
 * poner la cápsula); la cápsula barre como siempre. El servidor acaba el panzazo (ATortugaCharacter::TickDive) cuando
 * este componente dice que ya se ha levantado; el resto de máquinas solo ven el movimiento replicado.
 *
 * Tumbada, además, el cuerpo entero choca: la cabeza y las patas, que sobresalen de la cápsula, no se meten en las paredes
 * (KeepBellyBodyOutOfWalls).
 *
 * Consola (igual en todas las máquinas; en PIE es una sola): TN.Dive.Slide, TN.Dive.Friction, TN.Dive.Slope,
 * TN.Dive.MaxTime, TN.Dive.Body y TN.Dive.Debug. Ver Docs/Animacion_Tortuga.md.
 */
UCLASS()
class TORTUNABO_API UTN_TurtleMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UTN_TurtleMovementComponent();

	// ── Estado del arrastre (lo leen el personaje, la animación, el sonido y el polvo) ──

	ETNBellyPhase GetBellyPhase() const { return BellyPhase; }

	/** Segundos en la fase actual (arrastrándose, reptando o levantándose). */
	float GetBellyTime() const { return BellyTime; }

	/** Número del panzazo del que viene el arrastre (0 = ninguno todavía). */
	uint8 GetSlideSerial() const { return SlideSerial; }

	bool IsBellySliding() const { return BellyPhase == ETNBellyPhase::Slide; }

	/** En el suelo sobre la tripa (arrastrándose o reptando sin sitio para levantarse). */
	bool IsOnBelly() const { return BellyPhase == ETNBellyPhase::Slide || BellyPhase == ETNBellyPhase::Rest; }

	/** Ya se ha levantado del panzazo número DiveSerial (su cápsula vuelve a ser la de pie). */
	bool HasStoodUpFromDive(uint8 DiveSerial) const;

	/** Durante el panzazo número DiveSerial admite el movimiento del jugador: casi parada, reptando o ya levantándose. */
	bool AcceptsInputDuringDive(uint8 DiveSerial) const;

	/** Pesos de la superficie bajo la tripa en el último paso del arrastre (arena, tierra, roca, madera y agua). */
	const float* GetSlideSurface() const { return SlideSurface; }

	/**
	 * Repetición de movimientos tras una corrección (FTNSavedMove_Turtle::PrepMoveFor): deja el estado del arrastre como
	 * estaba al empezar ese movimiento. Si entonces iba sobre la tripa, también la cápsula encogida.
	 */
	void RestoreBellyState(uint8 InPhase, float InTime, uint8 InSerial, float InCapsuleHalfHeight);

	/**
	 * Estado del arrastre al empezar el movimiento que se va a guardar (FTNSavedMove_Turtle::SetInitialPosition). El
	 * cliente lee el salto antes de guardar el movimiento: si ese salto la ha levantado de la tripa, devuelve (y olvida)
	 * el estado de antes del salto, como hace el motor con JumpCurrentCountPreJump.
	 */
	void ConsumeMoveStartBellyState(uint8& OutPhase, float& OutTime, uint8& OutSerial, float& OutCapsuleHalfHeight);

	// ── UCharacterMovementComponent ──────────────────────────────────────────

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;
	virtual float GetMaxSpeed() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	// ── Ajustes del arrastre ─────────────────────────────────────────────────
	// Rozamiento por superficie: lo que frena por sí solo (cm/s²), aparte del freno por velocidad (BellyDrag). Con la
	// entrada típica (panzazo andando: 350 + 450 cm/s, un 90 % tras el golpe = 720 cm/s) y BellyDrag 1,5 se para en:
	// arena 0,57 s (1,8 m), tierra 0,79 s (2,3 m), roca 0,87 s (2,5 m), madera 1 s (2,7 m) y agua o fango 1,18 s (3,1 m).
	// TN.Dive.Friction los multiplica todos en caliente.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Friction", meta = (ClampMin = "0.0"))
	float BellyFrictionSand = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Friction", meta = (ClampMin = "0.0"))
	float BellyFrictionSoil = 480.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Friction", meta = (ClampMin = "0.0"))
	float BellyFrictionRock = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Friction", meta = (ClampMin = "0.0"))
	float BellyFrictionWood = 310.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Friction", meta = (ClampMin = "0.0"))
	float BellyFrictionWater = 220.f;

	/** Freno proporcional a la velocidad (1/s): corta antes los arrastres muy rápidos. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Friction", meta = (ClampMin = "0.0"))
	float BellyDrag = 1.5f;

	/** Pasado este tiempo arrastrándose (s), el rozamiento crece: ninguna bajada la arrastra para siempre. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Friction", meta = (ClampMin = "0.0"))
	float BellyFrictionRampStart = 1.2f;

	/** Cuánto crece el rozamiento por segundo pasado BellyFrictionRampStart (2,5 = x3,5 al segundo). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Friction", meta = (ClampMin = "0.0"))
	float BellyFrictionRamp = 2.5f;

	/** Velocidad a lo largo del suelo que conserva al caer de tripa (el resto se lo come el golpe). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Speed", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BellyLandingKeep = 0.9f;

	/** Tope de la velocidad al empezar a arrastrarse (cm/s): algo más que esprintar, pero frena en seguida. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Speed", meta = (ClampMin = "0.0"))
	float BellyMaxEntrySpeed = 850.f;

	/** Tope de la velocidad arrastrándose (cm/s), también cuesta abajo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Speed", meta = (ClampMin = "0.0"))
	float BellyMaxSpeed = 1000.f;

	/** Multiplica la gravedad a lo largo de las pendientes (cuesta abajo acelera; cuesta arriba frena antes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Speed", meta = (ClampMin = "0.0"))
	float BellySlopeGravity = 1.15f;

	/** Tiempo mínimo arrastrándose antes de levantarse sola (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Time", meta = (ClampMin = "0.0"))
	float BellyMinSeconds = 0.3f;

	/** Por debajo de esta velocidad (cm/s), pasado el tiempo mínimo, se levanta. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Time", meta = (ClampMin = "0.0"))
	float BellyStopSpeed = 60.f;

	/** Tope de tiempo arrastrándose (s): se levanta aunque siga moviéndose. TN.Dive.MaxTime lo cambia en caliente. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Time", meta = (ClampMin = "0.2"))
	float BellyMaxSeconds = 2.6f;

	/**
	 * Por debajo de esta velocidad (cm/s) se puede salir del arrastre saltando (brinco) o moviéndose. Quien lleva el
	 * avance pulsado todo el panzazo se levanta aquí: se pierde solo el final lento (unos 0,3 s y 30 cm).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Exit", meta = (ClampMin = "0.0"))
	float BellyExitSpeed = 180.f;

	/** Tiempo mínimo en el suelo antes de poder salir a propósito (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Exit", meta = (ClampMin = "0.0"))
	float BellyMinExitSeconds = 0.15f;

	/** Reptar sin sitio para ponerse de pie (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Exit", meta = (ClampMin = "0.0"))
	float BellyCrawlSpeed = 150.f;

	/** Lo que tarda en levantarse (s): mientras, la velocidad máxima sube desde BellyGetUpSpeedFraction hasta la normal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Exit", meta = (ClampMin = "0.0"))
	float BellyGetUpSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Exit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BellyGetUpSpeedFraction = 0.35f;

	/** Rebote: fracción de la velocidad contra la pared que devuelve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Bounce", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BellyBounceRestitution = 0.35f;

	/** Rebote: fracción de la velocidad a lo largo de la pared que conserva. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Bounce", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BellyBounceTangentKeep = 0.75f;

	/** Velocidad contra la pared (cm/s) por debajo de la cual no rebota: se queda pegada, como andando. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Bounce", meta = (ClampMin = "0.0"))
	float BellyBounceMinSpeed = 120.f;

	/** El cuerpo gira hacia donde se desliza (grados/s) si va a más de BellyTurnMinSpeed y la diferencia es menor que BellyTurnMaxAngle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Turn", meta = (ClampMin = "0.0"))
	float BellyTurnRate = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Turn", meta = (ClampMin = "0.0"))
	float BellyTurnMinSpeed = 120.f;

	/** Tras un rebote hacia atrás no se da la vuelta: se aleja de la pared mirándola. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Turn", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float BellyTurnMaxAngle = 100.f;

	// ── Cuerpo tumbado ───────────────────────────────────────────────────────
	// La cápsula del movimiento es vertical (radio 34) y solo cubre el centro del cuerpo tumbado, que mide ~1,4 m de
	// largo: la cabeza y las aletas sobresalen por delante y las patas por detrás. Tumbada (panzazo en el aire, arrastre y
	// reptar), tras cada movimiento una esfera a la altura del caparazón barre desde el centro hacia cada punta; si una
	// punta se metería en una pared, la tortuga se aparta lo justo (y rebota si iba contra ella). Las medidas suponen el
	// cuerpo centrado sobre la cápsula (ATortugaCharacter::DiveBodyCenterShift). TN.Dive.Body 0 lo apaga.

	/** Del centro de la cápsula a la punta de delante del cuerpo tumbado (cabeza y aletas), en cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Body", meta = (ClampMin = "0.0"))
	float BellyBodyReachFront = 66.f;

	/** Del centro de la cápsula a la punta de detrás (las patas), en cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Body", meta = (ClampMin = "0.0"))
	float BellyBodyReachBack = 62.f;

	/** Medio grosor del cuerpo tumbado: radio de la esfera que barre (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Body", meta = (ClampMin = "5.0"))
	float BellyBodyRadius = 14.f;

	/**
	 * Altura del centro de la esfera sobre la base de la cápsula (cm). Lo que quede por debajo de esa altura menos el radio
	 * (bordillos y baches bajos) no frena al cuerpo: lo pisa la cápsula como siempre.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Belly Slide|Body", meta = (ClampMin = "0.0"))
	float BellyBodyProbeHeight = 32.f;

protected:
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

private:
	ATortugaCharacter* GetTurtle() const;

	/** Esta máquina simula el movimiento de la tortuga (el dueño, el servidor o el propio anfitrión; no los demás). */
	bool SimulatesBelly() const;

	/** Empieza el arrastre del panzazo Serial con la inercia a lo largo del suelo tocado. */
	void StartBellySlide(const FHitResult& FloorHit, uint8 Serial, bool bFromAir);

	/** Deja la velocidad horizontal a lo largo del suelo tocado, con Keep de lo que tenía y como mucho Cap. */
	void RedirectAlongFloor(const FHitResult& FloorHit, float Keep, float Cap);

	/** Antes de cada movimiento: entra, sigue, se levanta o repta. */
	void TickBellyPhase(float DeltaSeconds);

	/** Cápsula de pie sin moverse de base; false (sin tocar nada) si no cabe. */
	bool TryStandUp();

	void EnterGetUp();

	/** Arrastrándose, casi parada y pasado el mínimo: se puede salir saltando o moviéndose. */
	bool CanLeaveSlide() const;

	/** Superficie bajo la tripa (el suelo del movimiento) y rozamiento que toca ahora. */
	void UpdateSlideSurface();
	float SlideFrictionNow() const;

	/** Velocidad del arrastre en este paso: pendiente, rozamiento, freno por velocidad y tope. */
	void CalcBellySlideVelocity(float DeltaTime);

	/**
	 * Tumbada, tras cada movimiento: si la cabeza o las patas (fuera de la cápsula) se meterían en una pared, aparta a la
	 * tortuga lo justo, le quita la velocidad contra la pared y, arrastrándose, apunta el rebote.
	 */
	void KeepBellyBodyOutOfWalls();

	/** TN.Dive.Debug: línea en pantalla y flechas. */
	void ShowBellyDebug() const;

	ETNBellyPhase BellyPhase = ETNBellyPhase::None;
	float BellyTime = 0.f;
	uint8 SlideSerial = 0;

	float SlideSurface[TNTurtleSurface::Num] = { 0.f, 0.f, 1.f, 0.f, 0.f };
	float LastSlideFriction = 0.f;
	FVector LastSlopeAccel = FVector::ZeroVector;

	/** Velocidad que quería llevar el arrastre en este movimiento (antes de chocar) y pared contra la que ha chocado. */
	FVector SlideIntentVelocity = FVector::ZeroVector;
	FVector PendingBounceNormal = FVector::ZeroVector;
	bool bPendingBounce = false;

	/** Último apartón del cuerpo tumbado contra una pared (TN.Dive.Debug). */
	FVector LastBodyPush = FVector::ZeroVector;

	/** Estado de antes del brinco que la levantó de la tripa en el movimiento que se está guardando (ver ConsumeMoveStartBellyState). */
	bool bHasPreJumpBelly = false;
	uint8 PreJumpPhase = 0;
	float PreJumpTime = 0.f;
	uint8 PreJumpSerial = 0;
	float PreJumpCapsuleHalfHeight = 0.f;

	TNTurtleSurface::FNameCache SurfaceNameCache;
	TWeakObjectPtr<ATN_ProcMapGenerator> Generator;
	double NextGeneratorLookup = 0.0;
};
