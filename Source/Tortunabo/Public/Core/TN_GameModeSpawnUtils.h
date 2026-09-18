#pragma once

#include "CoreMinimal.h"

class AActor;
class AController;
class AGameModeBase;
class AGameStateBase;
class APlayerController;
class APlayerStart;
class UWorld;

/**
 * @brief Baraja PlayerStarts (Fisher-Yates) y devuelve el primero sin ningún pawn ajeno a menos de 200cm.
 * @param World Mundo donde iterar los pawns actuales.
 * @param PlayerStarts Pool de candidatos; se baraja in-place (el llamador puede usar PlayerStarts[0] como fallback tras la llamada).
 * @param Player Controller que va a poseer el spawn — se excluye de la comprobación de ocupación.
 * @return El primer PlayerStart libre encontrado, o nullptr si todos están ocupados.
 * @note Compartido por ATN_RunGameMode y ATN_HQGameMode::ChoosePlayerStart_Implementation.
 *       El manejo del pool vacío y del fallback "todos ocupados" queda en cada llamador
 *       (difieren en logging entre Run y HQ).
 */
AActor* TN_PickUnoccupiedPlayerStart(UWorld* World, TArray<AActor*>& PlayerStarts, AController* Player);

/**
 * @brief Garantiza que el jugador tenga un pawn vivo: RestartPlayer, y si sigue sin pawn,
 *        busca PlayerStart (o usa FallbackProvider) y spawnea+posee un pawn por defecto.
 * @param GameMode GameMode que orquesta el spawn (RestartPlayer/FindPlayerStart/SpawnDefaultPawnFor/SetPlayerDefaults son públicos en AGameModeBase).
 * @param PlayerController Jugador a garantizar con pawn.
 * @param FallbackProvider Invocado solo si FindPlayerStart no encuentra nada; produce el PlayerStart de emergencia del GameMode.
 * @param LogTag Prefijo de log ("Run" / "Lobby").
 * @note Compartido por ATN_RunGameMode::EnsurePlayerSpawned y ATN_HQGameMode::EnsurePlayerSpawned.
 */
void TN_EnsurePlayerSpawned(AGameModeBase* GameMode, APlayerController* PlayerController, TFunctionRef<APlayerStart* ()> FallbackProvider, const TCHAR* LogTag);

/**
 * @brief Devuelve el primer APlayerStart existente en el mundo, o spawnea uno de emergencia en el origen si no hay ninguno.
 * @param World Mundo donde buscar/spawnear.
 * @param SpawnActorName Nombre único del actor de emergencia (evita colisión de nombres entre mapas).
 * @param LogTag Prefijo de log ("Run" / "Lobby").
 * @param MapDescriptor Texto insertado en el warning ("run map" / "lobby map").
 * @note Compartido por ATN_RunGameMode::EnsureFallbackPlayerStart y ATN_HQGameMode::EnsureFallbackPlayerStart.
 */
APlayerStart* TN_EnsureFallbackPlayerStart(UWorld* World, FName SpawnActorName, const TCHAR* LogTag, const TCHAR* MapDescriptor);

/**
 * @brief Cuenta cuántos elementos del PlayerArray son ATN_CoopPlayerState (jugadores coop conectados).
 * @param GameState GameState del que iterar PlayerArray. Se asume no-nulo (mismo contrato que los
 *        call sites originales, que ya lo desreferenciaban sin guard).
 * @note Compartido por ATN_RunGameMode y ATN_HQGameMode: 7 sitios reimplementaban el mismo bucle
 *       de conteo (solo cambiaba el nombre de la variable acumuladora).
 */
int32 TN_CountConnectedCoopPlayers(const AGameStateBase* GameState);
