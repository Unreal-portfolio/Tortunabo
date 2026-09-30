#include "Player/TN_Ghost.h"
#include "Player/TN_GhostEgg.h"
#include "Player/TN_SpectatorGhost.h"
#include "Player/MP_GamePlayerController.h"
#include "TN_GhostInternal.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_Log.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "HAL/IConsoleManager.h"
#include "World/TN_RescuePickup.h"

// Fantasma espectador y vuelta a la vida desde un huevo (contrato: Public/Player/TN_Ghost.h; Docs/Fantasma_Espectador.md).

namespace TNGhostApiDetail
{
	/** Vuelo en U del fantasma hasta el huevo, vibración del huevo con él dentro y, sin vuelo, lo que tarda en entrar. */
	constexpr float FlightSeconds = 1.2f;
	constexpr float VibrateSeconds = 1.f;
	constexpr float NoFlightArriveSeconds = 0.4f;
	/** Semialtura de cápsula si el peón por defecto no la dice (la de los sitios de salida del mapa procedural). */
	constexpr float DefaultHalfHeight = 108.f;
	/** TN.Ghost.Revive: el huevo, a esta distancia delante de la primera tortuga viva. */
	constexpr float EggAheadDistance = 450.f;

	/** Semialtura de la cápsula de la tortuga que va a salir (la del peón por defecto del GameMode). */
	float PawnHalfHeight(AGameModeBase* GameMode, AController* Controller)
	{
		UClass* PawnClass = GameMode ? GameMode->GetDefaultPawnClassForController(Controller) : nullptr;
		const ACharacter* Defaults = PawnClass ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr;
		const UCapsuleComponent* Capsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr;
		return Capsule ? Capsule->GetScaledCapsuleHalfHeight() : DefaultHalfHeight;
	}

	/** PlayerController número Index del servidor (en el orden en que entraron: 0 = anfitrión). */
	APlayerController* PlayerByIndex(UWorld* World, int32 Index)
	{
		int32 Current = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* Candidate = It->Get())
			{
				if (Current == Index)
				{
					return Candidate;
				}
				++Current;
			}
		}
		return nullptr;
	}

	/** TN.Ghost.Revive: huevo en el suelo delante de la primera tortuga viva (de otro jugador; si no hay, de la suya). */
	void DebugRevive(UWorld* World, APlayerController* Target)
	{
		APawn* Reference = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Other = It->Get();
			APawn* OtherPawn = Other ? Other->GetPawn() : nullptr;
			if (!Other || Other == Target || !OtherPawn || OtherPawn->IsHidden())
			{
				continue;
			}
			if (const ATN_CoopPlayerState* OtherPS = Other->GetPlayerState<ATN_CoopPlayerState>())
			{
				if (!OtherPS->IsAliveAndPlaying())
				{
					continue;
				}
			}
			Reference = OtherPawn;
			break;
		}
		if (!Reference)
		{
			Reference = Target->GetPawn();
		}
		FVector Base = Target->GetFocalLocation();
		double Yaw = Target->GetControlRotation().Yaw;
		if (Reference)
		{
			Base = Reference->GetActorLocation() + Reference->GetActorForwardVector().GetSafeNormal2D() * EggAheadDistance;
			Yaw = Reference->GetActorRotation().Yaw;
		}
		else if (const ATN_SpectatorGhost* Ghost = ATN_SpectatorGhost::FindFor(Target))
		{
			Base = Ghost->GetVisualLocation();
		}
		// Al suelo.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TNGhostEggGround), false);
		if (Reference)
		{
			Params.AddIgnoredActor(Reference);
		}
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Base + FVector(0.0, 0.0, 400.0), Base - FVector(0.0, 0.0, 3000.0), ECC_Visibility, Params))
		{
			Base = Hit.ImpactPoint;
		}
		else if (Reference)
		{
			Base.Z = Reference->GetActorLocation().Z - PawnHalfHeight(World->GetAuthGameMode(), Target);
		}
		// Solo para probar: un muerto del cooperativo vuelve vivo (en juego, las reglas son de quien llame a ReviveIntoEgg).
		if (ATN_CoopPlayerState* PS = Target->GetPlayerState<ATN_CoopPlayerState>())
		{
			if (!PS->bIsAlive || PS->bIsEliminated)
			{
				PS->bIsAlive = true;
				PS->bIsEliminated = false;
				PS->bHasFinishedRun = false;
				PS->bIsDBNO = false;
				PS->DBNOBleedoutTimeRemaining = -1.f;
				PS->DeathZoneTimeRemaining = -1.f;
				PS->FinishRank = 0;
				PS->FinishTimeSeconds = -1.f;
			}
		}
		const bool bStarted = TNGhost::ReviveIntoEgg(Target, FTransform(FRotator(0.0, Yaw, 0.0), Base));
		UE_LOG(LogTortunabo, Log, TEXT("[Fantasma] TN.Ghost.Revive %s: %s."), *GetNameSafe(Target->PlayerState),
			bStarted ? TEXT("sale de un huevo") : TEXT("no se puede (¿ya está volviendo?)"));
	}

	/** TN.Ghost.Become: su tortuga se queda quieta y oculta (como al llegar a la meta) y pasa a fantasma espectador. */
	void DebugBecome(APlayerController* Target)
	{
		if (TNGhost::IsGhost(Target))
		{
			return;
		}
		AMP_GamePlayerController* GamePC = Cast<AMP_GamePlayerController>(Target);
		if (!GamePC)
		{
			return;
		}
		if (APawn* Pawn = Target->GetPawn())
		{
			if (ACharacter* Character = Cast<ACharacter>(Pawn))
			{
				if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
				{
					Move->StopMovementImmediately();
					Move->DisableMovement();
				}
			}
			Pawn->DisableInput(Target);
			Pawn->SetActorHiddenInGame(true);
			Pawn->SetActorEnableCollision(false);
		}
		GamePC->EnterSpectateMode();
		UE_LOG(LogTortunabo, Log, TEXT("[Fantasma] TN.Ghost.Become %s."), *GetNameSafe(Target->PlayerState));
	}

