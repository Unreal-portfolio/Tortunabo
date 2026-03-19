#include "Multiplayer/MP_GameInstance.h"
#include "OnlineSubsystem.h"
#include "Online.h"
#include "OnlineSessionSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "UObject/UObjectGlobals.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Multiplayer/TN_CosmeticSaveGame.h"
#include "UI/HUD/TN_LoadingScreenWidget.h"
#include "Voice/ProximityVoiceComponent.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UMP_GameInstance::UMP_GameInstance()
{
	// LoadingScreenWidgetClass is assigned in a BP derived GameInstance.
	// If not set, ShowLoadingScreen falls back to the C++ base widget.
	DefaultUnlockedHelmets = { FName(TEXT("Helmet_Default")) };
	HelmetCrateTable =
	{
		{ FName(TEXT("Helmet_Default")), 40.0f },
		{ FName(TEXT("Helmet_Bronze")), 30.0f },
		{ FName(TEXT("Helmet_Silver")), 20.0f },
		{ FName(TEXT("Helmet_Gold")), 9.0f },
		{ FName(TEXT("Helmet_Mythic")), 1.0f }
	};
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
		// Remove UEngine's default network failure handler and install our own.
		// UEngine::HandleNetworkFailure calls SetClientTravel("?closed") for ALL driver
		// failures, which navigates back to the menu immediately. We need to suppress
		// that travel during Listen() retries (Steam socket collision during ServerTravel).
		// By replacing the handler, we replicate UEngine's disconnect logic for all cases
		// EXCEPT the ones we're retrying.
		GEngine->OnNetworkFailure().RemoveAll(GEngine);
		GEngine->OnNetworkFailure().AddUObject(this, &UMP_GameInstance::OnNetworkFailure);
	}

	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UMP_GameInstance::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMP_GameInstance::HandlePostLoadMap);
	LoadCosmeticProfile();
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
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	SaveCosmeticProfile();

	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && InviteAcceptedDelegateHandle.IsValid())
	{
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedDelegateHandle);
		InviteAcceptedDelegateHandle.Reset();
	}

	Super::Shutdown();
}

void UMP_GameInstance::ShowLoadingScreen(const FString& Reason)
{
	if (bIsLoadingScreenVisible)
	{
		RefreshLoadingText(Reason);
		return;
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	UClass* WidgetClass = LoadingScreenWidgetClass
		? LoadingScreenWidgetClass.Get()
		: UTN_LoadingScreenWidget::StaticClass();

	LoadingScreenWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!LoadingScreenWidget)
	{
		return;
	}

	LoadingScreenWidget->AddToViewport(100000);
	bIsLoadingScreenVisible = true;
	RefreshLoadingText(Reason);
}

void UMP_GameInstance::HideLoadingScreen()
{
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
	}

	bIsLoadingScreenVisible = false;
}

TArray<FName> UMP_GameInstance::GetUnlockedHelmetIds() const
{
	return CosmeticProfile ? CosmeticProfile->UnlockedHelmetIds : TArray<FName>();
}

bool UMP_GameInstance::IsHelmetUnlocked(FName HelmetId) const
{
	return CosmeticProfile && HelmetId != NAME_None && CosmeticProfile->UnlockedHelmetIds.Contains(HelmetId);
}

bool UMP_GameInstance::UnlockHelmet(FName HelmetId)
{
	if (!CosmeticProfile || HelmetId == NAME_None)
	{
		return false;
	}

	if (CosmeticProfile->UnlockedHelmetIds.Contains(HelmetId))
	{
		return true;
	}

	CosmeticProfile->UnlockedHelmetIds.Add(HelmetId);
	SaveCosmeticProfile();
	return true;
}

bool UMP_GameInstance::EquipHelmet(FName HelmetId)
{
	if (!IsHelmetUnlocked(HelmetId))
	{
		return false;
	}

	CosmeticProfile->EquippedHelmetId = HelmetId;
	SaveCosmeticProfile();
	return true;
}

FName UMP_GameInstance::GetEquippedHelmetId() const
{
	return CosmeticProfile ? CosmeticProfile->EquippedHelmetId : NAME_None;
}

