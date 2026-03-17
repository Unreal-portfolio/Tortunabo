#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Engine/EngineBaseTypes.h"
#include "MP_GameInstance.generated.h"

class UNetDriver;

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

	FString BuildStatusLog() const;

	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	int32 GetMaxPlayers() const { return MaxPlayers; }

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

	void UpdateStatus(const FString& Message);

	static constexpr int32 MaxStatusLines = 12;
	TArray<FString> StatusLog;

private:
	IOnlineSessionPtr GetSessionInterface() const;

	bool bPendingHostAfterDestroy = false;
	bool bPendingJoinAfterDestroy = false;
	FOnlineSessionSearchResult PendingInviteResult;

	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void EnsureSteamAppIdFile();

	FDelegateHandle InviteAcceptedDelegateHandle;
};