#if !UE_BUILD_SHIPPING
	/**
	 * Consola del anfitrión (o de una partida sin red): se hace ya. Un cliente no puede: no hay RPC de pruebas que un
	 * invitado pueda llamar (Plan maestro §4, N-A).
	 */
	void RunFromConsole(TNGhostInternal::EDebugCommand Command, const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Fantasma] TN.Ghost.* solo en la consola del anfitrión."));
			return;
		}
		const int32 Index = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1;
		TNGhostInternal::RunDebugCommand(World, Command, Index);
	}

	void ReviveFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		RunFromConsole(TNGhostInternal::EDebugCommand::Revive, Args, World);
	}

	void BecomeFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		RunFromConsole(TNGhostInternal::EDebugCommand::Become, Args, World);
	}

	FAutoConsoleCommandWithWorldAndArgs GhostReviveCommand(
		TEXT("TN.Ghost.Revive"),
		TEXT("TN.Ghost.Revive [índice de jugador]: vuelve a la vida a ese jugador (0 = anfitrión) en un huevo delante de la primera ")
		TEXT("tortuga viva. Sin índice: el primer fantasma. Solo en la consola del anfitrión."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ReviveFromConsole));

	FAutoConsoleCommandWithWorldAndArgs GhostBecomeCommand(
		TEXT("TN.Ghost.Become"),
		TEXT("TN.Ghost.Become [índice de jugador]: ese jugador (0 = anfitrión, por defecto) pasa a fantasma espectador; su tortuga ")
		TEXT("se queda oculta hasta que vuelva a la vida (para probar en cualquier modo, también en el lobby)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BecomeFromConsole));
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Contrato (TNGhost)
// ─────────────────────────────────────────────────────────────────────────────

TNGhost::FOnHatched& TNGhost::OnHatched()
{
	static FOnHatched Delegate;
	return Delegate;
}

bool TNGhost::IsReviving(const APlayerController* PC)
{
	const ATN_SpectatorGhost* Ghost = ATN_SpectatorGhost::FindFor(PC);
	return Ghost && Ghost->IsReviving();
}

bool TNGhost::IsGhost(const APlayerController* PC)
{
	const ATN_SpectatorGhost* Ghost = ATN_SpectatorGhost::FindFor(PC);
	return Ghost && Ghost->IsActiveGhost();
}

bool TNGhost::ReviveIntoEgg(APlayerController* PC, const FTransform& EggTransform)
{
	using namespace TNGhostApiDetail;
	if (!PC || !PC->HasAuthority() || IsReviving(PC))
	{
		return false;
	}
	UWorld* World = PC->GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return false;
	}

	ATN_SpectatorGhost* Ghost = ATN_SpectatorGhost::FindFor(PC);
	const bool bFly = Ghost && Ghost->IsActiveGhost();
	if (!bFly)
	{
		// Aún tiene tortuga: se le quita sin vuelo. Pasa a fantasma (que no se ve) para llevar el huevo y su pantalla.
		APawn* OldPawn = PC->GetPawn();
		if (AMP_GamePlayerController* GamePC = Cast<AMP_GamePlayerController>(PC))
		{
			GamePC->EnterSpectateMode();
		}
		else
		{
			TNGhostInternal::OnEnterSpectate(PC);
			PC->ChangeState(NAME_Spectating);
			PC->ClientGotoState(NAME_Spectating);
		}
		Ghost = ATN_SpectatorGhost::FindFor(PC);
		if (Ghost && !Ghost->IsActiveGhost())
		{
			Ghost = nullptr;
		}
		if (IsValid(OldPawn) && !OldPawn->IsActorBeingDestroyed())
		{
			if (OldPawn->GetController() == PC)
			{
				PC->UnPossess();
			}
			OldPawn->Destroy();
		}
		if (Ghost)
		{
			Ghost->ForgetLeftBody();
		}
	}
	if (!Ghost)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Fantasma] ReviveIntoEgg: %s no tiene fantasma."), *GetNameSafe(PC));
		return false;
	}

	// El huevo, de pie en el suelo y mirando hacia donde saldrá la tortuga (solo el giro de su X).
	const FTransform EggXf(FRotator(0.0, EggTransform.Rotator().Yaw, 0.0), EggTransform.GetLocation());
	ATN_GhostEgg* Egg = World->SpawnActorDeferred<ATN_GhostEgg>(ATN_GhostEgg::StaticClass(), EggXf, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Egg)
	{
		return false;
	}
	const float Now = ATN_SpectatorGhost::ServerNow(World);
	const float Arrive = Now + (bFly ? FlightSeconds : NoFlightArriveSeconds);
	const float HatchAt = Arrive + VibrateSeconds;
	Egg->InitEgg(PC, Now, Arrive, HatchAt);
	Egg->FinishSpawning(EggXf);
	Ghost->BeginRevive(Egg, bFly ? FlightSeconds : 0.f, Now, Arrive, HatchAt);
	UE_LOG(LogTortunabo, Log, TEXT("[Fantasma] %s vuelve a la vida en un huevo en (%.0f, %.0f, %.0f)%s."), *GetNameSafe(PC->PlayerState),
		EggXf.GetLocation().X, EggXf.GetLocation().Y, EggXf.GetLocation().Z, bFly ? TEXT("") : TEXT(" (sin vuelo: tenía tortuga)"));
	return true;
}

