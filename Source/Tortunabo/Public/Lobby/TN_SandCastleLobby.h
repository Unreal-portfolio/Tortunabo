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
 * El lobby como castillo de arena (LVL_Lobby / LVL_HQ). Lo coloca ATN_HQGameMode en el origen del lobby (a ras del
 * suelo) y esconde las piezas de la maqueta que sustituye: las paredes «Extrude», las vallas y torres de la zona de
 * salida, sus puertas y los huevos del centro. Todo se construye en código en cada máquina (malla procedural con
 * colisión) y solo se replican el estado de la puerta y de los huevos.
 *
 * Coordenadas del lobby (cm; el jugador aparece en el origen mirando a +Y; su izquierda es +X):
 * - Patio: suelo de arena y murallas con almenas (x ±2400, y de -2400 a 2150) con torres de cubo en las esquinas,
 *   marcas de cubo y conchas incrustadas. Barreras invisibles por fuera de los adarves.
 * - Puerta enorme (y = 2150, 10 m de ancho) entre el patio y la sala de espera: se abre al acercarse alguien y
 *   durante la cuenta atrás.
 * - Sala de espera (x ±1600, y de 2150 a 3150): ocho huevos. Cada jugador se mete en uno (la tapa baja y lo tapa) y
 *   queda listo (ATN_HQGameMode::SetPlayerReadyState); sustituye a la zona de listos de la maqueta, que se apaga.
 *   Al fondo, la puerta del mar, que se abre al empezar la cuenta atrás.
 * - Mini parkour: escalera de arena hasta el adarve sur, circuito de saltos y panzazo con una rampa de concha, pilares
 *   de cubo, pasarela de palos de polo y torreón hasta el adarve este.
 * La tienda (+X), el general y los probadores (-X), los selectores de modo y la salida siguen donde estaban.
 *
 * Consola: TN.Lobby.Castle 0 lo desactiva (hay que volver a cargar el lobby).
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API ATN_SandCastleLobby : public AActor
{
	GENERATED_BODY()

public:
	ATN_SandCastleLobby();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** false con TN.Lobby.Castle 0. */
	static bool IsEnabled();

	/** Número de huevos de la sala de espera. */
	static constexpr int32 NumEggs = 8;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle")
	TObjectPtr<USceneComponent> CastleRoot;

	/** Suelo, murallas, torres y parkour (con colisión). */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CastleMesh;

	/** Adornos sin colisión: conchas, estrellas, banderas, nidos y bases de los huevos, el mar. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> DecorMesh;

	/** Hojas de la puerta grande y de la del mar (bisagra en el origen de cada componente). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GateLeaves;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SeaLeaves;

	/** Tapas de los huevos. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> EggLids;

	/** Bloqueo de la puerta grande (solo cerrada) y de la del mar (siempre: fuera no hay suelo). */
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> GateBlock;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> Barriers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Signs;

private:
	/** Puerta grande abierta (servidor: alguien cerca o cuenta atrás). */
	UPROPERTY(Replicated)
	bool bGateOpen = false;

	/** Huevos ocupados (bit por huevo). */
	UPROPERTY(Replicated)
	int32 EggMask = 0;

	void BuildCastle();
	void BuildGatesAndEggs();
	void AddBarrier(const FVector& Center, const FVector& Extent, float Yaw);
	void AddSign(const FString& Text, const FVector& Location, float Yaw, float WorldSize, const FColor& Color);

	/** Esconde las piezas de la maqueta que el castillo sustituye y apaga la zona de listos vieja (cada máquina). */
	void HideMaquette();

	/** Servidor: quién está en qué huevo (listos) y si la puerta grande tiene que abrirse. */
	void ServerUpdate(float DeltaSeconds);

	float GateOpenness = 0.f;
	float SeaOpenness = 0.f;
	float EggClose[NumEggs] = {};
	float Clock = 0.f;
	float ServerTimer = 0.f;
	float GateHoldTimer = 0.f;
	bool bGateBlocking = true;

	/** Servidor: el último estado de listo enviado por jugador. */
	TMap<TWeakObjectPtr<APlayerController>, bool> ReadySent;
};
