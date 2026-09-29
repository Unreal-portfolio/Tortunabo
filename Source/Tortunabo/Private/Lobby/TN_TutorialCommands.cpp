// ─────────────────────────────────────────────────────────────────────────────
// Consola del tutorial de la primera partida (Docs/Tutorial.md, Docs/Comandos_Prueba.md). Cada comando actúa sobre la ventana
// donde se escribe: su guardado (UMP_GameInstance, una ranura por ventana de PIE) y su jugador (UTN_TutorialPlayerComponent,
// que se lo pide al servidor). Sirven igual en el anfitrión y en un cliente.
// ─────────────────────────────────────────────────────────────────────────────

#include "Lobby/TN_TutorialCourse.h"
#include "Lobby/TN_TutorialPlayerComponent.h"
#include "TN_TutorialLayout.h"
#include "TN_TutorialTexts.h"
#include "Core/TN_Log.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace TNTutorialCommandsDetail
{
	UMP_GameInstance* GameInstanceOf(const UWorld* World)
	{
		return World ? World->GetGameInstance<UMP_GameInstance>() : nullptr;
	}

	/** El componente del jugador de esta ventana, si está en el lobby (donde está el recorrido); si no, lo dice. */
	UTN_TutorialPlayerComponent* LobbyComponent(const UWorld* World, const TCHAR* Command)
	{
		UTN_TutorialPlayerComponent* Comp = UTN_TutorialPlayerComponent::FindLocal(World);
		if (!Comp || !ATN_TutorialCourse::Find(World))
		{
			UE_LOG(LogTortunabo, Display, TEXT("[Tutorial] %s: aquí no hay tutorial (solo en el lobby del castillo)."), Command);
			return nullptr;
		}
		return Comp;
	}

	void LogStations()
	{
		UE_LOG(LogTortunabo, Display, TEXT("[Tutorial] Estaciones (TN.Tutorial.Station N):"));
		for (int32 Index = 0; Index < TNTutorial::NumStations; ++Index)
		{
			UE_LOG(LogTortunabo, Display, TEXT("[Tutorial]   %2d  %s"), Index + 1,
				*TNTutorialTexts::Title(static_cast<TNTutorial::EStation>(Index)).ToString());
		}
	}

	void HandleReset(const TArray<FString>& /*Args*/, UWorld* World)
	{
		UMP_GameInstance* GI = GameInstanceOf(World);
		if (!GI)
		{
			UE_LOG(LogTortunabo, Display, TEXT("[Tutorial] TN.Tutorial.Reset: sin GameInstance del juego."));
			return;
		}
		GI->ResetTutorialProgress();
		if (ATN_TutorialCourse::Find(World))
		{
			UE_LOG(LogTortunabo, Display, TEXT("[Tutorial] Estás en el lobby: TN.Tutorial.Start para empezarlo ya."));
		}
	}

	void HandleStart(const TArray<FString>& /*Args*/, UWorld* World)
	{
		UTN_TutorialPlayerComponent* Comp = LobbyComponent(World, TEXT("TN.Tutorial.Start"));
		if (!Comp)
		{
			return;
		}
		if (UMP_GameInstance* GI = GameInstanceOf(World))
		{
			GI->ResetTutorialProgress();
		}
		Comp->RequestStart(true);
		UE_LOG(LogTortunabo, Display, TEXT("[Tutorial] TN.Tutorial.Start: pedido al servidor (desde la salida)."));
	}

	void HandleSkip(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (UTN_TutorialPlayerComponent* Comp = UTN_TutorialPlayerComponent::FindLocal(World))
		{
			// Dentro: el servidor lo baja al lobby y se apunta como hecho. Fuera: solo se apunta.
			Comp->RequestSkip();
			UE_LOG(LogTortunabo, Display, TEXT("[Tutorial] TN.Tutorial.Skip: %s"),
				Comp->IsInTutorial() ? TEXT("de vuelta al lobby, apuntado como hecho.") : TEXT("apuntado como hecho."));
			return;
		}
		if (UMP_GameInstance* GI = GameInstanceOf(World))
		{
			GI->SetTutorialCompleted();
		}
	}

	void HandleStation(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() == 0 || !Args[0].IsNumeric())
		{
			LogStations();
			return;
		}
		UTN_TutorialPlayerComponent* Comp = LobbyComponent(World, TEXT("TN.Tutorial.Station"));
		if (!Comp)
		{
			return;
		}
		const int32 Number = FMath::Clamp(FCString::Atoi(*Args[0]), 1, TNTutorial::NumStations);
		Comp->RequestStation(Number - 1);
		UE_LOG(LogTortunabo, Display, TEXT("[Tutorial] TN.Tutorial.Station: a la %d (%s)."), Number,
			*TNTutorialTexts::Title(static_cast<TNTutorial::EStation>(Number - 1)).ToString());
	}

	void HandleInfo(const TArray<FString>& /*Args*/, UWorld* World)
	{
		const UMP_GameInstance* GI = GameInstanceOf(World);
		const UTN_TutorialPlayerComponent* Comp = UTN_TutorialPlayerComponent::FindLocal(World);
		const ATN_TutorialCourse* Course = ATN_TutorialCourse::Find(World);
		const int32 CurrentStation = Comp ? Comp->GetCurrentStation() : INDEX_NONE;
		UE_LOG(LogTortunabo, Display, TEXT("[Tutorial] Guardado %s: %s · jugador %s%s · recorrido %s%s"),
			GI ? *GI->GetTutorialSlotName() : TEXT("(sin GameInstance)"),
			GI && GI->HasCompletedTutorial() ? TEXT("hecho") : TEXT("por hacer"),
			!Comp ? TEXT("sin componente (fuera del lobby)") : Comp->IsInTutorial() ? TEXT("dentro") : TEXT("fuera"),
			Comp && Comp->IsInTutorial() && CurrentStation != INDEX_NONE ? *FString::Printf(TEXT(", estación %d"), CurrentStation + 1) : TEXT(""),
			!Course ? TEXT("no hay") : Course->IsBuilt() ? TEXT("montado") : TEXT("sin montar"),
			Course && Course->HasAuthority() ? *FString::Printf(TEXT(", %d dentro (servidor)"), Course->NumParticipants()) : TEXT(""));
	}

	FAutoConsoleCommandWithWorldAndArgs ResetCommand(
		TEXT("TN.Tutorial.Reset"),
		TEXT("Deja el tutorial por hacer en esta ventana (su guardado): empieza la próxima vez que llegues a un lobby."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleReset));

	FAutoConsoleCommandWithWorldAndArgs StartCommand(
		TEXT("TN.Tutorial.Start"),
		TEXT("En el lobby: empieza el tutorial ahora, desde la salida (aunque ya esté hecho o estés dentro)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStart));

	FAutoConsoleCommandWithWorldAndArgs SkipCommand(
		TEXT("TN.Tutorial.Skip"),
		TEXT("Salta el tutorial (como la opción del menú de pausa): de vuelta al lobby y apuntado como hecho."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleSkip));

	FAutoConsoleCommandWithWorldAndArgs StationCommand(
		TEXT("TN.Tutorial.Station"),
		TEXT("En el lobby: TN.Tutorial.Station N lleva a la estación N (1-19) del tutorial, metiéndote en él si hace falta. Sin número, la lista."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStation));

	FAutoConsoleCommandWithWorldAndArgs InfoCommand(
		TEXT("TN.Tutorial.Info"),
		TEXT("Escribe en el registro el estado del tutorial de esta ventana: ranura del guardado, hecho o no, estación y participantes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleInfo));
}
