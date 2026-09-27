#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_PlaygroundPiece.generated.h"

class ACharacter;
class APawn;
class UBoxComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTN_PlaygroundSynthComponent;

/** Piezas de parkour en miniatura para el patio del castillo de arena. */
UENUM(BlueprintType)
enum class ETNPlaygroundPieceType : uint8
{
	BucketPost      UMETA(DisplayName = "Poste de cubos de playa"),
	PopsicleSteps   UMETA(DisplayName = "Escalón de palitos de helado"),
	ShellSlide      UMETA(DisplayName = "Rampa de concha (tobogán)"),
	CookiePlatform  UMETA(DisplayName = "Plataforma de galleta"),
	CastleTunnel    UMETA(DisplayName = "Túnel de castillo (panzazo)"),
	SpadeSpinner    UMETA(DisplayName = "Barra giratoria de pala"),
};

/** Colores de juguete de playa (los de las banderas del castillo). */
UENUM(BlueprintType)
enum class ETNPlaygroundTint : uint8
{
	Coral  UMETA(DisplayName = "Coral"),
	Teal   UMETA(DisplayName = "Turquesa"),
	Gold   UMETA(DisplayName = "Amarillo"),
	Sky    UMETA(DisplayName = "Celeste"),
	Lilac  UMETA(DisplayName = "Lila"),
	Mint   UMETA(DisplayName = "Menta"),
	Pink   UMETA(DisplayName = "Rosa"),
};

