#include "Player/MP_GamePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Voice/ProximityVoiceComponent.h"
#include "UI/HUD/TN_CoopFlowHUDWidget.h"
#include "UI/HUD/TN_RadialWheelWidgetBase.h"
#include "UI/HUD/TN_EmoteWheelDataAsset.h"
#include "UI/HUD/TN_QuickChatWheelDataAsset.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_MatchFlowTypes.h"
#include "Player/TortugaCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"

AMP_GamePlayerController::AMP_GamePlayerController()
{
	// Widget classes are assigned via EditDefaultsOnly in a BP derived class (e.g. BP_GamePlayerController).
	// No hardcoded defaults — set CoopFlowWidgetClass, VoiceIndicatorWidgetClass and CosmeticsWidgetClass
	// in the BP CDO so they can live at any content path.
	OpenEmoteWheelAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_OpenEmoteWheel.IA_OpenEmoteWheel")));
	OpenQuickChatWheelAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_OpenChatWheel.IA_OpenChatWheel")));
	RadialNavigateAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_RadialNavigate.IA_RadialNavigate")));
}

void AMP_GamePlayerController::BeginPlay()
{
	Super::BeginPlay();


	ApplyGameplayInputMode();

	if (IsLocalController() && GetPawn())
	{
		CreateVoiceHUD();
	}

	if (IsLocalController())
	{
		CacheRadialInputAssets();
		CreateCoopFlowHUD();
		CreatePlayerHUD();
		CreateRadialWidgets();
		SyncCosmeticsToServer();
	}
}

void AMP_GamePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AMP_GamePlayerController::SpectateNextPlayer);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AMP_GamePlayerController::SpectatePreviousPlayer);
		InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &AMP_GamePlayerController::SpectateNextPlayer);
		InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &AMP_GamePlayerController::SpectatePreviousPlayer);
	}

	CacheRadialInputAssets();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (LoadedOpenEmoteWheelAction)
		{
			EnhancedInput->BindAction(LoadedOpenEmoteWheelAction, ETriggerEvent::Started, this, &AMP_GamePlayerController::OnOpenEmoteWheelStarted);
			EnhancedInput->BindAction(LoadedOpenEmoteWheelAction, ETriggerEvent::Completed, this, &AMP_GamePlayerController::OnOpenEmoteWheelReleased);
			EnhancedInput->BindAction(LoadedOpenEmoteWheelAction, ETriggerEvent::Canceled, this, &AMP_GamePlayerController::OnOpenEmoteWheelReleased);
		}

		if (LoadedOpenQuickChatWheelAction)
		{
			EnhancedInput->BindAction(LoadedOpenQuickChatWheelAction, ETriggerEvent::Started, this, &AMP_GamePlayerController::OnOpenQuickChatWheelStarted);
			EnhancedInput->BindAction(LoadedOpenQuickChatWheelAction, ETriggerEvent::Completed, this, &AMP_GamePlayerController::OnOpenQuickChatWheelReleased);
			EnhancedInput->BindAction(LoadedOpenQuickChatWheelAction, ETriggerEvent::Canceled, this, &AMP_GamePlayerController::OnOpenQuickChatWheelReleased);
		}

		if (LoadedRadialNavigateAction)
		{
			EnhancedInput->BindAction(LoadedRadialNavigateAction, ETriggerEvent::Triggered, this, &AMP_GamePlayerController::OnRadialNavigateTriggered);
			EnhancedInput->BindAction(LoadedRadialNavigateAction, ETriggerEvent::Completed, this, &AMP_GamePlayerController::OnRadialNavigateCompleted);
			EnhancedInput->BindAction(LoadedRadialNavigateAction, ETriggerEvent::Canceled, this, &AMP_GamePlayerController::OnRadialNavigateCompleted);
		}
	}
}

