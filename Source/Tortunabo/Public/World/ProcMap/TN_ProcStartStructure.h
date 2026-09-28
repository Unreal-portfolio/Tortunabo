#pragma once

#include "CoreMinimal.h"
#include "Core/TN_MatchStartTypes.h"
#include "GameFramework/Actor.h"
#include "TN_ProcStartStructure.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Estructura de salida del mapa procedural: la misma pieza en la que los jugadores se pusieron listos en el lobby
 * (ATN_SandCastleLobby), hecha con el mismo kit (TNCastleKit), así que la geometría es idéntica.
 * - Puerta doble (ETNMatchStartStyle::Gate): sala entre dos puertas al fondo del claro de salida, con la puerta 1
 *   contra el talud (parece que se sale de la pared). Los jugadores aparecen dentro (cuatro sitios); al abrirse, la
 *   puerta 2 gira hacia fuera y se sale corriendo.
 * - Huevos (ETNMatchStartStyle::Eggs): montículo con la pila de cuatro huevos; cada jugador aparece dentro de uno con la
 *   tapa puesta y unas paredes invisibles que lo sujetan. Al abrirse, las tapas saltan dando vueltas, cada tortuga se ve
 *   1 s en su huevo roto (se pone de pie, se sacude la cáscara y mira al camino: TNEggHatch, la pieza común con la
 *   carrera) y sale despedida de un salto (servidor y cliente dueño a la vez y con el reloj del servidor, como en el
 *   probador). Consola: TN.Proc.Egg repite la salida sin regenerar.
 *
 * La crea ATN_ProcMapGenerator en el servidor (una por mapa; se destruye al regenerar) y se replica siempre: solo viajan
 * el estilo, si está abierta y desde cuándo, y cada máquina construye sus mallas. Espacio local: origen a ras de suelo,
 * +Y hacia el camino (la puerta 2 o el escalón del montículo); -Y, hacia el talud.
 */
UCLASS()
class TORTUNABO_API ATN_ProcStartStructure : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcStartStructure();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Sitios de salida (uno por jugador): los cuatro de la sala o los cuatro huevos. */
	static constexpr int32 NumSpots = 4;

	/**
	 * Semialtura de cápsula con la que se calculan los sitios cuando no se conoce la del peón (PlayerStart, reaparición):
	 * el centro queda a 110 cm del suelo, como en el anillo de salida del generador.
	 */
	static constexpr float DefaultSpawnHalfHeight = 108.f;

	/**
	 * Colocación (la hace el generador): distancia del centro del claro de salida al origen de la estructura, hacia el
	 * fondo del claro. La puerta doble deja la puerta 1 a 1,5 m del borde (contra el talud); el montículo, algo más dentro.
	 */
	static double GetBackDistance(ETNMatchStartStyle InStyle, double ClearingRadius);

	/**
	 * Colocación: puntos (XY locales) donde mirar la altura del terreno y si manda el más alto (puerta doble: el terreno
	 * nunca asoma por el suelo de la sala, el zócalo tapa por debajo) o el más bajo (montículo: nunca queda flotando).
	 */
	static void GetFootprintSamples(ETNMatchStartStyle InStyle, TArray<FVector2D>& OutLocalPoints, bool& bOutUseHighest);

	/** Servidor, antes de FinishSpawning: estilo de la estructura (si ya ha empezado, la rehace). */
	void SetStyle(ETNMatchStartStyle InStyle);

	ETNMatchStartStyle GetStyle() const { return Style; }

	bool IsOpen() const { return bOpen; }

	/**
	 * Sitio (mundo) del jugador Slot: dentro de la sala o de su huevo, con una cápsula de semialtura CapsuleHalfHeight de
	 * pie sobre el suelo y mirando al camino (+Y local). false si el slot no tiene sitio (más de cuatro jugadores): el
	 * llamador usa otra salida.
	 */
	bool GetSpawnTransform(int32 Slot, float CapsuleHalfHeight, FTransform& OutTransform) const;

	/** Servidor: abre la puerta 2 o rompe los huevos (y lanza fuera a quien esté dentro). */
	void Open();

	/** Servidor: la deja cerrada de golpe (ronda nueva sin regenerar el mapa). */
	void Close();

	/**
	 * Servidor (consola TN.Proc.Egg), solo con huevos: los cierra otra vez con cada tortuga dentro del suyo (por orden de
	 * jugador) y al poco los vuelve a romper. Para probar la salida sin regenerar el mapa.
	 */
	void ReplayEggs();

