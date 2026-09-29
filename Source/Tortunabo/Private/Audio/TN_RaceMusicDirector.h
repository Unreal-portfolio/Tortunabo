#pragma once

// C++ puro (sin tipos de Unreal), como TN_MatchMusicDirector.h: se compila y se prueba fuera del motor en el arnés de
// Tools/RaceMusic. UTN_RaceMusicSubsystem (TN_RaceMusicSubsystem.cpp) le pasa fotos del estado replicado de la carrera y
// aplica lo que decide.
#include <algorithm>
#include <cstdint>

namespace TNRaceMusic
{
	/** Fase de la carrera (mismo orden que ETNBeachRacePhase). */
	enum class EPhase : uint8_t { Waiting, Racing, RoundResults, Champion, SprintIntro };

	/** Cuenta de 10 s tras la primera en el agua (mismo orden que ETNBeachFinishCountdown). */
	enum class EFinish : uint8_t { None, Counting, TimeUp, AllIn };

	/** Foto del estado que le interesa al director (la rellena el subsistema a partir del GameState y el PlayerState locales). */
	struct FDirectorSnapshot
	{
		double NowSeconds = 0.0;
		/** El GameState es el de la carrera en la playa (ATN_BeachRaceGameState). */
		bool bRaceWorld = false;
		EPhase Phase = EPhase::Waiting;
		bool bSprintFinal = false;
		/** En Waiting, segundos que quedan de la cuenta 3, 2, 1 de salida (0 = aún sin cuenta, o la primera ronda, que sale del huevo). */
		float PhaseSecondsLeft = 0.f;
		EFinish Finish = EFinish::None;
		/** Segundos que quedan de la cuenta de 10 s y su duración total. */
		float FinishSecondsLeft = 0.f;
		float FinishSecondsTotal = 10.f;
		/** Segundos que quedan del tiempo de la ronda (o del sprint); -1 si no hay límite o ya no cuenta. */
		float RoundSecondsLeft = -1.f;
		/** El jugador local ya ha llegado (bHasFinishedRun sin bIsEliminated): a partir de aquí suena la música de fin de partida. */
		bool bLocalFinished = false;
		/** Está sonando música de fin de partida (victoria, derrota o eliminado) en el jugador local. */
		bool bOtherMusicPlaying = false;
	};

	/** Lo que debe hacer la música. Se decide en cada foto; el motor suaviza tensión y «ducking». */
	struct FDirectorDecision
	{
		bool bPlay = false;
		float FadeSeconds = 1.2f;
		/** 0..1: capa de tensión. */
		float Tension = 0.f;
		/** 0..1: cuánto se aparta (0 nada, 1 al máximo: -10 dB y paso bajo). */
		float Duck = 0.f;
		/** Motivo (texto fijo) para el registro de depuración. */
		const char* Reason = "";
	};

	/**
	 * Director de la música de la carrera: qué suena, cuánto se aparta y cuánta tensión lleva.
	 *  - Waiting con la cuenta 3, 2, 1 de salida: entra (con la introducción) apartada (Duck 0,55) y se abre al dar la salida.
	 *    Antes de la cuenta (generación de la ronda, huevos cerrándose) no suena; la primera ronda tras el viaje, cuya
	 *    cuenta es el huevo de la pantalla de carga, empieza a sonar al salir.
	 *  - Racing: suena entera. Tensión: en el último minuto del tiempo de la ronda sube de 0,35 a 1; en la cuenta de 10 s
	 *    tras la primera en el agua, de 0,7 a 1 (y se aparta, Duck 0,5, para que se oigan el «¡toc!» y las voces); en el
	 *    sprint final, al menos 0,5 durante toda la ronda.
	 *  - TimeUp / AllIn («¡TIEMPO!», «¡TODAS AL AGUA!»), recuento, título del sprint y podio: se calla del todo con un
	 *    fundido (el recuento y el podio ya llevan su música de victoria o derrota).
	 *  - Cede el sitio a la música de fin de partida: si el jugador local ya ha llegado, o suena su victoria, derrota o
	 *    eliminado, se calla, y hasta 1,6 s después de que ese tema termine (para no pisar su fundido).
	 *  - Un corte de la decisión por una fase transitoria (entre la cuenta de salida y Racing hay un instante con
	 *    Waiting y cuenta a 0) se aguanta 0,45 s antes de callar.
	 */
	class FDirector
	{
	public:
		static constexpr double YieldTailSeconds = 1.6;
		static constexpr double SoftOffDelaySeconds = 0.45;
		static constexpr float LastMinuteSeconds = 60.f;
		static constexpr float CountdownDuck = 0.55f;
		static constexpr float FinishDuck = 0.50f;
		static constexpr float SprintBaseTension = 0.5f;

