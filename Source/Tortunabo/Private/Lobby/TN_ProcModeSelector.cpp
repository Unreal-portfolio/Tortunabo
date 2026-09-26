#include "Lobby/TN_ProcModeSelector.h"
#include "Core/TN_Log.h"
#include "Core/TN_GameModeSpawnUtils.h"
#include "Multiplayer/MP_GameInstance.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

ATN_ProcModeSelector::ATN_ProcModeSelector()
{
	// La etiqueta debe verse al momento en todos: sin dormancia (la base duerme).
	NetDormancy = DORM_Awake;
	CooldownSeconds = 0.4f;
	PromptText = NSLOCTEXT("Tortunabo", "ProcSelectorPrompt", "Cambiar");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Pedestal(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Mesh && Pedestal.Succeeded())
	{
		Mesh->SetStaticMesh(Pedestal.Object);
		Mesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.1f));
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	}

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(SceneRoot);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 190.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(38.f);
	Label->SetTextRenderColor(FColor(255, 214, 90));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATN_ProcModeSelector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ProcModeSelector, Selection);
}

void ATN_ProcModeSelector::BeginPlay()
{
	Super::BeginPlay();

	// El host arranca con lo que ya estaba elegido (sobrevive a volver al lobby).
	if (HasAuthority())
	{
		if (const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
		{
			Selection = Kind == ETNProcSelectorKind::Mode
				? static_cast<uint8>(GI->SelectedProcMode)
				: static_cast<uint8>(GI->SelectedProcDifficulty);
		}
	}
	RefreshLabel();
}

void ATN_ProcModeSelector::OnInteracted_Implementation(APawn* Interactor)
{
	Super::OnInteracted_Implementation(Interactor);
	if (!HasAuthority())
	{
		return;
	}
	UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	if (Kind == ETNProcSelectorKind::Mode)
	{
		const AGameStateBase* LobbyState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		const int32 Players = LobbyState ? TN_CountConnectedCoopPlayers(LobbyState) : 1;
		const int32 Count = static_cast<int32>(ETNProcGameMode::Count);
		int32 Next = static_cast<int32>(GI->SelectedProcMode);
		for (int32 Step = 0; Step < Count; ++Step)
		{
			Next = (Next + 1) % Count;
			// 2vs2 solo se ofrece con exactamente 4 jugadores.
			if (static_cast<ETNProcGameMode>(Next) != ETNProcGameMode::TwoVsTwo || Players == 4)
			{
				break;
			}
		}
		GI->SelectedProcMode = static_cast<ETNProcGameMode>(Next);
		Selection = static_cast<uint8>(Next);
	}
	else
	{
		const int32 Count = static_cast<int32>(ETNProcDifficulty::Count);
		const int32 Next = (static_cast<int32>(GI->SelectedProcDifficulty) + 1) % Count;
		GI->SelectedProcDifficulty = static_cast<ETNProcDifficulty>(Next);
		Selection = static_cast<uint8>(Next);
	}

	UE_LOG(LogTortunabo, Log, TEXT("[ProcModeSelector] %s → %s"), *GetNameSafe(Interactor), *DescribeSelection().ToString());
	OnRep_Selection();
	ForceNetUpdate();
}

void ATN_ProcModeSelector::OnRep_Selection()
{
	RefreshLabel();
	if (ChangeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ChangeSound, GetActorLocation());
	}
	OnSelectionChanged(DescribeSelection());
}

void ATN_ProcModeSelector::RefreshLabel()
{
	if (Label)
	{
		Label->SetText(DescribeSelection());
	}
}

FText ATN_ProcModeSelector::DescribeSelection() const
{
	if (Kind == ETNProcSelectorKind::Mode)
	{
		switch (static_cast<ETNProcGameMode>(Selection))
		{
		case ETNProcGameMode::Coop:     return NSLOCTEXT("Tortunabo", "ProcModeCoop", "MODO: COOP");
		case ETNProcGameMode::Race:     return NSLOCTEXT("Tortunabo", "ProcModeRace", "MODO: CARRERA");
		case ETNProcGameMode::TwoVsTwo: return NSLOCTEXT("Tortunabo", "ProcMode2v2", "MODO: 2 VS 2");
		default:                        return NSLOCTEXT("Tortunabo", "ProcModeClassic", "MODO: CLÁSICO");
		}
	}
	switch (static_cast<ETNProcDifficulty>(Selection))
	{
	case ETNProcDifficulty::Easy: return NSLOCTEXT("Tortunabo", "ProcDiffEasy", "DIFICULTAD: FÁCIL");
	case ETNProcDifficulty::Hard: return NSLOCTEXT("Tortunabo", "ProcDiffHard", "DIFICULTAD: DIFÍCIL");
	default:                      return NSLOCTEXT("Tortunabo", "ProcDiffNormal", "DIFICULTAD: NORMAL");
	}
}
