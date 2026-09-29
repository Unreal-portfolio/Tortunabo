#pragma once

#include "CoreMinimal.h"

/**
 * @brief Nombres de sala para las partidas públicas y privadas.
 *
 * Una lista fija de nombres graciosos de la playa, las tortugas y su ecosistema (arena, mareas, cangrejos, gaviotas,
 * huevos, caparazones, bañistas...). Cada nombre es un texto del canal de localización del motor (Docs/Localizacion.md):
 * el origen está en español de España (espacio «TNRoomNames», clave «Room_000»...) y cada idioma lo adapta con la misma
 * gracia, no lo traduce literal (un juego de palabras se cambia por otro que funcione en el idioma de destino). Las
 * adaptaciones inglesas de siempre están en Tools/Localization/room_names_en.csv.
 *
 * La sesión anuncia solo el índice (ajuste de sesión ROOMNAME_ID); cada jugador lo lee en el idioma que ha elegido en el
 * juego (ajuste «Idioma» del menú de pausa; no la cultura de Windows ni la del editor), así que la misma sala se llama
 * «Caparazones al Horno» para uno y con su versión inglesa para otro. El orden de la lista no cambia nunca entre
 * versiones: los índices viajan por la red y se guardan (quitar un nombre rompe los de después; los nuevos, siempre al final).
 */
namespace TNRoomNames
{
	/** Cuántos nombres hay: índices válidos de 0 a Num() - 1. */
	TORTUNABO_API int32 Num();

	/** true si el idioma elegido en el juego es el español (el de los textos origen). */
	TORTUNABO_API bool IsSpanish();

	/** El nombre del índice en el idioma del juego. Fuera de rango: un nombre de reserva («Sala sin nombre»). */
	TORTUNABO_API FText Get(int32 Id);

	/** El nombre del índice en su idioma de origen, el español (para los registros y la localización), con la misma reserva. */
	TORTUNABO_API FString GetSource(int32 Id);

	/** La clave estable del índice en la localización: «Room_000»...; el espacio de nombres es «TNRoomNames». */
	TORTUNABO_API FString GetKey(int32 Id);

	/** GetSource (bSpanish = true) o el nombre en el idioma del juego (false, como Get). Se conserva por los usos que ya había. */
	TORTUNABO_API FString GetIn(int32 Id, bool bSpanish);

	/** Un índice al azar, distinto de Avoid si hay más de uno (para el botón de «otro nombre»). */
	TORTUNABO_API int32 Random(int32 Avoid = INDEX_NONE);
}
