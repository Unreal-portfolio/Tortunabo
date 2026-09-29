#pragma once

#include "CoreMinimal.h"

/**
 * @brief Nombres de sala para las partidas públicas y privadas.
 *
 * Una lista fija de nombres graciosos de la playa, las tortugas y su ecosistema (arena, mareas, cangrejos, gaviotas,
 * huevos, caparazones, bañistas...). Cada nombre existe en español y en inglés, y el inglés es una adaptación con la
 * misma gracia, no una traducción literal (un juego de palabras se cambia por otro que funcione en inglés).
 *
 * La sesión anuncia solo el índice (ajuste de sesión ROOMNAME_ID); cada jugador lo lee en el idioma de su juego, así
 * que la misma sala se llama «Caparazones al Horno» para uno y con su versión inglesa para otro. El orden de la lista
 * no cambia nunca entre versiones: los índices viajan por la red y se guardan (quitar un nombre rompe los de después;
 * los nuevos, siempre al final).
 */
namespace TNRoomNames
{
	/** Cuántos nombres hay: índices válidos de 0 a Num() - 1. */
	TORTUNABO_API int32 Num();

	/** true si el juego está en español (la cultura actual empieza por «es»); si no, se usa el inglés. */
	TORTUNABO_API bool IsSpanish();

	/** El nombre del índice en el idioma del juego. Fuera de rango: un nombre de reserva («Sala sin nombre» / «Nameless Nest»). */
	TORTUNABO_API FText Get(int32 Id);

	/** El nombre del índice en un idioma concreto (true: español; false: inglés), con la misma reserva. */
	TORTUNABO_API FString GetIn(int32 Id, bool bSpanish);

	/** Un índice al azar, distinto de Avoid si hay más de uno (para el botón de «otro nombre»). */
	TORTUNABO_API int32 Random(int32 Avoid = INDEX_NONE);
}
