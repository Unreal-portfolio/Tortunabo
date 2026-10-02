// PlayerController del Rally, sin el lobby ni el HUD de la tortuga a pie: crea el HUD del Rally solo en los jugadores
// locales y manda al servidor los cosméticos guardados (las tortugas sentadas en el buggy los pintan desde el
// PlayerState). Los controles (conducir, disparar, enderezar y pedir la reaparición) los pone el buggy o la artillera.
// Voz (#329): la misma voz por proximidad del juego (UProximityVoiceComponent en el peón, «pulsar para hablar» de los
// ajustes) con interfono entre las dos ocupantes del buggy (ATN_RallyPlayerState::GetVoiceIntercomGroup).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/TN_CosmeticsSync.h"
#include "Voice/TN_VoiceRouting.h"
#include "TN_RallyPlayerController.generated.h"

class ATN_Buggy;
class UTN_RallyHUDWidget;

UCLASS()
class TORTUNABO_API ATN_RallyPlayerController : public APlayerController, public ITN_VoiceListener
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

	// ITN_VoiceListener
	virtual void SendVoiceToOwningClient(const TArray<uint8>& CompressedData, int32 SenderSampleRate, AActor* SpeakerActor,
		bool bIntercom) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Jugador local: pone el salpicadero y el cartel del arco en su buggy (UTN_RallyDashboardComponent). */
	virtual void PlayerTick(float DeltaTime) override;
	/** Servidor: el peón nuevo (buggy de la conductora o peón de la artillera) lleva voz por proximidad. */
	virtual void OnPossess(APawn* InPawn) override;
	/** Jugador local: deja en el log qué peón posee (buggy de conductora o peón de artillera) y con qué roles. */
	virtual void AcknowledgePossession(APawn* InPawn) override;
	/**
	 * Servidor, al desconectarse: el motor destruiría el peón (el buggy de la conductora o el peón de la artillera) antes de
	 * Logout, y la carrera ya no sabría de qué equipo era. Sentada en un buggy, se deja tal cual: ATN_RallyGameMode::Logout,
	 * que llega justo después, la saca de su plaza y pasa la artillera al volante o retira el equipo.
	 */
	virtual void PawnLeavingGame() override;

private:
	/** Buggy en que va el jugador: el que conduce o el de su peón de artillera; nullptr si no va en ninguno. */
	ATN_Buggy* FindLocalBuggy() const;

	float DashboardCheckAccumulator = 0.f;

	/** Jugador local: manda al servidor el casco, el color, el caparazón y los ojos de su save (TNCosmeticsSync). */
	void SyncCosmeticsToServer();

	/** Validado contra DT_Helmets y DT_Skins del servidor (lo mismo que AMP_GamePlayerController). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSyncCosmetics(const FTNCosmeticLoadout& Loadout);

	/** Voz de otra tortuga que ha filtrado el servidor (TNVoiceRouting); bIntercom = de su mismo buggy. */
	UFUNCTION(Client, Unreliable)
	void ClientReceiveVoice(const TArray<uint8>& CompressedData, int32 SenderSampleRate, AActor* SpeakerActor, bool bIntercom);

	UPROPERTY(Transient)
	TObjectPtr<UTN_RallyHUDWidget> RallyHUD;
};