/**
 * Pieza suelta del parque de pruebas del lobby, configurable con PieceType. Todas con colisión convexa (un
 * UProceduralMeshComponent por defecto: se puede nombrar por red) y arte de playa construido en código. El origen está en
 * el suelo (cm):
 *
 * - Poste de cubos: cubos de plástico boca abajo apilados (colores Tint y SecondTint alternos, asa y nervios), de PostHeight
 *   de alto y ~64 de radio abajo y 49 arriba; estrella de mar en la cima. Centrado.
 * - Escalón de palitos: StepCount bloques forrados de palitos de helado (algunos teñidos) que suben hacia +X desde X = 0:
 *   cada uno StepDepth de fondo, StepHeight más alto que el anterior y StepWidth de ancho (centrado en Y).
 * - Rampa de concha: vieira tumbada (cara de nácar arriba) que baja hacia +X desde la charnela en X = 0 (a SlideHeight)
 *   hasta la arena, con SlideAngle de pendiente (por encima de ~44° no se sube andando: es tobogán, y empuja cuesta abajo
 *   con SlideBoost); detrás, plataforma de arena de 90 de fondo y, con bSlideSteps, escalera hacia -X.
 * - Plataforma de galleta: galleta de chocolate con pepitas de CookieRadius sobre una columna de galletas rellenas; la cara
 *   de arriba a PlatformHeight. Centrada.
 * - Túnel de castillo: muro de arena a lo largo de X, de TunnelLength de largo, 264 de ancho y 262 de alto, con almenas.
 *   Por dentro, 184 de ancho y techo de colisión a 178 (se pasa de pie: la tortuga mide 140), con bocas en embudo (el
 *   techo sube a 225 en la fachada). Dentro, las tortugas mantienen TunnelAssistSpeed a lo largo del túnel (salen
 *   disparadas, andando o en panzazo). Centrado.
 * - Barra giratoria de pala: cubo en el centro (radio 40, 50 de alto) y NumArms palas de ArmLength a ras del suelo (la hoja
 *   llega a ArmHeight + 12) que giran a SpinSpeed grados por segundo con el reloj del servidor. Hay que saltarlas: quien
 *   no, sale despedido (KnockSpeed de lado y KnockUp hacia arriba). Centrada.
 *
 * Red: como la medusa y el puente, lo que mueve personajes (empujones del túnel y del tobogán, golpes de la pala) lo
 * aplican a la vez el servidor y el cliente que controla a la tortuga. La pala usa el reloj del servidor en todas las
 * máquinas y el servidor mira el golpe de cada cliente en el instante que ese cliente veía (resta su ping): los dos suelen
 * coincidir. El golpe suena en todas las máquinas por un multicast no fiable.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_PlaygroundPiece : public AActor
{
	GENERATED_BODY()

public:
	ATN_PlaygroundPiece();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Rehace malla y colisión con las propiedades actuales (tras cambiarlas en ejecución sin SpawnActorDeferred). */
	UFUNCTION(BlueprintCallable, Category = "Pieza")
	void RebuildPiece();

	/** Altura (cm) de la cara de arriba donde se puede estar de pie. */
	UFUNCTION(BlueprintPure, Category = "Pieza")
	float GetStandHeight() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza")
	ETNPlaygroundPieceType PieceType = ETNPlaygroundPieceType::BucketPost;

	/** Color principal (cubos, mango de la pala, concha por fuera, banderín del túnel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza")
	ETNPlaygroundTint Tint = ETNPlaygroundTint::Coral;

	/** Color secundario (cubos alternos, hoja de la pala y cubo del centro). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza")
	ETNPlaygroundTint SecondTint = ETNPlaygroundTint::Teal;

	/** Semilla de los detalles (palitos teñidos, pepitas, piedrecitas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza")
	int32 DecorSeed = 1;

	// ── Poste de cubos ───────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Poste de cubos",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::BucketPost", EditConditionHides, ClampMin = "60.0", ClampMax = "400.0"))
	float PostHeight = 180.f;

	// ── Escalón de palitos ───────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Escalón de palitos",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::PopsicleSteps", EditConditionHides, ClampMin = "1", ClampMax = "5"))
	int32 StepCount = 3;

	/** Lo que sube cada escalón (hasta ~45 se sube andando; más alto, saltando). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Escalón de palitos",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::PopsicleSteps", EditConditionHides, ClampMin = "20.0", ClampMax = "70.0"))
	float StepHeight = 36.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Escalón de palitos",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::PopsicleSteps", EditConditionHides, ClampMin = "40.0", ClampMax = "100.0"))
	float StepDepth = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Escalón de palitos",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::PopsicleSteps", EditConditionHides, ClampMin = "80.0", ClampMax = "200.0"))
	float StepWidth = 120.f;

	// ── Rampa de concha ──────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Rampa de concha",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::ShellSlide", EditConditionHides, ClampMin = "60.0", ClampMax = "300.0"))
	float SlideHeight = 160.f;

	/** Pendiente (grados): por debajo de ~44° es rampa (se sube andando); por encima, tobogán. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Rampa de concha",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::ShellSlide", EditConditionHides, ClampMin = "20.0", ClampMax = "60.0"))
	float SlideAngle = 46.f;

	/** Ancho del labio de la concha (abajo); en la charnela mide 88. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Rampa de concha",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::ShellSlide", EditConditionHides, ClampMin = "120.0", ClampMax = "260.0"))
	float SlideWidth = 190.f;

	/** Escalera de arena detrás de la plataforma (hacia -X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Rampa de concha",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::ShellSlide", EditConditionHides))
	bool bSlideSteps = true;

	/** Empuje cuesta abajo sobre la concha (cm/s²; 0 = nada). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Pieza|Rampa de concha",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::ShellSlide", EditConditionHides, ClampMin = "0.0", ClampMax = "2000.0"))
	float SlideBoost = 900.f;

	// ── Plataforma de galleta ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Plataforma de galleta",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::CookiePlatform", EditConditionHides, ClampMin = "40.0", ClampMax = "300.0"))
	float PlatformHeight = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Plataforma de galleta",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::CookiePlatform", EditConditionHides, ClampMin = "80.0", ClampMax = "200.0"))
	float CookieRadius = 115.f;

	// ── Túnel de castillo ────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Túnel de castillo",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::CastleTunnel", EditConditionHides, ClampMin = "120.0", ClampMax = "400.0"))
	float TunnelLength = 220.f;

	/** Velocidad mínima a lo largo del túnel para quien está dentro (cm/s; 0 = sin ayuda). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Pieza|Túnel de castillo",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::CastleTunnel", EditConditionHides, ClampMin = "0.0", ClampMax = "900.0"))
	float TunnelAssistSpeed = 480.f;

	// ── Barra giratoria de pala ──────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Barra giratoria",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::SpadeSpinner", EditConditionHides, ClampMin = "120.0", ClampMax = "350.0"))
	float ArmLength = 230.f;

	/** Altura (cm) del mango sobre la arena; la hoja llega 12 más arriba. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Barra giratoria",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::SpadeSpinner", EditConditionHides, ClampMin = "15.0", ClampMax = "60.0"))
	float ArmHeight = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Barra giratoria",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::SpadeSpinner", EditConditionHides, ClampMin = "1", ClampMax = "3"))
	int32 NumArms = 2;

	/** Grados por segundo (negativo = al revés). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Pieza|Barra giratoria",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::SpadeSpinner", EditConditionHides, ClampMin = "-240.0", ClampMax = "240.0"))
	float SpinSpeed = 90.f;

	/** Ángulo de las palas en el instante 0 del reloj del servidor (y el que se ve en el editor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Config, Category = "Pieza|Barra giratoria",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::SpadeSpinner", EditConditionHides))
	float StartAngle = 0.f;

	/** Velocidad horizontal del empujón (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Pieza|Barra giratoria",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::SpadeSpinner", EditConditionHides, ClampMin = "200.0", ClampMax = "1500.0"))
	float KnockSpeed = 650.f;

	/** Velocidad vertical del empujón (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Pieza|Barra giratoria",
		meta = (EditCondition = "PieceType == ETNPlaygroundPieceType::SpadeSpinner", EditConditionHides, ClampMin = "0.0", ClampMax = "1200.0"))
	float KnockUp = 420.f;

	/** Volumen de los efectos (golpe y barrido de la pala). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pieza|Sonido", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float EffectsVolume = 1.f;

protected:
	UFUNCTION()
	void OnRep_Config();

	/** Golpe de la pala en todas las máquinas (el cliente dueño ya lo ha oído al predecirlo). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastKnockFX(APawn* Victim);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pieza")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Todo lo que no gira (malla en ejecución). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pieza")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Colisión convexa de la pieza (solo colisión, sin secciones que dibujar). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pieza")
	TObjectPtr<UProceduralMeshComponent> BodyCollision;

	/** Pivote que gira con las palas (barra giratoria). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pieza")
	TObjectPtr<USceneComponent> SpinPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pieza")
	TObjectPtr<UStaticMeshComponent> SpinMesh;

	/** Cajas de las palas: solo bloquean cuerpos con física (caparazones); a las tortugas las golpea la prueba geométrica. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pieza")
	TArray<TObjectPtr<UBoxComponent>> SpinBlockers;

	/** Zona de ayuda del túnel o del tobogán (solapamiento con personajes). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pieza")
	TObjectPtr<UBoxComponent> AssistZone;

	UPROPERTY(Transient)
	TObjectPtr<UTN_PlaygroundSynthComponent> Voice;

private:
	/** Malla, colisión y zonas; no hace nada si la configuración no ha cambiado (salvo bForce). */
	void BuildAll(bool bForce);
	uint32 ConfigHash() const;
	bool NeedsTick() const;

	/** Reloj del servidor suavizado (las palas van igual en todas las máquinas). */
	double AdvanceClock(float DeltaSeconds);
	double SpinAngleDeg(double AtTime) const;

	void TickSpinner(float DeltaSeconds);
	void TickTunnel();
	void TickSlide(float DeltaSeconds);

	/** Empujón de la pala a Victim, que está en el ángulo Theta (radianes, espacio del actor). */
	void Knock(ACharacter* Victim, double Theta, double WorldNow);
	void PlayKnockFX(const FVector& WorldAt);

	uint32 BuiltHash = 0;
	double Clock = 0.0;
	bool bClockValid = false;
	float WhooshCooldown = 0.f;
	/** Dirección cuesta abajo del tobogán (espacio del actor). */
	FVector SlideDownDir = FVector::ForwardVector;
	TMap<TWeakObjectPtr<ACharacter>, double> LastKnockTime;
};