void TNGhost::GetHUDSubject(const APlayerController* PC, APawn*& OutPawn, APlayerState*& OutPlayerState)
{
	OutPawn = PC ? PC->GetPawn() : nullptr;
	OutPlayerState = PC ? PC->PlayerState.Get() : nullptr;
	if (!PC || !IsGhost(PC))
	{
		return;
	}
	// De fantasma, la tortuga que sigue (el ViewTarget) y su jugador; si aún no sigue a nadie, solo su PlayerState.
	OutPawn = nullptr;
	APawn* Watched = Cast<APawn>(PC->GetViewTarget());
	APlayerState* WatchedPS = Watched && !Watched->IsA<ASpectatorPawn>() ? Watched->GetPlayerState() : nullptr;
	if (WatchedPS && WatchedPS != PC->PlayerState)
	{
		OutPawn = Watched;
		OutPlayerState = WatchedPS;
	}
}

bool TNGhost::IsGhostPlayer(const APlayerState* PlayerState)
{
	return PlayerState && ATN_SpectatorGhost::FindForPlayerState(PlayerState) != nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Enganches internos
// ─────────────────────────────────────────────────────────────────────────────

void TNGhostInternal::OnEnterSpectate(APlayerController* PC)
{
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World || !PC->HasAuthority())
	{
		return;
	}
	APawn* Body = PC->GetPawn();
	if (ATN_SpectatorGhost* Existing = ATN_SpectatorGhost::FindFor(PC))
	{
		if (Existing->IsActiveGhost())
		{
			if (Body)
			{
				Existing->SetLeftBody(Body);
			}
			return;
		}
	}
	// Empieza detrás y encima de la tortuga que deja; su dueño lo pone enseguida donde está su cámara.
	FVector Start = PC->GetFocalLocation();
	double Yaw = PC->GetControlRotation().Yaw;
	if (Body)
	{
		Start = Body->GetActorLocation() - Body->GetActorForwardVector().GetSafeNormal2D() * 260.0 + FVector(0.0, 0.0, 150.0);
		Yaw = Body->GetActorRotation().Yaw;
	}
	FActorSpawnParameters Params;
	Params.Owner = PC;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ATN_SpectatorGhost* Ghost = World->SpawnActor<ATN_SpectatorGhost>(ATN_SpectatorGhost::StaticClass(), Start, FRotator(0.0, Yaw, 0.0), Params))
	{
		Ghost->InitGhost(PC, Body);
	}
}

