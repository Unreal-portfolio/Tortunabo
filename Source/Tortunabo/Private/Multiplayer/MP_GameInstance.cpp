#include "Multiplayer/MP_GameInstance.h"
#include "OnlineSubsystem.h"
#include "Online.h"
#include "OnlineSessionSettings.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UMP_GameInstance::UMP_GameInstance()
{
}

void UMP_GameInstance::Init()
{
	Super::Init();
	EnsureSteamAppIdFile();

	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(FName(TEXT("Steam")));
	if (!OSS)
	{
		OSS = IOnlineSubsystem::Get();
	}

	if (OSS)
	{
		const FString SubsystemName = OSS->GetSubsystemName().ToString();
		UpdateStatus(FString::Printf(TEXT("Online Subsystem: %s"), *SubsystemName));

		if (SubsystemName == TEXT("NULL"))
		{
			UpdateStatus(TEXT("WARNING: Steam not available in PIE. Use Standalone Game to test multiplayer."));
		}
		else
		{
			IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
			if (Sessions.IsValid())
			{
				InviteAcceptedDelegateHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
					FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UMP_GameInstance::OnSessionUserInviteAccepted));
			}
		}
	}
	else
	{
		UpdateStatus(TEXT("ERROR: No Online Subsystem. Is Steam running?"));
	}

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UMP_GameInstance::OnNetworkFailure);
	}
}

void UMP_GameInstance::EnsureSteamAppIdFile()
{
#if !UE_BUILD_SHIPPING
	if (SteamDevAppId <= 0)
	{
		UpdateStatus(TEXT("WARNING: SteamDevAppId invalido; no se genero steam_appid.txt"));
		return;
	}

	const FString AppIdText = FString::Printf(TEXT("%d\n"), SteamDevAppId);
	const FString AppIdValue = FString::FromInt(SteamDevAppId);
	const FString PrimaryPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Win64"), TEXT("steam_appid.txt"));
	const FString FallbackPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("steam_appid.txt"));

	if (GConfig)
	{
		GConfig->SetInt(TEXT("OnlineSubsystemSteam"), TEXT("SteamDevAppId"), SteamDevAppId, GEngineIni);
	}

	FPlatformMisc::SetEnvironmentVar(TEXT("SteamAppId"), *AppIdValue);
	FPlatformMisc::SetEnvironmentVar(TEXT("SteamGameId"), *AppIdValue);

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PrimaryPath), true);

	bool bWritten = FFileHelper::SaveStringToFile(AppIdText, *PrimaryPath, FFileHelper::EEncodingOptions::ForceAnsi);
	if (!bWritten)
	{
		bWritten = FFileHelper::SaveStringToFile(AppIdText, *FallbackPath, FFileHelper::EEncodingOptions::ForceAnsi);
	}

	if (bWritten)
	{
		UpdateStatus(FString::Printf(TEXT("Steam AppID %d preparado automaticamente"), SteamDevAppId));
	}
	else
	{
		UpdateStatus(TEXT("WARNING: No se pudo escribir steam_appid.txt automaticamente"));
	}
#endif
}

void UMP_GameInstance::Shutdown()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && InviteAcceptedDelegateHandle.IsValid())
	{
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedDelegateHandle);
		InviteAcceptedDelegateHandle.Reset();
	}

	Super::Shutdown();
}

IOnlineSessionPtr UMP_GameInstance::GetSessionInterface() const
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(FName(TEXT("Steam")));
	if (!OSS)
	{
		OSS = IOnlineSubsystem::Get();
	}

	if (!OSS)
	{
		UE_LOG(LogTemp, Error, TEXT("[MP] Online Subsystem is NULL. Make sure Steam is running."));
		return nullptr;
	}

	return OSS->GetSessionInterface();
}

void UMP_GameInstance::UpdateStatus(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("[MP] %s"), *Message);

	if (StatusLog.Num() >= MaxStatusLines)
	{
		StatusLog.RemoveAt(0);
	}
	StatusLog.Add(Message);

	OnStatusChanged.Broadcast(BuildStatusLog());
}

FString UMP_GameInstance::BuildStatusLog() const
{
	return FString::Join(StatusLog, TEXT("\n"));
}

