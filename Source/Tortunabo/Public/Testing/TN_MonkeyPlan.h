#pragma once

#include "CoreMinimal.h"

/**
 * Lógica pura del monkey test (Docs/Estres-Monkey-2026-09-29.md): el plan de entrada aleatoria reproducible por semilla, la
 * detección de atasco y las estadísticas de fotogramas. Sin mundo ni actores: la usan UTN_MonkeyComponent y
 * UTN_MonkeySubsystem, y se testea igual (Tortunabo.Monkey).
 */
namespace TNMonkey
{
	/** Lo que el mono hace con la tortuga en un tramo. */
	enum class EAction : uint8
	{
		Idle,
		Walk,
		Sprint,
		Jump,
		DoubleJump,
		Shell,
		Interact,
		UseItem,
		Emote,
		Look,
		Pause,
		Drop,
		Count
	};

	inline const TCHAR* ActionName(EAction Action)
	{
		static const TCHAR* const Names[] = { TEXT("Idle"), TEXT("Walk"), TEXT("Sprint"), TEXT("Jump"), TEXT("DoubleJump"), TEXT("Shell"),
			TEXT("Interact"), TEXT("UseItem"), TEXT("Emote"), TEXT("Look"), TEXT("Pause"), TEXT("Drop") };
		static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EAction::Count), "ActionName: falta un nombre");
		const int32 Index = static_cast<int32>(Action);
		return Index >= 0 && Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("?");
	}

	/** Un tramo de entrada: qué hacer, cuánto dura y con qué parámetros. */
	struct FStep
	{
		EAction Action = EAction::Idle;
		float Seconds = 0.f;
		/** Empuje del stick (X = lateral, Y = adelante) mientras dura el tramo (Walk y Sprint). */
		FVector2D Move = FVector2D::ZeroVector;
		/** Giro de cámara en grados por segundo (Look; los demás tramos giran poco). */
		float YawRate = 0.f;
		/** Emote (0-9) o, en Shell, si al acabar se sale del caparazón (1) o se deja (0). */
		int32 Arg = 0;
	};

	/** Peso de cada acción al sortear el siguiente tramo (más andar y correr que el resto: es una carrera). */
	inline float ActionWeight(EAction Action)
	{
		switch (Action)
		{
			case EAction::Walk:       return 28.f;
			case EAction::Sprint:     return 22.f;
			case EAction::Jump:       return 12.f;
			case EAction::DoubleJump: return 8.f;
			case EAction::Shell:      return 6.f;
			case EAction::Interact:   return 8.f;
			case EAction::UseItem:    return 6.f;
			case EAction::Emote:      return 4.f;
			case EAction::Look:       return 6.f;
			case EAction::Idle:       return 3.f;
			case EAction::Pause:      return 2.f;
			case EAction::Drop:       return 2.f;
			default:                  return 0.f;
		}
	}

	/** Mezcla la semilla de la sesión con el jugador para que cada tortuga haga cosas distintas pero reproducibles. */
	inline int32 MixSeed(int32 Seed, int32 PlayerIndex)
	{
		return static_cast<int32>(static_cast<uint32>(Seed) * 2654435761u + static_cast<uint32>(PlayerIndex + 1) * 40503u + 0x9E3779B9u);
	}

	/** Genera la secuencia de tramos de un jugador: misma semilla y mismo jugador, misma secuencia. */
	class FPlanner
	{
	public:
		FPlanner() = default;
		FPlanner(int32 Seed, int32 PlayerIndex) : Stream(MixSeed(Seed, PlayerIndex)) {}

		FStep Next()
		{
			float Total = 0.f;
			for (int32 Index = 0; Index < static_cast<int32>(EAction::Count); ++Index)
			{
				Total += ActionWeight(static_cast<EAction>(Index));
			}
			float Roll = Stream.FRand() * Total;
			EAction Picked = EAction::Walk;
			for (int32 Index = 0; Index < static_cast<int32>(EAction::Count); ++Index)
			{
				Roll -= ActionWeight(static_cast<EAction>(Index));
				if (Roll <= 0.f)
				{
					Picked = static_cast<EAction>(Index);
					break;
				}
			}
			return Build(Picked);
		}

	private:
		FStep Build(EAction Action)
		{
			FStep Step;
			Step.Action = Action;
			switch (Action)
			{
				case EAction::Walk:
				case EAction::Sprint:
					Step.Seconds = Stream.FRandRange(0.8f, 2.5f);
					// Casi siempre hacia delante (la carrera va hacia delante), con desvío lateral y algún paso atrás.
					Step.Move = FVector2D(Stream.FRandRange(-0.6f, 0.6f), Stream.FRand() < 0.85f ? Stream.FRandRange(0.5f, 1.f) : Stream.FRandRange(-1.f, -0.3f));
					Step.YawRate = Stream.FRandRange(-25.f, 25.f);
					break;
				case EAction::Jump:
					Step.Seconds = 0.4f;
					break;
				case EAction::DoubleJump:
					Step.Seconds = 1.0f;
					break;
				case EAction::Shell:
					Step.Seconds = Stream.FRandRange(0.8f, 3.f);
					Step.Arg = Stream.FRand() < 0.7f ? 1 : 0;
					Step.Move = FVector2D(0.f, Stream.FRandRange(0.f, 1.f));
					break;
				case EAction::Interact:
					Step.Seconds = Stream.FRandRange(0.2f, 1.2f);
					break;
				case EAction::UseItem:
					Step.Seconds = 0.3f;
					break;
				case EAction::Emote:
					Step.Seconds = Stream.FRandRange(0.8f, 2.5f);
					Step.Arg = Stream.RandRange(0, 9);
					break;
				case EAction::Look:
					Step.Seconds = Stream.FRandRange(0.5f, 1.5f);
					Step.YawRate = Stream.FRandRange(-120.f, 120.f);
					break;
				case EAction::Pause:
					Step.Seconds = Stream.FRandRange(0.5f, 2.f);
					break;
				case EAction::Drop:
					Step.Seconds = 0.3f;
					break;
				default:
					Step.Seconds = Stream.FRandRange(0.3f, 1.5f);
					break;
			}
			return Step;
		}

		FRandomStream Stream;
	};

	/**
	 * Atasco: la tortuga lleva Window segundos intentando moverse (acumulados, sin contar el tiempo aturdida, en caparazón o
	 * llevada) y ha avanzado menos de MinTravel cm en total. Update devuelve true una vez por atasco y reinicia la ventana.
	 */
	class FStuckDetector
	{
	public:
		float Window = 6.f;
		float MinTravel = 100.f;

		bool Update(float DeltaSeconds, const FVector& Location, bool bTryingToMove)
		{
			if (!bStarted)
			{
				bStarted = true;
				Anchor = Location;
			}
			if (!bTryingToMove)
			{
				// Parada por su gusto o aturdida: la ventana no avanza, pero el ancla sigue a la tortuga.
				Anchor = Location;
				Trying = 0.f;
				return false;
			}
			Trying += DeltaSeconds;
			if (Trying < Window)
			{
				return false;
			}
			const bool bStuck = FVector::Dist(Anchor, Location) < MinTravel;
			Anchor = Location;
			Trying = 0.f;
			return bStuck;
		}

	private:
		bool bStarted = false;
		FVector Anchor = FVector::ZeroVector;
		float Trying = 0.f;
	};

	/** Percentil P (0-1) de las muestras, por interpolación entre vecinas; 0 sin muestras. */
	inline float Percentile(TArray<float> Samples, float P)
	{
		if (Samples.Num() == 0)
		{
			return 0.f;
		}
		Samples.Sort();
		const float Position = FMath::Clamp(P, 0.f, 1.f) * static_cast<float>(Samples.Num() - 1);
		const int32 Low = FMath::FloorToInt(Position);
		const int32 High = FMath::Min(Low + 1, Samples.Num() - 1);
		return FMath::Lerp(Samples[Low], Samples[High], Position - static_cast<float>(Low));
	}

	/** Resumen de una lista de tiempos de fotograma (ms). */
	struct FFrameSummary
	{
		int32 Frames = 0;
		float Average = 0.f;
		float P50 = 0.f;
		float P95 = 0.f;
		float P99 = 0.f;
		float Max = 0.f;
	};

	inline FFrameSummary Summarize(const TArray<float>& FrameMs)
	{
		FFrameSummary Out;
		Out.Frames = FrameMs.Num();
		if (FrameMs.Num() == 0)
		{
			return Out;
		}
		double Sum = 0.0;
		for (const float Ms : FrameMs)
		{
			Sum += Ms;
			Out.Max = FMath::Max(Out.Max, Ms);
		}
		Out.Average = static_cast<float>(Sum / FrameMs.Num());
		Out.P50 = Percentile(FrameMs, 0.5f);
		Out.P95 = Percentile(FrameMs, 0.95f);
		Out.P99 = Percentile(FrameMs, 0.99f);
		return Out;
	}
}
