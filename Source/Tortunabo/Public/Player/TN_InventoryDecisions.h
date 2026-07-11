#pragma once

#include "CoreMinimal.h"

/**
 * Decisiones del inventario de 2 slots como funciones PURAS (Fase 4.3): sin UWorld,
 * sin componentes, sin estado — entradas escalares, salida determinista.
 * UTN_InventoryComponent delega aquí para que la lógica testeada por el Automation
 * framework sea EXACTAMENTE la que corre en juego, no una copia.
 *
 * Regla: aquí solo se decide QUÉ hacer con la ocupación de los slots; la mutación
 * (asignar ítems, RefreshEquippedVisual, SFX, replicación) queda en el componente.
 * Si una decisión necesita algo del componente (flags, matches de UseType), este
 * lo calcula y lo pasa como parámetro.
 */

namespace TNInventoryLogic
{
	/** Destino de un ítem entrante (orden = precedencia: equipado > guardado). */
	enum class EAddDecision : uint8
	{
		ToEquipped,      // Slot equipado libre → va a la mano
		ToStored,        // Equipado ocupado, guardado libre → va al caparazón
		ReplaceEquipped, // Ambos llenos y el reemplazo está permitido
		Rejected         // Ambos llenos sin reemplazo
	};

	/** Slot que recibe un ítem nuevo. Nunca decide ReplaceEquipped (eso es DecideAddOrReplace). */
	inline EAddDecision DecideAddSlot(bool bHasEquipped, bool bHasStored)
	{
		if (!bHasEquipped) { return EAddDecision::ToEquipped; }
		if (!bHasStored)   { return EAddDecision::ToStored; }
		return EAddDecision::Rejected;
	}

	/**
	 * Variante con reemplazo: con hueco libre NUNCA reemplaza (usa el hueco);
	 * solo con ambos slots llenos entra bReplaceIfFull en juego.
	 */
	inline EAddDecision DecideAddOrReplace(bool bHasEquipped, bool bHasStored, bool bReplaceIfFull)
	{
		const EAddDecision Base = DecideAddSlot(bHasEquipped, bHasStored);
		if (Base != EAddDecision::Rejected)
		{
			return Base;
		}
		return bReplaceIfFull ? EAddDecision::ReplaceEquipped : EAddDecision::Rejected;
	}

	/** true si el inventario puede recibir un ítem (hay hueco o el reemplazo está permitido). */
	inline bool CanReceiveItem(bool bHasEquipped, bool bHasStored, bool bAllowReplaceIfFull)
	{
		return !bHasEquipped || !bHasStored || bAllowReplaceIfFull;
	}

	/** Resultado de consumir el ítem equipado. */
	enum class EConsumeDecision : uint8
	{
		Rejected,                // No hay equipado que consumir
		ConsumeAndPromoteStored, // El guardado sube a la mano
		ConsumeToEmpty           // No había guardado → la mano queda vacía
	};

	/** Decisión al consumir el equipado: el guardado promociona si existe. */
	inline EConsumeDecision DecideConsumeEquipped(bool bHasEquipped, bool bHasStored)
	{
		if (!bHasEquipped)
		{
			return EConsumeDecision::Rejected;
		}
		return bHasStored ? EConsumeDecision::ConsumeAndPromoteStored : EConsumeDecision::ConsumeToEmpty;
	}

	/** true si rotar slots tiene efecto (con ambos vacíos es un no-op). Rotar con 1 solo ítem es legal. */
	inline bool ShouldSwapSlots(bool bHasEquipped, bool bHasStored)
	{
		return bHasEquipped || bHasStored;
	}

	/** Slot del que se consume un ítem buscado por UseType (precedencia: equipado primero). */
	enum class EUseTypeSource : uint8
	{
		None,     // Ningún slot tiene un ítem de ese tipo
		Equipped, // El equipado matchea (gana aunque el guardado también matchee)
		Stored    // Solo el guardado matchea
	};

	/** El componente calcula los matches (flag + UseType) y los pasa como bools. */
	inline EUseTypeSource DecideConsumeByUseType(bool bEquippedMatches, bool bStoredMatches)
	{
		if (bEquippedMatches) { return EUseTypeSource::Equipped; }
		if (bStoredMatches)   { return EUseTypeSource::Stored; }
		return EUseTypeSource::None;
	}
}
