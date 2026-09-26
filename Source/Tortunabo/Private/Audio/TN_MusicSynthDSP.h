#pragma once

// C++ puro (sin tipos de Unreal): así se puede compilar y medir fuera del motor (ver el arnés en
// Scripts / scratchpad de validación). El componente (TN_MusicSynthComponent.cpp) es el único que toca UE.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <iterator>

/**
 * Motor de música sintetizada de UTN_MusicSynthComponent: dos temas originales ("La Concha Dorada" para la tienda y
 * uno de lounge/bossa para el probador) compuestos como secuencias de notas en código, con instrumentos sintetizados
 * (resonadores modales tipo campana/madera, FM de dos operadores, percusión de ruido filtrado) y un mezclador con
 * fundido cruzado entre pistas y limitador suave.
 *
 * Hilos: FMusicSharedParams lo escribe el hilo de juego (atómicos relajados) y lo lee el hilo de render de audio una
 * vez por bloque de control. Todo lo demás (FMusicEngine y lo que cuelga de él) vive y se actualiza solo en el hilo de
 * audio: sin asignaciones, sin bloqueos, sin UObjects. La composición es enteramente determinista (sin azar): el mismo
 * instante de reproducción scomputa siempre la misma señal, lo que permite validarla con un arnés fuera del motor.
 */
namespace TNMusic
{
	constexpr float MusicPi = 3.14159265358979323846f;
	constexpr float MusicTwoPi = 6.28318530717958647692f;
	constexpr float MusicHalfPi = 1.57079632679489661923f;

	/** Muestras por bloque de control: la programación de notas y el suavizado de parámetros se recalculan a este ritmo. */
	constexpr int32_t MusicBlockFrames = 32;

	inline float MusicClamp01(float InX) { return std::clamp(InX, 0.f, 1.f); }

	/** Acercamiento exponencial a un objetivo con constante de tiempo InSeconds en un paso de InDt segundos. */
	inline float MusicTimeCoef(float InSeconds, float InDt)
	{
		return InSeconds <= 1e-4f ? 1.f : 1.f - std::exp(-InDt / InSeconds);
	}

	/** MIDI (con fracción, para vibrato/afinación fina) a Hz. */
	inline float MusicMidiToHz(float InMidiNote)
	{
		return 440.f * std::pow(2.f, (InMidiNote - 69.f) / 12.f);
	}

	/** Saturación suave (aproximación racional de tanh, no pasa de ±1): red de seguridad del limitador. */
	inline float MusicSoftClip(float InX)
	{
		const float C = std::clamp(InX, -3.f, 3.f);
		return C * (27.f + C * C) / (27.f + 9.f * C * C);
	}

	/** Redondea a cero valores subnormales o casi nulos: evita denormales costosos en las recurrencias IIR. */
	inline void MusicFlush(float& InOutValue)
	{
		if (std::fabs(InOutValue) < 1e-20f) { InOutValue = 0.f; }
	}

	/** xorshift32 propio, sin estado global: solo se usa para la textura de la percusión de ruido (shaker, escobillas), nunca para decisiones de composición, así que el resultado es siempre el mismo en cada reproducción. */
	struct FMusicRandom
	{
		uint32_t State = 0x9E3779B9u;

		explicit FMusicRandom(uint32_t InSeed = 0x9E3779B9u) { SetSeed(InSeed); }

		void SetSeed(uint32_t InSeed)
		{
			State = InSeed != 0u ? InSeed : 0x9E3779B9u;
			for (int32_t k = 0; k < 6; ++k) { NextU(); }
		}

		uint32_t NextU()
		{
			uint32_t X = State;
			X ^= X << 13;
			X ^= X >> 17;
			X ^= X << 5;
			State = X;
			return X;
		}

		/** [-1, 1). */
		float Bipolar() { return static_cast<float>(NextU() >> 8) * (1.f / 8388608.f) - 1.f; }
	};

	/** Filtro de un polo (paso bajo y, restando, paso alto). Igual de barato que un acumulador con fuga. */
	struct FMusicOnePole
	{
		float Z = 0.f;
		float K = 1.f;

		void SetHz(float InHz, float InInvRate) { K = 1.f - std::exp(-MusicTwoPi * std::max(1.f, InHz) * InInvRate); }
		float Low(float InX) { Z += K * (InX - Z); return Z; }
		float High(float InX) { Z += K * (InX - Z); return InX - Z; }
		void Flush() { MusicFlush(Z); }
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Envolvente genérica: ataque lineal, sostenido opcional (mientras dura la nota) y caída exponencial.
	// ─────────────────────────────────────────────────────────────────────────

	/** Ataque-sostenido-caída por muestra: sirve tanto para acordes sostenidos (Rhodes) como para notas percusivas (sostenido = 0 s). */
	struct FMusicEnvAsr
	{
		enum class EStage : uint8_t { Idle, Attack, Hold, Release };

		EStage Stage = EStage::Idle;
		float Value = 0.f;
		float AttackStep = 1.f;
		float ReleaseCoef = 0.f;
		float HoldSamplesLeft = 0.f;

		bool IsActive() const { return Stage != EStage::Idle; }

		/** InAttackSeconds hasta el pico, InHoldSeconds a volumen pleno y luego InReleaseSeconds de caída exponencial hasta silencio. */
		void Trigger(float InAttackSeconds, float InHoldSeconds, float InReleaseSeconds, float InRate)
		{
			Stage = EStage::Attack;
			AttackStep = 1.f / std::max(1.f, InAttackSeconds * InRate);
			ReleaseCoef = 1.f - std::exp(-1.f / std::max(1.f, InReleaseSeconds * InRate));
			HoldSamplesLeft = InHoldSeconds * InRate;
		}

		/** Corta el sostenido ya (nota soltada antes de tiempo): pasa directa a la caída. */
		void ForceRelease() { if (Stage != EStage::Idle) { Stage = EStage::Release; } }