FName UMP_GameInstance::OpenHelmetCrate()
{
	if (HelmetCrateTable.Num() == 0)
	{
		return NAME_None;
	}

	float TotalWeight = 0.0f;
	for (const FTN_HelmetCrateEntry& Entry : HelmetCrateTable)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return NAME_None;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FTN_HelmetCrateEntry& Entry : HelmetCrateTable)
	{
		Roll -= FMath::Max(0.0f, Entry.Weight);
		if (Roll <= 0.0f && Entry.HelmetId != NAME_None)
		{
			UnlockHelmet(Entry.HelmetId);
			return Entry.HelmetId;
		}
	}

	return NAME_None;
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
	ShowLoadingScreen(TEXT("Creando lobby..."));

	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UpdateStatus(TEXT("ERROR: No session interface"));
		HideLoadingScreen();
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
		HideLoadingScreen();
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
	ShowLoadingScreen(TEXT("Buscando lobbies..."));

	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UpdateStatus(TEXT("ERROR: No session interface"));
		HideLoadingScreen();
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
		HideLoadingScreen();
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
		HideLoadingScreen();
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
		HideLoadingScreen();
		return;
	}

	FString ConnectInfo;
	if (Sessions.IsValid() && Sessions->GetResolvedConnectString(SessionName, ConnectInfo) && !ConnectInfo.IsEmpty())
	{
		ShowLoadingScreen(TEXT("Conectando a la partida..."));
		APlayerController* PC = GetFirstLocalPlayerController();
		if (PC)
		{
			PC->ClientTravel(ConnectInfo, TRAVEL_Absolute);
		}
	}
	else
	{
		UpdateStatus(TEXT("ERROR: Could not resolve connect string"));
		HideLoadingScreen();
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
	ShowLoadingScreen(TEXT("Volviendo al menu..."));

	// Stop all audio capture before travel to prevent WASAPI crash.
	UProximityVoiceComponent::ShutdownAllCapture(GetWorld());

	DestroyCurrentSession();

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->ClientTravel(MenuMapPath, TRAVEL_Absolute);
	}
}

void UMP_GameInstance::HandlePreLoadMap(const FString& MapName)
{
	// Marcar que estamos en una transición de nivel.
	// OnNetworkFailure usa este flag para no destruir la sesión si el error
	// es transitorio (ej. colisión de socket Steam durante ServerTravel).
	bIsPendingTravel = true;

	// Safety net: stop all audio capture streams before any map transition.
	// Individual GameModes already call ShutdownAllCapture explicitly, but this
	// catches any travel path we might have missed (invites, network failures, etc.).
	UProximityVoiceComponent::ShutdownAllCapture(GetWorld());

	ShowLoadingScreen(FString::Printf(TEXT("Cargando mapa: %s"), *MapName));
}

void UMP_GameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// Si todavía estamos reintentando Listen() (colisión de socket Steam),
	// NO resetear los flags de travel. El retry necesita bIsPendingTravel == true
	// para que OnNetworkFailure/HandleNetworkError no destruyan el mundo.
	if (TravelListenRetryCount > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[MP] HandlePostLoadMap: Mapa cargado pero Listen() pendiente de retry (%d/%d). "
			     "Manteniendo bIsPendingTravel=true."),
			TravelListenRetryCount, MaxTravelListenRetries);
		// NO ocultar loading screen — el retry mostrará el estado actualizado.
		return;
	}

	bIsPendingTravel = false;
	TravelListenRetryCount = 0;
	HideLoadingScreen();
}

void UMP_GameInstance::LoadCosmeticProfile()
{
	const FString SlotName = BuildCosmeticSaveSlot();
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		CosmeticProfile = Cast<UTN_CosmeticSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}

	if (!CosmeticProfile)
	{
		CosmeticProfile = Cast<UTN_CosmeticSaveGame>(UGameplayStatics::CreateSaveGameObject(UTN_CosmeticSaveGame::StaticClass()));
	}

	if (!CosmeticProfile)
	{
		return;
	}

	for (const FName DefaultHelmet : DefaultUnlockedHelmets)
	{
		if (DefaultHelmet != NAME_None)
		{
			CosmeticProfile->UnlockedHelmetIds.AddUnique(DefaultHelmet);
		}
	}

	if (CosmeticProfile->EquippedHelmetId == NAME_None && CosmeticProfile->UnlockedHelmetIds.Num() > 0)
	{
		CosmeticProfile->EquippedHelmetId = CosmeticProfile->UnlockedHelmetIds[0];
	}
}

void UMP_GameInstance::SaveCosmeticProfile() const
{
	if (!CosmeticProfile)
	{
		return;
	}

	UGameplayStatics::SaveGameToSlot(CosmeticProfile, BuildCosmeticSaveSlot(), 0);
}

FString UMP_GameInstance::BuildCosmeticSaveSlot() const
{
	FString Suffix = TEXT("Local");
	if (const ULocalPlayer* LP = GetFirstGamePlayer())
	{
		if (!LP->GetNickname().IsEmpty())
		{
			Suffix = LP->GetNickname();
		}
		else
		{
			Suffix = FString::Printf(TEXT("LocalPlayer_%d"), LP->GetLocalPlayerIndex());
		}
	}

	return FString::Printf(TEXT("%s_%s"), *CosmeticSaveSlotPrefix, *Suffix);
}

