#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Engine/EngineBaseTypes.h"
#include "MP_GameInstance.generated.h"

class UNetDriver;
class UUserWidget;
class UTN_CosmeticSaveGame;
class UTN_TutorialSaveGame;

USTRUCT(BlueprintType)
struct FTN_HelmetCrateEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FName HelmetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta=(ClampMin="0.0"))
	float Weight = 1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatusChanged, const FString&, StatusMessage);

UCLASS(Config=Game)
class TORTUNABO_API UMP_GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UMP_GameInstance();

	virtual void Init() override;
	virtual void Shutdown() override;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer")
	FOnStatusChanged OnStatusChanged;

	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void HostSession();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void FindAndJoinSession();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void DestroyCurrentSession();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void InviteFriends();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void HandleReturnToMenu();

	/**
	 * Llamado por ClientNotifyServerTravel (Client RPC) en el lado del CLIENTE.
	 * Marca bIsPendingTravel = true y muestra loading screen, de modo que cuando
	 * el servidor destruya el NetDriver y el cliente reciba ConnectionLost,
	 * OnNetworkFailure active auto-rejoin en vez de destruir la sesión.
	 */
	void NotifyClientPendingTravel();

	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	void ShowLoadingScreen(const FString& Reason = TEXT("Cargando..."));

	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	void HideLoadingScreen();

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	TArray<FName> GetUnlockedHelmetIds() const;

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool IsHelmetUnlocked(FName HelmetId) const;

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool UnlockHelmet(FName HelmetId);

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool EquipHelmet(FName HelmetId);

	/**
	 * Equipa el helmet directamente sin verificar si está desbloqueado.
	 * Usado por las estatuas de cosmético del lobby (donde el helmet siempre está disponible).
	 * Si HelmetId != NAME_None, lo añade automáticamente a la lista de desbloqueados.
	 * NAME_None = desequipar (válido siempre).
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool ForceEquipHelmet(FName HelmetId);

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	FName GetEquippedHelmetId() const;

	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	FName OpenHelmetCrate();

	FString BuildStatusLog() const;

	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	int32 GetMaxPlayers() const { return MaxPlayers; }

	/** Devuelve el DataTable de cascos para lookup externo (TortugaCharacter, widget). */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	UDataTable* GetHelmetDataTable() const { return HelmetDataTable; }

	// ── Skin de personaje ────────────────────────────────────────────────────

	/** Equipa el skin de personaje indicado y lo persiste. NAME_None = sin skin. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool EquipSkin(FName SkinId);

	/** Devuelve el skin equipado actualmente (NAME_None = sin skin). */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	FName GetEquippedSkinId() const;

	/** Devuelve el DataTable de skins para lookup externo. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	UDataTable* GetSkinDataTable() const { return SkinDataTable; }

	// ── Tutorial state ───────────────────────────────────────────────────────

	/**
	 * Returns true if this machine's player has already been spawned in the
	 * tutorial zone at least once (flag is persisted across sessions).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool HasCompletedTutorial() const;

	/**
	 * Marks the tutorial as completed and immediately persists the flag to disk.
	 * Called by TN_HQGameMode the first time it routes a player to the tutorial zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SetTutorialCompleted();

	/**
	 * Número de jugadores conectados en el lobby ANTES de hacer ServerTravel al Run.
	 * TN_HQGameMode lo asigna justo antes de viajar; TN_RunGameMode lo lee para
	 * saber cuántos jugadores esperar en el nuevo mapa.
	 * Persiste a través del non-seamless travel (GameInstance sobrevive).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer")
	int32 PendingTravelPlayerCount = 0;

protected:
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer")
	FString GameMapPath = TEXT("/Game/Maps/Lobby/LVL_HQ");

	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer")
	FString MenuMapPath = TEXT("/Game/Maps/Lobby/LVL_Menu");

	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer")
	int32 MaxPlayers = 4;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Multiplayer|Steam", meta=(ClampMin="1"))
	int32 SteamDevAppId = 480;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Loading")
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Cosmetics")
	TArray<FName> DefaultUnlockedHelmets;

	UPROPERTY(EditDefaultsOnly, Category = "Cosmetics")
	TArray<FTN_HelmetCrateEntry> HelmetCrateTable;

	UPROPERTY(EditDefaultsOnly, Category = "Cosmetics")
	FString CosmeticSaveSlotPrefix = TEXT("Cosmetics");

	/**
	 * DataTable con filas FTN_HelmetData (ID, mesh, icono, escala, offset).
	 * Asigna DT_Helmets aquí en BP_GameInstance → Class Defaults.
	 * Usado por TortugaCharacter::UpdateHelmetMesh para instanciar el mesh del casco.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Cosmetics")
	TObjectPtr<UDataTable> HelmetDataTable;

	/**
	 * DataTable con filas FTN_SkinData (ID, material de cuerpo, icono).
	 * Asigna DT_Skins aquí en BP_GameInstance → Class Defaults.
	 * Usado por TortugaCharacter::UpdateSkinVisual para cambiar el material del personaje.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Cosmetics")
	TObjectPtr<UDataTable> SkinDataTable;

	void UpdateStatus(const FString& Message);

	static constexpr int32 MaxStatusLines = 12;
	TArray<FString> StatusLog;

private:
	IOnlineSessionPtr GetSessionInterface() const;

	bool bPendingHostAfterDestroy = false;
	bool bPendingJoinAfterDestroy = false;
	FOnlineSessionSearchResult PendingInviteResult;

	/**
	 * true durante el intervalo entre PreLoadMap y PostLoadMap.
	 * Usado para diferenciar errores de red en inicio de conexión (sesión zombi)
	 * vs. errores transitorios durante un ServerTravel normal (no destruir sesión).
	 */
	bool bIsPendingTravel = false;

	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void EnsureSteamAppIdFile();
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void RefreshLoadingText(const FString& Reason) const;
	void LoadCosmeticProfile();
	void SaveCosmeticProfile() const;
	FString BuildCosmeticSaveSlot() const;

	void LoadTutorialProfile();
	void SaveTutorialProfile() const;

	/** Reintenta crear el listen server tras un NetDriverListenFailure durante travel. */
	void RetryListenServer();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingScreenWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTN_CosmeticSaveGame> CosmeticProfile;

	UPROPERTY(Transient)
	TObjectPtr<UTN_TutorialSaveGame> TutorialProfile;

	bool bIsLoadingScreenVisible = false;

	/** true si HandlePostLoadMap debe reintentar crear el listen server. */
	bool bNeedsListenRetry = false;

	/** URL pendiente para el listen retry (guardada desde HandlePreLoadMap). */
	FString PendingListenURL;

	/** Reintentos restantes para el listen server. */
	int32 ListenRetryCount = 0;

	/** Máximo de reintentos para crear el listen server. */
	static constexpr int32 MaxListenRetries = 5;

	/** Timer para reintentos de listen. */
	FTimerHandle ListenRetryTimerHandle;

	FDelegateHandle InviteAcceptedDelegateHandle;

	// ── Auto-rejoin tras ConnectionLost durante travel ─────────────────────
	/**
	 * true cuando el cliente pierde conexión durante un travel legítimo del servidor.
	 * En vez de destruir la sesión, intentamos reconectar vía Steam session.
	 */
	bool bPendingAutoRejoin = false;

	int32 AutoRejoinRetryCount = 0;
	static constexpr int32 MaxAutoRejoinRetries = 8;

	FTimerHandle AutoRejoinTimerHandle;

	/** Intenta reconectar al host resolviendo el connect string de la sesión Steam. */
	void AttemptAutoRejoin();
};