		float Process()
		{
			switch (Stage)
			{
			case EStage::Attack:
				Value += AttackStep;
				if (Value >= 1.f) { Value = 1.f; Stage = EStage::Hold; }
				break;
			case EStage::Hold:
				HoldSamplesLeft -= 1.f;
				if (HoldSamplesLeft <= 0.f) { Stage = EStage::Release; }
				break;
			case EStage::Release:
				Value -= Value * ReleaseCoef;
				if (Value < 1e-4f) { Value = 0.f; Stage = EStage::Idle; }
				break;
			default:
				break;
			}
			return Value;
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Voz modal: banco de hasta 4 parciales resonantes (recurrencia de dos polos = seno amortiguado), como un
	// objeto golpeado. Cubre el steel pan y el «ding» (campana), la marimba y la kalimba (madera/lengüeta), el bajo
	// pizzicato/upright (cuerda) y las congas (parche), cada uno con su propio preajuste de parciales.
	// ─────────────────────────────────────────────────────────────────────────

	/** Un parcial: relación con la fundamental, tiempo de caída a -60 dB y amplitud relativa. */
	struct FMusicPartialSpec
	{
		float Ratio = 1.f;
		float T60Seconds = 0.3f;
		float Amp = 1.f;
	};

	/** Preajuste de una voz modal (hasta 4 parciales). Los define GetModalPreset más abajo. */
	struct FMusicModalPreset
	{
		static constexpr int32_t MaxPartials = 4;
		FMusicPartialSpec Partials[MaxPartials];
		int32_t NumPartials = 0;
	};

	struct FMusicModalPartial
	{
		float Y1 = 0.f;
		float Y2 = 0.f;
		float B1 = 0.f;
		float B2 = 0.f;

		void Strike(float InHz, float InT60, float InAmp, float InInvRate)
		{
			const float W = MusicTwoPi * std::min(InHz * InInvRate, 0.45f);
			const float Radius = std::exp(-6.9078f * InInvRate / std::max(0.005f, InT60));
			B1 = 2.f * Radius * std::cos(W);
			B2 = -Radius * Radius;
			Y1 = InAmp * std::sin(W);
			Y2 = 0.f;
		}

		float Tick()
		{
			const float Y = B1 * Y1 + B2 * Y2;
			Y2 = Y1;
			Y1 = Y;
			return Y;
		}

		bool IsActive() const { return std::fabs(Y1) + std::fabs(Y2) > 1e-6f; }

		void Flush()
		{
			MusicFlush(Y1);
			MusicFlush(Y2);
			if (!IsActive()) { Y1 = Y2 = 0.f; }
		}
	};

	/** Objeto golpeado: hasta 4 parciales panoramizados igual, sumados. Se dispara una vez y se apaga solo. */
	struct FMusicModalVoice
	{
		FMusicModalPartial Partials[FMusicModalPreset::MaxPartials];
		int32_t NumPartials = 0;
		float PanL = 0.7071f;
		float PanR = 0.7071f;

		bool IsActive() const
		{
			for (int32_t k = 0; k < NumPartials; ++k) { if (Partials[k].IsActive()) { return true; } }
			return false;
		}

		void Trigger(const FMusicModalPreset& InPreset, float InFreqHz, float InAmp, float InPan, float InInvRate)
		{
			NumPartials = std::min(InPreset.NumPartials, FMusicModalPreset::MaxPartials);
			const float Angle = (std::clamp(InPan, -1.f, 1.f) + 1.f) * (MusicPi * 0.25f);
			PanL = std::cos(Angle);
			PanR = std::sin(Angle);
			for (int32_t k = 0; k < NumPartials; ++k)
			{
				const FMusicPartialSpec& Spec = InPreset.Partials[k];
				Partials[k].Strike(InFreqHz * Spec.Ratio, Spec.T60Seconds, InAmp * Spec.Amp, InInvRate);
			}
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (int32_t k = 0; k < NumPartials; ++k)
			{
				FMusicModalPartial& P = Partials[k];
				if (!P.IsActive()) { continue; }
				for (int32_t i = 0; i < InNumFrames; ++i)
				{
					const float S = P.Tick();
					InOutL[i] += S * PanL;
					InOutR[i] += S * PanR;
				}
				P.Flush();
			}
		}
	};

	/** Identificadores de preajuste modal (ver GetModalPreset). */
	namespace EModalPreset
	{
		constexpr int32_t SteelPan = 0;
		constexpr int32_t RegisterDing = 1;
		constexpr int32_t SparkleChime = 2;
		constexpr int32_t Marimba = 3;
		constexpr int32_t Kalimba = 4;
		constexpr int32_t PizzicatoBass = 5;
		constexpr int32_t UprightBass = 6;
		constexpr int32_t Count = 7;
	}

	inline const FMusicModalPreset& GetModalPreset(int32_t InIndex)
	{
		static const FMusicModalPreset Table[EModalPreset::Count] = {
			// SteelPan: metálico y algo inarmónico (relaciones no enteras), ataque instantáneo (impulso del resonador).
			{ { {1.00f, 0.75f, 1.00f}, {2.00f, 0.55f, 0.50f}, {3.01f, 0.40f, 0.30f}, {4.16f, 0.26f, 0.16f} }, 4 },
			// RegisterDing: campanilla brillante y corta, tipo caja registradora.
			{ { {1.00f, 0.35f, 1.00f}, {2.76f, 0.22f, 0.50f}, {5.40f, 0.12f, 0.25f}, {8.93f, 0.06f, 0.15f} }, 4 },
			// SparkleChime: brillo/glitter mágico, aún más corto y aireado.
			{ { {1.00f, 0.45f, 0.70f}, {2.00f, 0.35f, 0.50f}, {3.00f, 0.28f, 0.38f}, {4.98f, 0.16f, 0.22f} }, 4 },
			// Marimba: madera cálida; el tercer parcial (muy alto y brevísimo) es el «clac» de la maza.
			{ { {1.00f, 0.35f, 1.00f}, {3.93f, 0.18f, 0.32f}, {9.40f, 0.015f, 0.10f} }, 3 },
			// Kalimba: lengüeta metálica, más brillante y algo más larga que la marimba.
			{ { {1.00f, 0.55f, 1.00f}, {2.40f, 0.30f, 0.38f}, {6.80f, 0.03f, 0.16f} }, 3 },
			// Bajo pizzicato: pulsación corta y percusiva (tienda).
			{ { {1.00f, 0.26f, 1.00f}, {2.00f, 0.13f, 0.22f}, {7.00f, 0.015f, 0.14f} }, 3 },
			// Bajo «upright»: pulsación algo más larga y cálida (probador).
			{ { {1.00f, 0.42f, 1.00f}, {2.00f, 0.22f, 0.20f}, {5.50f, 0.025f, 0.09f} }, 3 },
		};
		return Table[std::clamp(InIndex, 0, EModalPreset::Count - 1)];
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Voz FM de dos operadores: piano eléctrico tipo Rhodes (cuerpo senoidal largo + «mordisco» de timbre breve).
	// ─────────────────────────────────────────────────────────────────────────

	struct FMusicFmVoice
	{
		float PhaseCarrier = 0.f;
		float PhaseMod = 0.f;
		float IncCarrier = 0.f;
		float IncMod = 0.f;
		float ModIndex = 0.f;
		float ModIndexCoef = 0.f;
		FMusicEnvAsr BodyEnv;
		FMusicEnvAsr BarkEnv;
		float PanL = 0.7071f;
		float PanR = 0.7071f;

		bool IsActive() const { return BodyEnv.IsActive() || BarkEnv.IsActive(); }

		void Trigger(float InFreqHz, float InAmp, float InLengthSeconds, float InPan, float InInvRate)
		{
			PhaseCarrier = 0.f;
			PhaseMod = 0.f;
			IncCarrier = std::min(InFreqHz * InInvRate, 0.45f);
			IncMod = std::min(InFreqHz * 14.0f * InInvRate, 0.5f);
			ModIndex = 2.2f;
			// El índice de modulación se apaga en ~90 ms: el «mordisco» de tine solo se oye en el ataque.
			ModIndexCoef = 1.f - std::exp(-1.f / (0.09f * std::max(1.f, 1.f / InInvRate)));
			const float Angle = (std::clamp(InPan, -1.f, 1.f) + 1.f) * (MusicPi * 0.25f);
			PanL = std::cos(Angle);
			PanR = std::sin(Angle);
			BodyEnv.Trigger(0.006f, std::max(0.02f, InLengthSeconds), 0.45f, 1.f / InInvRate);
			BarkEnv.Trigger(0.003f, 0.f, 0.11f, 1.f / InInvRate);
			BodyAmp = InAmp;
		}

		/** Suelta antes de tiempo (nota cortada por un acorde nuevo): entra en caída ya. */
		void Release() { BodyEnv.ForceRelease(); }

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				ModIndex -= ModIndex * ModIndexCoef;
				const float Mod = std::sin(PhaseMod * MusicTwoPi) * ModIndex;
				const float Carrier = std::sin(PhaseCarrier * MusicTwoPi + Mod * 0.3f);
				const float Body = BodyEnv.Process();
				const float Bark = BarkEnv.Process();
				const float S = BodyAmp * (Body * std::sin(PhaseCarrier * MusicTwoPi) * 0.75f + (Body + Bark) * Carrier * 0.55f);
				InOutL[i] += S * PanL;
				InOutR[i] += S * PanR;
				PhaseCarrier += IncCarrier;
				if (PhaseCarrier >= 1.f) { PhaseCarrier -= 1.f; }
				PhaseMod += IncMod;
				if (PhaseMod >= 1.f) { PhaseMod -= 1.f; }
			}
		}

	private:
		float BodyAmp = 1.f;
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Voz «silbido»: seno con un poco de aire (ruido pasa-alto) y vibrato; para la melodía del probador.
	// ─────────────────────────────────────────────────────────────────────────

	struct FMusicWhistleVoice
	{
		float Phase = 0.f;
		float Inc = 0.f;
		float VibPhase = 0.f;
		float VibInc = 0.f;
		FMusicEnvAsr Env;
		FMusicOnePole AirFilter;
		FMusicRandom Rng{ 0x51A17F3u };
		float PanL = 0.7071f;
		float PanR = 0.7071f;
		float Amp = 1.f;

		bool IsActive() const { return Env.IsActive(); }

		void Trigger(float InFreqHz, float InAmp, float InLengthSeconds, float InPan, float InInvRate)
		{
			Phase = 0.f;
			Inc = std::min(InFreqHz * InInvRate, 0.45f);
			VibInc = 5.4f * InInvRate;
			const float Angle = (std::clamp(InPan, -1.f, 1.f) + 1.f) * (MusicPi * 0.25f);
			PanL = std::cos(Angle);
			PanR = std::sin(Angle);
			AirFilter.SetHz(2600.f, InInvRate);
			Env.Trigger(0.03f, std::max(0.02f, InLengthSeconds - 0.05f), 0.16f, 1.f / InInvRate);
			Amp = InAmp;
		}

		void Release() { Env.ForceRelease(); }

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				const float E = Env.Process();
				VibPhase += VibInc;
				if (VibPhase >= 1.f) { VibPhase -= 1.f; }
				const float Vibrato = 1.f + 0.006f * std::sin(VibPhase * MusicTwoPi);
				const float Air = AirFilter.High(Rng.Bipolar()) * 0.05f;
				const float S = Amp * E * (std::sin(Phase * MusicTwoPi) * 0.92f + Air);
				InOutL[i] += S * PanL;
				InOutR[i] += S * PanR;
				Phase += Inc * Vibrato;
				if (Phase >= 1.f) { Phase -= 1.f; }
			}
			AirFilter.Flush();
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Percusión de ruido filtrado: shaker/maracas (tienda) y escobillas/hi-hat (probador).
	// ─────────────────────────────────────────────────────────────────────────

	struct FMusicNoiseHit
	{
		FMusicOnePole HighA;
		FMusicOnePole HighB;
		FMusicOnePole Low;
		FMusicRandom Rng;
		float Env = 0.f;
		float Decay = 0.f;
		float Amp = 0.f;

		explicit FMusicNoiseHit(uint32_t InSeed = 0x2545F491u) : Rng(InSeed) {}

		bool IsActive() const { return Env > 1e-4f; }

		/** InHighHz filtra los graves fuera (más alto = más «sh» agudo), InLowHz suaviza el filo, InDecaySeconds es la caída a -60 dB. */
		void Trigger(float InHighHz, float InLowHz, float InDecaySeconds, float InAmp, float InInvRate)
		{
			HighA.SetHz(InHighHz, InInvRate);
			HighB.SetHz(InHighHz, InInvRate);
			Low.SetHz(InLowHz, InInvRate);
			Env = 1.f;
			Decay = 1.f - std::exp(-6.9078f * InInvRate / std::max(0.005f, InDecaySeconds));
			Amp = InAmp;
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				const float N = Rng.Bipolar();
				const float Filtered = Low.Low(HighB.High(HighA.High(N)));
				const float S = Filtered * Env * Amp;
				InOutL[i] += S * 0.72f;
				InOutR[i] += S * 0.72f;
				Env -= Env * Decay;
				if (Env < 1e-4f) { Env = 0.f; }
			}
			HighA.Flush();
			HighB.Flush();
			Low.Flush();
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Depósito de voces de tamaño fijo: sin asignaciones. Roba la voz más antigua si todas están ocupadas.
	// ─────────────────────────────────────────────────────────────────────────

	template <typename TVoice, int32_t NumVoices>
	struct TMusicVoicePool
	{
		TVoice Voices[NumVoices];
		int32_t RobinCursor = 0;

		TVoice& Allocate()
		{
			for (TVoice& V : Voices) { if (!V.IsActive()) { return V; } }
			TVoice& Stolen = Voices[RobinCursor];
			RobinCursor = (RobinCursor + 1) % NumVoices;
			return Stolen;
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (TVoice& V : Voices) { if (V.IsActive()) { V.Render(InOutL, InOutR, InNumFrames); } }
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Datos de la composición: acordes, notas de melodía y patrones rítmicos de cada tema.
	// ─────────────────────────────────────────────────────────────────────────

	/** Tipos de acorde: los intervalos (en semitonos desde la fundamental) están en GetChordIntervals. */
	namespace EChordType
	{
		constexpr uint8_t Maj = 0;
		constexpr uint8_t Min = 1;
		constexpr uint8_t Dom7 = 2;
		constexpr uint8_t Maj7 = 3;
		constexpr uint8_t Min7 = 4;
		constexpr uint8_t Maj9 = 5;
		constexpr uint8_t Min9 = 6;
	}

	constexpr int32_t MusicMaxChordTones = 5;

	struct FMusicChordIntervals
	{
		int8_t Tones[MusicMaxChordTones];
		int32_t NumTones;
	};

	inline const FMusicChordIntervals& GetChordIntervals(uint8_t InType)
	{
		static const FMusicChordIntervals Table[7] = {
			{ {0, 4, 7, 0, 0}, 3 },      // Maj
			{ {0, 3, 7, 0, 0}, 3 },      // Min
			{ {0, 4, 7, 10, 0}, 4 },     // Dom7
			{ {0, 4, 7, 11, 0}, 4 },     // Maj7
			{ {0, 3, 7, 10, 0}, 4 },     // Min7
			{ {0, 4, 7, 11, 14}, 5 },    // Maj9
			{ {0, 3, 7, 10, 14}, 5 },    // Min9
		};
		return Table[std::min<uint8_t>(InType, 6)];
	}

	/** Acorde de un compás: fundamental en semitonos desde la tónica de la canción y tipo. */
	struct FMusicChordSpec
	{
		int8_t RootSemitone;
		uint8_t Type;
	};

	/** Tono InToneIndex del acorde (envuelve a la octava siguiente si se pide más allá del número de notas). */
	inline int32_t MusicChordTone(const FMusicChordSpec& InChord, int32_t InToneIndex)
	{
		const FMusicChordIntervals& Intervals = GetChordIntervals(InChord.Type);
		const int32_t Oct = InToneIndex / Intervals.NumTones;
		const int32_t Idx = InToneIndex % Intervals.NumTones;
		return InChord.RootSemitone + Intervals.Tones[Idx] + Oct * 12;
	}

	/** Nota de melodía: instante y duración en tiempos (negras) dentro de su sección de 8 compases, semitono absoluto desde la tónica de la canción. */
	struct FMusicNoteEvent
	{
		float StartBeat;
		float LengthBeats;
		int8_t Semitone;
		float Velocity;
	};

	/** Paso de acompañamiento (arpegio de acordes o línea de bajo): tono del acorde vigente en vez de altura fija. Se repite cada compás. */
	struct FMusicChordStep
	{
		float StartBeat;
		float LengthBeats;
		int8_t ToneIndex;
		int8_t OctaveOffset;
		float Velocity;
	};

	/** Golpe de percusión afinada (congas): posición fija (no sigue el acorde). Se repite cada compás. */
	struct FMusicPercTonalHit
	{
		float StartBeat;
		bool bHigh;
		float Velocity;
	};

	/** Un acorde entero puesto a sonar a la vez (comping de Rhodes). Se repite cada compás. */
	struct FMusicChordStab
	{
		float StartBeat;
		float LengthBeats;
		float Velocity;
	};

	constexpr int32_t MusicBeatsPerBar = 4;
	constexpr int32_t MusicBarsPerPart = 8;
	constexpr float MusicBeatsPerPart = static_cast<float>(MusicBeatsPerBar * MusicBarsPerPart);
	constexpr int32_t MusicStepsPerBar = 16;

	/** Una sección de 8 compases (A o B): todo lo que hace falta para que la sequenciadora la reproduzca. */
	struct FMusicSongPart
	{
		const FMusicChordSpec* Chords;     // MusicBarsPerPart entradas, una por compás.
		const FMusicNoteEvent* Melody;
		int32_t NumMelody;
		const FMusicChordStep* ChordArp;   // patrón de un compás, se repite leyendo el acorde de cada compás.
		int32_t NumChordArp;
		const FMusicChordStep* Bass;       // patrón de un compás.
		int32_t NumBass;
		const float* PercNoisePattern;     // MusicStepsPerBar valores (0 = sin golpe); shaker o escobillas.
		const FMusicPercTonalHit* PercTonal;
		int32_t NumPercTonal;
		const FMusicChordStab* ChordStabs; // comping de Rhodes (probador); vacío en la tienda.
		int32_t NumChordStabs;
		const FMusicNoteEvent* Accent;     // ding (tienda) o brillos mágicos (probador); semitono relativo al acorde vigente si Sparkle.
		int32_t NumAccent;
		bool bAccentIsSparkle;             // false = ding (campana registradora); true = arpegio ascendente tipo brillo.
	};

	/** La canción entera: forma (orden de secciones), instrumentación y registro de cada parte. */
	struct FMusicSong
	{
		static constexpr int32_t MaxSections = 4;
		const FMusicSongPart* Sections[MaxSections];
		int32_t NumSections;
		float Bpm;
		bool bUseSteelPanMelody;   // true = tienda (steel pan modal); false = probador (silbido).
		bool bUseModalChords;      // true = tienda (marimba/kalimba); false = probador (Rhodes FM).
		bool bBassIsPizzicato;     // preajuste modal del bajo.
		int32_t MelodyBaseMidi;
		int32_t ChordBaseMidi;
		int32_t BassBaseMidi;
		int32_t AccentBaseMidi;
		float NoiseHighHz;         // shaker más agudo que las escobillas.
		float NoiseLowHz;
		float NoiseDecaySeconds;
		float NoiseGain;
		float MixTrim;             // ganancia general de la canción (todos los instrumentos), calibrada en el arnés.
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Composición: «La Concha Dorada» (tienda, Fa mayor, 112 BPM, AABA de 32 compases).
	// ─────────────────────────────────────────────────────────────────────────

	namespace ShopData
	{
		// Fa mayor: F=0 G=2 A=4 Bb=5 C=7 D=9 E=11.
		constexpr int8_t F = 0, G = 2, A = 4, Bb = 5, C = 7, D = 9, E = 11;

		constexpr FMusicChordSpec ChordsA[MusicBarsPerPart] = {
			{ F, EChordType::Maj }, { F, EChordType::Maj }, { Bb, EChordType::Maj }, { Bb, EChordType::Maj },
			{ C, EChordType::Dom7 }, { G, EChordType::Min }, { C, EChordType::Dom7 }, { F, EChordType::Maj },
		};
		// Puente: vi - vi - ii/IV(Cm7) - V7/IV(F7) - IV - IV - V7 - V7 (deja preparada la vuelta al Fa mayor de A).
		constexpr FMusicChordSpec ChordsB[MusicBarsPerPart] = {
			{ D, EChordType::Min }, { D, EChordType::Min }, { C, EChordType::Min7 }, { F, EChordType::Dom7 },
			{ Bb, EChordType::Maj }, { Bb, EChordType::Maj }, { C, EChordType::Dom7 }, { C, EChordType::Dom7 },
		};

		// Melodía de A: pregunta (compases 1-2, sobre I), respuesta (3-4, sobre IV), tensión (5-6, sobre V7/ii) y
		// cierre (7-8) que resuelve en la tónica antes de repetir. Todo diatónico a Fa mayor.
		constexpr FMusicNoteEvent MelodyA[] = {
			{ 0.00f, 0.5f, 12, 0.90f }, { 0.50f, 0.5f, 16, 0.80f }, { 1.00f, 0.5f, 19, 0.85f },
			{ 1.50f, 0.25f, 19, 0.60f }, { 1.75f, 0.25f, 17, 0.60f }, { 2.00f, 1.0f, 16, 0.85f },
			{ 3.00f, 0.5f, 12, 0.65f }, { 3.50f, 0.5f, 14, 0.70f },
			{ 4.00f, 0.5f, 16, 0.85f }, { 4.50f, 0.5f, 19, 0.80f }, { 5.00f, 1.0f, 21, 0.90f },
			{ 6.00f, 0.5f, 19, 0.70f }, { 6.50f, 0.5f, 16, 0.65f }, { 7.00f, 1.5f, 12, 0.80f },
			{ 8.00f, 0.5f, 17, 0.80f }, { 8.50f, 0.5f, 21, 0.75f }, { 9.00f, 1.0f, 24, 0.85f },
			{ 10.00f, 0.5f, 21, 0.65f }, { 10.50f, 0.5f, 19, 0.60f }, { 11.00f, 1.5f, 17, 0.75f },
			{ 12.50f, 0.5f, 19, 0.55f }, { 13.00f, 0.5f, 17, 0.55f }, { 13.50f, 0.5f, 14, 0.50f }, { 14.00f, 2.0f, 12, 0.65f },
			{ 16.00f, 0.5f, 19, 0.85f }, { 16.50f, 0.5f, 23, 0.80f }, { 17.00f, 0.5f, 26, 0.90f },
			{ 17.50f, 0.5f, 23, 0.70f }, { 18.00f, 1.0f, 21, 0.75f }, { 19.00f, 1.0f, 19, 0.70f },
			{ 20.00f, 0.5f, 21, 0.65f }, { 20.50f, 0.5f, 17, 0.60f }, { 21.00f, 1.0f, 14, 0.65f },
			{ 22.00f, 0.5f, 16, 0.60f }, { 22.50f, 0.5f, 12, 0.55f }, { 23.00f, 1.0f, 12, 0.70f },
			{ 24.00f, 0.75f, 12, 0.85f }, { 24.75f, 0.25f, 16, 0.65f }, { 25.00f, 0.5f, 19, 0.80f },
			{ 25.50f, 0.5f, 17, 0.65f }, { 26.00f, 1.0f, 16, 0.80f },
			{ 27.00f, 0.5f, 12, 0.60f }, { 27.50f, 0.5f, 14, 0.60f },
			{ 28.00f, 0.5f, 16, 0.75f }, { 28.50f, 0.5f, 19, 0.70f }, { 29.00f, 1.0f, 17, 0.75f },
			{ 30.00f, 1.0f, 16, 0.65f }, { 31.00f, 1.0f, 12, 0.85f },
		};

		// Melodía del puente: más larga y ligada, con la tensión de las dominantes secundarias.
		constexpr FMusicNoteEvent MelodyB[] = {
			{ 0.00f, 1.5f, 21, 0.75f }, { 1.50f, 0.5f, 19, 0.60f }, { 2.00f, 2.0f, 17, 0.80f },
			{ 4.00f, 1.5f, 19, 0.75f }, { 5.50f, 0.5f, 22, 0.65f }, { 6.00f, 2.0f, 20, 0.80f },
			{ 8.00f, 1.0f, 17, 0.70f }, { 9.00f, 1.0f, 21, 0.75f }, { 10.00f, 2.0f, 24, 0.85f },
			{ 12.00f, 1.0f, 20, 0.70f }, { 13.00f, 1.0f, 17, 0.65f }, { 14.00f, 2.0f, 19, 0.75f },
			{ 16.00f, 2.0f, 17, 0.80f }, { 18.00f, 2.0f, 16, 0.75f },
			{ 20.00f, 2.0f, 14, 0.70f }, { 22.00f, 2.0f, 12, 0.75f },
			{ 24.00f, 1.0f, 19, 0.75f }, { 25.00f, 1.0f, 23, 0.75f }, { 26.00f, 2.0f, 26, 0.85f },
			{ 28.00f, 2.0f, 23, 0.75f }, { 30.00f, 2.0f, 19, 0.80f },
		};

		// Arpegio calipso de marimba/kalimba: rebote 3ª-5ª con acento de octava en el tiempo 3.
		constexpr FMusicChordStep ChordArp[] = {
			{ 0.00f, 0.5f, 0, 0, 0.70f }, { 0.50f, 0.5f, 1, 0, 0.55f }, { 1.00f, 0.5f, 2, 0, 0.65f }, { 1.50f, 0.5f, 1, 0, 0.50f },
			{ 2.00f, 0.5f, 0, 1, 0.60f }, { 2.50f, 0.5f, 2, 0, 0.50f }, { 3.00f, 0.5f, 1, 0, 0.60f }, { 3.50f, 0.5f, 2, 0, 0.50f },
		};

		// Bajo pizzicato: «boom-chick» calipso (1 y el «y» del 2, con paseo a la octava en el 3-4).
		constexpr FMusicChordStep Bass[] = {
			{ 0.00f, 0.9f, 0, 0, 0.90f }, { 1.50f, 0.4f, 2, 0, 0.65f }, { 2.00f, 0.9f, 0, 0, 0.80f }, { 3.00f, 0.9f, 0, 1, 0.60f },
		};

		// Shaker/maracas: corcheas rectas con acento en los «y».
		constexpr float ShakerPattern[MusicStepsPerBar] = {
			0.45f, 0.f, 0.32f, 0.f, 0.45f, 0.f, 0.36f, 0.f, 0.45f, 0.f, 0.32f, 0.f, 0.45f, 0.f, 0.38f, 0.f
		};

		// Congas: dos golpes sincopados por compás.
		constexpr FMusicPercTonalHit Congas[] = {
			{ 1.75f, true, 0.60f }, { 2.50f, false, 0.65f }, { 3.75f, true, 0.55f },
		};

		// Ding de caja registradora: doble golpe al principio de cada A (no en el puente).
		constexpr FMusicNoteEvent DingA[] = { { 0.00f, 0.1f, 24, 1.f }, { 0.15f, 0.1f, 28, 0.8f } };

		constexpr FMusicSongPart PartA = {
			ChordsA, MelodyA, static_cast<int32_t>(std::size(MelodyA)),
			ChordArp, static_cast<int32_t>(std::size(ChordArp)),
			Bass, static_cast<int32_t>(std::size(Bass)),
			ShakerPattern, Congas, static_cast<int32_t>(std::size(Congas)),
			nullptr, 0,
			DingA, static_cast<int32_t>(std::size(DingA)), false
		};
		constexpr FMusicSongPart PartB = {
			ChordsB, MelodyB, static_cast<int32_t>(std::size(MelodyB)),
			ChordArp, static_cast<int32_t>(std::size(ChordArp)),
			Bass, static_cast<int32_t>(std::size(Bass)),
			ShakerPattern, Congas, static_cast<int32_t>(std::size(Congas)),
			nullptr, 0,
			nullptr, 0, false
		};
	}

	inline const FMusicSong& GetShopSong()
	{
		static const FMusicSongPart* Sections[4] = { &ShopData::PartA, &ShopData::PartA, &ShopData::PartB, &ShopData::PartA };
		static const FMusicSong Song = {
			{ Sections[0], Sections[1], Sections[2], Sections[3] }, 4, 112.f,
			/*SteelPan*/ true, /*ModalChords*/ true, /*Pizzicato*/ true,
			/*MelodyBaseMidi F5*/ 65, /*ChordBaseMidi F4*/ 53, /*BassBaseMidi F2*/ 29, /*AccentBaseMidi F6*/ 77,
			/*NoiseHighHz*/ 6500.f, /*NoiseLowHz*/ 3200.f, /*NoiseDecaySeconds*/ 0.055f, /*NoiseGain*/ 0.5f, /*MixTrim*/ 1.55f
		};
		return Song;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Composición: tema del probador (lounge/bossa juguetón, Sol mayor, 96 BPM, A-B-A de 24 compases).
	// ─────────────────────────────────────────────────────────────────────────

	namespace BoothData
	{
		// Sol mayor: G=0 A=2 B=4 C=5 D=7 E=9 Fs=11.
		constexpr int8_t G = 0, A = 2, B = 4, C = 5, D = 7, E = 9, Fs = 11;

		// Vuelta I-vi-ii-V muy clásica de lounge/jazz, dos veces (con IV en medio para variar).
		constexpr FMusicChordSpec ChordsA[MusicBarsPerPart] = {
			{ G, EChordType::Maj9 }, { E, EChordType::Min7 }, { A, EChordType::Min7 }, { D, EChordType::Dom7 },
			{ G, EChordType::Maj9 }, { C, EChordType::Maj7 }, { A, EChordType::Min7 }, { D, EChordType::Dom7 },
		};
		// Puente: IV - iii - V7/vi (dominante prestada, sorpresa «juguetona») - ii - V7.
		constexpr FMusicChordSpec ChordsB[MusicBarsPerPart] = {
			{ C, EChordType::Maj7 }, { C, EChordType::Maj9 }, { B, EChordType::Min7 }, { E, EChordType::Dom7 },
			{ A, EChordType::Min7 }, { A, EChordType::Min9 }, { D, EChordType::Dom7 }, { D, EChordType::Dom7 },
		};

		// Silbido/flauta: frase juguetona con pregunta-respuesta, ligada como una melodía tarareada.
		constexpr FMusicNoteEvent MelodyA[] = {
			{ 0.00f, 1.0f, 19, 0.75f }, { 1.00f, 0.5f, 16, 0.60f }, { 1.50f, 0.5f, 14, 0.60f }, { 2.00f, 1.5f, 12, 0.75f },
			{ 4.00f, 1.0f, 9, 0.65f }, { 5.00f, 0.5f, 12, 0.60f }, { 5.50f, 0.5f, 14, 0.55f }, { 6.00f, 1.5f, 16, 0.70f },
			{ 8.00f, 1.0f, 19, 0.75f }, { 9.00f, 0.5f, 23, 0.65f }, { 9.50f, 0.5f, 21, 0.60f }, { 10.00f, 1.5f, 19, 0.75f },
			{ 12.00f, 1.0f, 17, 0.65f }, { 13.00f, 0.75f, 16, 0.60f }, { 13.75f, 0.75f, 14, 0.55f }, { 14.50f, 1.5f, 12, 0.70f },
			{ 16.00f, 1.0f, 19, 0.75f }, { 17.00f, 0.5f, 16, 0.60f }, { 17.50f, 0.5f, 14, 0.60f }, { 18.00f, 1.5f, 12, 0.75f },
			{ 20.00f, 1.0f, 9, 0.65f }, { 21.00f, 0.5f, 12, 0.60f }, { 21.50f, 0.5f, 14, 0.55f }, { 22.00f, 2.0f, 19, 0.75f },
		};

		constexpr FMusicNoteEvent MelodyB[] = {
			{ 0.00f, 2.0f, 17, 0.70f }, { 2.50f, 1.5f, 21, 0.65f },
			{ 4.00f, 2.0f, 16, 0.70f }, { 6.50f, 1.5f, 20, 0.65f },
			{ 8.00f, 1.0f, 14, 0.60f }, { 9.00f, 1.0f, 17, 0.65f }, { 10.00f, 2.0f, 21, 0.75f },
			{ 12.00f, 2.0f, 20, 0.70f }, { 14.50f, 1.5f, 17, 0.65f },
			{ 16.00f, 1.0f, 12, 0.60f }, { 17.00f, 1.0f, 16, 0.65f }, { 18.00f, 2.0f, 19, 0.70f },
			{ 20.00f, 2.0f, 21, 0.75f }, { 22.50f, 1.5f, 19, 0.70f },
		};

		// Comping de Rhodes: acorde en el 1, síncopa en el «y» del 2 ligada al 3, respiro y remate.
		constexpr FMusicChordStab ChordStabs[] = { { 0.00f, 0.75f, 0.65f }, { 1.50f, 1.40f, 0.55f }, { 3.00f, 0.80f, 0.50f } };

		// Bajo andante: negras, fundamental-5ª-octava-3ª.
		constexpr FMusicChordStep Bass[] = {
			{ 0.00f, 0.85f, 0, 0, 0.75f }, { 1.00f, 0.85f, 2, 0, 0.60f }, { 2.00f, 0.85f, 0, 1, 0.65f }, { 3.00f, 0.85f, 1, 0, 0.55f },
		};

		// Escobillas: corcheas suaves con un leve acento en el «y» del 4.
		constexpr float BrushPattern[MusicStepsPerBar] = {
			0.28f, 0.f, 0.20f, 0.f, 0.26f, 0.f, 0.20f, 0.f, 0.26f, 0.f, 0.20f, 0.f, 0.24f, 0.f, 0.34f, 0.f
		};

		// Brillos mágicos: arpegio ascendente rápido del acorde vigente (semitonos relativos: 0 = tónica del acorde en MusicChordTone).
		constexpr FMusicNoteEvent SparkleA[] = {
			{ 31.00f, 0.2f, 0, 0.55f }, { 31.25f, 0.2f, 4, 0.50f }, { 31.50f, 0.2f, 7, 0.50f }, { 31.75f, 0.25f, 12, 0.55f },
		};
		constexpr FMusicNoteEvent SparkleB[] = {
			{ 27.00f, 0.2f, 0, 0.55f }, { 27.25f, 0.2f, 4, 0.55f }, { 27.50f, 0.2f, 7, 0.50f }, { 27.75f, 0.25f, 10, 0.55f },
		};

		constexpr FMusicSongPart PartA = {
			ChordsA, MelodyA, static_cast<int32_t>(std::size(MelodyA)),
			nullptr, 0,
			Bass, static_cast<int32_t>(std::size(Bass)),
			BrushPattern, nullptr, 0,
			ChordStabs, static_cast<int32_t>(std::size(ChordStabs)),
			SparkleA, static_cast<int32_t>(std::size(SparkleA)), true
		};
		constexpr FMusicSongPart PartB = {
			ChordsB, MelodyB, static_cast<int32_t>(std::size(MelodyB)),
			nullptr, 0,
			Bass, static_cast<int32_t>(std::size(Bass)),
			BrushPattern, nullptr, 0,
			ChordStabs, static_cast<int32_t>(std::size(ChordStabs)),
			SparkleB, static_cast<int32_t>(std::size(SparkleB)), true
		};
	}

	inline const FMusicSong& GetBoothSong()
	{
		static const FMusicSongPart* Sections[3] = { &BoothData::PartA, &BoothData::PartB, &BoothData::PartA };
		static const FMusicSong Song = {
			{ Sections[0], Sections[1], Sections[2], nullptr }, 3, 96.f,
			/*SteelPan*/ false, /*ModalChords*/ false, /*Pizzicato*/ false,
			/*MelodyBaseMidi G5*/ 67, /*ChordBaseMidi G3*/ 43, /*BassBaseMidi G2*/ 31, /*AccentBaseMidi G6*/ 79,
			/*NoiseHighHz*/ 5200.f, /*NoiseLowHz*/ 2200.f, /*NoiseDecaySeconds*/ 0.09f, /*NoiseGain*/ 0.32f, /*MixTrim*/ 0.72f
		};
		return Song;
	}

	inline const FMusicSong& GetSong(int32_t InTrackIndex)
	{
		// 0 = None (silencio; no debería llamarse, pero devuelve la tienda por seguridad), 1 = Shop, 2 = Booth.
		return InTrackIndex == 2 ? GetBoothSong() : GetShopSong();
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Reproductor de una canción: cursores de sección/compás y los ocho depósitos de voces.
	// ─────────────────────────────────────────────────────────────────────────

	class FMusicSongPlayer
	{
	public:
		void Reset(const FMusicSong& InSong, float InRate)
		{
			Song = &InSong;
			Rate = std::max(8000.f, InRate);
			InvRate = 1.f / Rate;
			SectionIdx = 0;
			SectionLocalBeat = 0.0;
			CurBar = -1;
			MelodyCursor = ChordArpCursor = BassCursor = AccentCursor = ChordStabCursor = 0;
			PercStep16Last = -1;
			BeatsPerSecond = InSong.Bpm / 60.f;
		}

		bool IsValid() const { return Song != nullptr; }

		/** Avanza InNumFrames muestras: programa las notas que tocan y hace sonar las voces activas en el bloque. */
		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			if (!Song) { return; }
			const FMusicSongPart* Part = Song->Sections[SectionIdx];
			if (!Part) { SectionIdx = 0; Part = Song->Sections[SectionIdx]; }
			const double DtBeats = static_cast<double>(InNumFrames) * InvRate * BeatsPerSecond;

			const int32_t BarInPart = static_cast<int32_t>(SectionLocalBeat / MusicBeatsPerBar);
			if (BarInPart != CurBar)
			{
				CurBar = BarInPart;
				ChordArpCursor = 0;
				BassCursor = 0;
				ChordStabCursor = 0;
				PercStep16Last = -1;
			}
			const int32_t ClampedBar = std::clamp(CurBar, 0, MusicBarsPerPart - 1);
			const FMusicChordSpec& NowChord = Part->Chords[ClampedBar];
			const float BeatInBar = static_cast<float>(SectionLocalBeat - static_cast<double>(CurBar) * MusicBeatsPerBar);

			while (MelodyCursor < Part->NumMelody && Part->Melody[MelodyCursor].StartBeat <= SectionLocalBeat)
			{
				TriggerMelody(Part->Melody[MelodyCursor]);
				++MelodyCursor;
			}
			while (ChordArpCursor < Part->NumChordArp && Part->ChordArp[ChordArpCursor].StartBeat <= BeatInBar)
			{
				TriggerChordArp(NowChord, Part->ChordArp[ChordArpCursor]);
				++ChordArpCursor;
			}
			while (BassCursor < Part->NumBass && Part->Bass[BassCursor].StartBeat <= BeatInBar)
			{
				TriggerBass(NowChord, Part->Bass[BassCursor]);
				++BassCursor;
			}
			while (ChordStabCursor < Part->NumChordStabs && Part->ChordStabs[ChordStabCursor].StartBeat <= BeatInBar)
			{
				TriggerChordStab(NowChord, Part->ChordStabs[ChordStabCursor]);
				++ChordStabCursor;
			}
			if (Part->PercNoisePattern)
			{
				const int32_t Step16 = std::clamp(static_cast<int32_t>(BeatInBar * (MusicStepsPerBar / MusicBeatsPerBar) + 1e-4f), 0, MusicStepsPerBar - 1);
				if (Step16 != PercStep16Last)
				{
					PercStep16Last = Step16;
					const float Vel = Part->PercNoisePattern[Step16];
					if (Vel > 1e-4f) { TriggerNoise(Vel); }
				}
			}
			for (int32_t k = 0; k < Part->NumPercTonal; ++k)
			{
				// Las congas son pocas por compás: basta comprobar si el paso cayó justo en este bloque.
				const float T = Part->PercTonal[k].StartBeat;
				if (T > BeatInBar - static_cast<float>(DtBeats) && T <= BeatInBar) { TriggerConga(Part->PercTonal[k]); }
			}
			while (AccentCursor < Part->NumAccent && Part->Accent[AccentCursor].StartBeat <= SectionLocalBeat)
			{
				TriggerAccent(NowChord, Part->Accent[AccentCursor], Part->bAccentIsSparkle);
				++AccentCursor;
			}

			SectionLocalBeat += DtBeats;
			if (SectionLocalBeat >= MusicBeatsPerPart)
			{
				SectionLocalBeat -= MusicBeatsPerPart;
				SectionIdx = (SectionIdx + 1) % std::max(1, Song->NumSections);
				while (!Song->Sections[SectionIdx]) { SectionIdx = (SectionIdx + 1) % std::max(1, Song->NumSections); }
				MelodyCursor = 0;
				AccentCursor = 0;
				CurBar = -1;
			}

			for (int32_t i = 0; i < InNumFrames; ++i) { LocalL[i] = 0.f; LocalR[i] = 0.f; }
			MelodyModal.Render(LocalL, LocalR, InNumFrames);
			MelodyWhistle.Render(LocalL, LocalR, InNumFrames);
			ChordModal.Render(LocalL, LocalR, InNumFrames);
			ChordFm.Render(LocalL, LocalR, InNumFrames);
			BassVoices.Render(LocalL, LocalR, InNumFrames);
			PercTonalVoices.Render(LocalL, LocalR, InNumFrames);
			Accents.Render(LocalL, LocalR, InNumFrames);
			PercNoiseVoices.Render(LocalL, LocalR, InNumFrames);
			// Ganancia general de la canción (MixTrim): un único dial calibrado en el arnés en vez de retocar cada
			// instrumento por separado.
			const float Trim = Song->MixTrim;
			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				InOutL[i] += LocalL[i] * Trim;
				InOutR[i] += LocalR[i] * Trim;
			}
		}

	private:
		// Ganancia de cada voz con velocidad 1: calibradas en el arnés fuera del motor (Scripts/scratchpad de
		// validación) para que la mezcla completa (con el limitador puesto) quede entre -18 y -20 dBFS RMS con
		// picos por debajo de -1 dBFS. El comping de Rhodes sposa hasta 5 tonos a la vez, así que lleva el recorte
		// más fuerte de todos.
		static constexpr float kMelodyTrim = 0.40f;
		static constexpr float kChordArpTrim = 0.26f;
		static constexpr float kBassTrim = 0.50f;
		static constexpr float kChordStabTrim = 0.15f;
		static constexpr float kCongaTrim = 0.36f;
		static constexpr float kDingTrim = 0.18f;
		static constexpr float kSparkleTrim = 0.26f;

		void TriggerMelody(const FMusicNoteEvent& InNote)
		{
			const float Hz = MusicMidiToHz(static_cast<float>(Song->MelodyBaseMidi + InNote.Semitone));
			const float LenSec = InNote.LengthBeats / BeatsPerSecond;
			const float Pan = -0.15f;
			const float Amp = InNote.Velocity * kMelodyTrim;
			if (Song->bUseSteelPanMelody)
			{
				MelodyModal.Allocate().Trigger(GetModalPreset(EModalPreset::SteelPan), Hz, Amp, Pan, InvRate);
			}
			else
			{
				MelodyWhistle.Allocate().Trigger(Hz, Amp, LenSec, Pan, InvRate);
			}
		}

		void TriggerChordArp(const FMusicChordSpec& InChord, const FMusicChordStep& InStep)
		{
			const int32_t Semi = MusicChordTone(InChord, InStep.ToneIndex) + InStep.OctaveOffset * 12;
			const float Hz = MusicMidiToHz(static_cast<float>(Song->ChordBaseMidi + Semi));
			const int32_t Preset = Song->bUseModalChords ? (((InStep.ToneIndex + CurBar) & 1) ? EModalPreset::Kalimba : EModalPreset::Marimba) : EModalPreset::Marimba;
			ChordModal.Allocate().Trigger(GetModalPreset(Preset), Hz, InStep.Velocity * kChordArpTrim, 0.35f, InvRate);
		}

		void TriggerBass(const FMusicChordSpec& InChord, const FMusicChordStep& InStep)
		{
			const int32_t Semi = MusicChordTone(InChord, InStep.ToneIndex) + InStep.OctaveOffset * 12;
			const float Hz = MusicMidiToHz(static_cast<float>(Song->BassBaseMidi + Semi));
			const int32_t Preset = Song->bBassIsPizzicato ? EModalPreset::PizzicatoBass : EModalPreset::UprightBass;
			BassVoices.Allocate().Trigger(GetModalPreset(Preset), Hz, InStep.Velocity * kBassTrim, 0.f, InvRate);
		}

		void TriggerChordStab(const FMusicChordSpec& InChord, const FMusicChordStab& InStab)
		{
			const FMusicChordIntervals& Intervals = GetChordIntervals(InChord.Type);
			const float LenSec = InStab.LengthBeats / BeatsPerSecond;
			for (int32_t k = 0; k < Intervals.NumTones; ++k)
			{
				const int32_t Semi = InChord.RootSemitone + Intervals.Tones[k];
				const float Hz = MusicMidiToHz(static_cast<float>(Song->ChordBaseMidi + Semi));
				ChordFm.Allocate().Trigger(Hz, InStab.Velocity * kChordStabTrim, LenSec, 0.3f, InvRate);
			}
		}

		void TriggerConga(const FMusicPercTonalHit& InHit)
		{
			const float Hz = InHit.bHigh ? 260.f : 165.f;
			const int32_t Preset = EModalPreset::Marimba;
			PercTonalVoices.Allocate().Trigger(GetModalPreset(Preset), Hz, InHit.Velocity * kCongaTrim, InHit.bHigh ? 0.5f : -0.5f, InvRate);
		}

		void TriggerNoise(float InVelocity)
		{
			PercNoiseVoices.Allocate().Trigger(Song->NoiseHighHz, Song->NoiseLowHz, Song->NoiseDecaySeconds, InVelocity * Song->NoiseGain, InvRate);
		}

		void TriggerAccent(const FMusicChordSpec& InChord, const FMusicNoteEvent& InNote, bool bInSparkle)
		{
			if (bInSparkle)
			{
				const int32_t Semi = InChord.RootSemitone + InNote.Semitone;
				const float Hz = MusicMidiToHz(static_cast<float>(Song->AccentBaseMidi + Semi));
				Accents.Allocate().Trigger(GetModalPreset(EModalPreset::SparkleChime), Hz, InNote.Velocity * kSparkleTrim, 0.5f, InvRate);
			}
			else
			{
				const float Hz = MusicMidiToHz(static_cast<float>(Song->AccentBaseMidi + InNote.Semitone));
				Accents.Allocate().Trigger(GetModalPreset(EModalPreset::RegisterDing), Hz, InNote.Velocity * kDingTrim, 0.f, InvRate);
			}
		}

		const FMusicSong* Song = nullptr;
		float Rate = 48000.f;
		float InvRate = 1.f / 48000.f;
		float BeatsPerSecond = 112.f / 60.f;
		int32_t SectionIdx = 0;
		double SectionLocalBeat = 0.0;
		int32_t CurBar = -1;
		int32_t MelodyCursor = 0;
		int32_t ChordArpCursor = 0;
		int32_t BassCursor = 0;
		int32_t ChordStabCursor = 0;
		int32_t AccentCursor = 0;
		int32_t PercStep16Last = -1;

		TMusicVoicePool<FMusicModalVoice, 3> MelodyModal;
		TMusicVoicePool<FMusicWhistleVoice, 2> MelodyWhistle;
		TMusicVoicePool<FMusicModalVoice, 4> ChordModal;
		TMusicVoicePool<FMusicFmVoice, 5> ChordFm;
		TMusicVoicePool<FMusicModalVoice, 2> BassVoices;
		TMusicVoicePool<FMusicModalVoice, 2> PercTonalVoices;
		TMusicVoicePool<FMusicModalVoice, 3> Accents;
		TMusicVoicePool<FMusicNoiseHit, 2> PercNoiseVoices;
		float LocalL[MusicBlockFrames] = {};
		float LocalR[MusicBlockFrames] = {};
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Parámetros compartidos con el hilo de juego y motor completo (dos capas en fundido cruzado + limitador).
	// ─────────────────────────────────────────────────────────────────────────

	namespace ETrack
	{
		constexpr uint8_t None = 0;
		constexpr uint8_t Shop = 1;
		constexpr uint8_t Booth = 2;
	}

	struct FMusicSharedParams
	{
		/** Pista a la que PlayTrack/StopMusic quiere llegar (None = silencio). */
		std::atomic<uint8_t> RequestedTrack{ ETrack::None };
		/** Duración del fundido asociado a la última petición (se toma en el instante en que RequestedTrack cambia). */
		std::atomic<float> FadeSeconds{ 0.8f };
		/** Volumen general (0..1), suavizado en el motor. */
		std::atomic<float> Volume{ 1.f };

		static float Get(const std::atomic<float>& InValue) { return InValue.load(std::memory_order_relaxed); }
		static void Set(std::atomic<float>& InValue, float InNewValue) { InValue.store(InNewValue, std::memory_order_relaxed); }
	};

	/** Limitador suave: envolvente de pico (ataque 1,5 ms, caída 200 ms) y saturación suave para lo que se escape. */
	struct FMusicLimiter
	{
		float Env = 0.f;
		float AttackK = 0.f;
		float ReleaseK = 0.f;
		float Threshold = 0.80f;

		void Init(float InRate)
		{
			AttackK = 1.f - std::exp(-1.f / (0.0015f * InRate));
			ReleaseK = 1.f - std::exp(-1.f / (0.2f * InRate));
		}

		void Process(float& InOutL, float& InOutR)
		{
			const float Peak = std::max(std::fabs(InOutL), std::fabs(InOutR));
			Env += (Peak > Env ? AttackK : ReleaseK) * (Peak - Env);
			MusicFlush(Env);
			const float Gain = Env > Threshold ? Threshold / Env : 1.f;
			InOutL = MusicSoftClip(InOutL * Gain);
			InOutR = MusicSoftClip(InOutR * Gain);
		}
	};

	/** Una de las dos capas de reproducción del motor: su reproductor de canción y su posición en el fundido (0..1, curva de potencia constante). */
	struct FMusicLayer
	{
		FMusicSongPlayer Player;
		uint8_t TrackId = ETrack::None;
		float FadePos = 0.f;       // 0..1, lineal; el volumen real aplicado es sin(FadePos * pi/2).
		float FadeTarget = 0.f;
		float FadeStep = 1.f;      // por muestra.

		bool IsSilent() const { return TrackId == ETrack::None || (FadePos <= 1e-4f && FadeTarget <= 1e-4f); }
	};

	/** Motor completo: dos capas en fundido cruzado, volumen general suavizado y limitador de salida. */
	class FMusicEngine
	{
	public:
		void Init(float InRate)
		{
			Rate = std::max(8000.f, InRate);
			InvRate = 1.f / Rate;
			Limiter.Init(Rate);
			Volume = 0.f;
			PrevVolume = 0.f;
			LastRequested = ETrack::None;
		}

		void Render(float* InOut, int32_t InNumFrames, int32_t InOutChannels, const FMusicSharedParams& InParams)
		{
			const int32_t Channels = std::max(1, InOutChannels);
			int32_t Done = 0;
			while (Done < InNumFrames)
			{
				const int32_t N = std::min(MusicBlockFrames, InNumFrames - Done);
				RenderBlock(N, InParams);
				float* Dst = InOut + static_cast<size_t>(Done) * Channels;
				for (int32_t i = 0; i < N; ++i)
				{
					if (Channels == 1)
					{
						Dst[i] = 0.5f * (MixL[i] + MixR[i]);
					}
					else
					{
						Dst[i * Channels] = MixL[i];
						Dst[i * Channels + 1] = MixR[i];
						for (int32_t c = 2; c < Channels; ++c) { Dst[i * Channels + c] = 0.f; }
					}
				}
				Done += N;
			}
		}

	private:
		void RenderBlock(int32_t InN, const FMusicSharedParams& InParams)
		{
			const float Dt = static_cast<float>(InN) * InvRate;
			const uint8_t Requested = InParams.RequestedTrack.load(std::memory_order_relaxed);
			if (Requested != LastRequested)
			{
				LastRequested = Requested;
				const float FadeSeconds = std::max(0.02f, FMusicSharedParams::Get(InParams.FadeSeconds));
				bool bMatched = false;
				for (FMusicLayer& L : Layers)
				{
					if (L.TrackId == Requested && Requested != ETrack::None)
					{
						L.FadeTarget = 1.f;
						L.FadeStep = std::fabs(1.f - L.FadePos) / std::max(1.f, FadeSeconds * Rate);
						bMatched = true;
					}
					else
					{
						L.FadeTarget = 0.f;
						L.FadeStep = std::fabs(0.f - L.FadePos) / std::max(1.f, FadeSeconds * Rate);
					}
				}
				if (!bMatched && Requested != ETrack::None)
				{
					FMusicLayer& L = Layers[0].FadePos <= Layers[1].FadePos ? Layers[0] : Layers[1];
					L.Player.Reset(GetSong(static_cast<int32_t>(Requested)), Rate);
					L.TrackId = Requested;
					L.FadePos = 0.f;
					L.FadeTarget = 1.f;
					L.FadeStep = 1.f / std::max(1.f, FadeSeconds * Rate);
				}
			}

			Volume += (std::clamp(FMusicSharedParams::Get(InParams.Volume), 0.f, 1.5f) - Volume) * MusicTimeCoef(0.25f, Dt);

			for (int32_t i = 0; i < InN; ++i) { MixL[i] = 0.f; MixR[i] = 0.f; }

			for (FMusicLayer& L : Layers)
			{
				if (L.TrackId == ETrack::None) { continue; }
				const float StepSigned = L.FadeTarget > L.FadePos ? L.FadeStep : -L.FadeStep;
				const float NewFadePos = std::clamp(L.FadePos + StepSigned * static_cast<float>(InN), 0.f, 1.f);
				if (NewFadePos <= 1e-4f && L.FadeTarget <= 1e-4f)
				{
					// Ya se ha apagado del todo: libera la capa (no vuelve a costar CPU hasta la próxima petición).
					L.FadePos = 0.f;
					L.TrackId = ETrack::None;
					continue;
				}
				const float GainFrom = std::sin(std::clamp(L.FadePos, 0.f, 1.f) * MusicHalfPi);
				L.FadePos = NewFadePos;
				const float GainTo = std::sin(L.FadePos * MusicHalfPi);
				for (int32_t i = 0; i < InN; ++i) { TmpL[i] = 0.f; TmpR[i] = 0.f; }
				L.Player.Render(TmpL, TmpR, InN);
				const float InvN = 1.f / static_cast<float>(InN);
				float G = GainFrom;
				const float DG = (GainTo - GainFrom) * InvN;
				for (int32_t i = 0; i < InN; ++i)
				{
					G += DG;
					MixL[i] += TmpL[i] * G;
					MixR[i] += TmpR[i] * G;
				}
			}

			const float InvBlock = 1.f / static_cast<float>(InN);
			float Gv = PrevVolume;
			const float DGv = (Volume - PrevVolume) * InvBlock;
			PrevVolume = Volume;
			for (int32_t i = 0; i < InN; ++i)
			{
				Gv += DGv;
				float OutL = MixL[i] * Gv;
				float OutR = MixR[i] * Gv;
				Limiter.Process(OutL, OutR);
				MixL[i] = OutL;
				MixR[i] = OutR;
			}
		}

		float Rate = 48000.f;
		float InvRate = 1.f / 48000.f;
		float Volume = 0.f;
		float PrevVolume = 0.f;
		uint8_t LastRequested = ETrack::None;
		FMusicLayer Layers[2];
		FMusicLimiter Limiter;
		float MixL[MusicBlockFrames] = {};
		float MixR[MusicBlockFrames] = {};
		float TmpL[MusicBlockFrames] = {};
		float TmpR[MusicBlockFrames] = {};
	};
}
