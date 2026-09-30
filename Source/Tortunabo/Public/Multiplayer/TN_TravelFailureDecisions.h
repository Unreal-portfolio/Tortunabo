#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"

/**
 * Qué hacer cuando un viaje de mapa falla (UEngine::OnTravelFailure: mapa sin cocinar, paquete que falta, URL mala). Sin
 * esto, un ServerTravel fallido dejaba al invitado colgado en la pantalla de carga y sin mensaje (F_gaps_steam N-E).
 * Lógica pura, sin mundo: la usa UTN_TravelFailureSubsystem y la prueban los tests Tortunabo.Net.TravelFailure.
 */
namespace TNTravel
{
	enum class ETravelFailureAction : uint8
	{
		/** El anfitrión vuelve al lobby con todos (ServerTravel). */
		ReturnHostToLobby,
		/** Sesión cerrada y al menú con el aviso. */
		ReturnToMenu,
		/** Ya estaba en el menú: solo el aviso. */
		StayInMenu
	};

	/** Lobby por defecto si no se sabe de cuál salió la partida (UMP_GameInstance::GameMapPath). */
	inline constexpr const TCHAR* DefaultLobbyPath = TEXT("/Game/Maps/Lobby/LVL_Lobby");

	/**
	 * @param NetMode        Modo de red del mundo que ha fallado.
	 * @param bInMenu        El mundo es el del menú principal.
	 * @param PriorFailures  Fallos seguidos desde el último mapa cargado: el segundo ya no reintenta el lobby (sin bucle).
	 */
	inline ETravelFailureAction DecideTravelFailure(ENetMode NetMode, bool bInMenu, int32 PriorFailures)
	{
		if (bInMenu)
		{
			return ETravelFailureAction::StayInMenu;
		}
		const bool bHost = NetMode == NM_ListenServer || NetMode == NM_DedicatedServer;
		return bHost && PriorFailures == 0 ? ETravelFailureAction::ReturnHostToLobby : ETravelFailureAction::ReturnToMenu;
	}

	/** El lobby del que salió la partida (UMP_GameInstance::LobbyReturnMapPath) o, si no se sabe, el de por defecto. */
	inline FString LobbyTravelURL(const FString& LobbyReturnMapPath)
	{
		return LobbyReturnMapPath.IsEmpty() ? FString(DefaultLobbyPath) : LobbyReturnMapPath;
	}

	/** Para el registro. */
	inline const TCHAR* ActionName(ETravelFailureAction Action)
	{
		switch (Action)
		{
		case ETravelFailureAction::ReturnHostToLobby: return TEXT("anfitrión al lobby");
		case ETravelFailureAction::ReturnToMenu:      return TEXT("al menú");
		default:                                      return TEXT("se queda en el menú");
		}
	}
}
