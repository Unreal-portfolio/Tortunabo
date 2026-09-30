#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class AActor;
class AController;
class AGameModeBase;
class AGameStateBase;
class APawn;
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
 * @brief Como TN_PickUnoccupiedPlayerStart, pero si todos los PlayerStart están ocupados (partidas de más jugadores que
 *        PlayerStart tiene el mapa: hasta 8) crea uno nuevo junto a los del mapa en vez de apilar al jugador encima.
 * @details Los sitios nuevos salen de desplazar un PlayerStart existente a los lados o hacia atrás, en pasos de 2,2 m
 *          (o de ~1,1 m si no cabe así), en el orden en que menos se alejan. Cada uno se comprueba: hay suelo firme
 *          y casi a la misma cota que el de origen, cabe la cápsula del peón, un barrido desde el origen no choca con
 *          nada y no queda encima de otro peón. Entre los sitios posibles de la misma distancia se elige el más
 *          despejado. Los PlayerStart del mapa se usan igual que antes: solo se crea uno nuevo cuando no queda ninguno
 *          libre. Los nuevos son actores de servidor, copian la etiqueta y la orientación del PlayerStart de origen, se
 *          añaden a PlayerStarts y se reutilizan en llamadas siguientes (el jugador que se va deja libre el suyo).
 * @param World Mundo donde iterar los peones actuales y hacer las comprobaciones.
 * @param PlayerStarts Pool de candidatos; se baraja in-place y, si se crea un sitio nuevo, se le añade.
 * @param Player Controller que va a poseer el spawn: su peón, si tiene, no cuenta como ocupante.
 * @param PawnClass Clase del peón que va a aparecer (su cápsula manda en las comprobaciones); nula = una tortuga normal.
 * @param LogTag Prefijo de log ("Run" / "Lobby").
 * @return Un PlayerStart libre (del pool o nuevo), o nullptr si no hay sitio ni se pudo crear uno: el llamador usa su respaldo.
 */
AActor* TN_PickSpreadPlayerStart(UWorld* World, TArray<AActor*>& PlayerStarts, AController* Player, TSubclassOf<APawn> PawnClass, const TCHAR* LogTag);

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

/** Largo máximo de un nombre de jugador: el de Steam (32). */
constexpr int32 TN_MaxPlayerNameLength = 32;

/**
 * @brief Devuelve al jugador su nombre completo tras AGameModeBase::InitNewPlayer, que corta la opción ?Name= a 20
 *        caracteres (un nombre de Steam de hasta 32 llegaba recortado al HUD, al tendero y a los resultados).
 * @param GameMode GameMode que acaba de inicializar al jugador.
 * @param PlayerController Jugador recién entrado.
 * @param Options Opciones del login (las mismas que recibió InitNewPlayer).
 * @note Se llama desde InitNewPlayer de ATN_HQGameMode y ATN_RunGameMode, justo después de Super.
 */
void TN_RestoreFullPlayerName(AGameModeBase* GameMode, APlayerController* PlayerController, const FString& Options);