protected:
	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<USceneComponent> StructureRoot;

	/**
	 * Suelo, fachadas, paredes y torres de la puerta doble, o el montículo de los huevos (con colisión). Las tres mallas
	 * son RF_Transient y sus punteros, Transient (como en ATN_SandCastleLobby: guardados llegarían a nulo al cargar).
	 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "StartStructure")
	TObjectPtr<UProceduralMeshComponent> SolidMesh;

	/** Adornos sin colisión: arcos, carteles, conchas, estrellas, antorchas, banderas y las bases de los huevos. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "StartStructure")
	TObjectPtr<UProceduralMeshComponent> DecorMesh;

	/**
	 * Barreras invisibles: sobre las paredes y fachadas de la sala (nadie sale saltando) o, en los huevos, las paredes
	 * que sujetan a cada tortuga dentro del suyo hasta que se rompe.
	 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "StartStructure")
	TObjectPtr<UProceduralMeshComponent> BarrierMesh;

	/** Hojas de la puerta 1 (siempre cerrada; bisagra en el origen de cada una). */
	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<UStaticMeshComponent> Gate1LeafLeft;

	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<UStaticMeshComponent> Gate1LeafRight;

	/** Hojas de la puerta 2 (se abren hacia fuera, +Y). */
	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<UStaticMeshComponent> Gate2LeafLeft;

	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<UStaticMeshComponent> Gate2LeafRight;

	/** Bloqueo de la puerta 1 (siempre). */
	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<UBoxComponent> Gate1Block;

	/** Bloqueo de la puerta 2 (hasta que empieza a abrirse). */
	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<UBoxComponent> Gate2Block;

	/** Tapas de los huevos. */
	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TArray<TObjectPtr<UStaticMeshComponent>> EggLids;

	/** Rótulo del cartel de la puerta 2, por la cara de fuera (la del camino). */
	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<UTextRenderComponent> GateSignText;

	/** Luz cálida de las antorchas de la sala. */
	UPROPERTY(VisibleAnywhere, Category = "StartStructure")
	TObjectPtr<UPointLightComponent> RoomLight;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Style)
	ETNMatchStartStyle Style = ETNMatchStartStyle::Gate;

	/** La puerta 2 se abre o los huevos se rompen. */
	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	/**
	 * Hora del servidor (GetServerWorldTimeSeconds) a la que empezó a abrirse: con ella van la pausa en el huevo y el
	 * lanzamiento de cada tortuga, igual en todas las máquinas.
	 */
	UPROPERTY(Replicated)
	float OpenServerTime = 0.f;

	UFUNCTION()
	void OnRep_Style();

	UFUNCTION()
	void OnRep_Open();

	/** Construye todas las mallas del estilo actual (cada máquina). */
	void Build();

	/** Todo cerrado y quieto: hojas cerradas, bloqueos y paredes de los huevos puestos, tapas sobre sus bases. */
	void ApplyClosedPose();

	/**
	 * Empieza a abrirse. bLive = false (se ha unido con la estructura ya abierta): directamente al final, sin lanzar a
	 * nadie.
	 */
	void StartOpening(bool bLive);

	/** Pose de la apertura en este instante; false cuando ya ha terminado. */
	bool UpdateOpening();

	/**
	 * Se rompe el huevo Index: sueltan las paredes invisibles y quien esté dentro pasa 1 s en el huevo roto y sale
	 * despedido (TNEggHatch).
	 */
	void HatchEgg(int32 Index);

	/** La apertura llega a esta máquina a tiempo de vivirla (con la pausa en el huevo y el salto). */
	bool IsOpeningFresh() const;

	/** Coloca la tapa Index a T segundos de romperse su huevo; false cuando ya ha desaparecido. */
	bool PoseLid(int32 Index, double T);

	/** Bloqueo de la puerta 2 (solo en la puerta doble). */
	void SetGate2Blocking(bool bBlock);

	/** Instante local (s del mundo) en que empezó a abrirse. */
	double OpenStartTime = 0.0;

	/** La apertura lanza a las tortugas de los huevos (false al unirse con la estructura ya abierta). */
	bool bLaunchOnHatch = false;

	/** Huevos ya rotos en esta máquina (bit por huevo). */
	int32 HatchedMask = 0;

	/** TN.Proc.Egg: rotura pendiente de los huevos que ha vuelto a cerrar (servidor). */
	FTimerHandle ReplayHandle;
};