		void Reset() { *this = FDirector(); }

		FDirectorDecision Update(const FDirectorSnapshot& In)
		{
			FDirectorDecision Out;
			if (!In.bRaceWorld)
			{
				Out.FadeSeconds = 1.5f;
				Out.Reason = "no es la carrera";
				bLastPlay = false;
				return Out;
			}

			if (In.bOtherMusicPlaying) { LastOtherSeconds = In.NowSeconds; }
			if (In.bOtherMusicPlaying || In.NowSeconds - LastOtherSeconds < YieldTailSeconds)
			{
				Out.FadeSeconds = 0.9f;
				Out.Reason = "cede a la musica de fin de partida";
				bLastPlay = false;
				return Out;
			}
			if (In.bLocalFinished)
			{
				Out.FadeSeconds = 1.0f;
				Out.Reason = "el jugador ya ha llegado";
				bLastPlay = false;
				return Out;
			}

			bool bSoftOff = false;
			switch (In.Phase)
			{
			case EPhase::Waiting:
				if (In.PhaseSecondsLeft > 0.02f)
				{
					Out.bPlay = true;
					Out.FadeSeconds = 1.5f;
					Out.Duck = CountdownDuck;
					Out.Tension = In.bSprintFinal ? SprintBaseTension : 0.f;
					Out.Reason = "cuenta de salida";
				}
				else
				{
					bSoftOff = true;
					Out.Reason = "preparando la ronda";
				}
				break;
			case EPhase::Racing:
				DecideRacing(In, Out);
				break;
			case EPhase::RoundResults:
				Out.FadeSeconds = 1.0f;
				Out.Reason = "recuento";
				break;
			case EPhase::SprintIntro:
				Out.FadeSeconds = 0.8f;
				Out.Reason = "titulo del sprint";
				break;
			case EPhase::Champion:
				Out.FadeSeconds = 1.0f;
				Out.Reason = "podio";
				break;
			}

			if (Out.bPlay)
			{
				LastPlaySeconds = In.NowSeconds;
				LastTension = Out.Tension;
				LastDuck = Out.Duck;
			}
			else if (bSoftOff && bLastPlay && In.NowSeconds - LastPlaySeconds < SoftOffDelaySeconds)
			{
				// Instante entre la cuenta de salida y la carrera: no se cae para volver a entrar.
				Out.bPlay = true;
				Out.FadeSeconds = 1.5f;
				Out.Tension = LastTension;
				Out.Duck = LastDuck;
				Out.Reason = "aguantando el cambio de fase";
			}
			bLastPlay = Out.bPlay;
			return Out;
		}

	private:
		static void DecideRacing(const FDirectorSnapshot& In, FDirectorDecision& Out)
		{
			if (In.Finish == EFinish::TimeUp || In.Finish == EFinish::AllIn)
			{
				Out.FadeSeconds = 0.8f;
				Out.Reason = In.Finish == EFinish::TimeUp ? "tiempo" : "todas al agua";
				return;
			}
			Out.bPlay = true;
			Out.FadeSeconds = 1.2f;
			Out.Reason = "carrera";
			float Tension = In.bSprintFinal ? SprintBaseTension : 0.f;
			if (In.RoundSecondsLeft >= 0.f && In.RoundSecondsLeft <= LastMinuteSeconds)
			{
				const float Left = std::clamp(In.RoundSecondsLeft / LastMinuteSeconds, 0.f, 1.f);
				Tension = std::max(Tension, 0.35f + 0.65f * (1.f - Left));
				Out.Reason = "ultimo minuto";
			}
			if (In.Finish == EFinish::Counting)
			{
				const float Left = std::clamp(In.FinishSecondsLeft / std::max(1.f, In.FinishSecondsTotal), 0.f, 1.f);
				Tension = std::max(Tension, 0.7f + 0.3f * (1.f - Left));
				Out.Duck = FinishDuck + (In.FinishSecondsLeft <= 3.f ? 0.15f : 0.f);
				Out.Reason = "cuenta de 10 s";
			}
			Out.Tension = std::clamp(Tension, 0.f, 1.f);
		}

		double LastOtherSeconds = -1.0e9;
		double LastPlaySeconds = -1.0e9;
		bool bLastPlay = false;
		float LastTension = 0.f;
		float LastDuck = 0.f;
	};
}
