#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Misc/PackageName.h"

/**
 * Qué hacer cuando un viaje de mapa falla (UEngine::OnTravelFailure: mapa sin cocinar, paquete que falta, URL mala). Sin
 * esto, un ServerTravel fallido dejaba al invitado colgado en la pantalla de carga y sin mensaje (F_gaps_steam N-E).
 * Lógica pura, sin mundo: la usa UTN_TravelFailureSubsystem y la prueban los tests Tortunabo.Net.TravelFailure.
 */
namespace TNTravel
{
	enum class ETravelFailureAction : uint8
	{
		/** El anfitrión recarga el lobby con todos (ServerTravel, un tick después del fallo). */
		ReturnHostToLobby,
		/** El anfitrión ya está en un lobby en pie: no hay a dónde volver ni nada que recargar. */
		StayInLobby,
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
	 * @param bLobbyIntact   El mundo es el lobby y sigue en pie (no estaba lanzando la partida). Un lobby que ya había
	 *                       empezado la cuenta atrás no lo está: el servidor destruye las tortugas antes de viajar, así que
	 *                       se recarga (ServerTravel al mismo lobby), como al volver de una partida.
	 * @param PriorFailures  Fallos seguidos desde el último mapa cargado: el segundo ya no reintenta el lobby (sin bucle).
	 */
	inline ETravelFailureAction DecideTravelFailure(ENetMode NetMode, bool bInMenu, bool bLobbyIntact, int32 PriorFailures)
	{
		if (bInMenu)
		{
			return ETravelFailureAction::StayInMenu;
		}
		const bool bHost = NetMode == NM_ListenServer || NetMode == NM_DedicatedServer;
		if (bHost && bLobbyIntact)
		{
			return ETravelFailureAction::StayInLobby;
		}
		return bHost && PriorFailures == 0 ? ETravelFailureAction::ReturnHostToLobby : ETravelFailureAction::ReturnToMenu;
	}

	/** El lobby del que salió la partida (UMP_GameInstance::LobbyReturnMapPath) o, si no se sabe, el de por defecto. */
	inline FString LobbyTravelURL(const FString& LobbyReturnMapPath)
	{
		return LobbyReturnMapPath.IsEmpty() ? FString(DefaultLobbyPath) : LobbyReturnMapPath;
	}

	/**
	 * true si MapName (nombre corto del mundo, sin el prefijo de PIE) es el del lobby al que se vuelve (LobbyTravelURL).
	 * Las opciones de la URL (?...) no cuentan.
	 */
	inline bool IsLobbyMap(const FString& MapName, const FString& LobbyReturnMapPath)
	{
		FString LobbyMap = LobbyTravelURL(LobbyReturnMapPath);
		int32 OptionsAt = INDEX_NONE;
		if (LobbyMap.FindChar(TEXT('?'), OptionsAt))
		{
			LobbyMap.LeftInline(OptionsAt);
		}
		return !MapName.IsEmpty() && MapName == FPackageName::GetShortName(LobbyMap);
	}

	/**
	 * ¿Ha arrancado el ServerTravel que se acaba de pedir? UWorld::ServerTravel devuelve true aunque no haga nada (si ya hay un
	 * NextURL o un viaje sin cortes en marcha lo ignora), así que se mira el efecto: un viaje sin cortes en marcha o un NextURL
	 * puesto (el viaje duro sale cuando acaba su cuenta atrás). Hay que mirar antes de pedirlo que no hubiera ya uno.
	 */
	inline bool DidTravelStart(bool bAccepted, bool bInSeamlessTravel, bool bHasNextURL)
	{
		return bAccepted && (bInSeamlessTravel || bHasNextURL);
	}

	/**
	 * El paquete del mapa al que va un ServerTravel(URL, bAbsolute) pedido desde un mundo cuya última URL es LastURL, como lo
	 * calcula AGameModeBase::ProcessServerTravel (una URL relativa sin mapa, p. ej. «?Restart», se queda en el mapa de LastURL).
	 * Vacío si la URL no es válida (de eso ya se encarga el motor).
	 */
	inline FString TravelMapPackage(const FURL& LastURL, const FString& URL, bool bAbsolute)
	{
		FURL Base = LastURL; // FURL pide la base sin const.
		const FURL Next(&Base, *URL, bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative);
		return Next.Valid ? Next.Map : FString();
	}

	/**
	 * true si TravelURL es el viaje que UEngine::HandleDisconnect deja pedido tras un fallo (SetClientTravel «?closed»: la
	 * entrada por defecto, el menú). Solo ese se anula para que el anfitrión siga en su sesión; cualquier otro viaje se respeta.
	 */
	inline bool IsEngineDisconnectTravel(const FString& TravelURL)
	{
		return TravelURL.Equals(TEXT("?closed"), ESearchCase::IgnoreCase);
	}

	/** Para el registro. */
	inline const TCHAR* ActionName(ETravelFailureAction Action)
	{
		switch (Action)
		{
		case ETravelFailureAction::ReturnHostToLobby: return TEXT("anfitrión al lobby");
		case ETravelFailureAction::StayInLobby:       return TEXT("se queda en el lobby");
		case ETravelFailureAction::ReturnToMenu:      return TEXT("al menú");
		default:                                      return TEXT("se queda en el menú");
		}
	}
}