void TNGhostInternal::OnPossess(APlayerController* PC, APawn* Pawn)
{
	if (!PC || !Pawn || !PC->HasAuthority() || Pawn->IsA<ASpectatorPawn>())
	{
		return;
	}
	// Vuelve a tener tortuga (sale del huevo, lo rescatan, ronda nueva): su fantasma se desvanece. Si estaba volviendo a
	// la vida por otro lado, su huevo se abrirá vacío (CompleteRevive).
	if (ATN_SpectatorGhost* Ghost = ATN_SpectatorGhost::FindFor(PC))
	{
		Ghost->EndGhost();
	}
}

void TNGhostInternal::CompleteRevive(ATN_GhostEgg* Egg)
{
	using namespace TNGhostApiDetail;
	if (!Egg || !Egg->HasAuthority() || Egg->HasHatched())
	{
		return;
	}
	UWorld* World = Egg->GetWorld();
	APlayerController* PC = Egg->GetReviver();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	ATN_SpectatorGhost* Ghost = PC ? ATN_SpectatorGhost::FindFor(PC) : nullptr;
	if (!PC || !GameMode || !Ghost || !Ghost->IsReviving())
	{
		// Se ha ido o ya ha vuelto por otro lado (rescate, ronda nueva): el huevo se abre vacío.
		Egg->Hatch(nullptr);
		return;
	}

	// Lo que quedaba de su tortuga de antes (oculta o panza arriba) y su rescate del cooperativo ya no hacen falta.
	APawn* Body = Ghost->GetLeftBody();
	if (IsValid(Body) && !Body->GetController() && !Body->IsActorBeingDestroyed())
	{
		Body->Destroy();
	}
	Ghost->ForgetLeftBody();
	if (const APlayerState* OwnState = PC->PlayerState)
	{
		for (TActorIterator<ATN_RescuePickup> It(World); It; ++It)
		{
			if (It->GetDeadPlayerId() == OwnState->GetPlayerId())
			{
				It->Destroy();
			}
		}
	}

	// Sale como al reaparecer (ATN_ProcMapGameMode::RespawnControllerFresh): ya no es solo espectador y vuelve a jugar.
	if (APlayerState* OwnState = PC->PlayerState)
	{
		OwnState->SetIsSpectator(false);
		OwnState->SetIsOnlyASpectator(false);
	}
	if (APawn* Stale = PC->GetPawn())
	{
		PC->UnPossess();
		Stale->Destroy();
	}
	PC->ChangeState(NAME_Playing);
	PC->ClientGotoState(NAME_Playing);

	// Dentro del huevo, de pie sobre el suelo y mirando hacia su X.
	const FRotator Facing(0.0, Egg->GetActorRotation().Yaw, 0.0);
	FTransform SpawnXf(Facing, Egg->GetActorLocation() + FVector(0.0, 0.0, PawnHalfHeight(GameMode, PC) + 4.0));
	GameMode->RestartPlayerAtTransform(PC, SpawnXf);
	if (!PC->GetPawn())
	{
		// Algo ocupaba el sitio: un poco más arriba (caerá dentro del huevo).
		SpawnXf.AddToTranslation(FVector(0.0, 0.0, 120.0));
		GameMode->RestartPlayerAtTransform(PC, SpawnXf);
	}
	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[Fantasma] %s no ha podido salir del huevo (RestartPlayerAtTransform sin tortuga): sigue de fantasma."),
			*GetNameSafe(PC->PlayerState));
		if (AMP_GamePlayerController* GamePC = Cast<AMP_GamePlayerController>(PC))
		{
			GamePC->EnterSpectateMode();
		}
		else
		{
			PC->ChangeState(NAME_Spectating);
			PC->ClientGotoState(NAME_Spectating);
		}
		Ghost->CancelRevive();
		Egg->Hatch(nullptr);
		return;
	}

	// Cámara, entrada y giro como al reanimar (ATN_RunGameMode) o reaparecer (ATN_ProcMapGameMode).
	Pawn->EnableInput(PC);
	PC->SetViewTarget(Pawn);
	PC->ClientSetRotation(Facing, true);
	if (AMP_GamePlayerController* GamePC = Cast<AMP_GamePlayerController>(PC))
	{
		GamePC->ClientRestorePlayerInput();
		if (GamePC->IsLocalController())
		{
			GamePC->ForceRestoreInput();
		}
	}
	// El saltito de salida del huevo (su dueño la lanza también a la vez: ATN_GhostEgg).
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			if (Move->MovementMode == MOVE_None)
			{
				Move->SetMovementMode(MOVE_Falling);
			}
		}
		Character->LaunchCharacter(Egg->GetHopVelocity(), true, true);
	}
	Egg->Hatch(Pawn);
	UE_LOG(LogTortunabo, Log, TEXT("[Fantasma] %s sale del huevo."), *GetNameSafe(PC->PlayerState));
	TNGhost::OnHatched().Broadcast(PC, Pawn);
}

