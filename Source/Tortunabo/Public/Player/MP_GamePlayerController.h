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

UCLASS()
class TORTUNABO_API AMP_GamePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMP_GamePlayerController();

	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void EnterSpectateMode();

	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void SpectateNextPlayer();

	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void SpectatePreviousPlayer();

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void OpenCosmeticsMenu();

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool RequestEquipHelmet(FName HelmetId);

	/** Desequipa el casco actual. Actualiza PlayerState en el servidor. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void RequestUnequipHelmet();

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	FName OpenHelmetCrate();

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

	// ── Quick Chat (Rocket League style) ─────────────────────────────────────

	/**
	 * Send a quick chat message by compact ID from the local catalog.
	 * Server validates ID + cooldown and writes to replicated GameState history.
	 */
	UFUNCTION(BlueprintCallable, Category = "QuickChat")
	void SendQuickChat(uint8 MessageID);

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

	UFUNCTION(BlueprintPure, Category = "QuickChat")
	bool ResolveQuickChatDisplayData(const FTN_QuickChatEntry& Entry, FText& OutSenderName, FText& OutMessageText, UTexture2D*& OutIcon) const;

	/** Cooldown between quick chat messages (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QuickChat", meta = (ClampMin = "0.5"))
	float QuickChatCooldownSeconds = 2.f;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
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

	UFUNCTION(Server, Reliable)
	void ServerSyncUnlockedHelmets(const TArray<FName>& UnlockedHelmetIds);

	UFUNCTION(Server, Reliable)
	void ServerSetEquippedHelmet(FName HelmetId);

	void SyncCosmeticsToServer();

	void ApplyGameplayInputMode();
	void ApplyRadialInputMode();
	void RestorePostRadialInputMode();
	void CreateVoiceHUD();
	void CreateCoopFlowHUD();
	void CreatePlayerHUD();
	void CreateRadialWidgets();
	void SpectateByDirection(int32 Direction);
	void CacheRadialInputAssets();

	void OnOpenEmoteWheelStarted();
	void OnOpenEmoteWheelReleased();
	void OnOpenQuickChatWheelStarted();
	void OnOpenQuickChatWheelReleased();

	void OnRadialNavigateTriggered(const FInputActionValue& Value);
	void OnRadialNavigateCompleted(const FInputActionValue& Value);

	void OpenRadialWheel(ETN_RadialWheelType WheelType);
	void CloseRadialWheel(bool bConfirmSelection);
	void UpdateRadialWheelInput();
	FVector2D ComputeMouseWheelVector() const;
	FVector2D ResolveCurrentWheelVector() const;
	UTN_RadialWheelWidgetBase* GetActiveWheelWidget() const;

	TSet<FName> ServerUnlockedHelmets;

	/** Server RPC: validate message ID, apply rate limit, write to GameState. */
	UFUNCTION(Server, Reliable)
	void ServerSendQuickChat(uint8 MessageID);

	ETN_RadialWheelType ActiveWheelType = ETN_RadialWheelType::None;
	FVector2D CachedStickVector = FVector2D::ZeroVector;
	float LastStickInputRealTime = -1000.f;
	FTimerHandle RadialWheelUpdateTimerHandle;
	FVector2D CachedMousePositionBeforeWheel = FVector2D::ZeroVector;
	bool bHadMousePositionBeforeWheel = false;
};
