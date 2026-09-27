#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

class APawn;
class APlayerController;

/**
 * Fantasma del espectador y vuelta a la vida desde un huevo, en todos los modos (Docs/Fantasma_Espectador.md). Es el
 * contrato entre quien lo usa (modo carrera: sprint final de desempate; cooperativo: revivir en los nidos, pendiente) y
 * la implementación (Private/Player/TN_Ghost*.cpp y lo que cuelgue de ahí).
 *
 *  - Quien está de espectador (muerto en el cooperativo, ya en la meta o fuera del sprint final en la carrera) es un
 *    fantasmita: los demás lo ven flotando donde está su cámara, mirando a la tortuga que sigue, con la colita al
 *    viento; él ve la interfaz de la tortuga que sigue, cambia de tortuga y elige cámara libre alrededor de ella o fija
 *    a la vista del jugador.
 *  - ReviveIntoEgg lo devuelve a la vida desde un huevo: el fantasma hace una U invertida y se mete de cabeza en el
 *    huevo, el huevo vibra y eclosiona y la tortuga sale como del huevo de salida o del vestidor; en su pantalla, negro
 *    y una cáscara oscura que se resquebraja con líneas de luz por el medio y se abre.
 */
namespace TNGhost
{
	/** Servidor: la tortuga de PC acaba de salir del huevo de ReviveIntoEgg (Pawn = la tortuga nueva, ya poseída). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHatched, APlayerController* /*PC*/, APawn* /*Pawn*/);
	TORTUNABO_API FOnHatched& OnHatched();

	/**
	 * Servidor: devuelve a la vida a PC desde un huevo que aparece en EggTransform (apoyado en el suelo; su X es hacia
	 * donde mirará la tortuga al salir). Si PC es un fantasma, vuela hasta el huevo y se mete; si aún tiene tortuga, se
	 * le quita sin vuelo (solo el huevo y la pantalla). La tortuga se crea al eclosionar con
	 * AGameModeBase::RestartPlayerAtTransform y después se emite OnHatched. Devuelve false si no se puede (PC nulo, sin
	 * autoridad o ya volviendo a la vida).
	 */
	TORTUNABO_API bool ReviveIntoEgg(APlayerController* PC, const FTransform& EggTransform);

	/** true si PC está volviendo a la vida con ReviveIntoEgg (del vuelo del fantasma a la salida del huevo). */
	TORTUNABO_API bool IsReviving(const APlayerController* PC);

	/** true si el jugador de PC es ahora mismo un fantasma espectador (en cualquier máquina que tenga su controlador). */
	TORTUNABO_API bool IsGhost(const APlayerController* PC);
}