void TNGhostInternal::RunDebugCommand(UWorld* World, EDebugCommand Command, int32 PlayerIndex)
{
	using namespace TNGhostApiDetail;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// En la carrera no se muere nunca y nadie vuelve a la vida en un huevo (los finalistas del sprint salen de los huevos de
	// salida): volver a la vida es del cooperativo. Se mira por el nombre de la clase para no depender del modo carrera.
	if (Command == EDebugCommand::Revive)
	{
		const AGameStateBase* GameState = World->GetGameState();
		if (GameState && GameState->GetClass()->GetName().Contains(TEXT("BeachRace")))
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Fantasma] TN.Ghost.Revive no hace nada en la carrera: volver a la vida en un huevo es del cooperativo."));
			return;
		}
	}
	APlayerController* Target = PlayerIndex >= 0 ? PlayerByIndex(World, PlayerIndex) : nullptr;
	if (!Target && PlayerIndex < 0)
	{
		if (Command == EDebugCommand::Revive)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (TNGhost::IsGhost(It->Get()))
				{
					Target = It->Get();
					break;
				}
			}
		}
		else
		{
			Target = PlayerByIndex(World, 0);
		}
	}
	if (!Target)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Fantasma] TN.Ghost: no hay jugador %d (ni ningún fantasma)."), PlayerIndex);
		return;
	}
	if (Command == EDebugCommand::Revive)
	{
		DebugRevive(World, Target);
	}
	else
	{
		DebugBecome(Target);
	}
}
