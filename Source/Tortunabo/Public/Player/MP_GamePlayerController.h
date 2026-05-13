#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/HUD/TN_RadialWheelTypes.h"
#include "MP_GamePlayerController.generated.h"

struct FInputActionValue;
class UUserWidget;
class UInputAction;
class UTexture2D;
class UTN_RadialWheelWidgetBase;
class UTN_EmoteWheelDataAsset;
class UTN_QuickChatWheelDataAsset;

/**
 * @brief PlayerController principal del gameplay. Centraliza HUD, espectador, cosméticos, ruedas radiales, VOIP, Quick Chat y emotes.
 *
 * Responsabilidades:
 *  - Creación/refresh del HUD (Stamina, CoopFlow, VoiceIndicator, Cosmetics, PlayerHUD).
 *  - Modo espectador: enter, next/previous con filtro por vivos/no eliminados.
 *  - Sincronización servidor↔cliente de cosméticos (helmet + skin) con Server RPCs y persistencia en GameInstance.
 *  - Quick Chat (RL-style): envío por catálogo con cooldown server-side.
 *  - Ruedas radiales (Emote + QuickChat) con input mouse/stick.
 *  - VOIP: receptor de audio filtrado por proximidad desde el servidor.
 *  - Auto-rejoin: hooks de seamless travel para preservar la sesión Steam.
 */