void AMP_GamePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ApplyGameplayInputMode();

	if (!InPawn)
	{
		return;
	}

	UProximityVoiceComponent* ExistingVoice = InPawn->FindComponentByClass<UProximityVoiceComponent>();
	if (!ExistingVoice)
	{
		UProximityVoiceComponent* VoiceComp = NewObject<UProximityVoiceComponent>(InPawn, TEXT("ProximityVoice"));
		if (VoiceComp)
		{
			VoiceComp->RegisterComponent();
		}
	}

	if (IsLocalController())
	{
		CreateVoiceHUD();
		CreateCoopFlowHUD();
		CreatePlayerHUD();
		CreateRadialWidgets();
		SyncCosmeticsToServer();
	}
}

void AMP_GamePlayerController::ApplyGameplayInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);

	if (GEngine && GEngine->GameViewport)
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

void AMP_GamePlayerController::ApplyRadialInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
	SetIgnoreLookInput(true);
}

void AMP_GamePlayerController::RestorePostRadialInputMode()
{
	ApplyGameplayInputMode();
}

void AMP_GamePlayerController::ForceRestoreInput()
{
	ResetIgnoreInputFlags();
	ApplyGameplayInputMode();
}

void AMP_GamePlayerController::ClientReceiveVoice_Implementation(const TArray<uint8>& CompressedData, int32 SenderSampleRate, AActor* SpeakerActor)
{
	if (!SpeakerActor)
	{
		return;
	}

	if (UProximityVoiceComponent* VoiceComp = SpeakerActor->FindComponentByClass<UProximityVoiceComponent>())
	{
		VoiceComp->PlayRemoteVoice(CompressedData, SenderSampleRate);
	}
}

void AMP_GamePlayerController::ClientRestorePlayerInput_Implementation()
{
	// ResetIgnoreInputFlags() establece IgnoreMoveInput = 0 e IgnoreLookInput = 0.
	// Esto limpia cualquier contador incremental dejado por ChangeState(Spectating)
	// / StartSpectatingOnly, que incrementa IgnoreMoveInput y no lo decrementa
	// cuando el servidor llama Possess + ClientRestart.
	ForceRestoreInput();

	UE_LOG(LogTemp, Log, TEXT("[PC] ClientRestorePlayerInput: input flags reset, gameplay mode restored."));
}

void AMP_GamePlayerController::CacheRadialInputAssets()
{
	if (!LoadedOpenEmoteWheelAction && !OpenEmoteWheelAction.IsNull())
	{
		LoadedOpenEmoteWheelAction = OpenEmoteWheelAction.LoadSynchronous();
	}

	if (!LoadedOpenQuickChatWheelAction && !OpenQuickChatWheelAction.IsNull())
	{
		LoadedOpenQuickChatWheelAction = OpenQuickChatWheelAction.LoadSynchronous();
	}

	if (!LoadedRadialNavigateAction && !RadialNavigateAction.IsNull())
	{
		LoadedRadialNavigateAction = RadialNavigateAction.LoadSynchronous();
	}
}

void AMP_GamePlayerController::EnterSpectateMode()
{
	ChangeState(NAME_Spectating);
	StartSpectatingOnly();
	SpectateNextPlayer();
}

void AMP_GamePlayerController::SpectateNextPlayer()
{
	SpectateByDirection(1);
}

void AMP_GamePlayerController::SpectatePreviousPlayer()
{
	SpectateByDirection(-1);
}

