#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_SandCastleLobby.generated.h"

class APlayerController;
class UBoxComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * El lobby como castillo de arena redondo (LVL_Lobby). Va colocado en el nivel y se construye en el editor
 * (OnConstruction) y en ejecución, así que se ve y se ajusta sin darle al Play. Solo se replican el estado de la puerta
 * y de los huevos.
 *
 * Plano (cm, centro del círculo en el origen del actor; +Y es la puerta, las 12 del reloj; las 3 quedan a -X):
 * - Muralla redonda de radio interior Radius con almenas, marcas de cubo y conchas, y torres de cubo de alturas
 *   distintas repartidas sin simetría. La puerta grande (las 12) está entre dos torres altas unidas por un arco con el
 *   cartel del juego; se abre cuando alguien se acerca y durante la cuenta atrás.
 * - Muro interior recto de las 3:40 a las 8:20, algo por debajo del centro (CutY). Separa la plaza del lobby (arriba)
 *   del patio de pruebas (abajo). En su centro, la torre del homenaje (las 6) con un paso por dentro, una escalera de
 *   caracol por fuera y un balcón con almenas que mira a la plaza.
 * - Plaza: pila de cuatro huevos en un montículo de dos alturas (EggsCenter); meterse en uno marca al jugador como
 *   listo (ATN_HQGameMode::SetPlayerReadyState) y con todos dentro empieza la cuenta atrás.
 * - Los puestos (tienda de las 10 a las 11, cuartel de la 1 a las 2, probadores de las 2 a las 3:30 y medusas de las
 *   8:20 a las 10) y las piezas del patio de pruebas son actores propios colocados en el nivel; LayoutSpot() da sus
 *   sitios.
 *
 * Consola: TN.Lobby.Castle 0 esconde el castillo en ejecución (hay que volver a cargar el lobby).
 */
UCLASS()
class TORTUNABO_API ATN_SandCastleLobby : public AActor
{
	GENERATED_BODY()

public:
	ATN_SandCastleLobby();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostRegisterAllComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** false con TN.Lobby.Castle 0. */
	static bool IsEnabled();

	/** Número de huevos de la pila (uno por jugador). */
	static constexpr int32 NumEggs = 4;

	/** Radio interior de la muralla (cm). */
	static constexpr double Radius = 2400.0;

	/** Cota (Y local) del muro interior que separa la plaza del patio de pruebas. */
	static constexpr double CutY = -800.0;

	/**
	 * Punto de la plaza a la hora ClockHour del reloj (12 = la puerta, +Y; 3 = -X) y a Dist cm del centro, en
	 * coordenadas locales del castillo; Yaw mira al centro del círculo.
	 */
	static FVector LayoutSpot(double ClockHour, double Dist, float& OutYawToCenter);

	/** Sitios de salida de los jugadores en la plaza (locales), entre la puerta y la pila de huevos, mirando a la pila. */
	static void GetSpawnSpots(TArray<FTransform>& OutLocalSpots);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle")
	TObjectPtr<USceneComponent> CastleRoot;

	/** Suelo, murallas, torres, torre del homenaje, escalera, balcón y montículo de los huevos (con colisión). */
	UPROPERTY(VisibleAnywhere, Category = "Castle")
	TObjectPtr<UProceduralMeshComponent> CastleMesh;

	/** Adornos sin colisión: conchas, estrellas, banderas, bases de los huevos, el mar y la playa de fuera. */
	UPROPERTY(VisibleAnywhere, Category = "Castle")
	TObjectPtr<UProceduralMeshComponent> DecorMesh;

	/** Barreras invisibles (borde de fuera de la muralla y del balcón): nadie se cae fuera del castillo. */
	UPROPERTY(VisibleAnywhere, Category = "Castle")
	TObjectPtr<UProceduralMeshComponent> BarrierMesh;

	/** Hojas de la puerta grande (bisagra en el origen de cada una). */
	UPROPERTY(VisibleAnywhere, Category = "Castle")
	TObjectPtr<UStaticMeshComponent> GateLeafLeft;

	UPROPERTY(VisibleAnywhere, Category = "Castle")
	TObjectPtr<UStaticMeshComponent> GateLeafRight;

	/** Bloqueo de la puerta grande (solo cerrada). */
	UPROPERTY(VisibleAnywhere, Category = "Castle")
	TObjectPtr<UBoxComponent> GateBlock;

	/** Tapas de los huevos. */
	UPROPERTY(VisibleAnywhere, Category = "Castle")
	TArray<TObjectPtr<UStaticMeshComponent>> EggLids;

	/** Rótulo del cartel de madera sobre el arco de la puerta. */
	UPROPERTY(VisibleAnywhere, Category = "Castle")
	TObjectPtr<UTextRenderComponent> GateSignText;

	/** Nombre del cartel de la puerta. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Castle")
	FText GateName = NSLOCTEXT("Tortunabo", "CastleGateName", "TORTUNAVY");

private:
	/** Puerta grande abierta (servidor: alguien cerca o cuenta atrás). */
	UPROPERTY(Replicated)
	bool bGateOpen = false;

	/** Huevos ocupados (bit por huevo). */
	UPROPERTY(Replicated)
	int32 EggMask = 0;

	/** Construye todas las mallas (editor y ejecución); no hace nada si ya están hechas y no se fuerza. */
	void BuildAll(bool bForce);
	void BuildCastle();
	void BuildGateAndEggs();

	/** Esconde las piezas de la maqueta que el castillo sustituye y apaga la zona de listos vieja (cada máquina). */
	void HideMaquette();

	/** Servidor: quién está en qué huevo (listos) y si la puerta grande tiene que abrirse. */
	void ServerUpdate(float DeltaSeconds);

	float GateOpenness = 0.f;
	float EggClose[NumEggs] = {};
	float Clock = 0.f;
	float ServerTimer = 0.f;
	float GateHoldTimer = 0.f;
	bool bGateBlocking = true;
	bool bBuilt = false;

	/** Servidor: el último estado de listo enviado por jugador. */
	TMap<TWeakObjectPtr<APlayerController>, bool> ReadySent;
};