void UMP_GameInstance::RefreshLoadingText(const FString& Reason) const
{
	if (!LoadingScreenWidget)
	{
		return;
	}

	if (UTextBlock* Status = Cast<UTextBlock>(LoadingScreenWidget->GetWidgetFromName(TEXT("StatusText"))))
	{
		Status->SetText(FText::FromString(Reason));
		return;
	}

	if (UTN_LoadingScreenWidget* TypedLoading = Cast<UTN_LoadingScreenWidget>(LoadingScreenWidget))
	{
		TypedLoading->SetStatusMessage(FText::FromString(Reason));
	}
}

void UMP_GameInstance::RetryListenAfterTravel(UWorld* World)
{
	if (!World)
	{
		return;
	}

	// Cancelar timer previo si existe
	World->GetTimerManager().ClearTimer(TravelListenRetryTimerHandle);

	TWeakObjectPtr<UWorld> WeakWorld(World);
	TWeakObjectPtr<UMP_GameInstance> WeakThis(this);

	World->GetTimerManager().SetTimer(TravelListenRetryTimerHandle,
		[WeakWorld, WeakThis]()
		{
			UWorld* W = WeakWorld.Get();
			UMP_GameInstance* GI = WeakThis.Get();
			if (!W || !GI) { return; }

			FURL ListenURL;
			ListenURL.AddOption(TEXT("listen"));
			if (W->Listen(ListenURL))
			{
				UE_LOG(LogTemp, Log, TEXT("[MP] ✓ Listen() reintento exitoso tras colisión de socket Steam."));
				GI->UpdateStatus(TEXT("Conexión establecida."));
				GI->bIsPendingTravel = false;
				GI->TravelListenRetryCount = 0;
				GI->HideLoadingScreen();
			}
			else
			{
				// Listen() falló de nuevo — OnNetworkFailure se encargará del siguiente reintento.
				UE_LOG(LogTemp, Warning, TEXT("[MP] Listen() reintento %d falló — esperando siguiente intento..."),
					GI->TravelListenRetryCount);
			}
		},
		TravelListenRetryDelaySec, false);
}