void AMP_GamePlayerController::SpectateByDirection(int32 Direction)
{
	if (Direction == 0 || !GetWorld() || !PlayerState)
	{
		return;
	}

	// Solo permitir espectear si el jugador local está muerto/eliminado.
	// Evita que la rueda del ratón cambie la cámara mientras se está vivo.
	const ATN_CoopPlayerState* LocalPS = GetPlayerState<ATN_CoopPlayerState>();
	if (!LocalPS || (LocalPS->bIsAlive && !LocalPS->bIsEliminated))
	{
		return;
	}

	TArray<APlayerState*> Candidates;
	for (APlayerState* PS : GetWorld()->GetGameState()->PlayerArray)
	{
		ATN_CoopPlayerState* CoopPS = Cast<ATN_CoopPlayerState>(PS);
		// Skip self
		if (!CoopPS || CoopPS == Cast<ATN_CoopPlayerState>(PlayerState))
		{
			continue;
		}
		// Skip players without a live pawn
		if (!CoopPS->GetPawn())
		{
			continue;
		}
		// Skip eliminated/dead players (their pawn stays as corpse but shouldn't be spectated)
		if (CoopPS->bIsEliminated || !CoopPS->bIsAlive)
		{
			continue;
		}
		Candidates.Add(CoopPS);
	}

	if (Candidates.Num() == 0)
	{
		return;
	}

	Candidates.Sort([](const APlayerState& A, const APlayerState& B)
	{
		return A.GetPlayerId() < B.GetPlayerId();
	});

	int32 CurrentIndex = INDEX_NONE;
	AActor* CurrentViewTarget = GetViewTarget();
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		if (Candidates[i]->GetPawn() == CurrentViewTarget)
		{
			CurrentIndex = i;
			break;
		}
	}

	int32 NextIndex = 0;
	if (CurrentIndex != INDEX_NONE)
	{
		NextIndex = (CurrentIndex + Direction + Candidates.Num()) % Candidates.Num();
	}
	else if (Direction < 0)
	{
		NextIndex = Candidates.Num() - 1;
	}

	SetViewTargetWithBlend(Candidates[NextIndex]->GetPawn(), 0.25f);
}

void AMP_GamePlayerController::RefreshHUDAfterPossession()
{
	if (!IsLocalController())
	{
		return;
	}

	// Re-añadir todos los widgets al viewport.
	// Necesario después de seamless travel: UWorld::CleanupWorld elimina todos los
	// widgets del viewport, pero el PC persiste y los widgets siguen vivos en memoria.
	// Sin esto, los widgets existen (pointer no nulo) pero no son visibles.
	CreateVoiceHUD();
	CreateCoopFlowHUD();
	CreatePlayerHUD();
	CreateRadialWidgets();
}

void AMP_GamePlayerController::CreateVoiceHUD()
{
	if (!IsLocalController() || !VoiceIndicatorWidgetClass)
	{
		return;
	}

	// Crear el widget solo si no existe.
	if (!VoiceIndicatorWidget)
	{
		VoiceIndicatorWidget = CreateWidget<UUserWidget>(this, VoiceIndicatorWidgetClass);
	}

	// Re-añadir al viewport si fue eliminado durante seamless travel.
	// AddToViewport es idempotente: no-op si el widget ya está en el viewport.
	if (VoiceIndicatorWidget && !VoiceIndicatorWidget->IsInViewport())
	{
		VoiceIndicatorWidget->AddToViewport(10);
	}
}

void AMP_GamePlayerController::CreateCoopFlowHUD()
{
	if (!IsLocalController())
	{
		return;
	}

	// Crear el widget solo si no existe.
	if (!CoopFlowWidget)
	{
		UClass* WidgetClass = CoopFlowWidgetClass
			? CoopFlowWidgetClass.Get()
			: UTN_CoopFlowHUDWidget::StaticClass();

		if (!CoopFlowWidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HUD] CoopFlowWidgetClass no asignado en %s. Asignalo en el BP derivado del PlayerController. Usando clase C++ como fallback."), *GetNameSafe(this));
		}

		CoopFlowWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	}

	// Re-añadir al viewport si fue eliminado durante seamless travel.
	if (CoopFlowWidget && !CoopFlowWidget->IsInViewport())
	{
		CoopFlowWidget->AddToViewport(5);
	}
}

