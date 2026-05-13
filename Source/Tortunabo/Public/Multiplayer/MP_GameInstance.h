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

/** @brief Entrada de la tabla de loot de cascos: id + peso para sorteo ponderado. */
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

/**
 * @brief GameInstance global del proyecto. Sobrevive a los travels y centraliza:
 *  - Subsistema online (sesiones Steam): host, find/join, invite, destroy.
 *  - Persistencia de cosméticos (helmet + skin) vía UTN_CosmeticSaveGame.
 *  - Persistencia de score acumulado y estado del tutorial.
 *  - Loading screen entre mapas y status log.
 *  - PendingTravelPlayerCount: contador puente HQ → Run para saber cuántos esperar.
 *  - Auto-rejoin a la sesión Steam si el cliente pierde conexión durante un ServerTravel legítimo.
 */
UCLASS(Config=Game)
class TORTUNABO_API UMP_GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UMP_GameInstance();

	/** @brief Registra delegates online, carga saveGames de cosméticos/tutorial y prepara el listener de network failures. */
	virtual void Init() override;

	/** @brief Desregistra delegates y libera handles de timer antes del shutdown del engine. */
	virtual void Shutdown() override;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer")
	FOnStatusChanged OnStatusChanged;

	/** @brief Crea una sesión Steam (presence) y carga el mapa lobby como listen-server. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void HostSession();

	/** @brief Busca sesiones públicas y se une a la primera disponible. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void FindAndJoinSession();

	/** @brief Destruye la sesión Steam actual liberando el slot. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void DestroyCurrentSession();

	/** @brief Abre el overlay de Steam con la lista de amigos para invitar. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void InviteFriends();

	/** @brief Vuelve al menú principal cerrando la sesión activa de forma limpia. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void HandleReturnToMenu();

	/**
	 * Llamado por ClientNotifyServerTravel (Client RPC) en el lado del CLIENTE.
	 * Marca bIsPendingTravel = true y muestra loading screen, de modo que cuando
	 * el servidor destruya el NetDriver y el cliente reciba ConnectionLost,
	 * OnNetworkFailure active auto-rejoin en vez de destruir la sesión.
	 */
	void NotifyClientPendingTravel();

	/** @brief Muestra el widget de loading screen con un mensaje opcional. */
	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	void ShowLoadingScreen(const FString& Reason = TEXT("Cargando..."));

	/** @brief Quita el widget de loading screen del viewport. */
	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	void HideLoadingScreen();

	/** @brief Devuelve la lista de IDs de cascos desbloqueados del save game local. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	TArray<FName> GetUnlockedHelmetIds() const;

	/** @brief Indica si el casco está desbloqueado en el save game del jugador local. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool IsHelmetUnlocked(FName HelmetId) const;

	/** @brief Marca el casco como desbloqueado y persiste. Devuelve false si ya lo estaba. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool UnlockHelmet(FName HelmetId);

	/** @brief Equipa un casco YA desbloqueado. Devuelve false si no estaba en la lista. */
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

	/** @brief Devuelve el ID del casco equipado actualmente (NAME_None si ninguno). */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	FName GetEquippedHelmetId() const;

	/** @brief Sortea un casco aleatorio según los pesos de HelmetCrateTable, lo desbloquea y devuelve su ID. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	FName OpenHelmetCrate();

	/** @brief Devuelve el log de status formateado (últimos MaxStatusLines mensajes). */
	FString BuildStatusLog() const;

	/** @brief Máximo de jugadores configurado para la sesión. */
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

	// ── Race Score ───────────────────────────────────────────────────────────

	/**
	 * Añade puntos al marcador acumulado del jugador local y los persiste (#26).
	 * Llamado al entrar en Results cuando RaceScore del PlayerState es > 0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void AddRaceScore(int32 Points);

	/** Devuelve el total de puntos de carrera acumulados del jugador local. */
	UFUNCTION(BlueprintCallable, Category = "Score")
	int32 GetAccumulatedRaceScore() const;

	// ── Tutorial state ───────────────────────────────────────────────────────

	/**
	 * @brief Devuelve true si esta máquina ya ha spawneado al jugador en la zona de tutorial.
	 * @note El flag se persiste en disco y se consulta una sola vez al entrar al HQ.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool HasCompletedTutorial() const;

	/**
	 * @brief Marca el tutorial como completado y persiste el flag inmediatamente.
	 * @note Llamado por TN_HQGameMode la primera vez que enruta a un jugador a la zona de tutorial.
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
	/** @brief Callback online: sesión Steam creada — dispara ServerTravel al mapa lobby. */
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	/** @brief Callback online: búsqueda terminada — intenta join al primer resultado. */
	void OnFindSessionsComplete(bool bWasSuccessful);

	/** @brief Callback online: join completado — resuelve connect string y conecta al host. */
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	/** @brief Callback online: sesión destruida — relanza host/join si había uno pendiente. */
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	/** @brief Callback online: el jugador aceptó una invitación de Steam — guarda y hace join. */
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

	/** @brief Añade un mensaje al log circular y emite OnStatusChanged. */
	void UpdateStatus(const FString& Message);

	static constexpr int32 MaxStatusLines = 12;
	TArray<FString> StatusLog;

private:
	/** @brief Devuelve la interfaz online de sesiones (o nullptr si OnlineSubsystem no está disponible). */
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

	/** @brief Hook de fallo de red: decide si reconectar (auto-rejoin), reintentar listen o destruir sesión. */
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/** @brief Garantiza que existe el fichero steam_appid.txt junto al ejecutable. */
	void EnsureSteamAppIdFile();

	/** @brief Hook pre-load del mapa: muestra loading screen y captura URL para retries. */
	void HandlePreLoadMap(const FString& MapName);

	/** @brief Hook post-load del mapa: oculta loading y reanuda listen retry si aplica. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** @brief Actualiza el texto del loading screen sin destruir el widget. */
	void RefreshLoadingText(const FString& Reason) const;

	/** @brief Carga el UTN_CosmeticSaveGame del slot (o crea uno vacío). */
	void LoadCosmeticProfile();

	/** @brief Persiste el UTN_CosmeticSaveGame en disco. */
	void SaveCosmeticProfile() const;

	/** @brief Construye el nombre de slot del save (incluye sufijo de Steam ID si está disponible). */
	FString BuildCosmeticSaveSlot() const;

	/** @brief Carga el UTN_TutorialSaveGame del disco. */
	void LoadTutorialProfile();

	/** @brief Persiste el UTN_TutorialSaveGame en disco. */
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