void UMP_GameInstance::HostSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UpdateStatus(TEXT("ERROR: No session interface"));
		return;
	}

	FNamedOnlineSession* Existing = Sessions->GetNamedSession(NAME_GameSession);
	if (Existing)
	{
		bPendingHostAfterDestroy = true;
		bPendingJoinAfterDestroy = false;
		Sessions->ClearOnDestroySessionCompleteDelegates(this);
		Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMP_GameInstance::OnDestroySessionComplete));
		Sessions->DestroySession(NAME_GameSession);
		UpdateStatus(TEXT("Destroying old session first..."));
		return;
	}

	Sessions->ClearOnCreateSessionCompleteDelegates(this);
	Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMP_GameInstance::OnCreateSessionComplete));

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = false;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowInvites = true;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.Set(FName(TEXT("SEARCH_KEYWORDS")), FString(TEXT("TortunaboLobby")), EOnlineDataAdvertisementType::ViaOnlineService);

	UpdateStatus(TEXT("Creating Steam lobby..."));
	Sessions->CreateSession(0, NAME_GameSession, Settings);
}

void UMP_GameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegates(this);
	}

	if (!bWasSuccessful)
	{
		UpdateStatus(FString::Printf(TEXT("ERROR: Failed to create session '%s'"), *SessionName.ToString()));
		return;
	}

	UpdateStatus(FString::Printf(TEXT("Lobby '%s' created! Travelling to game map..."), *SessionName.ToString()));

	UWorld* World = GetWorld();
	if (World)
	{
		World->ServerTravel(GameMapPath + TEXT("?listen"));
	}
}

void UMP_GameInstance::FindAndJoinSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UpdateStatus(TEXT("ERROR: No session interface"));
		return;
	}

	Sessions->ClearOnFindSessionsCompleteDelegates(this);
	Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMP_GameInstance::OnFindSessionsComplete));

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->bIsLanQuery = false;
	SessionSearch->MaxSearchResults = 50;
	SessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);

	UpdateStatus(TEXT("Searching Steam lobbies..."));
	Sessions->FindSessions(0, SessionSearch.ToSharedRef());
}

void UMP_GameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegates(this);
	}

	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		UpdateStatus(TEXT("Search failed. Make sure Steam is online."));
		return;
	}

	int32 MatchIndex = INDEX_NONE;
	for (int32 i = 0; i < SessionSearch->SearchResults.Num(); ++i)
	{
		const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[i];
		FString KeywordValue;
		if (Result.Session.SessionSettings.Get(FName(TEXT("SEARCH_KEYWORDS")), KeywordValue) && KeywordValue == TEXT("TortunaboLobby"))
		{
			MatchIndex = i;
			break;
		}
	}

	if (MatchIndex == INDEX_NONE)
	{
		UpdateStatus(FString::Printf(TEXT("No matching lobbies found (%d total seen)."), SessionSearch->SearchResults.Num()));
		return;
	}

	UpdateStatus(TEXT("Found matching lobby! Joining..."));
	Sessions->ClearOnJoinSessionCompleteDelegates(this);
	Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMP_GameInstance::OnJoinSessionComplete));
	Sessions->JoinSession(0, NAME_GameSession, SessionSearch->SearchResults[MatchIndex]);
}

void UMP_GameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegates(this);
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		UpdateStatus(FString::Printf(TEXT("ERROR joining '%s': code %d"), *SessionName.ToString(), static_cast<int32>(Result)));
		return;
	}

	FString ConnectInfo;
	if (Sessions.IsValid() && Sessions->GetResolvedConnectString(SessionName, ConnectInfo) && !ConnectInfo.IsEmpty())
	{
		APlayerController* PC = GetFirstLocalPlayerController();
		if (PC)
		{
			PC->ClientTravel(ConnectInfo, TRAVEL_Absolute);
		}
	}
	else
	{
		UpdateStatus(TEXT("ERROR: Could not resolve connect string"));
	}
}

