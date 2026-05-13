#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TN_CoopPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaceScoreChanged, int32, NewScore);

/**
 * @brief PlayerState replicado por jugador — contiene estado individual de partida y cosméticos equipados.
 *
 * Datos replicados:
 *  - Flujo: bIsInReadyZone, bHasFinishedRun, bIsAlive, bIsEliminated, bIsDBNO.
 *  - Métricas: FinishRank, FinishTimeSeconds, RaceScore (con delegate OnRaceScoreChanged).
 *  - Tiempos visibles en HUD: DBNOBleedoutTimeRemaining, DeathZoneTimeRemaining.
 *  - Cosméticos equipados: EquippedHelmetId, EquippedSkinId (ambos con OnRep).
 *
 * Rate-limit server-side: timestamps para QuickChat y emotes (anti-spam por jugador).
 */
UCLASS()
class TORTUNABO_API ATN_CoopPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ATN_CoopPlayerState();

	/** Disparado en clientes cuando RaceScore cambia. HUD se suscribe en NativeConstruct. */
	UPROPERTY(BlueprintAssignable, Category = "Coop|Score")
	FOnRaceScoreChanged OnRaceScoreChanged;

	/**
	 * @brief Indica si el servidor puede aceptar otro QuickChat de este jugador respetando el cooldown.
	 * @param Now Tiempo actual del servidor (s).
	 * @param CooldownSeconds Cooldown configurado entre mensajes (s).
	 */
	bool CanServerSendQuickChat(float Now, float CooldownSeconds) const;

	/** @brief Marca el tiempo del último QuickChat aceptado (server-side). */
	void MarkServerQuickChatSent(float Now);

	/**
	 * @brief Indica si el servidor puede aceptar otro emote del tipo dado respetando su cooldown.
	 * @param EmoteID ID del emote consultado.
	 * @param Now Tiempo actual del servidor (s).
	 * @param CooldownSeconds Cooldown configurado para emotes (s).
	 */
	bool CanServerPlayEmote(uint8 EmoteID, float Now, float CooldownSeconds) const;

	/** @brief Marca el tiempo del último emote aceptado de este tipo (server-side). */
	void MarkServerEmotePlayed(uint8 EmoteID, float Now);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bIsInReadyZone = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bHasFinishedRun = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bIsAlive = true;

	/**
	 * Down But Not Out: true cuando el jugador está incapacitado pero aún no muerto.
	 * Los compañeros pueden revivirle vía emote en rango. Si expira el bleedout, muere.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop|DBNO")
	bool bIsDBNO = false;

	/**
	 * Segundos restantes antes de que el DBNO bleed-out termine y el jugador muera definitivamente.
	 * -1 = no está en DBNO. Replicado sólo al owner (para la barra de bleedout del HUD).
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop|DBNO")
	float DBNOBleedoutTimeRemaining = -1.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	float DeathZoneTimeRemaining = -1.f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedHelmetId, Category = "Cosmetics")
	FName EquippedHelmetId = NAME_None;

	/** @brief OnRep de EquippedHelmetId: reaplica el mesh del casco en el pawn local. */
	UFUNCTION()
	void OnRep_EquippedHelmetId();

	/** @brief Fuerza la aplicación del casco a todos los clientes (evita la race condition del dirty-trick). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastForceApplyHelmet(FName HelmId);

	/** ID del skin de personaje activo. NAME_None = aspecto por defecto del BP. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedSkinId, Category = "Cosmetics")
	FName EquippedSkinId = NAME_None;

	/** @brief OnRep de EquippedSkinId: reaplica los materiales del skin en el pawn local. */
	UFUNCTION()
	void OnRep_EquippedSkinId();

	/** @brief Fuerza la aplicación del skin en todos los clientes (cubre race condition post-travel). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastForceApplySkin(FName SkinId);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	float FinishTimeSeconds = -1.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 FinishRank = 0;

	/**
	 * True si el jugador fue eliminado (muerto) en lugar de terminar normalmente.
	 * La UI muestra "ELIMINADO" cuando esto es true, usando FinishRank para ordenar
	 * entre jugadores eliminados (asignado por orden de muerte en el servidor).
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bIsEliminated = false;

	/**
	 * Puntos ganados en esta carrera (#26).
	 * Calculados por ATN_RunGameMode al cruzar la meta según posición de llegada
	 * + sumas de ScorePickups durante la run.
	 * Replicado para que el HUD lo muestre en tiempo real (delegate OnRaceScoreChanged).
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RaceScore, Category = "Coop|Score")
	int32 RaceScore = 0;

	/** @brief OnRep de RaceScore: dispara OnRaceScoreChanged para refrescar el HUD. */
	UFUNCTION()
	void OnRep_RaceScore();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	float ServerLastQuickChatTime = -10000.f;
	TMap<uint8, float> ServerLastEmoteTimes;
};