void AMP_GamePlayerController::CreatePlayerHUD()
{
	if (!IsLocalController())
	{
		return;
	}

	// Crear el widget solo si no existe.
	if (!PlayerHUDWidget)
	{
		if (!PlayerHUDWidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HUD] PlayerHUDWidgetClass no asignado en %s. Asignalo en BP_GamePlayerController → Class Defaults."), *GetNameSafe(this));
			return;
		}

		PlayerHUDWidget = CreateWidget<UUserWidget>(this, PlayerHUDWidgetClass);
	}

	// Re-añadir al viewport si fue eliminado durante seamless travel.
	// Tras travel, OnPossess llama a esta función. El widget existe (pointer no nulo)
	// pero fue eliminado del viewport por UWorld::CleanupWorld → RemoveAllViewportWidgets.
	if (PlayerHUDWidget && !PlayerHUDWidget->IsInViewport())
	{
		PlayerHUDWidget->AddToViewport(4);
		UE_LOG(LogTemp, Log, TEXT("[HUD] PlayerHUDWidget re-añadido al viewport (tras seamless travel)"));
	}
}

void AMP_GamePlayerController::CreateRadialWidgets()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!EmoteWheelWidget && EmoteWheelWidgetClass)
	{
		EmoteWheelWidget = CreateWidget<UTN_RadialWheelWidgetBase>(this, EmoteWheelWidgetClass);
	}
	if (EmoteWheelWidget && !EmoteWheelWidget->IsInViewport())
	{
		EmoteWheelWidget->AddToViewport(30);
		EmoteWheelWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!QuickChatWheelWidget && QuickChatWheelWidgetClass)
	{
		QuickChatWheelWidget = CreateWidget<UTN_RadialWheelWidgetBase>(this, QuickChatWheelWidgetClass);
	}
	if (QuickChatWheelWidget && !QuickChatWheelWidget->IsInViewport())
	{
		QuickChatWheelWidget->AddToViewport(31);
		QuickChatWheelWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AMP_GamePlayerController::OnOpenEmoteWheelStarted()
{
	OpenRadialWheel(ETN_RadialWheelType::Emote);
}

void AMP_GamePlayerController::OnOpenEmoteWheelReleased()
{
	if (ActiveWheelType == ETN_RadialWheelType::Emote)
	{
		CloseRadialWheel(true);
	}
}

void AMP_GamePlayerController::OnOpenQuickChatWheelStarted()
{
	OpenRadialWheel(ETN_RadialWheelType::QuickChat);
}

void AMP_GamePlayerController::OnOpenQuickChatWheelReleased()
{
	if (ActiveWheelType == ETN_RadialWheelType::QuickChat)
	{
		CloseRadialWheel(true);
	}
}

void AMP_GamePlayerController::OnRadialNavigateTriggered(const FInputActionValue& Value)
{
	CachedStickVector = Value.Get<FVector2D>().GetClampedToMaxSize(1.f);
	LastStickInputRealTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
}

void AMP_GamePlayerController::OnRadialNavigateCompleted(const FInputActionValue& Value)
{
	CachedStickVector = FVector2D::ZeroVector;
}

UTN_RadialWheelWidgetBase* AMP_GamePlayerController::GetActiveWheelWidget() const
{
	switch (ActiveWheelType)
	{
	case ETN_RadialWheelType::Emote:
		return EmoteWheelWidget;
	case ETN_RadialWheelType::QuickChat:
		return QuickChatWheelWidget;
	default:
		return nullptr;
	}
}

void AMP_GamePlayerController::OpenRadialWheel(ETN_RadialWheelType WheelType)
{
	if (!IsLocalController() || ActiveWheelType != ETN_RadialWheelType::None)
	{
		return;
	}

	CreateRadialWidgets();

	UTN_RadialWheelWidgetBase* Widget = nullptr;
	TArray<FTN_RadialWheelEntryView> Entries;

	if (WheelType == ETN_RadialWheelType::Emote)
	{
		Widget = EmoteWheelWidget;
		if (EmoteWheelDataAsset)
		{
			Entries = EmoteWheelDataAsset->BuildWheelEntries();
		}
	}
	else if (WheelType == ETN_RadialWheelType::QuickChat)
	{
		Widget = QuickChatWheelWidget;
		if (QuickChatWheelDataAsset)
		{
			Entries = QuickChatWheelDataAsset->BuildWheelEntries();
		}
	}

	if (!Widget || Entries.Num() == 0)
	{
		return;
	}

	ActiveWheelType = WheelType;
	Widget->SetEntries(Entries);
	Widget->SetVisibility(ESlateVisibility::Visible);

	float MouseX = 0.f;
	float MouseY = 0.f;
	bHadMousePositionBeforeWheel = GetMousePosition(MouseX, MouseY);
	if (bHadMousePositionBeforeWheel)
	{
		CachedMousePositionBeforeWheel = FVector2D(MouseX, MouseY);
	}

	int32 ViewX = 0;
	int32 ViewY = 0;
	GetViewportSize(ViewX, ViewY);
	SetMouseLocation(ViewX / 2, ViewY / 2);

	CachedStickVector = FVector2D::ZeroVector;
	ApplyRadialInputMode();

	if (GetWorld())
	{
		GetWorldTimerManager().SetTimer(RadialWheelUpdateTimerHandle, this, &AMP_GamePlayerController::UpdateRadialWheelInput, 1.f / 60.f, true);
	}
}

void AMP_GamePlayerController::CloseRadialWheel(bool bConfirmSelection)
{
	UTN_RadialWheelWidgetBase* Widget = GetActiveWheelWidget();
	if (!Widget)
	{
		ActiveWheelType = ETN_RadialWheelType::None;
		return;
	}

	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(RadialWheelUpdateTimerHandle);
	}

	if (bConfirmSelection)
	{
		FTN_RadialWheelEntryView SelectedEntry;
		if (Widget->TryConfirmSelection(SelectedEntry))
		{
			if (ActiveWheelType == ETN_RadialWheelType::Emote)
			{
				RequestPlayEmoteById(SelectedEntry.EntryId);
			}
			else if (ActiveWheelType == ETN_RadialWheelType::QuickChat)
			{
				SendQuickChat(SelectedEntry.EntryId);
			}
		}
	}

	Widget->ClearSelection();
	Widget->SetVisibility(ESlateVisibility::Collapsed);
	ActiveWheelType = ETN_RadialWheelType::None;

	if (bHadMousePositionBeforeWheel)
	{
		SetMouseLocation(FMath::RoundToInt(CachedMousePositionBeforeWheel.X), FMath::RoundToInt(CachedMousePositionBeforeWheel.Y));
	}

	RestorePostRadialInputMode();
}