void UMP_GameInstance::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// ──────────────────────────────────────────────────────────────────────
	// NOTA: Este handler REEMPLAZA a UEngine::HandleNetworkFailure (que fue
	// removido del delegate en Init). Somos los ÚNICOS que escuchamos
	// OnNetworkFailure. Eso nos da control total sobre cuándo llamar
	// a HandleDisconnect (que navega a ?closed).
	// ──────────────────────────────────────────────────────────────────────

	FString FailureTypeStr;
	switch (FailureType)
	{
	case ENetworkFailure::NetDriverAlreadyExists:   FailureTypeStr = TEXT("NetDriverAlreadyExists"); break;
	case ENetworkFailure::NetDriverCreateFailure:   FailureTypeStr = TEXT("NetDriverCreateFailure"); break;
	case ENetworkFailure::NetDriverListenFailure:   FailureTypeStr = TEXT("NetDriverListenFailure"); break;
	case ENetworkFailure::ConnectionLost:           FailureTypeStr = TEXT("ConnectionLost"); break;
	case ENetworkFailure::ConnectionTimeout:        FailureTypeStr = TEXT("ConnectionTimeout"); break;
	case ENetworkFailure::FailureReceived:          FailureTypeStr = TEXT("FailureReceived"); break;
	case ENetworkFailure::OutdatedClient:           FailureTypeStr = TEXT("OutdatedClient"); break;
	case ENetworkFailure::OutdatedServer:           FailureTypeStr = TEXT("OutdatedServer"); break;
	case ENetworkFailure::PendingConnectionFailure: FailureTypeStr = TEXT("PendingConnectionFailure"); break;
	case ENetworkFailure::NetChecksumMismatch:      FailureTypeStr = TEXT("NetChecksumMismatch"); break;
	default:                                        FailureTypeStr = TEXT("Unknown"); break;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MP] NETWORK ERROR: %s - %s"), *FailureTypeStr, *ErrorString);

	// ---------- SERVIDOR: errores de socket/driver ----------
	if (FailureType == ENetworkFailure::NetDriverListenFailure ||
		FailureType == ENetworkFailure::NetDriverCreateFailure ||
		FailureType == ENetworkFailure::NetDriverAlreadyExists)
	{
		if (bIsPendingTravel)
		{
			// Error durante transición de nivel (ServerTravel lobby→game).
			// El socket Steam del listen server anterior no se ha liberado aún.
			if (TravelListenRetryCount < MaxTravelListenRetries)
			{
				++TravelListenRetryCount;
				UE_LOG(LogTemp, Warning,
					TEXT("[MP] %s durante transición de nivel — colisión de socket Steam. "
					     "Reintento %d/%d en %.1fs..."),
					*FailureTypeStr, TravelListenRetryCount, MaxTravelListenRetries, TravelListenRetryDelaySec);
				UpdateStatus(FString::Printf(TEXT("Reintentando conexión (%d/%d)..."), TravelListenRetryCount, MaxTravelListenRetries));

				RetryListenAfterTravel(World);

				// NO llamamos a HandleDisconnect → NO se encola SetClientTravel("?closed").
				// El mundo permanece vivo para que el timer de retry pueda disparar.
				return;
			}

			// Agotados los reintentos.
			bIsPendingTravel = false;
			TravelListenRetryCount = 0;
			UpdateStatus(TEXT("ERROR: No se pudo iniciar el servidor tras el viaje. Volviendo al menú..."));
			UE_LOG(LogTemp, Error,
				TEXT("[MP] %s — agotados %d reintentos de Listen(). El socket Steam no se liberó a tiempo. Volviendo al menú."),
				*FailureTypeStr, MaxTravelListenRetries);
			HandleReturnToMenu();
			return;
		}

		// Error al iniciar el listen server desde cero (sesión Steam zombi).
		HideLoadingScreen();
		UpdateStatus(FString::Printf(TEXT("NETWORK ERROR: %s - %s"), *FailureTypeStr, *ErrorString));
		DestroyCurrentSession();
		UE_LOG(LogTemp, Warning,
			TEXT("[MP] %s en inicio de conexión — sesión Steam destruida. "
			     "Reinicia Steam si el error persiste."),
			*FailureTypeStr);
		return;
	}

	// ---------- CLIENTE: checksum mismatch (versiones incompatibles) ----------
	if (FailureType == ENetworkFailure::NetChecksumMismatch)
	{
		HideLoadingScreen();
		UpdateStatus(TEXT("ERROR: Versiones incompatibles con el servidor.\nAsegúrate de que ambos jugadores tienen el mismo build compilado (sin Live Coding activo)."));
		DestroyCurrentSession();
		UE_LOG(LogTemp, Error,
			TEXT("[MP] NetChecksumMismatch — El cliente tiene un build distinto al servidor. "
			     "Recompila sin Live Coding y asegúrate de que todos usan el mismo binario. "
			     "Detalle: %s"),
			*ErrorString);
		// Checksum mismatch en cliente → necesita navegar de vuelta al menú.
		PerformDefaultDisconnect(World, NetDriver, FailureType);
		return;
	}

	// ---------- CLIENTE: otras desconexiones ----------
	if (FailureType == ENetworkFailure::ConnectionLost   ||
		FailureType == ENetworkFailure::ConnectionTimeout ||
		FailureType == ENetworkFailure::FailureReceived   ||
		FailureType == ENetworkFailure::PendingConnectionFailure)
	{
		HideLoadingScreen();
		UpdateStatus(FString::Printf(TEXT("NETWORK ERROR: %s - %s"), *FailureTypeStr, *ErrorString));
		DestroyCurrentSession();
		UE_LOG(LogTemp, Warning,
			TEXT("[MP] %s — Sesión cliente destruida para evitar estado zombie."),
			*FailureTypeStr);
		PerformDefaultDisconnect(World, NetDriver, FailureType);
		return;
	}

	// Resto de errores (OutdatedClient, OutdatedServer, etc.)
	UpdateStatus(FString::Printf(TEXT("NETWORK ERROR: %s - %s"), *FailureTypeStr, *ErrorString));
	PerformDefaultDisconnect(World, NetDriver, FailureType);
}

void UMP_GameInstance::PerformDefaultDisconnect(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType)
{
	// Replica la lógica de UEngine::HandleNetworkFailure para determinar si se
	// debe navegar de vuelta al menú (SetClientTravel "?closed").
	// Este método reemplaza al handler original que removimos del delegate.
	if (!NetDriver || !GEngine)
	{
		return;
	}

	const FName DriverName = NetDriver->NetDriverName;
	if (DriverName != NAME_GameNetDriver && DriverName != NAME_PendingNetDriver)
	{
		return;
	}

	// Si el driver ya fue destruido, no hacer nada.
	if (World && !GEngine->FindNamedNetDriver(World, DriverName))
	{
		return;
	}

	// Determinar si debemos navegar — misma lógica que UEngine::HandleNetworkFailure.
	const ENetMode FailureNetMode = NetDriver->GetNetMode();
	bool bShouldTravel = true;

	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
	case ENetworkFailure::NetGuidMismatch:
	case ENetworkFailure::NetChecksumMismatch:
		// Hosts no navegan cuando un cliente desconecta/tiene problemas.
		bShouldTravel = (FailureNetMode == NM_Client);
		break;
	default:
		break;
	}

	if (bShouldTravel && World)
	{
		GEngine->HandleDisconnect(World, NetDriver);
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

