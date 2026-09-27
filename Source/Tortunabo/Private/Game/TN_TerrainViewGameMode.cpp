#include "Game/TN_TerrainViewGameMode.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_Log.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapTypes.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATN_TerrainViewPlayerController::ATN_TerrainViewPlayerController()
{
	bShowHUD = false;
}

void ATN_TerrainViewPlayerController::TNRegen(int32 Seed)
{
	ServerRegen(Seed);
}

void ATN_TerrainViewPlayerController::ServerRegen_Implementation(int32 Seed)
{
	if (ATN_TerrainViewGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATN_TerrainViewGameMode>() : nullptr)
	{
		GM->Regenerate(Seed);
	}
}

ATN_TerrainViewGameMode::ATN_TerrainViewGameMode()
{
	GameStateClass = ATN_CoopGameState::StaticClass();
	PlayerStateClass = ATN_CoopPlayerState::StaticClass();
	PlayerControllerClass = ATN_TerrainViewPlayerController::StaticClass();
	static ConstructorHelpers::FClassFinder<APawn> Turtle(TEXT("/Game/Blueprints/Characters/BP_TortugaCharacter"));
	if (Turtle.Succeeded()) { DefaultPawnClass = Turtle.Class; }
}

ATN_ProcMapGenerator* ATN_TerrainViewGameMode::EnsureGenerator()
{
	if (Generator) { return Generator; }
	for (TActorIterator<ATN_ProcMapGenerator> It(GetWorld()); It; ++It)
	{
		Generator = *It;
		break;
	}
	if (!Generator)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Generator = GetWorld()->SpawnActor<ATN_ProcMapGenerator>(ATN_ProcMapGenerator::StaticClass(), FTransform::Identity, Params);
	}
	if (Generator)
	{
		Generator->SetSettingsIfMissing(LoadObject<UTN_ProcMapSettings>(nullptr, TEXT("/Game/ProcMap/DA_ProcMapSettings.DA_ProcMapSettings")));
		Generator->SetTerrainOnly(true);
	}
	return Generator;
}

void ATN_TerrainViewGameMode::BeginPlay()
{
	Super::BeginPlay();
	Regenerate(FixedSeed);
}

void ATN_TerrainViewGameMode::Regenerate(int32 Seed)
{
	ATN_ProcMapGenerator* Gen = EnsureGenerator();
	if (!Gen) { return; }
	const int32 UseSeed = Seed != 0 ? Seed : FMath::RandRange(1, 999999);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }
		Waiting.AddUnique(PC);
		// El terreno de antes desaparece: quieta hasta que PollReady la lleve a la salida del nuevo.
		if (const ACharacter* Character = Cast<ACharacter>(PC->GetPawn())) { Character->GetCharacterMovement()->DisableMovement(); }
	}
	ReadySince = -1.0;
	Gen->ServerGenerate(UseSeed, Mode, Difficulty);
	UE_LOG(LogTortunabo, Log, TEXT("[Terreno] Generando el mapa de solo terreno con la semilla %d."), UseSeed);
	GetWorldTimerManager().SetTimer(PollTimer, this, &ATN_TerrainViewGameMode::PollReady, 0.25f, true);
}

void ATN_TerrainViewGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Nadie aparece hasta que el mapa está hecho: hasta entonces no hay suelo.
	if (!NewPlayer) { return; }
	const ATN_ProcMapGenerator* Gen = Generator.Get();
	if (Gen && Gen->IsMapReady() && Gen->GetBuiltGeneration() == Gen->GetRequestedGeneration())
	{
		RestartPlayerAtTransform(NewPlayer, Gen->GetStartTransform(GetNumPlayers() - 1));
		return;
	}
	Waiting.AddUnique(NewPlayer);
}

void ATN_TerrainViewGameMode::PollReady()
{
	const ATN_ProcMapGenerator* Gen = Generator.Get();
	if (!Gen || !Gen->IsMapReady() || Gen->GetBuiltGeneration() != Gen->GetRequestedGeneration()) { return; }
	// La colisión del terreno se cocina en segundo plano: nadie aparece hasta que hay suelo bajo la salida (como mucho
	// 10 s con el mapa hecho). Si no, la tortuga cae a través del terreno y muere de la caída.
	const double Now = GetWorld()->GetTimeSeconds();
	if (ReadySince < 0.0) { ReadySince = Now; }
	if (Now - ReadySince < 10.0 && !Gen->MapCollisionUnder(Gen->GetStartTransform(0).GetLocation())) { return; }
	ReadySince = -1.0;
	GetWorldTimerManager().ClearTimer(PollTimer);
	int32 Index = 0;
	for (const TWeakObjectPtr<APlayerController>& Weak : Waiting)
	{
		APlayerController* PC = Weak.Get();
		if (!PC) { continue; }
		const FTransform Start = Gen->GetStartTransform(Index++);
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->TeleportTo(Start.GetLocation(), Start.Rotator(), false, true);
			PC->ClientSetRotation(Start.Rotator());
			if (const ACharacter* Character = Cast<ACharacter>(Pawn)) { Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
		}
		else
		{
			RestartPlayerAtTransform(PC, Start);
		}
	}
	Waiting.Reset();
}