FVector2D AMP_GamePlayerController::ComputeMouseWheelVector() const
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return FVector2D::ZeroVector;
	}

	int32 ViewX = 0;
	int32 ViewY = 0;
	GetViewportSize(ViewX, ViewY);

	const FVector2D Center(ViewX * 0.5f, ViewY * 0.5f);
	const FVector2D Delta(MouseX - Center.X, MouseY - Center.Y);
	const float Radius = FMath::Max(1.f, FMath::Min(ViewX, ViewY) * 0.25f);
	return FVector2D(Delta.X / Radius, -Delta.Y / Radius).GetClampedToMaxSize(1.f);
}

FVector2D AMP_GamePlayerController::ResolveCurrentWheelVector() const
{
	const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	const bool bUseStick = CachedStickVector.SizeSquared() > 0.04f && (Now - LastStickInputRealTime) <= 0.2f;
	return bUseStick ? CachedStickVector : ComputeMouseWheelVector();
}

void AMP_GamePlayerController::UpdateRadialWheelInput()
{
	if (UTN_RadialWheelWidgetBase* Widget = GetActiveWheelWidget())
	{
		Widget->UpdateInputVector(ResolveCurrentWheelVector());
	}
}

void AMP_GamePlayerController::OpenCosmeticsMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	if (CosmeticsWidget)
	{
		CosmeticsWidget->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	if (!CosmeticsWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI] CosmeticsWidgetClass no asignado en %s."), *GetNameSafe(this));
		return;
	}

	CosmeticsWidget = CreateWidget<UUserWidget>(this, CosmeticsWidgetClass);
	if (!CosmeticsWidget)
	{
		return;
	}

	CosmeticsWidget->AddToViewport(40);

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

