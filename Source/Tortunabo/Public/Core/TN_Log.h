#pragma once

#include "CoreMinimal.h"

/**
 * Categoría de log del juego. Sustituye a LogTemp en todo el módulo para poder
 * filtrar el log propio del ruido del engine en sesiones multijugador:
 *   log LogTortunabo Verbose            (consola in-game)
 *   -LogCmds="LogTortunabo Verbose"     (línea de comandos)
 */
DECLARE_LOG_CATEGORY_EXTERN(LogTortunabo, Log, All);