UCLASS()
class TORTUNABO_API AMP_GamePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMP_GamePlayerController();

	/** @brief Entra en modo espectador: posee un SpectatorPawn y comienza a observar al siguiente vivo. */
	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void EnterSpectateMode();

	/** @brief Cambia el target del espectador al siguiente jugador vivo y no eliminado. */
	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void SpectateNextPlayer();

	/** @brief Cambia el target del espectador al anterior jugador vivo y no eliminado. */
	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void SpectatePreviousPlayer();

	/** @brief Abre el widget de cosméticos. Disponible sólo en el lobby HQ. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void OpenCosmeticsMenu();

	/**
	 * @brief Solicita equipar un casco (con verificación de unlock vía GameInstance).
	 * @param HelmetId ID del casco a equipar.
	 * @return true si el GameInstance lo aceptó. Persiste y replica vía PlayerState.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool RequestEquipHelmet(FName HelmetId);

	/** @brief Desequipa el casco actual. Actualiza PlayerState en el servidor. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void RequestUnequipHelmet();

	/** @brief Abre una caja de cascos: sortea uno por pesos y lo equipa. Devuelve el ID. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	FName OpenHelmetCrate();

	/** @brief Client RPC: abre el widget de cosméticos en el cliente owner (llamado desde estatuas del lobby). */
	UFUNCTION(Client, Reliable)
	void ClientOpenCosmeticsMenu();

	/**
	 * Server → Client: "Prepárate, el servidor va a hacer ServerTravel."
	 * El cliente marca bIsPendingTravel en su GameInstance y muestra loading screen.
	 * Cuando el NetDriver se destruya y el cliente reciba ConnectionLost,
	 * OnNetworkFailure verá bIsPendingTravel=true y activará auto-rejoin.
	 */
	UFUNCTION(Client, Reliable)
	void ClientNotifyServerTravel();

	// ── Quick Chat (estilo Rocket League) ────────────────────────────────────

	/**
	 * @brief Envía un mensaje de Quick Chat por ID compacto del catálogo local.
	 *        El servidor valida el ID + cooldown y escribe en el historial replicado del GameState.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickChat")
	void SendQuickChat(uint8 MessageID);

	/**
	 * @brief Pide al servidor reproducir un emote por ID (Multicast tras validación).
	 * @param EmoteID Id del emote según el DataAsset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Emotes")
	void RequestPlayEmoteById(uint8 EmoteID);

	/**
	 * Llamado desde ATortugaCharacter::PawnClientRestart (cliente) para re-añadir
	 * los widgets del HUD al viewport tras seamless travel.
	 * En seamless travel el PC persiste pero UWorld::CleanupWorld elimina todos
	 * los widgets del viewport. Este método los vuelve a añadir.
	 */
	void RefreshHUDAfterPossession();

	/**
	 * Client RPC: limpia flags de ignorar input (del modo espectador)
	 * y restaura el modo de juego. Llamado desde RevivePlayer en el servidor.
	 * Sin esto, tras ser revivido el jugador no puede moverse porque
	 * ChangeState(Spectating) / BeginSpectatingState incrementa IgnoreMoveInput
	 * y la secuencia Possess+ClientRestart no lo decrementa en el cliente.
	 */
	UFUNCTION(Client, Reliable)
	void ClientRestorePlayerInput();

	/** @brief Versión sin RPC para el listen-server (los Client RPCs no se ejecutan en el host). */
	void ForceRestoreInput();

	/**
	 * @brief Recibe audio de voz filtrado por proximidad desde el servidor.
	 * @param CompressedData Buffer comprimido del emisor.
	 * @param SenderSampleRate SampleRate original del emisor.
	 * @param SpeakerActor Actor emisor (para calcular distancia).
	 */
	UFUNCTION(Client, Unreliable)
	void ClientReceiveVoice(const TArray<uint8>& CompressedData, int32 SenderSampleRate, AActor* SpeakerActor);

	/** @brief Notifica al cliente dueño que guarde el SkinId. Llamado desde estatuas del lobby (servidor). */
	void NotifySkinEquipped(FName SkinId);

	/** @brief Notifica al cliente dueño que guarde el HelmetId. Llamado desde estatuas del lobby (servidor). */
	void NotifyHelmetEquipped(FName HelmetId);

	/**
	 * @brief Resuelve la info de display (nombre, mensaje, icono) de una entrada de Quick Chat.
	 * @param Entry Entrada bruta replicada.
	 * @param OutSenderName Nombre del emisor (resuelto vía PlayerState).
	 * @param OutMessageText Texto del catálogo.
	 * @param OutIcon Icono asociado.
	 * @return true si se pudo resolver la entrada.
	 */
	UFUNCTION(BlueprintPure, Category = "QuickChat")
	bool ResolveQuickChatDisplayData(const FTN_QuickChatEntry& Entry, FText& OutSenderName, FText& OutMessageText, UTexture2D*& OutIcon) const;

	/** Cooldown entre mensajes de Quick Chat (segundos). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QuickChat", meta = (ClampMin = "0.5"))
	float QuickChatCooldownSeconds = 2.f;

protected:
	/** @brief Bindea delegates, crea HUD y carga assets de input radial. */
	virtual void BeginPlay() override;

	/** @brief OnPossess: refresca cosméticos del pawn poseído y sincroniza con servidor. */
	virtual void OnPossess(APawn* InPawn) override;

	/** @brief Carga UInputActions (soft refs) y bindea acciones de ruedas radiales y menú. */
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> VoiceIndicatorWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> CoopFlowWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> CosmeticsWidgetClass;

	/**
	 * HUD principal del jugador (barra de stamina, etc.).
	 * Asigna WBP_PlayerHUD en BP_GamePlayerController → Class Defaults.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> PlayerHUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Radial")
	TSubclassOf<UTN_RadialWheelWidgetBase> EmoteWheelWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Radial")
	TSubclassOf<UTN_RadialWheelWidgetBase> QuickChatWheelWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data|Radial")
	TObjectPtr<UTN_EmoteWheelDataAsset> EmoteWheelDataAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data|Radial")
	TObjectPtr<UTN_QuickChatWheelDataAsset> QuickChatWheelDataAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Radial")
	TSoftObjectPtr<UInputAction> OpenEmoteWheelAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Radial")
	TSoftObjectPtr<UInputAction> OpenQuickChatWheelAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Radial")
	TSoftObjectPtr<UInputAction> RadialNavigateAction;

	/** Input Action para volver al menú principal. Asignar IA_ReturnToMenu en el BP hijo. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Menu")
	TSoftObjectPtr<UInputAction> ReturnToMenuAction;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> VoiceIndicatorWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> CoopFlowWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> CosmeticsWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> PlayerHUDWidget;

	UPROPERTY()
	TObjectPtr<UTN_RadialWheelWidgetBase> EmoteWheelWidget;

	UPROPERTY()
	TObjectPtr<UTN_RadialWheelWidgetBase> QuickChatWheelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedOpenEmoteWheelAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedOpenQuickChatWheelAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedRadialNavigateAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedReturnToMenuAction;

	/** @brief Server RPC: sincroniza la lista de cascos desbloqueados del cliente al PC del servidor. */
	UFUNCTION(Server, Reliable)
	void ServerSyncUnlockedHelmets(const TArray<FName>& UnlockedHelmetIds);

	/** @brief Server RPC: asigna el casco equipado en el PlayerState. */
	UFUNCTION(Server, Reliable)
	void ServerSetEquippedHelmet(FName HelmetId);

	/** @brief Server RPC: aplica el skin de personaje en el PlayerState (llamado desde SyncCosmeticsToServer). */
	UFUNCTION(Server, Reliable)
	void ServerSetEquippedSkin(FName SkinId);

	/** @brief Client RPC: guarda SkinId en GameInstance del cliente dueño. */
	UFUNCTION(Client, Reliable)
	void ClientSaveSkin(FName SkinId);

	/** @brief Client RPC: guarda HelmetId en GameInstance del cliente dueño. */
	UFUNCTION(Client, Reliable)
	void ClientSaveHelmet(FName HelmetId);

	/** @brief Empuja al servidor la lista local de unlocks + cosméticos equipados (skin/helmet). */
	void SyncCosmeticsToServer();

	/** @brief Pone el input mode a Game (focus al viewport, sin cursor). */
	void ApplyGameplayInputMode();

	/** @brief Pone el input mode a GameAndUI con cursor visible para el wheel radial. */
	void ApplyRadialInputMode();

	/** @brief Restaura el input mode a Game tras cerrar una rueda radial. */
	void RestorePostRadialInputMode();

	/** @brief Crea el widget VoiceIndicator y lo añade al viewport. */
	void CreateVoiceHUD();

	/** @brief Crea el widget CoopFlow (estado de partida) y lo añade al viewport. */
	void CreateCoopFlowHUD();

	/** @brief Crea el widget PlayerHUD (stamina, inventario) y lo añade al viewport. */
	void CreatePlayerHUD();

	/** @brief Crea los widgets de las ruedas radiales (Emote + QuickChat) sin añadirlos al viewport. */
	void CreateRadialWidgets();

	/** @brief Cambia el target del espectador en la dirección dada (+1 / -1). */
	void SpectateByDirection(int32 Direction);

	/** @brief Resuelve los SoftObjectPtr de InputAction a TObjectPtr cargados. */
	void CacheRadialInputAssets();

	void OnOpenEmoteWheelStarted();
	void OnOpenEmoteWheelReleased();
	void OnOpenQuickChatWheelStarted();
	void OnOpenQuickChatWheelReleased();

	void OnRadialNavigateTriggered(const FInputActionValue& Value);
	void OnRadialNavigateCompleted(const FInputActionValue& Value);

	/** @brief Handler del input "volver al menú": pide al servidor cerrar la sesión y travel a LVL_Menu. */
	void OnReturnToMenuPressed();

	/** @brief Server RPC: cierra la sesión y manda a todos al menú principal. */
	UFUNCTION(Server, Reliable)
	void ServerRequestReturnToMenu();

	/** @brief Abre una rueda radial (Emote o QuickChat) según WheelType. */
	void OpenRadialWheel(ETN_RadialWheelType WheelType);

	/** @brief Cierra la rueda activa, opcionalmente confirmando la opción seleccionada. */
	void CloseRadialWheel(bool bConfirmSelection);

	/** @brief Timer (alta frecuencia) que recalcula la opción apuntada por mouse/stick. */
	void UpdateRadialWheelInput();

	/** @brief Vector normalizado de la posición del ratón respecto al centro de la rueda. */
	FVector2D ComputeMouseWheelVector() const;

	/** @brief Devuelve el vector del input activo (mouse o stick) según contexto. */
	FVector2D ResolveCurrentWheelVector() const;

	/** @brief Devuelve el widget de la rueda activa o nullptr si no hay ninguna. */
	UTN_RadialWheelWidgetBase* GetActiveWheelWidget() const;

	TSet<FName> ServerUnlockedHelmets;

	/** @brief Server RPC: valida ID, aplica rate limit y escribe el QuickChat en el GameState. */
	UFUNCTION(Server, Reliable)
	void ServerSendQuickChat(uint8 MessageID);

	ETN_RadialWheelType ActiveWheelType = ETN_RadialWheelType::None;
	FVector2D CachedStickVector = FVector2D::ZeroVector;
	float LastStickInputRealTime = -1000.f;
	FTimerHandle RadialWheelUpdateTimerHandle;
	FVector2D CachedMousePositionBeforeWheel = FVector2D::ZeroVector;
	bool bHadMousePositionBeforeWheel = false;
};
