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
 * Motor de música sintetizada de UTN_MusicSynthComponent: temas originales compuestos como secuencias de notas en
 * código («La Concha Dorada» para la tienda, un lounge/bossa para el probador y la música de fin de partida: la
 * fanfarria de victoria, la derrota cartoon y el jingle de eliminado), con instrumentos sintetizados (resonadores
 * modales tipo campana/madera/parche, FM de dos operadores para el Rhodes y los metales, trombón con sordina de
 * diente de sierra filtrado, silbidos, percusión de ruido filtrado) y un mezclador con fundido cruzado entre pistas
 * y limitador suave.
 *
 * Hilos: FMusicSharedParams lo escribe el hilo de juego (atómicos) y lo lee el hilo de render de audio una vez por
 * bloque de control; el hilo de audio solo escribe en él el aviso de «jingle terminado» (FinishedSerial). Todo lo
 * demás (FMusicEngine y lo que cuelga de él) vive y se actualiza solo en el hilo de audio: sin asignaciones, sin
 * bloqueos, sin UObjects. La composición es enteramente determinista (sin azar): el mismo instante de reproducción da
 * siempre la misma señal, lo que permite validarla con un arnés fuera del motor.
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

	/** Semitonos (con fracción) a factor de frecuencia. */
	inline float MusicSemitonesToRatio(float InSemitones)
	{
		return std::exp2(InSemitones * (1.f / 12.f));
	}

	/** Posición panorámica (-1..1) a ganancias de potencia constante. */
	inline void MusicPanGains(float InPan, float& OutL, float& OutR)
	{
		const float Angle = (std::clamp(InPan, -1.f, 1.f) + 1.f) * (MusicPi * 0.25f);
		OutL = std::cos(Angle);
		OutR = std::sin(Angle);
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

	/**
	 * Corrección polyBLEP del salto de un diente de sierra (InT = fase 0..1, InDt = incremento de fase por muestra):
	 * restada a la rampa ingenua, deja un diente de sierra sin aliasing apreciable a las alturas de un trombón.
	 */
	inline float MusicPolyBlep(float InT, float InDt)
	{
		if (InT < InDt)
		{
			const float X = InT / InDt;
			return X + X - X * X - 1.f;
		}
		if (InT > 1.f - InDt)
		{
			const float X = (InT - 1.f) / InDt;
			return X * X + X + X + 1.f;
		}
		return 0.f;
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
	// pizzicato/upright (cuerda), las congas y el timbal (parche) y la caja china (madera hueca), cada uno con su
	// propio preajuste de parciales.
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
		float SinW = 0.f;

		void Strike(float InHz, float InT60, float InAmp, float InInvRate)
		{
			const float W = MusicTwoPi * std::min(InHz * InInvRate, 0.45f);
			const float Radius = std::exp(-6.9078f * InInvRate / std::max(0.005f, InT60));
			B1 = 2.f * Radius * std::cos(W);
			B2 = -Radius * Radius;
			SinW = std::sin(W);
			Y1 = InAmp * SinW;
			Y2 = 0.f;
		}

		/** Vuelve a golpear sin reiniciar: suma un impulso a lo que ya resuena (redobles y trémolos, sin saltos). */
		void Excite(float InAmp) { Y1 += InAmp * SinW; }

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
		float PartialAmps[FMusicModalPreset::MaxPartials] = {};
		int32_t NumPartials = 0;
		float PanL = 0.7071f;
		float PanR = 0.7071f;
		/** Cambia en cada Trigger: un redoble solo vuelve a golpear la voz si nadie se la ha quitado. */
		uint32_t StrikeId = 0;

		bool IsActive() const
		{
			for (int32_t k = 0; k < NumPartials; ++k) { if (Partials[k].IsActive()) { return true; } }
			return false;
		}

		/** Nivel aproximado (para robar la voz más baja cuando no quedan libres). */
		float Level() const
		{
			float Sum = 0.f;
			for (int32_t k = 0; k < NumPartials; ++k) { Sum += std::fabs(Partials[k].Y1) + std::fabs(Partials[k].Y2); }
			return Sum;
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
				PartialAmps[k] = Spec.Amp;
			}
			++StrikeId;
		}

		/** Otro golpe sobre la misma nota sin cortar lo que suena (trémolo del steel pan, redoble de timbal). */
		void Restrike(float InAmp)
		{
			for (int32_t k = 0; k < NumPartials; ++k) { Partials[k].Excite(InAmp * PartialAmps[k]); }
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
		constexpr int32_t Timpani = 7;
		constexpr int32_t WoodBlock = 8;
		constexpr int32_t Count = 9;
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
			// Timbal: parche afinado (modos (1,1), (2,1), (3,1) y (4,1) ≈ 1 : 1,5 : 1,98 : 2,44), cuerpo largo.
			{ { {1.00f, 1.60f, 1.00f}, {1.50f, 1.00f, 0.42f}, {1.98f, 0.70f, 0.26f}, {2.44f, 0.45f, 0.14f} }, 4 },
			// Caja china: madera hueca, seca y muy corta («¡toc!»); también el tic-tac del reloj de la derrota.
			{ { {1.00f, 0.09f, 1.00f}, {2.64f, 0.045f, 0.42f}, {4.12f, 0.02f, 0.22f} }, 3 },
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
		float Level() const { return BodyAmp * (BodyEnv.Value + BarkEnv.Value); }

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
	// Voz FM de metal: la trompeta y el trombón de la fanfarria de victoria.
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Metal FM de dos operadores en relación 1:1 con el índice de modulación siguiendo a la envolvente (más fuerte =
	 * más brillante, como un metal de verdad), entrada del labio un pelín baja que se corrige en ~20 ms, soplo de aire
	 * en el ataque, vibrato que aparece a los ~0,25 s y crecida opcional del sostenido (el crescendo del «tááán»).
	 * Portadora y moduladora comparten fase, así que no hay componente continua. No reinicia la fase al dispararse:
	 * si se roba una voz que aún suena no hay salto en la forma de onda.
	 */
	struct FMusicBrassVoice
	{
		enum class EStage : uint8_t { Idle, Attack, Sustain, Release };

		EStage Stage = EStage::Idle;
		float Env = 0.f;
		float AttackStep = 0.f;
		float SustainNow = 0.8f;
		float SwellStep = 0.f;
		float DecayCoef = 0.f;
		float ReleaseCoef = 0.f;
		float HoldSamplesLeft = 0.f;
		float Phase = 0.f;
		float BaseInc = 0.f;
		float IndexMax = 3.f;
		float ScoopSemis = 0.f;
		float VibPhase = 0.f;
		float Elapsed = 0.f;
		float BreathEnv = 0.f;
		float BreathCoef = 0.f;
		float Amp = 0.f;
		float InvRate = 1.f / 48000.f;
		float PanL = 0.7071f;
		float PanR = 0.7071f;
		FMusicOnePole BreathFilter;
		FMusicRandom Rng{ 0xB4A55u };

		bool IsActive() const { return Stage != EStage::Idle; }
		float Level() const { return Env * Amp; }

		/** InVelocity (0..1) fija el brillo y la rapidez del ataque; InSwell (0..1) el crescendo durante la nota. */
		void Trigger(float InFreqHz, float InAmp, float InVelocity, float InLengthSeconds, float InPan, float InSwell, float InInvRate)
		{
			InvRate = InInvRate;
			const float SampleRate = 1.f / InInvRate;
			const float Vel = MusicClamp01(InVelocity);
			BaseInc = std::min(InFreqHz * InInvRate, 0.45f);
			const float AttackSeconds = 0.022f + 0.02f * (1.f - Vel);
			AttackStep = 1.f / std::max(1.f, AttackSeconds * SampleRate);
			const float Swell = MusicClamp01(InSwell);
			// «fp» si hay crescendo: tras el ataque baja al 60 % y vuelve a subir hasta el final de la nota.
			SustainNow = Swell > 0.f ? 0.6f : 0.82f;
			HoldSamplesLeft = std::max(0.01f, InLengthSeconds - AttackSeconds) * SampleRate;
			SwellStep = Swell * (1.f - SustainNow) / std::max(1.f, HoldSamplesLeft);
			DecayCoef = 1.f - std::exp(-1.f / (0.08f * SampleRate));
			ReleaseCoef = 1.f - std::exp(-1.f / (0.06f * SampleRate));
			IndexMax = 1.5f + 2.3f * Vel;
			ScoopSemis = -0.4f;
			Elapsed = 0.f;
			BreathEnv = 1.f;
			BreathCoef = 1.f - std::exp(-1.f / (0.03f * SampleRate));
			BreathFilter.SetHz(2200.f, InInvRate);
			Amp = InAmp;
			MusicPanGains(InPan, PanL, PanR);
			// La envolvente sube desde donde esté (0 si la voz estaba libre): sin escalones de volumen.
			Stage = EStage::Attack;
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			// Altura del bloque (<1 ms): entrada del labio que se corrige y vibrato que aparece poco a poco.
			const float BlockSeconds = static_cast<float>(InNumFrames) * InvRate;
			ScoopSemis *= std::exp(-BlockSeconds / 0.02f);
			if (std::fabs(ScoopSemis) < 1e-5f) { ScoopSemis = 0.f; }
			VibPhase += 5.2f * BlockSeconds;
			if (VibPhase >= 1.f) { VibPhase -= 1.f; }
			const float VibDepth = 0.0032f * MusicClamp01((Elapsed - 0.22f) / 0.3f);
			const float PitchRatio = MusicSemitonesToRatio(ScoopSemis) * (1.f + VibDepth * std::sin(VibPhase * MusicTwoPi));
			const float Inc = std::min(BaseInc * PitchRatio, 0.45f);
			Elapsed += BlockSeconds;

			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				switch (Stage)
				{
				case EStage::Attack:
					Env += AttackStep;
					if (Env >= 1.f) { Env = 1.f; Stage = EStage::Sustain; }
					break;
				case EStage::Sustain:
					SustainNow = std::min(1.f, SustainNow + SwellStep);
					Env += (SustainNow - Env) * DecayCoef;
					HoldSamplesLeft -= 1.f;
					if (HoldSamplesLeft <= 0.f) { Stage = EStage::Release; }
					break;
				case EStage::Release:
					Env -= Env * ReleaseCoef;
					if (Env < 1e-4f) { Env = 0.f; Stage = EStage::Idle; }
					break;
				default:
					break;
				}
				const float ModDepth = IndexMax * Env;
				const float Mod = std::sin(Phase * MusicTwoPi) * ModDepth;
				const float Tone = std::sin(Phase * MusicTwoPi + Mod);
				const float Breath = BreathFilter.High(Rng.Bipolar()) * BreathEnv * 0.12f;
				BreathEnv -= BreathEnv * BreathCoef;
				const float S = Amp * Env * (Tone + Breath);
				InOutL[i] += S * PanL;
				InOutR[i] += S * PanR;
				Phase += Inc;
				if (Phase >= 1.f) { Phase -= 1.f; }
			}
			BreathFilter.Flush();
			MusicFlush(BreathEnv);
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Trombón con sordina: el «wah-wah-wah-waaah» de la derrota.
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Trombón con sordina de desatascador («plunger»): diente de sierra polyBLEP por un filtro de estado variable
	 * resonante (TPT, estable aunque el corte se mueva) cuyo corte sigue a la sordina: se abre al atacar («u-a», el
	 * «wah»), se cierra al acabar la nota y, con temblor, bate al ritmo del vibrato. La altura entra un pelín baja, cae
	 * al final de la nota (InDroopSemitones, y sigue cayendo mientras se apaga) y el vibrato se ensancha con InWobble
	 * (0..1): el «waaah» tembloroso y hundido de la nota final. Ni la fase ni el filtro se reinician al dispararse.
	 */
	struct FMusicTromboneVoice
	{
		FMusicEnvAsr Env;
		float Phase = 0.f;
		float BaseHz = 220.f;
		float LengthSeconds = 0.5f;
		float Elapsed = 0.f;
		float Wobble = 0.f;
		float DroopSemis = 0.f;
		float VibPhase = 0.f;
		float FilterIc1 = 0.f;
		float FilterIc2 = 0.f;
		float Amp = 0.f;
		float InvRate = 1.f / 48000.f;
		float PanL = 0.7071f;
		float PanR = 0.7071f;

		bool IsActive() const { return Env.IsActive(); }
		float Level() const { return Env.Value * Amp; }

		void Trigger(float InFreqHz, float InAmp, float InLengthSeconds, float InPan, float InWobble, float InDroopSemitones, float InInvRate)
		{
			InvRate = InInvRate;
			BaseHz = InFreqHz;
			LengthSeconds = std::max(0.05f, InLengthSeconds);
			Elapsed = 0.f;
			Wobble = MusicClamp01(InWobble);
			DroopSemis = std::max(0.f, InDroopSemitones);
			VibPhase = 0.f;
			Amp = InAmp;
			MusicPanGains(InPan, PanL, PanR);
			Env.Trigger(0.028f, LengthSeconds, 0.09f, 1.f / InInvRate);
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			// Todo lo expresivo se calcula una vez por bloque (<1 ms): altura, sordina y coeficientes del filtro.
			const float BlockSeconds = static_cast<float>(InNumFrames) * InvRate;
			const float T = Elapsed;
			const float Progress = MusicClamp01(T / LengthSeconds);
			const float ScoopSemis = T < 0.5f ? -0.35f * std::exp(-T / 0.045f) : 0.f;
			// La caída empieza a mitad de nota, acelera y continúa (limitada) mientras la nota se apaga.
			const float DroopT = std::min(1.6f, MusicClamp01((Progress - 0.55f) / 0.45f) + std::max(0.f, (T - LengthSeconds) / 0.3f));
			const float DroopNow = -DroopSemis * DroopT * DroopT;
			const float VibRamp = MusicClamp01((T - 0.16f) / 0.35f);
			const float VibDepthSemis = (0.07f + 0.38f * Wobble) * VibRamp;
			VibPhase += (5.0f + 0.8f * Wobble) * BlockSeconds;
			if (VibPhase >= 1.f) { VibPhase -= 1.f; }
			const float VibSin = std::sin(VibPhase * MusicTwoPi);
			const float PitchHz = BaseHz * MusicSemitonesToRatio(ScoopSemis + DroopNow + VibDepthSemis * VibSin);
			const float Inc = std::min(PitchHz * InvRate, 0.45f);
			// Sordina: se abre en ~110 ms, se cierra en el último 30 % y, con temblor, bate con el vibrato.
			const float PlungerOpen = MusicClamp01(T / 0.11f);
			const float PlungerClose = MusicClamp01((Progress - 0.7f) / 0.3f);
			float WahPos = PlungerOpen * (1.f - 0.7f * PlungerClose);
			WahPos *= 1.f - Wobble * VibRamp * 0.4f * (0.5f + 0.5f * VibSin);
			const float CutoffHz = 360.f * std::exp2(2.2f * WahPos); // 360 Hz (cerrada) .. ~1650 Hz (abierta)
			const float G = std::tan(MusicPi * std::min(CutoffHz * InvRate, 0.45f));
			constexpr float Damping = 0.36f; // Q ≈ 2,8: pico de vocal bien marcado
			const float A1 = 1.f / (1.f + G * (G + Damping));
			const float A2 = G * A1;
			const float A3 = G * A2;
			// La nota final se va apagando mientras se hunde.
			const float DyingGain = 1.f - 0.4f * Wobble * std::min(1.f, DroopT);
			Elapsed += BlockSeconds;

			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				const float E = Env.Process();
				const float Saw = 2.f * Phase - 1.f - MusicPolyBlep(Phase, Inc);
				const float V3 = Saw - FilterIc2;
				const float V1 = A1 * FilterIc1 + A2 * V3;
				const float V2 = FilterIc2 + A2 * FilterIc1 + A3 * V3;
				FilterIc1 = 2.f * V1 - FilterIc1;
				FilterIc2 = 2.f * V2 - FilterIc2;
				const float S = Amp * E * DyingGain * V2;
				InOutL[i] += S * PanL;
				InOutR[i] += S * PanR;
				Phase += Inc;
				if (Phase >= 1.f) { Phase -= 1.f; }
			}
			MusicFlush(FilterIc1);
			MusicFlush(FilterIc2);
		}
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
		float Level() const { return Env.Value * Amp; }

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
	// Silbido expresivo: el silbato de émbolo del jingle de eliminado y el silbido desafinado de la derrota.
	// ─────────────────────────────────────────────────────────────────────────

	/** Carácter de un silbido expresivo. Los valores por defecto son un silbido limpio y afinado. */
	struct FMusicWhistleStyle
	{
		float DetuneCents = 0.f;  // desafinación fija
		float SagCents = 0.f;     // caída de afinación a lo largo de la nota (cuadrática: se nota al final)
		float ScoopCents = 0.f;   // entrada desde otra altura que se corrige en ~50 ms
		float VibRateHz = 5.4f;
		float VibDepth = 0.006f;  // fracción de la frecuencia
		float DriftCents = 0.f;   // vaivén lento (0,7 Hz) de afinación insegura
		float Air = 0.05f;        // soplo (ruido pasa-alto)
		float Harmonic2 = 0.f;    // segundo armónico (timbre más hueco de silbato)
	};

	/**
	 * Seno con aire como FMusicWhistleVoice, más glissando (curva rápida al principio y suave al llegar), desafinación,
	 * caída de afinación, vibrato configurable y ligado: una nota pegada a la anterior desliza desde la altura actual
	 * sin ataque nuevo (Legato), con un bachecito de volumen de 60 ms en forma de seno para que se note la nota.
	 */
	struct FMusicSlideWhistleVoice
	{
		FMusicEnvAsr Env;
		FMusicOnePole AirFilter;
		FMusicRandom Rng{ 0x7E57A11u };
		FMusicWhistleStyle Style;
		float Phase = 0.f;
		float FromHz = 440.f;
		float ToHz = 440.f;
		float GlideSeconds = 0.f;
		float LengthSeconds = 0.5f;
		float Elapsed = 0.f;
		float ScoopNow = 0.f;
		float VibPhase = 0.f;
		float VibFade = 0.f;
		float DriftPhase = 0.f;
		float ArticSeconds = 1.f;
		float DipPrev = 0.f;
		float Amp = 0.f;
		float AmpNow = 0.f;
		float AmpCoef = 1.f;
		float InvRate = 1.f / 48000.f;
		float PanL = 0.7071f;
		float PanR = 0.7071f;
		/** Cambia en cada Trigger o Legato: el reproductor solo liga con una voz que sigue siendo suya. */
		uint32_t NoteId = 0;

		bool IsActive() const { return Env.IsActive(); }
		float Level() const { return Env.Value * AmpNow; }

		/** Altura del glissando en el instante actual (sin vibrato ni desafinación). */
		float GlideHzNow() const
		{
			if (GlideSeconds <= 1e-4f || Elapsed >= GlideSeconds || FromHz <= 0.f) { return ToHz; }
			const float P = Elapsed / GlideSeconds;
			const float Eased = 1.f - (1.f - P) * (1.f - P);
			return FromHz * std::pow(ToHz / FromHz, Eased);
		}

		/** Caída de afinación acumulada en el instante actual (cents). */
		float SagCentsNow() const
		{
			const float NoteProgress = MusicClamp01(Elapsed / LengthSeconds);
			return Style.SagCents * NoteProgress * NoteProgress;
		}

		void Trigger(float InFromHz, float InToHz, float InGlideSeconds, float InAmp, float InLengthSeconds, float InPan,
			const FMusicWhistleStyle& InStyle, float InInvRate)
		{
			InvRate = InInvRate;
			const float SampleRate = 1.f / InInvRate;
			FromHz = InFromHz;
			ToHz = InToHz;
			GlideSeconds = std::max(0.f, InGlideSeconds);
			LengthSeconds = std::max(0.03f, InLengthSeconds);
			Elapsed = 0.f;
			Style = InStyle;
			ScoopNow = InStyle.ScoopCents;
			VibFade = 0.f;
			ArticSeconds = 1.f;
			DipPrev = 0.f;
			// Voz libre: arranca ya a su volumen (la envolvente hace el ataque); robada: el volumen se desliza.
			if (!Env.IsActive()) { AmpNow = InAmp; }
			Amp = InAmp;
			AmpCoef = 1.f - std::exp(-1.f / (0.01f * SampleRate));
			MusicPanGains(InPan, PanL, PanR);
			AirFilter.SetHz(2600.f, InInvRate);
			Env.Trigger(0.03f, std::max(0.02f, LengthSeconds - 0.05f), 0.16f, SampleRate);
			++NoteId;
		}

		/**
		 * Nota ligada: desliza desde la altura que suena ahora (con su caída incluida) hasta InToHz en InGlideSeconds,
		 * sin ataque nuevo; el vibrato y el vaivén siguen donde estaban, así que la altura no da saltos.
		 */
		void Legato(float InToHz, float InGlideSeconds, float InAmp, float InLengthSeconds, const FMusicWhistleStyle& InStyle)
		{
			const float SampleRate = 1.f / InvRate;
			FromHz = GlideHzNow() * std::exp2(SagCentsNow() * (1.f / 1200.f));
			ToHz = InToHz;
			GlideSeconds = std::max(0.f, InGlideSeconds);
			LengthSeconds = std::max(0.03f, InLengthSeconds);
			Elapsed = 0.f;
			Style = InStyle;
			ScoopNow = 0.f;
			ArticSeconds = 0.f;
			Amp = InAmp;
			// La envolvente vuelve a subir desde donde esté (si ya estaba arriba, no se nota): sin escalones.
			Env.Trigger(0.02f, std::max(0.02f, LengthSeconds - 0.05f), 0.16f, SampleRate);
			++NoteId;
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			const float BlockSeconds = static_cast<float>(InNumFrames) * InvRate;
			const float GlideHz = GlideHzNow();
			ScoopNow *= std::exp(-BlockSeconds / 0.05f);
			if (std::fabs(ScoopNow) < 1e-3f) { ScoopNow = 0.f; }
			DriftPhase += 0.7f * BlockSeconds;
			if (DriftPhase >= 1.f) { DriftPhase -= 1.f; }
			const float Cents = Style.DetuneCents + ScoopNow + SagCentsNow() + Style.DriftCents * std::sin(DriftPhase * MusicTwoPi);
			VibPhase += Style.VibRateHz * BlockSeconds;
			if (VibPhase >= 1.f) { VibPhase -= 1.f; }
			// El vibrato aparece en ~150 ms al atacar una nota; en las ligadas sigue como estaba.
			VibFade = std::min(1.f, VibFade + BlockSeconds / 0.15f);
			const float Vibrato = 1.f + Style.VibDepth * VibFade * std::sin(VibPhase * MusicTwoPi);
			const float Inc = std::min(GlideHz * std::exp2(Cents * (1.f / 1200.f)) * Vibrato * InvRate, 0.45f);
			// Bachecito de articulación del ligado (60 ms, forma de seno), interpolado dentro del bloque.
			ArticSeconds += BlockSeconds;
			const float DipNext = ArticSeconds < 0.06f ? 0.28f * std::sin(MusicPi * ArticSeconds / 0.06f) : 0.f;
			const float DipStep = (DipNext - DipPrev) / static_cast<float>(std::max(1, InNumFrames));
			float Dip = DipPrev;
			DipPrev = DipNext;
			Elapsed += BlockSeconds;

			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				const float E = Env.Process();
				AmpNow += (Amp - AmpNow) * AmpCoef;
				Dip += DipStep;
				const float Angle = Phase * MusicTwoPi;
				const float Air = AirFilter.High(Rng.Bipolar()) * Style.Air;
				const float S = AmpNow * E * (1.f - Dip) * (std::sin(Angle) * 0.92f + Style.Harmonic2 * std::sin(2.f * Angle) + Air);
				InOutL[i] += S * PanL;
				InOutR[i] += S * PanR;
				Phase += Inc;
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
		float Level() const { return Env * Amp; }

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

	/** Platillo de choque: ruido estéreo (dos generadores independientes) pasa-banda con un golpe brillante y una cola larga. */
	struct FMusicCymbalVoice
	{
		FMusicOnePole HighAL;
		FMusicOnePole HighBL;
		FMusicOnePole LowL;
		FMusicOnePole HighAR;
		FMusicOnePole HighBR;
		FMusicOnePole LowR;
		FMusicRandom RngL{ 0xC1A5B0u };
		FMusicRandom RngR{ 0x5EA1D7u };
		float Burst = 0.f;
		float Tail = 0.f;
		float BurstCoef = 0.f;
		float TailCoef = 0.f;
		float Amp = 0.f;

		bool IsActive() const { return Tail > 1e-4f || Burst > 1e-4f; }
		float Level() const { return (Burst + Tail) * Amp; }

		/** InDecaySeconds: caída de la cola a -60 dB (un choque largo ronda los 2-3 s). */
		void Trigger(float InAmp, float InDecaySeconds, float InInvRate)
		{
			HighAL.SetHz(2400.f, InInvRate);
			HighBL.SetHz(2400.f, InInvRate);
			LowL.SetHz(12500.f, InInvRate);
			HighAR.SetHz(2400.f, InInvRate);
			HighBR.SetHz(2400.f, InInvRate);
			LowR.SetHz(12500.f, InInvRate);
			Burst = 1.f;
			Tail = 1.f;
			BurstCoef = 1.f - std::exp(-6.9078f * InInvRate / 0.09f);
			TailCoef = 1.f - std::exp(-6.9078f * InInvRate / std::max(0.2f, InDecaySeconds));
			Amp = InAmp;
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				const float NoiseL = LowL.Low(HighBL.High(HighAL.High(RngL.Bipolar())));
				const float NoiseR = LowR.Low(HighBR.High(HighAR.High(RngR.Bipolar())));
				const float CymbalGain = Amp * (0.6f * Burst + 0.4f * Tail);
				InOutL[i] += NoiseL * CymbalGain;
				InOutR[i] += NoiseR * CymbalGain;
				Burst -= Burst * BurstCoef;
				Tail -= Tail * TailCoef;
			}
			if (Burst < 1e-4f) { Burst = 0.f; }
			if (Tail < 1e-4f) { Tail = 0.f; }
			HighAL.Flush();
			HighBL.Flush();
			LowL.Flush();
			HighAR.Flush();
			HighBR.Flush();
			LowR.Flush();
		}
	};

	/** Caja: ruido pasa-banda (la bordonera) más un cuerpo tonal corto que cae de altura. Con caída corta sirve para los golpes del redoble. */
	struct FMusicSnareVoice
	{
		FMusicOnePole HighA;
		FMusicOnePole HighB;
		FMusicOnePole Low;
		FMusicRandom Rng{ 0x5A4E71u };
		float NoiseEnv = 0.f;
		float NoiseCoef = 0.f;
		float BodyEnv = 0.f;
		float BodyCoef = 0.f;
		float BodyPhase = 0.f;
		float BodyInc = 0.f;
		float BodyIncTarget = 0.f;
		float BodyGlide = 0.f;
		float BodyAmount = 0.5f;
		float Amp = 0.f;
		float PanL = 0.7071f;
		float PanR = 0.7071f;

		bool IsActive() const { return NoiseEnv > 1e-4f || BodyEnv > 1e-4f; }
		float Level() const { return (NoiseEnv + BodyEnv) * Amp; }

		void Trigger(float InAmp, float InDecaySeconds, float InBodyAmount, float InPan, float InInvRate)
		{
			if (!IsActive()) { BodyPhase = 0.f; }
			HighA.SetHz(1600.f, InInvRate);
			HighB.SetHz(1600.f, InInvRate);
			Low.SetHz(7500.f, InInvRate);
			NoiseEnv = 1.f;
			NoiseCoef = 1.f - std::exp(-6.9078f * InInvRate / std::max(0.02f, InDecaySeconds));
			BodyEnv = 1.f;
			BodyCoef = 1.f - std::exp(-6.9078f * InInvRate / 0.07f);
			BodyInc = 250.f * InInvRate;
			BodyIncTarget = 180.f * InInvRate;
			BodyGlide = 1.f - std::exp(-InInvRate / 0.012f);
			BodyAmount = InBodyAmount;
			Amp = InAmp;
			MusicPanGains(InPan, PanL, PanR);
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				const float Rattle = Low.Low(HighB.High(HighA.High(Rng.Bipolar())));
				const float Body = std::sin(BodyPhase * MusicTwoPi);
				const float S = Amp * (Rattle * NoiseEnv + Body * BodyEnv * BodyAmount);
				InOutL[i] += S * PanL;
				InOutR[i] += S * PanR;
				NoiseEnv -= NoiseEnv * NoiseCoef;
				BodyEnv -= BodyEnv * BodyCoef;
				BodyInc += (BodyIncTarget - BodyInc) * BodyGlide;
				BodyPhase += BodyInc;
				if (BodyPhase >= 1.f) { BodyPhase -= 1.f; }
			}
			if (NoiseEnv < 1e-4f) { NoiseEnv = 0.f; }
			if (BodyEnv < 1e-4f) { BodyEnv = 0.f; }
			HighA.Flush();
			HighB.Flush();
			Low.Flush();
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Depósito de voces de tamaño fijo: sin asignaciones. Si todas están ocupadas roba una: la siguiente en rueda
	// (tienda y probador, como siempre) o la que menos suena (temas de fin de partida, sin chasquidos audibles).
	// ─────────────────────────────────────────────────────────────────────────

	template <typename TVoice, int32_t MaxVoices>
	struct TMusicVoicePool
	{
		TVoice Voices[MaxVoices];
		int32_t RobinCursor = 0;
		/** Voces en uso (≤ MaxVoices); cada tema la fija al empezar. */
		int32_t Capacity = MaxVoices;
		bool bStealQuietest = false;
		/** Diagnóstico (arnés): voces robadas mientras aún sonaban. */
		int32_t StealCount = 0;

		void Configure(int32_t InCapacity, bool bInStealQuietest)
		{
			Capacity = std::clamp(InCapacity, 1, MaxVoices);
			bStealQuietest = bInStealQuietest;
			RobinCursor = 0;
		}

		/** Silencia y reinicia todas las voces (al empezar un tema en una capa libre). */
		void Clear()
		{
			for (TVoice& V : Voices) { V = TVoice(); }
			RobinCursor = 0;
		}

		TVoice& Allocate()
		{
			for (int32_t k = 0; k < Capacity; ++k) { if (!Voices[k].IsActive()) { return Voices[k]; } }
			++StealCount;
			if (bStealQuietest)
			{
				int32_t Quietest = 0;
				float QuietestLevel = Voices[0].Level();
				for (int32_t k = 1; k < Capacity; ++k)
				{
					const float VoiceLevel = Voices[k].Level();
					if (VoiceLevel < QuietestLevel) { QuietestLevel = VoiceLevel; Quietest = k; }
				}
				return Voices[Quietest];
			}
			TVoice& Stolen = Voices[RobinCursor];
			RobinCursor = (RobinCursor + 1) % Capacity;
			return Stolen;
		}

		bool AnyActive() const
		{
			for (int32_t k = 0; k < Capacity; ++k) { if (Voices[k].IsActive()) { return true; } }
			return false;
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (int32_t k = 0; k < Capacity; ++k) { if (Voices[k].IsActive()) { Voices[k].Render(InOutL, InOutR, InNumFrames); } }
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

	/** Nota de melodía: instante y duración en tiempos (negras) dentro de su sección, semitono absoluto desde la tónica de la canción. */
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

	/** Golpe de percusión afinada (congas, o el tic-tac de caja china de la derrota): posición fija (no sigue el acorde). Se repite cada compás. */
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

	/** Tipos de efecto suelto (FMusicCueEvent): los instrumentos de la música de fin de partida. */
	namespace ECue
	{
		constexpr uint8_t Brass = 0;         // nota de metal FM (Param = crescendo durante la nota, 0..1)
		constexpr uint8_t Trombone = 1;      // trombón con sordina (Param = temblor final 0..1, Param2 = caída al final en semitonos)
		constexpr uint8_t SnareRoll = 2;     // redoble de caja (Velocity = final, Param = inicial, Param2 = golpes por tiempo)
		constexpr uint8_t SnareHit = 3;      // golpe de caja
		constexpr uint8_t Crash = 4;         // platillo de choque (Param = segundos de cola; 0 = 2,2 s)
		constexpr uint8_t Timpani = 5;       // timbal (Semitone = afinación)
		constexpr uint8_t TimpaniRoll = 6;   // redoble de timbal (como SnareRoll, sobre una sola voz que vuelve a golpear)
		constexpr uint8_t SteelPan = 7;
		constexpr uint8_t Marimba = 8;
		constexpr uint8_t Kalimba = 9;
		constexpr uint8_t WoodBlock = 10;    // caja china («¡toc!»)
		constexpr uint8_t Pizz = 11;         // bajo pizzicato suelto
		constexpr uint8_t SlideWhistle = 12; // silbato de émbolo: glissando de Param semitonos en Param2 tiempos
		/** Interno: trémolo del steel pan en las notas largas de la melodía (no se escribe en los datos). */
		constexpr uint8_t SteelPanTremolo = 100;
	}

	/**
	 * Efecto suelto dentro de una sección (las notas de la fanfarria, el trombón, los redobles, el platillo...):
	 * instante y duración en tiempos, semitono desde FMusicSong::CueBaseMidi y dos parámetros según el tipo (ECue).
	 */
	struct FMusicCueEvent
	{
		float StartBeat;
		float LengthBeats;
		uint8_t Type;
		int8_t Semitone;
		float Velocity;
		float Param = 0.f;
		float Param2 = 0.f;
	};

	constexpr int32_t MusicBeatsPerBar = 4;
	constexpr int32_t MusicBarsPerPart = 8;
	constexpr float MusicBeatsPerPart = static_cast<float>(MusicBeatsPerBar * MusicBarsPerPart);
	constexpr int32_t MusicStepsPerBar = 16;

	/**
	 * Una sección (A, B, la fanfarria...): todo lo que hace falta para que la secuenciadora la reproduzca. Los campos
	 * añadidos para la música de fin de partida llevan valores por defecto (sección de 8 compases, sin efectos sueltos
	 * y a ganancia 1), así que las secciones de la tienda y el probador no cambian.
	 */
	struct FMusicSongPart
	{
		const FMusicChordSpec* Chords;     // un acorde por compás (ceil(LengthBeats / 4) entradas).
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
		float LengthBeats = MusicBeatsPerPart;   // duración de la sección en tiempos (la fanfarria es más corta).
		const FMusicCueEvent* Cues = nullptr;    // efectos sueltos, ordenados por StartBeat.
		int32_t NumCues = 0;
		float PartGain = 1.f;                    // ganancia de todo lo que se dispara en la sección (en el disparo: sin saltos).
	};

	/** Compases de una sección (al menos 1): el último puede quedar incompleto si LengthBeats no es múltiplo de 4. */
	inline int32_t MusicPartBars(const FMusicSongPart& InPart)
	{
		return std::max(1, static_cast<int32_t>(std::ceil(InPart.LengthBeats / static_cast<float>(MusicBeatsPerBar) - 1e-3f)));
	}

	/**
	 * La canción entera: forma (orden de secciones), instrumentación y registro de cada parte. Los campos añadidos para
	 * la música de fin de partida llevan valores por defecto que reproducen el comportamiento de siempre.
	 */
	struct FMusicSong
	{
		static constexpr int32_t MaxSections = 6;
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
		/** Sección a la que se vuelve tras la última (la fanfarria o el «wah-wah» se tocan una vez); -1 = jingle sin bucle. */
		int32_t LoopStartSection = 0;
		/** Nota MIDI de referencia de los efectos sueltos (FMusicCueEvent::Semitone). */
		int32_t CueBaseMidi = 48;
		/** Melodía de silbido desafinado y ligado (derrota) en vez del silbido limpio del probador. */
		bool bMelodyOutOfTune = false;
		/** Notas largas del steel pan con trémolo (golpes repetidos sobre la misma nota, como un panista de verdad). */
		bool bSteelPanTremolo = false;
		/** Depósitos de voces más holgados y robo de la voz más baja (sin chasquidos al robar). */
		bool bRoomyVoicePools = false;
		/** Percusión afinada por compás: preajuste modal y alturas (congas de serie; caja china en la derrota). */
		int32_t PercTonalPreset = EModalPreset::Marimba;
		float PercTonalHighHz = 260.f;
		float PercTonalLowHz = 165.f;
		/** Fundido de entrada máximo: los jingles entran enseguida para no perder el golpe inicial. */
		float MaxFadeInSeconds = 1000.f;
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

	// ─────────────────────────────────────────────────────────────────────────
	// Composición: victoria («¡ta-ta-ta-tááán!» y bucle festivo; Si bemol mayor, 120 BPM).
	// Forma: fanfarria de 10 tiempos (5 s) una sola vez y luego A-A-B-A de 8 compases en bucle (64 s).
	// ─────────────────────────────────────────────────────────────────────────

	namespace VictoryData
	{
		// Si bemol mayor: Bb=0 C=2 D=4 Eb=5 F=7 G=9 A=11.
		constexpr int8_t Bb = 0, C = 2, D = 4, Eb = 5, F = 7, G = 9, A = 11;

		// La fanfarria no usa patrones por compás, pero cada sección lleva un acorde por compás: Fa7 y Si bemol.
		constexpr FMusicChordSpec ChordsFanfare[3] = { { F, EChordType::Dom7 }, { Bb, EChordType::Maj }, { Bb, EChordType::Maj } };

		// Fanfarria (semitonos desde Si bemol 2, MIDI 46): redoble de caja en crescendo con el timbal en Fa por debajo,
		// «ta-ta-ta» en tresillo de metales sobre Fa (V), «tááán» en Si bemol (I) con platillo, timbal y crescendo
		// hasta el «¡pam!» final, y una carrerilla de steel pan que desemboca en el bucle.
		constexpr FMusicCueEvent FanfareCues[] = {
			{ 0.000f, 2.00f, ECue::SnareRoll, 0, 0.80f, 0.10f, 8.f },
			{ 0.500f, 1.50f, ECue::TimpaniRoll, -5, 0.50f, 0.10f, 6.f },   // Fa 2
			{ 2.000f, 0.20f, ECue::Brass, 7, 0.76f },                      // Fa 3
			{ 2.000f, 0.20f, ECue::Brass, 11, 0.72f },                     // La 3
			{ 2.000f, 0.20f, ECue::Brass, 14, 0.74f },                     // Do 4
			{ 2.000f, 0.20f, ECue::Brass, 19, 0.80f },                     // Fa 4 (voz de arriba: «ta...»)
			{ 2.000f, 0.00f, ECue::SnareHit, 0, 0.55f },
			{ 2.333f, 0.20f, ECue::Brass, 7, 0.78f },
			{ 2.333f, 0.20f, ECue::Brass, 11, 0.74f },
			{ 2.333f, 0.20f, ECue::Brass, 14, 0.76f },
			{ 2.333f, 0.20f, ECue::Brass, 19, 0.82f },
			{ 2.333f, 0.00f, ECue::SnareHit, 0, 0.50f },
			{ 2.667f, 0.20f, ECue::Brass, 7, 0.80f },
			{ 2.667f, 0.20f, ECue::Brass, 11, 0.76f },
			{ 2.667f, 0.20f, ECue::Brass, 14, 0.78f },
			{ 2.667f, 0.20f, ECue::Brass, 19, 0.85f },
			{ 2.667f, 0.00f, ECue::SnareHit, 0, 0.60f },
			{ 3.000f, 3.75f, ECue::Brass, 0, 0.88f, 0.8f },                // Si bemol 2 («...tááán»)
			{ 3.000f, 3.75f, ECue::Brass, 7, 0.84f, 0.8f },                // Fa 3
			{ 3.000f, 3.75f, ECue::Brass, 16, 0.86f, 0.8f },               // Re 4
			{ 3.000f, 3.75f, ECue::Brass, 19, 0.86f, 0.8f },               // Fa 4
			{ 3.000f, 3.75f, ECue::Brass, 24, 0.95f, 0.8f },               // Si bemol 4 (voz de arriba)
			{ 3.000f, 0.00f, ECue::Crash, 0, 0.85f, 2.4f },
			{ 3.000f, 0.00f, ECue::Timpani, 0, 0.95f },                    // Si bemol 2
			{ 3.000f, 0.00f, ECue::SnareHit, 0, 0.75f },
			{ 5.000f, 1.80f, ECue::TimpaniRoll, 0, 0.75f, 0.15f, 6.f },
			{ 7.000f, 0.35f, ECue::Brass, 0, 1.00f },                      // «¡pam!»
			{ 7.000f, 0.35f, ECue::Brass, 7, 0.95f },
			{ 7.000f, 0.35f, ECue::Brass, 16, 0.95f },
			{ 7.000f, 0.35f, ECue::Brass, 24, 1.00f },
			{ 7.000f, 0.35f, ECue::Brass, 28, 1.00f },                     // Re 5
			{ 7.000f, 0.00f, ECue::Crash, 0, 1.00f, 2.8f },
			{ 7.000f, 0.00f, ECue::Timpani, 0, 1.00f },
			{ 7.000f, 0.00f, ECue::SnareHit, 0, 0.90f },
			{ 8.500f, 0.25f, ECue::SteelPan, 19, 0.50f },                  // Fa 4 ... carrerilla hacia el bucle
			{ 8.750f, 0.25f, ECue::SteelPan, 21, 0.54f },                  // Sol 4
			{ 9.000f, 0.25f, ECue::SteelPan, 23, 0.58f },                  // La 4
			{ 9.250f, 0.25f, ECue::SteelPan, 24, 0.62f },                  // Si bemol 4
			{ 9.500f, 0.25f, ECue::SteelPan, 26, 0.66f },                  // Do 5
			{ 9.750f, 0.25f, ECue::SteelPan, 28, 0.70f },                  // Re 5
		};

		// Bucle: I - IV - V7 - I - vi - ii7 - V7 - I y un puente IV - IVmaj7 - iii - vi - ii7 - V7 - I - V7 que vuelve a A.
		constexpr FMusicChordSpec ChordsA[MusicBarsPerPart] = {
			{ Bb, EChordType::Maj }, { Eb, EChordType::Maj }, { F, EChordType::Dom7 }, { Bb, EChordType::Maj },
			{ G, EChordType::Min }, { C, EChordType::Min7 }, { F, EChordType::Dom7 }, { Bb, EChordType::Maj },
		};
		constexpr FMusicChordSpec ChordsB[MusicBarsPerPart] = {
			{ Eb, EChordType::Maj }, { Eb, EChordType::Maj7 }, { D, EChordType::Min }, { G, EChordType::Min },
			{ C, EChordType::Min7 }, { F, EChordType::Dom7 }, { Bb, EChordType::Maj }, { F, EChordType::Dom7 },
		};

		// Steel pan (semitonos desde Si bemol 4, MIDI 70): calipso cantable con el 3+3+2 del género (negra con puntillo,
		// negra con puntillo, negra); las notas de tiempo y medio o más se tocan con trémolo.
		constexpr FMusicNoteEvent MelodyA[] = {
			{ 0.00f, 0.75f, 7, 0.85f }, { 0.75f, 0.75f, 4, 0.70f }, { 1.50f, 0.50f, 7, 0.75f }, { 2.00f, 1.00f, 9, 0.80f },
			{ 3.00f, 0.50f, 7, 0.65f }, { 3.50f, 0.50f, 4, 0.60f },
			{ 4.00f, 0.75f, 5, 0.80f }, { 4.75f, 0.75f, 9, 0.70f }, { 5.50f, 1.00f, 12, 0.85f }, { 6.50f, 0.50f, 9, 0.60f },
			{ 7.00f, 1.00f, 5, 0.70f },
			{ 8.00f, 0.50f, 2, 0.75f }, { 8.50f, 0.50f, 5, 0.70f }, { 9.00f, 0.75f, 11, 0.80f }, { 9.75f, 0.25f, 9, 0.60f },
			{ 10.00f, 1.00f, 7, 0.75f }, { 11.00f, 0.50f, 5, 0.60f }, { 11.50f, 0.50f, 2, 0.55f },
			{ 12.00f, 1.50f, 4, 0.80f }, { 13.50f, 0.50f, 0, 0.60f }, { 14.00f, 0.50f, 4, 0.65f }, { 14.50f, 0.50f, 7, 0.70f },
			{ 15.50f, 0.50f, 7, 0.60f },
			{ 16.00f, 0.75f, 9, 0.85f }, { 16.75f, 0.75f, 4, 0.70f }, { 17.50f, 0.50f, 9, 0.75f }, { 18.00f, 0.50f, 11, 0.70f },
			{ 18.50f, 1.00f, 12, 0.80f }, { 19.50f, 0.50f, 11, 0.60f },
			{ 20.00f, 0.75f, 9, 0.75f }, { 20.75f, 0.75f, 5, 0.65f }, { 21.50f, 0.50f, 9, 0.70f }, { 22.00f, 1.00f, 7, 0.70f },
			{ 23.00f, 0.50f, 5, 0.60f }, { 23.50f, 0.50f, 4, 0.60f },
			{ 24.00f, 0.50f, 2, 0.70f }, { 24.50f, 0.50f, 4, 0.65f }, { 25.00f, 0.50f, 5, 0.70f }, { 25.50f, 0.50f, 7, 0.70f },
			{ 26.00f, 1.00f, 11, 0.80f }, { 27.00f, 0.50f, 14, 0.75f }, { 27.50f, 0.50f, 11, 0.65f },
			{ 28.00f, 1.50f, 12, 0.85f }, { 29.50f, 0.50f, 7, 0.60f }, { 30.00f, 1.00f, 4, 0.70f },
		};

		// Puente: más lírico, con notas largas en trémolo que suben hasta el Mi bemol 6 de la dominante.
		constexpr FMusicNoteEvent MelodyB[] = {
			{ 0.00f, 1.50f, 9, 0.75f }, { 1.50f, 0.50f, 7, 0.60f }, { 2.00f, 1.00f, 5, 0.70f }, { 3.00f, 0.50f, 7, 0.60f },
			{ 3.50f, 0.50f, 9, 0.65f },
			{ 4.00f, 2.00f, 12, 0.80f }, { 6.00f, 1.00f, 9, 0.65f }, { 7.50f, 0.50f, 7, 0.60f },
			{ 8.00f, 1.50f, 7, 0.75f }, { 9.50f, 0.50f, 4, 0.60f }, { 10.00f, 1.00f, 11, 0.75f }, { 11.00f, 0.50f, 7, 0.60f },
			{ 11.50f, 0.50f, 11, 0.65f },
			{ 12.00f, 2.00f, 12, 0.80f }, { 14.00f, 0.50f, 11, 0.60f }, { 14.50f, 0.50f, 9, 0.60f }, { 15.00f, 1.00f, 4, 0.65f },
			{ 16.00f, 1.00f, 5, 0.70f }, { 17.00f, 1.00f, 9, 0.70f }, { 18.00f, 1.50f, 12, 0.75f }, { 19.50f, 0.50f, 9, 0.60f },
			{ 20.00f, 1.00f, 11, 0.75f }, { 21.00f, 1.00f, 14, 0.75f }, { 22.00f, 1.50f, 17, 0.80f }, { 23.50f, 0.50f, 14, 0.60f },
			{ 24.00f, 2.00f, 16, 0.85f }, { 26.00f, 1.00f, 12, 0.70f }, { 27.00f, 1.00f, 7, 0.65f },
			{ 28.00f, 1.00f, 11, 0.70f }, { 29.00f, 1.00f, 14, 0.70f }, { 30.00f, 0.50f, 17, 0.65f }, { 30.50f, 0.50f, 14, 0.60f },
			{ 31.00f, 0.50f, 11, 0.60f },
		};

		// Marimba/kalimba suave: fundamental, rebote 5ª-3ª y octava en el 4, con hueco en el tiempo 3.
		constexpr FMusicChordStep ChordArp[] = {
			{ 0.00f, 0.5f, 0, 0, 0.55f }, { 0.50f, 0.5f, 2, 0, 0.40f }, { 1.00f, 0.5f, 1, 0, 0.45f }, { 1.50f, 0.5f, 2, 0, 0.42f },
			{ 2.50f, 0.5f, 1, 0, 0.45f }, { 3.00f, 0.5f, 0, 1, 0.50f }, { 3.50f, 0.5f, 2, 0, 0.40f },
		};

		// Bajo «upright» calipso: fundamental, 5ª en el «y» del 2, octava, 5ª y 3ª que llevan al compás siguiente.
		constexpr FMusicChordStep Bass[] = {
			{ 0.00f, 0.90f, 0, 0, 0.85f }, { 1.50f, 0.45f, 2, 0, 0.55f }, { 2.00f, 0.90f, 0, 1, 0.65f }, { 3.00f, 0.45f, 2, 0, 0.50f },
			{ 3.50f, 0.45f, 1, 0, 0.50f },
		};

		// Shaker suave (semicorcheas ligeras).
		constexpr float ShakerPattern[MusicStepsPerBar] = {
			0.30f, 0.f, 0.16f, 0.08f, 0.28f, 0.f, 0.18f, 0.08f, 0.30f, 0.f, 0.16f, 0.08f, 0.28f, 0.f, 0.20f, 0.10f
		};

		// Congas suaves.
		constexpr FMusicPercTonalHit Congas[] = {
			{ 1.50f, true, 0.40f }, { 2.50f, false, 0.45f }, { 3.50f, true, 0.35f }, { 3.75f, true, 0.30f },
		};

		// Brillo de celebración al final de cada A (arpegio del acorde vigente, Si bemol).
		constexpr FMusicNoteEvent SparkleEnd[] = {
			{ 30.00f, 0.2f, 0, 0.45f }, { 30.25f, 0.2f, 4, 0.42f }, { 30.50f, 0.2f, 7, 0.42f }, { 30.75f, 0.25f, 12, 0.45f },
		};

		// Ganancia del bucle: festivo pero suave, un par de decibelios por debajo de la fanfarria.
		constexpr float LoopGain = 0.84f;

		constexpr FMusicSongPart PartFanfare = {
			.Chords = ChordsFanfare,
			.LengthBeats = 10.f,
			.Cues = FanfareCues,
			.NumCues = static_cast<int32_t>(std::size(FanfareCues)),
		};
		constexpr FMusicSongPart PartA = {
			.Chords = ChordsA,
			.Melody = MelodyA,
			.NumMelody = static_cast<int32_t>(std::size(MelodyA)),
			.ChordArp = ChordArp,
			.NumChordArp = static_cast<int32_t>(std::size(ChordArp)),
			.Bass = Bass,
			.NumBass = static_cast<int32_t>(std::size(Bass)),
			.PercNoisePattern = ShakerPattern,
			.PercTonal = Congas,
			.NumPercTonal = static_cast<int32_t>(std::size(Congas)),
			.Accent = SparkleEnd,
			.NumAccent = static_cast<int32_t>(std::size(SparkleEnd)),
			.bAccentIsSparkle = true,
			.PartGain = LoopGain,
		};
		constexpr FMusicSongPart PartB = {
			.Chords = ChordsB,
			.Melody = MelodyB,
			.NumMelody = static_cast<int32_t>(std::size(MelodyB)),
			.ChordArp = ChordArp,
			.NumChordArp = static_cast<int32_t>(std::size(ChordArp)),
			.Bass = Bass,
			.NumBass = static_cast<int32_t>(std::size(Bass)),
			.PercNoisePattern = ShakerPattern,
			.PercTonal = Congas,
			.NumPercTonal = static_cast<int32_t>(std::size(Congas)),
			.PartGain = LoopGain,
		};
	}

	inline const FMusicSong& GetVictorySong()
	{
		static const FMusicSong Song = {
			.Sections = { &VictoryData::PartFanfare, &VictoryData::PartA, &VictoryData::PartA, &VictoryData::PartB, &VictoryData::PartA },
			.NumSections = 5,
			.Bpm = 120.f,
			.bUseSteelPanMelody = true,
			.bUseModalChords = true,
			.bBassIsPizzicato = false,
			.MelodyBaseMidi = 70,   // Si bemol 4
			.ChordBaseMidi = 58,    // Si bemol 3
			.BassBaseMidi = 34,     // Si bemol 1
			.AccentBaseMidi = 82,   // Si bemol 5
			.NoiseHighHz = 6800.f,
			.NoiseLowHz = 3400.f,
			.NoiseDecaySeconds = 0.05f,
			.NoiseGain = 0.40f,
			.MixTrim = 1.30f,
			.LoopStartSection = 1,
			.CueBaseMidi = 46,      // Si bemol 2
			.bSteelPanTremolo = true,
			.bRoomyVoicePools = true,
			.MaxFadeInSeconds = 0.03f,
		};
		return Song;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Composición: derrota («wah-wah-wah-waaah» y bucle tristón; Re menor, 72 BPM).
	// Forma: trombón de 5 tiempos (4,2 s) una sola vez y luego A-B de 8 compases en bucle (53 s).
	// ─────────────────────────────────────────────────────────────────────────

	namespace DefeatData
	{
		// Re menor (con el Do sostenido de la dominante): D=0 E=2 F=3 G=5 A=7 Bb=8 C=10 Cs=11.
		constexpr int8_t D = 0, E = 2, F = 3, G = 5, A = 7, Bb = 8, C = 10, Cs = 11;

		constexpr FMusicChordSpec ChordsWah[2] = { { A, EChordType::Dom7 }, { A, EChordType::Dom7 } };

		// Trombón con sordina (semitonos desde Re 3, MIDI 50): Do 4 - Si 3 - Si bemol 3 - La 3, cromático hacia abajo,
		// cada «wah» cayendo un poco al final y el último largo, tembloroso y hundido, sobre un pizzicato de La.
		constexpr FMusicCueEvent WahCues[] = {
			{ 0.000f, 0.52f, ECue::Trombone, 10, 0.80f, 0.f, 0.35f },  // Do 4
			{ 0.625f, 0.52f, ECue::Trombone, 9, 0.78f, 0.f, 0.35f },   // Si 3
			{ 1.250f, 0.52f, ECue::Trombone, 8, 0.76f, 0.f, 0.45f },   // Si bemol 3
			{ 1.875f, 2.30f, ECue::Trombone, 7, 0.84f, 1.f, 1.6f },    // La 3: «waaah»
			{ 1.875f, 0.00f, ECue::Pizz, -5, 0.45f },                  // La 2
		};

		// Bucle: el bajo de lamento (i - VII - VI - V7) y un puente iv - VII - III - VImaj7 - iv - V7 - i - V7.
		constexpr FMusicChordSpec ChordsA[MusicBarsPerPart] = {
			{ D, EChordType::Min }, { C, EChordType::Maj }, { Bb, EChordType::Maj }, { A, EChordType::Dom7 },
			{ D, EChordType::Min }, { G, EChordType::Min }, { A, EChordType::Dom7 }, { D, EChordType::Min },
		};
		constexpr FMusicChordSpec ChordsB[MusicBarsPerPart] = {
			{ G, EChordType::Min7 }, { C, EChordType::Maj }, { F, EChordType::Maj }, { Bb, EChordType::Maj7 },
			{ G, EChordType::Min }, { A, EChordType::Dom7 }, { D, EChordType::Min }, { A, EChordType::Dom7 },
		};

		// Silbido desafinado (semitonos desde Re 5, MIDI 74): frase sencilla y cansina, con apoyaturas tristes (el Si
		// bemol sobre el acorde de Re menor, el Fa sobre La7) y notas largas que se le caen al final.
		constexpr FMusicNoteEvent MelodyA[] = {
			{ 0.00f, 1.50f, 7, 0.70f }, { 1.50f, 0.50f, 3, 0.55f }, { 2.00f, 1.50f, 0, 0.65f }, { 3.50f, 0.50f, 2, 0.50f },
			{ 4.00f, 1.00f, 2, 0.60f }, { 5.00f, 1.00f, 0, 0.55f }, { 6.00f, 1.50f, -2, 0.60f },
			{ 8.00f, 1.00f, 0, 0.60f }, { 9.00f, 0.50f, 3, 0.55f }, { 9.50f, 0.50f, 0, 0.50f }, { 10.00f, 1.50f, -4, 0.60f },
			{ 11.50f, 0.50f, -2, 0.50f },
			{ 12.00f, 1.00f, -1, 0.60f }, { 13.00f, 1.00f, 2, 0.60f }, { 14.00f, 2.00f, -5, 0.60f },
			{ 16.00f, 1.00f, 7, 0.70f }, { 17.00f, 0.50f, 7, 0.60f }, { 17.50f, 0.50f, 8, 0.60f }, { 18.00f, 1.00f, 7, 0.65f },
			{ 19.00f, 1.00f, 3, 0.55f },
			{ 20.00f, 1.50f, 5, 0.65f }, { 21.50f, 0.50f, 3, 0.50f }, { 22.00f, 1.50f, 0, 0.60f }, { 23.50f, 0.50f, 2, 0.50f },
			{ 24.00f, 1.00f, 3, 0.60f }, { 25.00f, 1.00f, 2, 0.55f }, { 26.00f, 1.00f, -1, 0.55f }, { 27.00f, 1.00f, 2, 0.50f },
			{ 28.00f, 3.00f, 0, 0.60f },
		};

		// Puente: intenta animarse (sube al Re 6) y se desinfla; en el último compás calla y contesta el trombón.
		constexpr FMusicNoteEvent MelodyB[] = {
			{ 0.00f, 1.50f, 12, 0.65f }, { 1.50f, 0.50f, 8, 0.50f }, { 2.00f, 1.50f, 5, 0.60f }, { 3.50f, 0.50f, 7, 0.50f },
			{ 4.00f, 1.00f, 5, 0.60f }, { 5.00f, 1.00f, 2, 0.55f }, { 6.00f, 1.50f, 10, 0.60f },
			{ 8.00f, 1.50f, 7, 0.65f }, { 9.50f, 0.50f, 3, 0.50f }, { 10.00f, 2.00f, 10, 0.60f },
			{ 12.00f, 1.00f, 12, 0.60f }, { 13.00f, 0.50f, 10, 0.55f }, { 13.50f, 0.50f, 8, 0.55f }, { 14.00f, 2.00f, 7, 0.55f },
			{ 16.00f, 1.00f, 5, 0.60f }, { 17.00f, 1.00f, 8, 0.60f }, { 18.00f, 1.00f, 12, 0.65f }, { 19.00f, 1.00f, 10, 0.55f },
			{ 20.00f, 1.00f, 8, 0.60f }, { 21.00f, 1.00f, 7, 0.55f }, { 22.00f, 1.00f, 5, 0.55f }, { 23.00f, 1.00f, 2, 0.50f },
			{ 24.00f, 2.00f, 3, 0.60f }, { 26.00f, 2.00f, 0, 0.55f },
		};

		// Suspiro del trombón al final del puente («wah-waaah» sobre La7: Mi 4 y Do sostenido 4) antes de volver a A.
		constexpr FMusicCueEvent SighCues[] = {
			{ 28.00f, 0.60f, ECue::Trombone, 14, 0.62f, 0.f, 0.3f },
			{ 29.00f, 2.20f, ECue::Trombone, 11, 0.60f, 0.8f, 1.2f },
		};

		// Plinks de marimba/kalimba a contratiempo, muy suaves.
		constexpr FMusicChordStep ChordArp[] = {
			{ 0.50f, 0.5f, 1, 0, 0.32f }, { 1.50f, 0.5f, 2, 0, 0.28f }, { 2.50f, 0.5f, 1, 0, 0.32f }, { 3.50f, 0.5f, 0, 1, 0.28f },
		};

		// Bajo pizzicato «de puntillas»: fundamental, 5ª abajo, fundamental, 5ª abajo y la 3ª de enganche.
		constexpr FMusicChordStep Bass[] = {
			{ 0.00f, 0.50f, 0, 0, 0.80f }, { 1.00f, 0.50f, 2, -1, 0.55f }, { 2.00f, 0.50f, 0, 0, 0.65f }, { 3.00f, 0.50f, 2, -1, 0.50f },
			{ 3.50f, 0.40f, 1, 0, 0.40f },
		};

		// Tic-tac de reloj (caja china) en los tiempos 2 y 4.
		constexpr FMusicPercTonalHit TickTock[] = { { 1.00f, true, 0.30f }, { 3.00f, false, 0.30f } };

		// Ganancias: el «wah-wah» manda y el bucle queda tranquilo por debajo.
		constexpr float WahGain = 0.72f;
		constexpr float LoopGain = 0.65f;

		constexpr FMusicSongPart PartWah = {
			.Chords = ChordsWah,
			.LengthBeats = 5.f,
			.Cues = WahCues,
			.NumCues = static_cast<int32_t>(std::size(WahCues)),
			.PartGain = WahGain,
		};
		constexpr FMusicSongPart PartA = {
			.Chords = ChordsA,
			.Melody = MelodyA,
			.NumMelody = static_cast<int32_t>(std::size(MelodyA)),
			.ChordArp = ChordArp,
			.NumChordArp = static_cast<int32_t>(std::size(ChordArp)),
			.Bass = Bass,
			.NumBass = static_cast<int32_t>(std::size(Bass)),
			.PercTonal = TickTock,
			.NumPercTonal = static_cast<int32_t>(std::size(TickTock)),
			.PartGain = LoopGain,
		};
		constexpr FMusicSongPart PartB = {
			.Chords = ChordsB,
			.Melody = MelodyB,
			.NumMelody = static_cast<int32_t>(std::size(MelodyB)),
			.ChordArp = ChordArp,
			.NumChordArp = static_cast<int32_t>(std::size(ChordArp)),
			.Bass = Bass,
			.NumBass = static_cast<int32_t>(std::size(Bass)),
			.PercTonal = TickTock,
			.NumPercTonal = static_cast<int32_t>(std::size(TickTock)),
			.Cues = SighCues,
			.NumCues = static_cast<int32_t>(std::size(SighCues)),
			.PartGain = LoopGain,
		};
	}

	inline const FMusicSong& GetDefeatSong()
	{
		static const FMusicSong Song = {
			.Sections = { &DefeatData::PartWah, &DefeatData::PartA, &DefeatData::PartB },
			.NumSections = 3,
			.Bpm = 72.f,
			.bUseSteelPanMelody = false,
			.bUseModalChords = true,
			.bBassIsPizzicato = true,
			.MelodyBaseMidi = 74,   // Re 5
			.ChordBaseMidi = 62,    // Re 4
			.BassBaseMidi = 38,     // Re 2
			.AccentBaseMidi = 86,
			.NoiseHighHz = 5200.f,
			.NoiseLowHz = 2200.f,
			.NoiseDecaySeconds = 0.09f,
			.NoiseGain = 0.f,
			.MixTrim = 1.20f,
			.LoopStartSection = 1,
			.CueBaseMidi = 50,      // Re 3
			.bMelodyOutOfTune = true,
			.bRoomyVoicePools = true,
			.PercTonalPreset = EModalPreset::WoodBlock,
			.PercTonalHighHz = 1250.f,
			.PercTonalLowHz = 940.f,
			.MaxFadeInSeconds = 0.03f,
		};
		return Song;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Composición: jingle de eliminado (Do mayor, 120 BPM, 3,5 tiempos sin bucle, ~2 s con las colas).
	// ─────────────────────────────────────────────────────────────────────────

	namespace EliminatedData
	{
		constexpr FMusicChordSpec ChordsJingle[1] = { { 0, EChordType::Maj } };

		// Semitonos desde Do 3 (MIDI 48): silbato de émbolo que se desploma («fiuuu»), «¡toc!» de caja china con un
		// golpe grave de marimba («¡plof!»), «uh-oh» de kalimba (Sol 5 - Mi 5) y un «ploc» amable en Do mayor: estás
		// fuera, pero la partida sigue y toca mirar a los demás.
		constexpr FMusicCueEvent JingleCues[] = {
			{ 0.00f, 1.30f, ECue::SlideWhistle, 43, 0.70f, -14.f, 1.15f },   // Sol 6 → Fa 5
			{ 1.30f, 0.00f, ECue::WoodBlock, 33, 0.80f },                     // La 5
			{ 1.30f, 0.00f, ECue::Marimba, -5, 0.70f },                       // Sol 2
			{ 1.80f, 0.00f, ECue::Kalimba, 31, 0.62f },                       // Sol 5 («uh...»)
			{ 2.30f, 0.00f, ECue::Kalimba, 28, 0.60f },                       // Mi 5 («...oh»)
			{ 3.00f, 0.00f, ECue::Pizz, -12, 0.60f },                         // Do 2
			{ 3.00f, 0.00f, ECue::Marimba, 12, 0.42f },                       // Do 4
			{ 3.00f, 0.00f, ECue::Marimba, 16, 0.40f },                       // Mi 4
			{ 3.00f, 0.00f, ECue::Marimba, 19, 0.40f },                       // Sol 4
		};

		constexpr FMusicSongPart PartJingle = {
			.Chords = ChordsJingle,
			.LengthBeats = 3.5f,
			.Cues = JingleCues,
			.NumCues = static_cast<int32_t>(std::size(JingleCues)),
		};
	}

	inline const FMusicSong& GetEliminatedSong()
	{
		static const FMusicSong Song = {
			.Sections = { &EliminatedData::PartJingle },
			.NumSections = 1,
			.Bpm = 120.f,
			.bUseSteelPanMelody = true,
			.bUseModalChords = true,
			.bBassIsPizzicato = true,
			.MelodyBaseMidi = 72,
			.ChordBaseMidi = 60,
			.BassBaseMidi = 36,
			.AccentBaseMidi = 84,
			.NoiseHighHz = 6000.f,
			.NoiseLowHz = 3000.f,
			.NoiseDecaySeconds = 0.05f,
			.NoiseGain = 0.f,
			.MixTrim = 1.50f,
			.LoopStartSection = -1,
			.CueBaseMidi = 48,      // Do 3
			.bRoomyVoicePools = true,
			.MaxFadeInSeconds = 0.03f,
		};
		return Song;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Pistas (el orden coincide con ETNMusicTrack del componente).
	// ─────────────────────────────────────────────────────────────────────────

	namespace ETrack
	{
		constexpr uint8_t None = 0;
		constexpr uint8_t Shop = 1;
		constexpr uint8_t Booth = 2;
		constexpr uint8_t Victory = 3;
		constexpr uint8_t Defeat = 4;
		constexpr uint8_t Eliminated = 5;
		constexpr uint8_t Count = 6;
	}

	inline const FMusicSong& GetSong(int32_t InTrackIndex)
	{
		// None (silencio) no debería llegar aquí; devuelve la tienda por seguridad.
		switch (InTrackIndex)
		{
		case ETrack::Booth: return GetBoothSong();
		case ETrack::Victory: return GetVictorySong();
		case ETrack::Defeat: return GetDefeatSong();
		case ETrack::Eliminated: return GetEliminatedSong();
		default: return GetShopSong();
		}
	}

	/** true si la pista es un jingle que suena una vez y se calla (sin bucle). */
	inline bool MusicIsOneShotTrack(uint8_t InTrack)
	{
		return InTrack != ETrack::None && InTrack < ETrack::Count && GetSong(InTrack).LoopStartSection < 0;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Reproductor de una canción: cursores de sección/compás, redobles en curso y los depósitos de voces.
	// ─────────────────────────────────────────────────────────────────────────

	/** Redoble o trémolo en curso: golpes repetidos a intervalos fijos con la intensidad interpolada (tiempos de la sección). */
	struct FMusicRollState
	{
		bool bActive = false;
		uint8_t Kind = 0;                    // ECue::SnareRoll, ECue::TimpaniRoll o ECue::SteelPanTremolo
		double StartBeat = 0.0;
		double EndBeat = 0.0;
		double NextBeat = 0.0;
		double StepBeats = 0.25;
		float VelFrom = 0.f;
		float VelTo = 0.f;
		int32_t Count = 0;
		FMusicModalVoice* Voice = nullptr;   // voz que se vuelve a golpear (timbal, steel pan)
		uint32_t VoiceStrikeId = 0;          // para no golpear una voz que ya se ha llevado otra nota
	};

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
			CueCursor = 0;
			PercStep16Last = -1;
			BeatsPerSecond = InSong.Bpm / 60.f;
			bSequenceDone = false;
			LastMelodyEndBeat = -1.0e9;
			LastWhistle = nullptr;
			LastWhistleNoteId = 0;
			RollRng.SetSeed(0x52A11u);
			for (FMusicRollState& Roll : Rolls) { Roll = FMusicRollState(); }
			ConfigurePools(InSong.bRoomyVoicePools);
		}

		bool IsValid() const { return Song != nullptr; }

		/** Jingle sin bucle que ya ha tocado todas sus secciones y cuyas voces se han apagado del todo. */
		bool IsFinished() const { return bSequenceDone && !AnyVoiceActive(); }

		/** Diagnóstico (arnés): voces robadas mientras aún sonaban, sumando todos los depósitos. */
		int32_t GetVoiceStealCount() const
		{
			return MelodyModal.StealCount + MelodyWhistle.StealCount + ChordModal.StealCount + ChordFm.StealCount
				+ BassVoices.StealCount + PercTonalVoices.StealCount + Accents.StealCount + PercNoiseVoices.StealCount
				+ MelodyWobbly.StealCount + BrassVoices.StealCount + TromboneVoices.StealCount + SnareVoices.StealCount
				+ CymbalVoices.StealCount + CueModal.StealCount + SlideWhistles.StealCount;
		}

		/** Avanza InNumFrames muestras: programa las notas que tocan y hace sonar las voces activas en el bloque. */
		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			if (!Song) { return; }
			if (!bSequenceDone)
			{
				ScheduleBlock(InNumFrames);
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
			// Instrumentos de la música de fin de partida (callados en la tienda y el probador).
			MelodyWobbly.Render(LocalL, LocalR, InNumFrames);
			BrassVoices.Render(LocalL, LocalR, InNumFrames);
			TromboneVoices.Render(LocalL, LocalR, InNumFrames);
			SnareVoices.Render(LocalL, LocalR, InNumFrames);
			CymbalVoices.Render(LocalL, LocalR, InNumFrames);
			CueModal.Render(LocalL, LocalR, InNumFrames);
			SlideWhistles.Render(LocalL, LocalR, InNumFrames);
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
		// picos por debajo de -1 dBFS. El comping de Rhodes suma hasta 5 tonos a la vez, así que lleva el recorte
		// más fuerte de todos.
		static constexpr float kMelodyTrim = 0.40f;
		static constexpr float kChordArpTrim = 0.26f;
		static constexpr float kBassTrim = 0.50f;
		static constexpr float kChordStabTrim = 0.15f;
		static constexpr float kCongaTrim = 0.36f;
		static constexpr float kDingTrim = 0.18f;
		static constexpr float kSparkleTrim = 0.26f;
		// Efectos sueltos de la música de fin de partida (misma calibración).
		static constexpr float kBrassTrim = 0.13f;
		static constexpr float kTromboneTrim = 0.42f;
		static constexpr float kSnareTrim = 0.30f;
		static constexpr float kCrashTrim = 0.22f;
		static constexpr float kTimpaniTrim = 0.42f;
		static constexpr float kCueMalletTrim = 0.30f;
		static constexpr float kWoodBlockTrim = 0.30f;
		static constexpr float kCuePizzTrim = 0.45f;
		static constexpr float kSlideWhistleTrim = 0.30f;
		/** Golpes por tiempo del trémolo del steel pan y fuerza de cada golpe respecto a la nota. */
		static constexpr double kTremoloStepBeats = 1.0 / 6.0;
		static constexpr float kTremoloVelocity = 0.5f;

		/** Tamaños de los depósitos: los de siempre (tienda, probador) o los holgados de los temas nuevos. */
		void ConfigurePools(bool bInRoomy)
		{
			MelodyModal.Configure(bInRoomy ? 6 : 3, bInRoomy);
			MelodyWhistle.Configure(2, bInRoomy);
			ChordModal.Configure(bInRoomy ? 6 : 4, bInRoomy);
			ChordFm.Configure(5, bInRoomy);
			BassVoices.Configure(bInRoomy ? 4 : 2, bInRoomy);
			PercTonalVoices.Configure(bInRoomy ? 3 : 2, bInRoomy);
			Accents.Configure(bInRoomy ? 4 : 3, bInRoomy);
			PercNoiseVoices.Configure(bInRoomy ? 3 : 2, bInRoomy);
			MelodyWobbly.Configure(3, true);
			BrassVoices.Configure(10, true);
			TromboneVoices.Configure(2, true);
			SnareVoices.Configure(6, true);
			CymbalVoices.Configure(2, true);
			CueModal.Configure(8, true);
			SlideWhistles.Configure(2, true);
			MelodyModal.Clear();
			MelodyWhistle.Clear();
			ChordModal.Clear();
			ChordFm.Clear();
			BassVoices.Clear();
			PercTonalVoices.Clear();
			Accents.Clear();
			PercNoiseVoices.Clear();
			MelodyWobbly.Clear();
			BrassVoices.Clear();
			TromboneVoices.Clear();
			SnareVoices.Clear();
			CymbalVoices.Clear();
			CueModal.Clear();
			SlideWhistles.Clear();
		}

		bool AnyVoiceActive() const
		{
			return MelodyModal.AnyActive() || MelodyWhistle.AnyActive() || ChordModal.AnyActive() || ChordFm.AnyActive()
				|| BassVoices.AnyActive() || PercTonalVoices.AnyActive() || Accents.AnyActive() || PercNoiseVoices.AnyActive()
				|| MelodyWobbly.AnyActive() || BrassVoices.AnyActive() || TromboneVoices.AnyActive() || SnareVoices.AnyActive()
				|| CymbalVoices.AnyActive() || CueModal.AnyActive() || SlideWhistles.AnyActive();
		}

		/** Dispara lo que toca en este bloque y avanza el reloj de la sección (con bucle o fin del jingle). */
		void ScheduleBlock(int32_t InNumFrames)
		{
			const FMusicSongPart* Part = Song->Sections[SectionIdx];
			if (!Part) { SectionIdx = 0; Part = Song->Sections[SectionIdx]; }
			if (!Part) { bSequenceDone = true; return; }
			const double DtBeats = static_cast<double>(InNumFrames) * InvRate * BeatsPerSecond;
			const float PartGain = Part->PartGain;

			const int32_t BarInPart = static_cast<int32_t>(SectionLocalBeat / MusicBeatsPerBar);
			if (BarInPart != CurBar)
			{
				CurBar = BarInPart;
				ChordArpCursor = 0;
				BassCursor = 0;
				ChordStabCursor = 0;
				PercStep16Last = -1;
			}
			const int32_t ClampedBar = std::clamp(CurBar, 0, MusicPartBars(*Part) - 1);
			const FMusicChordSpec& NowChord = Part->Chords[ClampedBar];
			const float BeatInBar = static_cast<float>(SectionLocalBeat - static_cast<double>(CurBar) * MusicBeatsPerBar);

			while (MelodyCursor < Part->NumMelody && Part->Melody[MelodyCursor].StartBeat <= SectionLocalBeat)
			{
				TriggerMelody(Part->Melody[MelodyCursor], PartGain);
				++MelodyCursor;
			}
			while (ChordArpCursor < Part->NumChordArp && Part->ChordArp[ChordArpCursor].StartBeat <= BeatInBar)
			{
				TriggerChordArp(NowChord, Part->ChordArp[ChordArpCursor], PartGain);
				++ChordArpCursor;
			}
			while (BassCursor < Part->NumBass && Part->Bass[BassCursor].StartBeat <= BeatInBar)
			{
				TriggerBass(NowChord, Part->Bass[BassCursor], PartGain);
				++BassCursor;
			}
			while (ChordStabCursor < Part->NumChordStabs && Part->ChordStabs[ChordStabCursor].StartBeat <= BeatInBar)
			{
				TriggerChordStab(NowChord, Part->ChordStabs[ChordStabCursor], PartGain);
				++ChordStabCursor;
			}
			if (Part->PercNoisePattern)
			{
				const int32_t Step16 = std::clamp(static_cast<int32_t>(BeatInBar * (MusicStepsPerBar / MusicBeatsPerBar) + 1e-4f), 0, MusicStepsPerBar - 1);
				if (Step16 != PercStep16Last)
				{
					PercStep16Last = Step16;
					const float Vel = Part->PercNoisePattern[Step16];
					if (Vel > 1e-4f) { TriggerNoise(Vel, PartGain); }
				}
			}
			for (int32_t k = 0; k < Part->NumPercTonal; ++k)
			{
				// Las congas son pocas por compás: basta comprobar si el paso cayó justo en este bloque.
				const float T = Part->PercTonal[k].StartBeat;
				if (T > BeatInBar - static_cast<float>(DtBeats) && T <= BeatInBar) { TriggerConga(Part->PercTonal[k], PartGain); }
			}
			while (AccentCursor < Part->NumAccent && Part->Accent[AccentCursor].StartBeat <= SectionLocalBeat)
			{
				TriggerAccent(NowChord, Part->Accent[AccentCursor], Part->bAccentIsSparkle, PartGain);
				++AccentCursor;
			}
			while (CueCursor < Part->NumCues && Part->Cues[CueCursor].StartBeat <= SectionLocalBeat)
			{
				TriggerCue(Part->Cues[CueCursor], PartGain);
				++CueCursor;
			}
			UpdateRolls();

			SectionLocalBeat += DtBeats;
			if (SectionLocalBeat >= Part->LengthBeats)
			{
				SectionLocalBeat -= Part->LengthBeats;
				const int32_t NumSections = std::max(1, Song->NumSections);
				int32_t NextSection = SectionIdx + 1;
				if (NextSection >= NumSections)
				{
					if (Song->LoopStartSection < 0)
					{
						// Jingle sin bucle: no se programa nada más; las voces que suenan se apagan solas.
						bSequenceDone = true;
						for (FMusicRollState& Roll : Rolls) { Roll.bActive = false; }
						return;
					}
					NextSection = std::clamp(Song->LoopStartSection, 0, NumSections - 1);
				}
				SectionIdx = NextSection;
				while (!Song->Sections[SectionIdx]) { SectionIdx = (SectionIdx + 1) % NumSections; }
				MelodyCursor = 0;
				AccentCursor = 0;
				CueCursor = 0;
				CurBar = -1;
				LastMelodyEndBeat = -1.0e9;
				for (FMusicRollState& Roll : Rolls) { Roll.bActive = false; }
			}
		}

		void TriggerMelody(const FMusicNoteEvent& InNote, float InGain)
		{
			const float Hz = MusicMidiToHz(static_cast<float>(Song->MelodyBaseMidi + InNote.Semitone));
			const float LenSec = InNote.LengthBeats / BeatsPerSecond;
			const float Pan = -0.15f;
			const float Amp = InNote.Velocity * kMelodyTrim * InGain;
			if (Song->bUseSteelPanMelody)
			{
				FMusicModalVoice& Voice = MelodyModal.Allocate();
				Voice.Trigger(GetModalPreset(EModalPreset::SteelPan), Hz, Amp, Pan, InvRate);
				if (Song->bSteelPanTremolo && InNote.LengthBeats >= 1.5f)
				{
					// Trémolo del panista: golpes seguidos sobre la misma nota hasta casi el final (sin reiniciarla).
					const float TremoloVel = InNote.Velocity * kTremoloVelocity * InGain;
					StartRoll(ECue::SteelPanTremolo, static_cast<double>(InNote.StartBeat) + kTremoloStepBeats,
						static_cast<double>(InNote.StartBeat) + InNote.LengthBeats - kTremoloStepBeats, kTremoloStepBeats,
						TremoloVel, TremoloVel * 0.85f, &Voice);
				}
			}
			else if (Song->bMelodyOutOfTune)
			{
				TriggerWobblyWhistle(InNote, Hz, LenSec, Amp, Pan);
			}
			else
			{
				MelodyWhistle.Allocate().Trigger(Hz, Amp, LenSec, Pan, InvRate);
			}
		}

		/**
		 * Silbido desafinado de la derrota: plano, con la afinación cayéndose a lo largo de cada nota (más en las
		 * largas), vibrato lento y ancho y un vaivén inseguro. Las notas pegadas a la anterior se ligan deslizando.
		 */
		void TriggerWobblyWhistle(const FMusicNoteEvent& InNote, float InHz, float InLenSec, float InAmp, float InPan)
		{
			FMusicWhistleStyle WhistleStyle;
			WhistleStyle.DetuneCents = -24.f;
			WhistleStyle.SagCents = InNote.LengthBeats >= 1.5f ? -55.f : -25.f;
			WhistleStyle.ScoopCents = -45.f;
			WhistleStyle.VibRateHz = 4.3f;
			WhistleStyle.VibDepth = 0.0085f;
			WhistleStyle.DriftCents = 9.f;
			WhistleStyle.Air = 0.06f;
			const double GapBeats = static_cast<double>(InNote.StartBeat) - LastMelodyEndBeat;
			const bool bLegato = LastWhistle && GapBeats < 0.2 && LastWhistle->IsActive() && LastWhistle->NoteId == LastWhistleNoteId;
			if (bLegato)
			{
				LastWhistle->Legato(InHz, 0.08f, InAmp, InLenSec, WhistleStyle);
			}
			else
			{
				FMusicSlideWhistleVoice& Voice = MelodyWobbly.Allocate();
				Voice.Trigger(InHz, InHz, 0.f, InAmp, InLenSec, InPan, WhistleStyle, InvRate);
				LastWhistle = &Voice;
			}
			LastWhistleNoteId = LastWhistle->NoteId;
			LastMelodyEndBeat = static_cast<double>(InNote.StartBeat) + InNote.LengthBeats;
		}

		void TriggerChordArp(const FMusicChordSpec& InChord, const FMusicChordStep& InStep, float InGain)
		{
			const int32_t Semi = MusicChordTone(InChord, InStep.ToneIndex) + InStep.OctaveOffset * 12;
			const float Hz = MusicMidiToHz(static_cast<float>(Song->ChordBaseMidi + Semi));
			const int32_t Preset = Song->bUseModalChords ? (((InStep.ToneIndex + CurBar) & 1) ? EModalPreset::Kalimba : EModalPreset::Marimba) : EModalPreset::Marimba;
			ChordModal.Allocate().Trigger(GetModalPreset(Preset), Hz, InStep.Velocity * kChordArpTrim * InGain, 0.35f, InvRate);
		}

		void TriggerBass(const FMusicChordSpec& InChord, const FMusicChordStep& InStep, float InGain)
		{
			const int32_t Semi = MusicChordTone(InChord, InStep.ToneIndex) + InStep.OctaveOffset * 12;
			const float Hz = MusicMidiToHz(static_cast<float>(Song->BassBaseMidi + Semi));
			const int32_t Preset = Song->bBassIsPizzicato ? EModalPreset::PizzicatoBass : EModalPreset::UprightBass;
			BassVoices.Allocate().Trigger(GetModalPreset(Preset), Hz, InStep.Velocity * kBassTrim * InGain, 0.f, InvRate);
		}

		void TriggerChordStab(const FMusicChordSpec& InChord, const FMusicChordStab& InStab, float InGain)
		{
			const FMusicChordIntervals& Intervals = GetChordIntervals(InChord.Type);
			const float LenSec = InStab.LengthBeats / BeatsPerSecond;
			for (int32_t k = 0; k < Intervals.NumTones; ++k)
			{
				const int32_t Semi = InChord.RootSemitone + Intervals.Tones[k];
				const float Hz = MusicMidiToHz(static_cast<float>(Song->ChordBaseMidi + Semi));
				ChordFm.Allocate().Trigger(Hz, InStab.Velocity * kChordStabTrim * InGain, LenSec, 0.3f, InvRate);
			}
		}

		void TriggerConga(const FMusicPercTonalHit& InHit, float InGain)
		{
			const float Hz = InHit.bHigh ? Song->PercTonalHighHz : Song->PercTonalLowHz;
			const int32_t Preset = Song->PercTonalPreset;
			PercTonalVoices.Allocate().Trigger(GetModalPreset(Preset), Hz, InHit.Velocity * kCongaTrim * InGain, InHit.bHigh ? 0.5f : -0.5f, InvRate);
		}

		void TriggerNoise(float InVelocity, float InGain)
		{
			PercNoiseVoices.Allocate().Trigger(Song->NoiseHighHz, Song->NoiseLowHz, Song->NoiseDecaySeconds, InVelocity * Song->NoiseGain * InGain, InvRate);
		}

		void TriggerAccent(const FMusicChordSpec& InChord, const FMusicNoteEvent& InNote, bool bInSparkle, float InGain)
		{
			if (bInSparkle)
			{
				const int32_t Semi = InChord.RootSemitone + InNote.Semitone;
				const float Hz = MusicMidiToHz(static_cast<float>(Song->AccentBaseMidi + Semi));
				Accents.Allocate().Trigger(GetModalPreset(EModalPreset::SparkleChime), Hz, InNote.Velocity * kSparkleTrim * InGain, 0.5f, InvRate);
			}
			else
			{
				const float Hz = MusicMidiToHz(static_cast<float>(Song->AccentBaseMidi + InNote.Semitone));
				Accents.Allocate().Trigger(GetModalPreset(EModalPreset::RegisterDing), Hz, InNote.Velocity * kDingTrim * InGain, 0.f, InvRate);
			}
		}

		/** Un efecto suelto de la sección (ver ECue). */
		void TriggerCue(const FMusicCueEvent& InCue, float InGain)
		{
			const float Hz = MusicMidiToHz(static_cast<float>(Song->CueBaseMidi + InCue.Semitone));
			const float LenSec = InCue.LengthBeats / BeatsPerSecond;
			const float Vel = InCue.Velocity * InGain;
			switch (InCue.Type)
			{
			case ECue::Brass:
			{
				// Metales repartidos en el panorama por altura (graves a la izquierda, trompetas a la derecha).
				const float BrassPan = std::clamp((static_cast<float>(InCue.Semitone) - 14.f) / 40.f, -0.3f, 0.3f);
				BrassVoices.Allocate().Trigger(Hz, Vel * kBrassTrim, InCue.Velocity, LenSec, BrassPan, InCue.Param, InvRate);
				break;
			}
			case ECue::Trombone:
				TromboneVoices.Allocate().Trigger(Hz, Vel * kTromboneTrim, LenSec, -0.12f, InCue.Param, InCue.Param2, InvRate);
				break;
			case ECue::SnareRoll:
				StartRoll(ECue::SnareRoll, static_cast<double>(InCue.StartBeat), static_cast<double>(InCue.StartBeat) + std::max(0.05f, InCue.LengthBeats),
					1.0 / std::max(1.0, static_cast<double>(InCue.Param2)), InCue.Param * InGain, Vel, nullptr);
				break;
			case ECue::TimpaniRoll:
			{
				// Una sola voz de timbal que se vuelve a golpear: el parche sigue resonando entre golpe y golpe.
				FMusicModalVoice& Voice = CueModal.Allocate();
				Voice.Trigger(GetModalPreset(EModalPreset::Timpani), Hz, InCue.Param * InGain * kTimpaniTrim, -0.2f, InvRate);
				const double Step = 1.0 / std::max(1.0, static_cast<double>(InCue.Param2));
				StartRoll(ECue::TimpaniRoll, static_cast<double>(InCue.StartBeat) + Step,
					static_cast<double>(InCue.StartBeat) + std::max(0.05f, InCue.LengthBeats), Step, InCue.Param * InGain, Vel, &Voice);
				break;
			}
			case ECue::SnareHit:
				SnareVoices.Allocate().Trigger(Vel * kSnareTrim, 0.2f, 0.55f, 0.12f, InvRate);
				break;
			case ECue::Crash:
				CymbalVoices.Allocate().Trigger(Vel * kCrashTrim, InCue.Param > 0.f ? InCue.Param : 2.2f, InvRate);
				break;
			case ECue::Timpani:
				CueModal.Allocate().Trigger(GetModalPreset(EModalPreset::Timpani), Hz, Vel * kTimpaniTrim, -0.2f, InvRate);
				break;
			case ECue::SteelPan:
				CueModal.Allocate().Trigger(GetModalPreset(EModalPreset::SteelPan), Hz, Vel * kCueMalletTrim, 0.2f, InvRate);
				break;
			case ECue::Marimba:
				CueModal.Allocate().Trigger(GetModalPreset(EModalPreset::Marimba), Hz, Vel * kCueMalletTrim, -0.15f, InvRate);
				break;
			case ECue::Kalimba:
				CueModal.Allocate().Trigger(GetModalPreset(EModalPreset::Kalimba), Hz, Vel * kCueMalletTrim, 0.25f, InvRate);
				break;
			case ECue::WoodBlock:
				CueModal.Allocate().Trigger(GetModalPreset(EModalPreset::WoodBlock), Hz, Vel * kWoodBlockTrim, 0.3f, InvRate);
				break;
			case ECue::Pizz:
				CueModal.Allocate().Trigger(GetModalPreset(EModalPreset::PizzicatoBass), Hz, Vel * kCuePizzTrim, 0.f, InvRate);
				break;
			case ECue::SlideWhistle:
			{
				FMusicWhistleStyle SlideStyle;
				SlideStyle.VibDepth = 0.004f;
				SlideStyle.Air = 0.09f;
				SlideStyle.Harmonic2 = 0.06f;
				const float ToHz = Hz * MusicSemitonesToRatio(InCue.Param);
				const float GlideSec = std::max(0.f, InCue.Param2) / BeatsPerSecond;
				SlideWhistles.Allocate().Trigger(Hz, ToHz, GlideSec, Vel * kSlideWhistleTrim, LenSec, 0.1f, SlideStyle, InvRate);
				break;
			}
			default:
				break;
			}
		}

		/** Arranca un redoble o trémolo; InVoice (opcional) es la voz modal que se vuelve a golpear. */
		void StartRoll(uint8_t InKind, double InFirstBeat, double InEndBeat, double InStepBeats, float InVelFrom, float InVelTo, FMusicModalVoice* InVoice)
		{
			FMusicRollState* FreeRoll = &Rolls[0];
			for (FMusicRollState& Roll : Rolls)
			{
				if (!Roll.bActive) { FreeRoll = &Roll; break; }
			}
			FreeRoll->bActive = InEndBeat > InFirstBeat;
			FreeRoll->Kind = InKind;
			FreeRoll->StartBeat = InFirstBeat;
			FreeRoll->EndBeat = InEndBeat;
			FreeRoll->NextBeat = InFirstBeat;
			FreeRoll->StepBeats = std::max(1e-3, InStepBeats);
			FreeRoll->VelFrom = InVelFrom;
			FreeRoll->VelTo = InVelTo;
			FreeRoll->Count = 0;
			FreeRoll->Voice = InVoice;
			FreeRoll->VoiceStrikeId = InVoice ? InVoice->StrikeId : 0u;
		}

		/** Da los golpes de redoble y trémolo que caen en este bloque. */
		void UpdateRolls()
		{
			for (FMusicRollState& Roll : Rolls)
			{
				while (Roll.bActive && Roll.NextBeat <= SectionLocalBeat)
				{
					if (Roll.NextBeat >= Roll.EndBeat) { Roll.bActive = false; break; }
					const double Span = std::max(1e-3, Roll.EndBeat - Roll.StartBeat);
					const float Progress = MusicClamp01(static_cast<float>((Roll.NextBeat - Roll.StartBeat) / Span));
					float Vel = Roll.VelFrom + (Roll.VelTo - Roll.VelFrom) * Progress;
					// Mano derecha / izquierda y un pelín de irregularidad humana (determinista).
					Vel *= (Roll.Count & 1) ? 0.84f : 1.f;
					Vel *= 1.f + 0.07f * RollRng.Bipolar();
					StrikeRoll(Roll, std::max(0.f, Vel));
					++Roll.Count;
					Roll.NextBeat += Roll.StepBeats;
				}
			}
		}

		void StrikeRoll(const FMusicRollState& InRoll, float InVel)
		{
			switch (InRoll.Kind)
			{
			case ECue::SnareRoll:
				SnareVoices.Allocate().Trigger(InVel * kSnareTrim, 0.11f, 0.22f, (InRoll.Count & 1) ? 0.18f : 0.06f, InvRate);
				break;
			case ECue::TimpaniRoll:
				if (InRoll.Voice && InRoll.Voice->StrikeId == InRoll.VoiceStrikeId) { InRoll.Voice->Restrike(InVel * kTimpaniTrim); }
				break;
			case ECue::SteelPanTremolo:
				if (InRoll.Voice && InRoll.Voice->StrikeId == InRoll.VoiceStrikeId) { InRoll.Voice->Restrike(InVel * kMelodyTrim); }
				break;
			default:
				break;
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
		int32_t CueCursor = 0;
		int32_t PercStep16Last = -1;
		bool bSequenceDone = false;
		double LastMelodyEndBeat = -1.0e9;
		FMusicSlideWhistleVoice* LastWhistle = nullptr;
		uint32_t LastWhistleNoteId = 0;
		FMusicRollState Rolls[3];
		FMusicRandom RollRng{ 0x52A11u };

		TMusicVoicePool<FMusicModalVoice, 6> MelodyModal;
		TMusicVoicePool<FMusicWhistleVoice, 2> MelodyWhistle;
		TMusicVoicePool<FMusicModalVoice, 6> ChordModal;
		TMusicVoicePool<FMusicFmVoice, 5> ChordFm;
		TMusicVoicePool<FMusicModalVoice, 4> BassVoices;
		TMusicVoicePool<FMusicModalVoice, 3> PercTonalVoices;
		TMusicVoicePool<FMusicModalVoice, 4> Accents;
		TMusicVoicePool<FMusicNoiseHit, 3> PercNoiseVoices;
		TMusicVoicePool<FMusicSlideWhistleVoice, 3> MelodyWobbly;
		TMusicVoicePool<FMusicBrassVoice, 10> BrassVoices;
		TMusicVoicePool<FMusicTromboneVoice, 2> TromboneVoices;
		TMusicVoicePool<FMusicSnareVoice, 6> SnareVoices;
		TMusicVoicePool<FMusicCymbalVoice, 2> CymbalVoices;
		TMusicVoicePool<FMusicModalVoice, 8> CueModal;
		TMusicVoicePool<FMusicSlideWhistleVoice, 2> SlideWhistles;
		float LocalL[MusicBlockFrames] = {};
		float LocalR[MusicBlockFrames] = {};
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Parámetros compartidos con el hilo de juego y motor completo (capas en fundido cruzado + limitador).
	// ─────────────────────────────────────────────────────────────────────────

	struct FMusicSharedParams
	{
		/** Pista a la que PlayTrack/StopMusic quiere llegar (None = silencio). */
		std::atomic<uint8_t> RequestedTrack{ ETrack::None };
		/**
		 * Número de petición: sube en cada PlayTrack aceptado (se publica después de la pista y el fundido, con orden de
		 * liberación). Permite volver a tocar un jingle sin bucle que ya terminó aunque la pista pedida sea la misma.
		 */
		std::atomic<uint32_t> RequestSerial{ 0u };
		/** Lo escribe el hilo de audio: número de la petición cuyo jingle sin bucle ha sonado entero (0xFFFFFFFF = ninguna). */
		std::atomic<uint32_t> FinishedSerial{ 0xFFFFFFFFu };
		/** Duración del fundido asociado a la última petición (se toma en el instante en que llega la petición). */
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

	/** Una de las capas de reproducción del motor: su reproductor de canción y su posición en el fundido (0..1, curva de potencia constante). */
	struct FMusicLayer
	{
		FMusicSongPlayer Player;
		uint8_t TrackId = ETrack::None;
		uint32_t Serial = 0;       // petición que la puso a sonar (para avisar cuando un jingle termina)
		float FadePos = 0.f;       // 0..1, lineal; el volumen real aplicado es sin(FadePos * pi/2).
		float FadeTarget = 0.f;
		float FadeStep = 1.f;      // por muestra.

		bool IsSilent() const { return TrackId == ETrack::None || (FadePos <= 1e-4f && FadeTarget <= 1e-4f); }
	};

	/**
	 * Motor completo: tres capas en fundido cruzado (la que entra, la que sale y una de reserva para cambios rápidos
	 * seguidos sin cortar ninguna), volumen general suavizado y limitador de salida.
	 */
	class FMusicEngine
	{
	public:
		static constexpr int32_t NumLayers = 3;

		void Init(float InRate)
		{
			Rate = std::max(8000.f, InRate);
			InvRate = 1.f / Rate;
			Limiter.Init(Rate);
			Volume = 0.f;
			PrevVolume = 0.f;
			LastRequested = ETrack::None;
			LastSerial = 0u;
		}

		void Render(float* InOut, int32_t InNumFrames, int32_t InOutChannels, FMusicSharedParams& InParams)
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

		/** Diagnóstico (arnés): voces robadas mientras sonaban en todas las capas. */
		int32_t GetVoiceStealCount() const
		{
			int32_t Total = 0;
			for (const FMusicLayer& L : Layers) { Total += L.Player.GetVoiceStealCount(); }
			return Total;
		}

	private:
		/** Atiende una petición nueva (pista distinta o número de petición nuevo) repartiendo los fundidos entre capas. */
		void HandleRequests(FMusicSharedParams& InParams)
		{
			const uint32_t Serial = InParams.RequestSerial.load(std::memory_order_acquire);
			const uint8_t RawRequested = InParams.RequestedTrack.load(std::memory_order_relaxed);
			if (Serial == LastSerial && RawRequested == LastRequested) { return; }
			LastSerial = Serial;
			LastRequested = RawRequested;

			uint8_t Requested = RawRequested < ETrack::Count ? RawRequested : ETrack::None;
			// Un jingle sin bucle que ya sonó entero con esta misma petición no vuelve a empezar solo (p. ej. al
			// rearrancar el componente tras un viaje o al volver una fuente 3D al alcance del oyente).
			if (Requested != ETrack::None && MusicIsOneShotTrack(Requested)
				&& InParams.FinishedSerial.load(std::memory_order_relaxed) == Serial)
			{
				Requested = ETrack::None;
			}

			const float FadeSeconds = std::max(0.02f, FMusicSharedParams::Get(InParams.FadeSeconds));
			// La pista que entra puede hacerlo más deprisa (jingles); la que sale siempre usa el fundido pedido.
			const float FadeInSeconds = Requested != ETrack::None
				? std::max(0.02f, std::min(FadeSeconds, GetSong(Requested).MaxFadeInSeconds))
				: FadeSeconds;
			bool bMatched = false;
			for (FMusicLayer& L : Layers)
			{
				if (!bMatched && Requested != ETrack::None && L.TrackId == Requested)
				{
					L.FadeTarget = 1.f;
					L.FadeStep = std::fabs(1.f - L.FadePos) / std::max(1.f, FadeInSeconds * Rate);
					L.Serial = Serial;
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
				// Capa libre si la hay; si no, la que menos suena.
				FMusicLayer* FreeLayer = &Layers[0];
				for (FMusicLayer& L : Layers)
				{
					if (L.TrackId == ETrack::None) { FreeLayer = &L; break; }
					if (L.FadePos < FreeLayer->FadePos) { FreeLayer = &L; }
				}
				FreeLayer->Player.Reset(GetSong(static_cast<int32_t>(Requested)), Rate);
				FreeLayer->TrackId = Requested;
				FreeLayer->Serial = Serial;
				FreeLayer->FadePos = 0.f;
				FreeLayer->FadeTarget = 1.f;
				FreeLayer->FadeStep = 1.f / std::max(1.f, FadeInSeconds * Rate);
			}
		}

		void RenderBlock(int32_t InN, FMusicSharedParams& InParams)
		{
			const float Dt = static_cast<float>(InN) * InvRate;
			HandleRequests(InParams);

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
				if (L.Player.IsFinished())
				{
					// Jingle sin bucle terminado (todas sus voces ya en silencio): se libera la capa sin salto y, si era
					// la pista pedida, se avisa al hilo de juego para que una nueva petición lo pueda repetir.
					if (L.FadeTarget > 0.5f) { InParams.FinishedSerial.store(L.Serial, std::memory_order_release); }
					L.TrackId = ETrack::None;
					L.FadePos = 0.f;
					L.FadeTarget = 0.f;
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
		uint32_t LastSerial = 0u;
		FMusicLayer Layers[NumLayers];
		FMusicLimiter Limiter;
		float MixL[MusicBlockFrames] = {};
		float MixR[MusicBlockFrames] = {};
		float TmpL[MusicBlockFrames] = {};
		float TmpR[MusicBlockFrames] = {};
	};
}