void UMP_GameInstance::InviteFriends()
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(FName(TEXT("Steam")));
	if (!OSS)
	{
		OSS = IOnlineSubsystem::Get();
	}

	if (!OSS)
	{
		UpdateStatus(TEXT("ERROR: Steam not available for invites."));
		return;
	}

	IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UpdateStatus(TEXT("ERROR: No session interface for invites."));
		return;
	}

	if (!Sessions->GetNamedSession(NAME_GameSession))
	{
		UpdateStatus(TEXT("No active session. Host a game first before inviting friends."));
		return;
	}

	IOnlineExternalUIPtr ExternalUI = OSS->GetExternalUIInterface();
	if (ExternalUI.IsValid())
	{
		ExternalUI->ShowInviteUI(0, NAME_GameSession);
		UpdateStatus(TEXT("Opening Steam invite overlay..."));
	}
	else
	{
		UpdateStatus(TEXT("ERROR: Steam overlay not available."));
	}
}

void UMP_GameInstance::OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful)
	{
		UpdateStatus(TEXT("ERROR: Failed to accept Steam invite."));
		return;
	}

	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UpdateStatus(TEXT("ERROR: No session interface to join invited session."));
		return;
	}

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bPendingHostAfterDestroy = false;
		bPendingJoinAfterDestroy = true;
		PendingInviteResult = InviteResult;
		Sessions->ClearOnDestroySessionCompleteDelegates(this);
		Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMP_GameInstance::OnDestroySessionComplete));
		Sessions->DestroySession(NAME_GameSession);
		UpdateStatus(TEXT("Destroying current session to join invite..."));
		return;
	}

	UpdateStatus(TEXT("Joining invited session..."));
	Sessions->ClearOnJoinSessionCompleteDelegates(this);
	Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMP_GameInstance::OnJoinSessionComplete));
	Sessions->JoinSession(ControllerId, NAME_GameSession, InviteResult);
}

void UMP_GameInstance::DestroyCurrentSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Sessions->ClearOnDestroySessionCompleteDelegates(this);
		Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMP_GameInstance::OnDestroySessionComplete));
		Sessions->DestroySession(NAME_GameSession);
		UpdateStatus(TEXT("Destroying session..."));
	}
}

void UMP_GameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegates(this);
	}

	UpdateStatus(FString::Printf(TEXT("Session '%s' destroyed (ok=%d)"), *SessionName.ToString(), bWasSuccessful));

	if (bPendingHostAfterDestroy)
	{
		bPendingHostAfterDestroy = false;
		bPendingJoinAfterDestroy = false;
		HostSession();
	}
	else if (bPendingJoinAfterDestroy)
	{
		bPendingJoinAfterDestroy = false;
		bPendingHostAfterDestroy = false;
		if (Sessions.IsValid())
		{
			Sessions->ClearOnJoinSessionCompleteDelegates(this);
			Sessions->AddOnJoinSessionCompleteDelegate_Handle(
				FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMP_GameInstance::OnJoinSessionComplete));
			Sessions->JoinSession(0, NAME_GameSession, PendingInviteResult);
		}
	}
}

void UMP_GameInstance::HandleReturnToMenu()
{
	DestroyCurrentSession();

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			PC->ClientTravel(MenuMapPath, TRAVEL_Absolute);
		}
	}
}

void UMP_GameInstance::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	FString FailureTypeStr;
	switch (FailureType)
	{
	case ENetworkFailure::NetDriverAlreadyExists: FailureTypeStr = TEXT("NetDriverAlreadyExists"); break;
	case ENetworkFailure::NetDriverCreateFailure: FailureTypeStr = TEXT("NetDriverCreateFailure"); break;
	case ENetworkFailure::NetDriverListenFailure: FailureTypeStr = TEXT("NetDriverListenFailure"); break;
	case ENetworkFailure::ConnectionLost: FailureTypeStr = TEXT("ConnectionLost"); break;
	case ENetworkFailure::ConnectionTimeout: FailureTypeStr = TEXT("ConnectionTimeout"); break;
	case ENetworkFailure::FailureReceived: FailureTypeStr = TEXT("FailureReceived"); break;
	case ENetworkFailure::OutdatedClient: FailureTypeStr = TEXT("OutdatedClient"); break;
	case ENetworkFailure::OutdatedServer: FailureTypeStr = TEXT("OutdatedServer"); break;
	case ENetworkFailure::PendingConnectionFailure: FailureTypeStr = TEXT("PendingConnectionFailure"); break;
	default: FailureTypeStr = TEXT("Unknown"); break;
	}

	UpdateStatus(FString::Printf(TEXT("NETWORK ERROR: %s - %s"), *FailureTypeStr, *ErrorString));
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