bool AMP_GamePlayerController::RequestEquipHelmet(FName HelmetId)
{
	if (HelmetId == NAME_None)
	{
		return false;
	}

	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		if (!GI->EquipHelmet(HelmetId))
		{
			return false;
		}
	}

	ServerSetEquippedHelmet(HelmetId);
	return true;
}

void AMP_GamePlayerController::RequestUnequipHelmet()
{
	// Limpiar localmente el casco equipado en el save
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->EquipHelmet(NAME_None);
	}
	// Enviar al servidor para actualizar PlayerState + notificar a todos
	ServerSetEquippedHelmet(NAME_None);
}

FName AMP_GamePlayerController::OpenHelmetCrate()
{
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		const FName Result = GI->OpenHelmetCrate();
		SyncCosmeticsToServer();
		if (Result != NAME_None)
		{
			ServerSetEquippedHelmet(Result);
		}
		return Result;
	}

	return NAME_None;
}

void AMP_GamePlayerController::ClientOpenCosmeticsMenu_Implementation()
{
	OpenCosmeticsMenu();
}

void AMP_GamePlayerController::ClientNotifyServerTravel_Implementation()
{
	// Marcar que estamos en travel para que OnNetworkFailure active auto-rejoin
	// en vez de destruir la sesión y mostrar error.
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->NotifyClientPendingTravel();
	}
	UE_LOG(LogTemp, Log, TEXT("[PC] ClientNotifyServerTravel received — prepared for reconnection."));
}

void AMP_GamePlayerController::ServerSyncUnlockedHelmets_Implementation(const TArray<FName>& UnlockedHelmetIds)
{
	ServerUnlockedHelmets.Reset();
	for (const FName HelmetId : UnlockedHelmetIds)
	{
		if (HelmetId != NAME_None)
		{
			ServerUnlockedHelmets.Add(HelmetId);
		}
	}

	if (ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		if (TNPS->EquippedHelmetId == NAME_None && ServerUnlockedHelmets.Num() > 0)
		{
			for (const FName HelmetId : ServerUnlockedHelmets)
			{
				TNPS->EquippedHelmetId = HelmetId;
				break;
			}
		}
	}
}

void AMP_GamePlayerController::ServerSetEquippedHelmet_Implementation(FName HelmetId)
{
	// NAME_None = desequipar (siempre permitido).
	// Otro ID: debe estar en el conjunto de cascos desbloqueados del jugador.
	if (HelmetId != NAME_None && !ServerUnlockedHelmets.Contains(HelmetId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] ServerSetEquippedHelmet: '%s' no desbloqueado para %s"),
			*HelmetId.ToString(), *GetNameSafe(this));
		return;
	}

	if (ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->EquippedHelmetId = HelmetId;
		TNPS->ForceNetUpdate(); // Forzar replicación inmediata del helmet a todos los clientes

		// El listen-server (authority) no recibe OnRep → aplica el mesh directamente.
		if (ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(GetPawn()))
		{
			TurtleChar->UpdateHelmetMesh(HelmetId);
		}
	}
}

