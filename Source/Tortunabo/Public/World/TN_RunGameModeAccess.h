#pragma once

class UWorld;
class ATN_RunGameMode;

/**
 * Resuelve el ATN_RunGameMode activo del nivel a partir del World, o nullptr
 * si el World no existe o no hay autoridad de GameMode (cliente). Compartido
 * por los volúmenes de World/ (DeathZone, Storm, FinishLine) que necesitan
 * notificar al GameMode de carrera.
 */
ATN_RunGameMode* TN_ResolveRunGameMode(UWorld* World);
