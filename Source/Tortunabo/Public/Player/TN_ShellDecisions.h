#pragma once

#include "CoreMinimal.h"

/**
 * Lógica pura del estado de caparazón: decide SI se puede entrar o salir, nunca
 * lo aplica. Sin UWorld, sin UObject, sin estado — todo entra por parámetro.
 *
 * UTN_ShellComponent delega en estas funciones para que los tests de
 * Tortunabo.Shell cubran el código real y no una copia paralela de las reglas.
 */
namespace TNShellLogic
{
	/** Entradas de estado del personaje que condicionan meterse en el caparazón. */
	struct FShellEnterContext
	{
		/**
		 * Nadando no se entra: el caparazón con física se hundiría y saldría en el acto. En el aire sí se entra (la
		 * bolita en pleno salto, que sigue volando como cuerpo físico con la velocidad que llevaba).
		 */
		bool bIsSwimming = false;

		bool bIsDead = false;
		bool bIsKnockedDown = false;
		bool bIsDiving = false;

		/** Con un objeto en la mano no se entra: las manos están ocupadas. */
		bool bHasEquippedItem = false;
	};

	/**
	 * @brief Decide si el personaje puede meterse en el caparazón.
	 * @note Se evalúa en el servidor; el cliente solo pide el cambio.
	 */
	inline bool CanEnterShell(const FShellEnterContext& Context)
	{
		if (Context.bIsDead || Context.bIsKnockedDown || Context.bIsDiving)
		{
			return false;
		}

		if (Context.bIsSwimming || Context.bHasEquippedItem)
		{
			return false;
		}

		return true;
	}

	/**
	 * @brief Decide si el personaje puede salir del caparazón.
	 * @param TimeInShellSeconds Segundos transcurridos desde que entró.
	 * @param MinTimeInShellSeconds Permanencia mínima antes de poder salir.
	 * @note El mínimo evita que machacar la tecla genere un tren de RPCs y
	 *       parpadeo visual en el resto de máquinas.
	 */
	inline bool CanExitShell(float TimeInShellSeconds, float MinTimeInShellSeconds)
	{
		return TimeInShellSeconds >= MinTimeInShellSeconds;
	}
}
