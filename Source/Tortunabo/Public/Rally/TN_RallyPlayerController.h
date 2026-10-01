// PlayerController del Rally, sin el lobby ni el HUD de la tortuga a pie: crea el HUD del Rally solo en los jugadores
// locales y manda al servidor los cosméticos guardados (las tortugas sentadas en el buggy los pintan desde el
// PlayerState). Los controles (conducir, disparar, enderezar y pedir la reaparición) los pone el buggy o la artillera.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/TN_CosmeticsSync.h"
#include "TN_RallyPlayerController.generated.h"

class UTN_RallyHUDWidget;

UCLASS()
class TORTUNABO_API ATN_RallyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATN_RallyPlayerController();

#if !UE_BUILD_SHIPPING
	/**
	 * Depuración (TN.Rally.DebugCosmetics): manda al servidor el aspecto guardado con el color, el caparazón y los ojos
	 * cambiados (NAME_None = el guardado), como si estuvieran desbloqueados. No toca el save.
	 */
	void DebugSendCosmetics(FName SkinId, FName ShellId, FName EyesId);
#endif

	/** HUD del Rally de este jugador (solo en el jugador local; nullptr en el resto). Lee calor, munición y tinta del buggy. */
	UFUNCTION(BlueprintPure, Category = "Rally")
	UTN_RallyHUDWidget* GetRallyHUD() const { return RallyHUD; }

	UPROPERTY(EditDefaultsOnly, Category = "Rally")
	TSubclassOf<UTN_RallyHUDWidget> HUDWidgetClass;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Jugador local: deja en el log qué peón posee (buggy de conductora o peón de artillera) y con qué roles. */
	virtual void AcknowledgePossession(APawn* InPawn) override;
	/**
	 * Servidor, al desconectarse: el motor destruiría el peón (el buggy de la conductora o el peón de la artillera) antes de
	 * Logout, y la carrera ya no sabría de qué equipo era. Sentada en un buggy, se deja tal cual: ATN_RallyGameMode::Logout,
	 * que llega justo después, la saca de su plaza y pasa la artillera al volante o retira el equipo.
	 */
	virtual void PawnLeavingGame() override;

private:
	/** Jugador local: manda al servidor el casco, el color, el caparazón y los ojos de su save (TNCosmeticsSync). */
	void SyncCosmeticsToServer();

	/** Validado contra DT_Helmets y DT_Skins del servidor (lo mismo que AMP_GamePlayerController). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSyncCosmetics(const FTNCosmeticLoadout& Loadout);

	UPROPERTY(Transient)
	TObjectPtr<UTN_RallyHUDWidget> RallyHUD;
};