void AMP_GamePlayerController::ServerSetEquippedSkin_Implementation(FName SkinId)
{
	if (ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->EquippedSkinId = SkinId;
		TNPS->ForceNetUpdate();

		// El listen-server (authority) no recibe OnRep → aplica visualmente de forma directa.
		if (ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(GetPawn()))
		{
			TurtleChar->UpdateSkinVisual(SkinId);
		}
	}
}

void AMP_GamePlayerController::ClientSaveSkin_Implementation(FName SkinId)
{
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->EquipSkin(SkinId);
	}
}

void AMP_GamePlayerController::ClientSaveHelmet_Implementation(FName HelmetId)
{
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->ForceEquipHelmet(HelmetId);
	}
}

void AMP_GamePlayerController::NotifySkinEquipped(FName SkinId)
{
	ClientSaveSkin(SkinId);
}

void AMP_GamePlayerController::NotifyHelmetEquipped(FName HelmetId)
{
	ClientSaveHelmet(HelmetId);
}

void AMP_GamePlayerController::SyncCosmeticsToServer()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		ServerSyncUnlockedHelmets(GI->GetUnlockedHelmetIds());
		const FName EquippedHelmetId = GI->GetEquippedHelmetId();
		if (EquippedHelmetId != NAME_None)
		{
			ServerSetEquippedHelmet(EquippedHelmetId);
		}
		// Sincronizar skin (NAME_None = sin skin, siempre enviar)
		ServerSetEquippedSkin(GI->GetEquippedSkinId());
	}
}

// ── Quick Chat ────────────────────────────────────────────────────────────────

void AMP_GamePlayerController::SendQuickChat(uint8 MessageID)
{
	if (!IsLocalController()) { return; }
	ServerSendQuickChat(MessageID);
}

void AMP_GamePlayerController::RequestPlayEmoteById(uint8 EmoteID)
{
	if (!IsLocalController())
	{
		return;
	}

	if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(GetPawn()))
	{
		Turtle->RequestWheelEmote(EmoteID);
	}
}

bool AMP_GamePlayerController::ResolveQuickChatDisplayData(const FTN_QuickChatEntry& Entry, FText& OutSenderName, FText& OutMessageText, UTexture2D*& OutIcon) const
{
	OutSenderName = FText::GetEmpty();
	OutMessageText = FText::GetEmpty();
	OutIcon = nullptr;

	const ATN_CoopGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATN_CoopGameState>() : nullptr;
	if (GS)
	{
		OutSenderName = GS->ResolveQuickChatSenderName(Entry.SenderPlayerId);
	}

	if (!QuickChatWheelDataAsset)
	{
		return false;
	}

	if (const FTN_QuickChatWheelEntry* ChatEntry = QuickChatWheelDataAsset->FindEntryById(Entry.MessageID))
	{
		OutMessageText = ChatEntry->Text;
		OutIcon = ChatEntry->Icon;
		return true;
	}

	return false;
}

void AMP_GamePlayerController::ServerSendQuickChat_Implementation(uint8 MessageID)
{
	UWorld* World = GetWorld();
	if (!World || !QuickChatWheelDataAsset) { return; }

	const FTN_QuickChatWheelEntry* ChatEntry = QuickChatWheelDataAsset->FindEntryById(MessageID);
	if (!ChatEntry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QuickChat] Invalid MessageID %d from %s"), static_cast<int32>(MessageID), *GetNameSafe(this));
		return;
	}

	ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>();
	if (!TNPS)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float Cooldown = ChatEntry->CooldownOverride > 0.f ? ChatEntry->CooldownOverride : QuickChatCooldownSeconds;
	if (!TNPS->CanServerSendQuickChat(Now, Cooldown))
	{
		return;
	}
	TNPS->MarkServerQuickChatSent(Now);

	ATN_CoopGameState* GS = World->GetGameState<ATN_CoopGameState>();
	if (!GS) { return; }

	const int32 SenderId = PlayerState ? PlayerState->GetPlayerId() : 0;
	GS->AddQuickChatEntry(SenderId, MessageID, Now);
}
