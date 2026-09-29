#pragma once

// C++ puro (sin tipos de Unreal), como TN_MusicSynthDSP.h: se compila y se mide fuera del motor en un arnés que
// renderiza WAV (Tools/RaceMusic). Reutiliza de aquel archivo las voces (resonadores modales, caja, metal, ruido, platillo),
// el depósito de voces y el limitador; lo demás (composición, secuenciador de semicorcheas, flauta, sala y control de
// tensión y de «ducking») es propio.
#include "TN_MusicSynthDSP.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iterator>

/**
 * Música de fondo de la carrera («Marcha de la Playa», por el juego «Tortuga Navy»): una banda militar de vacaciones en la
 * playa. Mi bemol mayor, 116 BPM en 4/4, forma A-A'-B-A'' de 32 compases (≈ 66 s) que se repite dando la vuelta con
 * relevo de instrumentos en cada pasada (la pieza dura dos vueltas, ≈ 2 min 12 s, hasta volver a sonar igual):
 *  - El «oom-pah» de marcha en el bajo (pizzicato), las marimbas a contratiempo, la caja de marcha con notas fantasma y
 *    redobles de relleno, el shaker y unas congas en tresillo (la parte playera), un toque de corneta al final de A' y A'',
 *    y la campana del barco («ding-ding») al empezar el puente.
 *  - La melodía la llevan el steel pan (con trémolo en las notas largas) y una flauta de banda militar (piccolo suave),
 *    que se turnan de una vuelta a otra.
 *  - Capa de tensión (parámetro Tension 0..1, lo mueve el juego): redoble continuo de caja en semicorcheas, timbal a negras,
 *    bajo a corcheas, ostinato de kalimba en tresillo, metales en los compases pares y un 6 % más de tempo. Sale y entra
 *    con las notas nuevas (nada se corta) y con 0 no cuesta ni una voz.
 *  - «Ducking» (parámetro Duck 0..1): baja hasta -10 dB y cierra un paso bajo (16 kHz a 2,2 kHz) para apartarse de los momentos
 *    fuertes; entra en 0,3 s y sale en 0,9 s.
 *  - Sala pequeña (reverberación de Schroeder/Freeverb, ~0,8 s) para que suene lejos, de fondo.
 * Al entrar desde el silencio empieza con dos compases de introducción (redoble que crece y toque de corneta) y el tema
 * entra en el compás 1. La composición es determinista: el mismo instante da siempre la misma señal.
 *
 * Hilos: FRaceMusicParams lo escribe el hilo de juego (atómicos) y el hilo de render de audio lo lee una vez por bloque de
 * control; todo lo demás vive solo en el hilo de audio (sin asignaciones, sin bloqueos, sin UObjects).
 */
namespace TNRaceMusic
{
	using TNMusic::FMusicBrassVoice;
	using TNMusic::FMusicChordSpec;
	using TNMusic::FMusicCymbalVoice;
	using TNMusic::FMusicEnvAsr;
	using TNMusic::FMusicLimiter;
	using TNMusic::FMusicModalPreset;
	using TNMusic::FMusicModalVoice;
	using TNMusic::FMusicNoiseHit;
	using TNMusic::FMusicOnePole;
	using TNMusic::FMusicRandom;
	using TNMusic::FMusicSnareVoice;
	using TNMusic::TMusicVoicePool;
	namespace EChordType = TNMusic::EChordType;
	namespace EModalPreset = TNMusic::EModalPreset;

	constexpr int32_t kStepsPerBar = 16;
	constexpr int32_t kBarsPerSection = 8;
	constexpr int32_t kNumSections = 4;
	constexpr int32_t kBarsPerLoop = kBarsPerSection * kNumSections;
	constexpr int32_t kStepsPerSection = kStepsPerBar * kBarsPerSection;
	constexpr int32_t kStepsPerLoop = kStepsPerBar * kBarsPerLoop;
	/** Dos compases de introducción al entrar desde el silencio. */
	constexpr int32_t kIntroSteps = kStepsPerBar * 2;
	constexpr float kBaseBpm = 116.f;
	/** Con tensión 1, el tempo sube este tanto por uno. */
	constexpr float kTensionTempoBoost = 0.06f;
	/** Caída máxima del «ducking» (dB) y corte del paso bajo con «ducking» 1. */
	constexpr float kDuckDb = -10.f;
	constexpr float kDuckOpenHz = 16000.f;
	constexpr float kDuckClosedHz = 2200.f;

	/** Capas que se pueden apagar (depuración: TN.Race.Music.Layers). */
	namespace ELayer
	{
		constexpr uint32_t Rhythm = 1u << 0;   // bajo, caja, shaker, congas y campana
		constexpr uint32_t Harmony = 1u << 1;  // marimbas a contratiempo
		constexpr uint32_t Melody = 1u << 2;   // steel pan y flauta
		constexpr uint32_t Brass = 1u << 3;    // toque de corneta
		constexpr uint32_t Tension = 1u << 4;  // capa de tensión
		constexpr uint32_t All = Rhythm | Harmony | Melody | Brass | Tension;
	}

	/** Parámetros compartidos con el hilo de juego. */
	struct FRaceMusicParams
	{
		/** true = suena; false = se funde a silencio y el motor deja de calcular (CPU a cero). */
		std::atomic<bool> bPlay{ false };
		/** Duración del fundido de entrada o de salida (s), leída en cada bloque. */
		std::atomic<float> FadeSeconds{ 2.f };
		/** Volumen general (0..1,5), suavizado en el motor. */
		std::atomic<float> Volume{ 1.f };
		/** Objetivo de tensión 0..1 (el motor lo suaviza: sube en ~2 s, baja en ~3 s). */
		std::atomic<float> Tension{ 0.f };
		/** Objetivo de «ducking» 0..1 (entra en ~0,3 s, sale en ~0,9 s). */
		std::atomic<float> Duck{ 0.f };
		/** Máscara de ELayer. */
		std::atomic<uint32_t> LayerMask{ ELayer::All };
		/** Sube para reiniciar la pieza desde el compás 1 (con introducción) si está sonando. */
		std::atomic<uint32_t> RestartSerial{ 0u };

		/** Lo escribe el hilo de audio (diagnóstico): ¿está el motor calculando? y compás dentro de la vuelta (-1 en la introducción). */
		std::atomic<bool> bDebugRunning{ false };
		std::atomic<int32_t> DebugBar{ -1 };
		std::atomic<int32_t> DebugPass{ 0 };
		std::atomic<float> DebugTension{ 0.f };
		std::atomic<float> DebugDuck{ 0.f };

		static float Get(const std::atomic<float>& InValue) { return InValue.load(std::memory_order_relaxed); }
		static void Set(std::atomic<float>& InValue, float InNewValue) { InValue.store(InNewValue, std::memory_order_relaxed); }
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Instrumentos propios: presets modales nuevos y la flauta de banda.
	// ─────────────────────────────────────────────────────────────────────────

	/** Conga: parche corto y seco (el preset «Marimba» de la tienda hacía de conga; este suena más a piel). */
	inline constexpr FMusicModalPreset kCongaPreset = { { {1.00f, 0.17f, 1.00f}, {1.85f, 0.09f, 0.34f}, {3.10f, 0.04f, 0.14f} }, 3 };
	/** Campana de barco: metal claro, con cola de casi un segundo. */
	inline constexpr FMusicModalPreset kShipBellPreset = { { {1.00f, 1.10f, 1.00f}, {2.01f, 0.70f, 0.42f}, {2.76f, 0.55f, 0.34f}, {5.40f, 0.30f, 0.16f} }, 4 };
	/** Timbal de pulso de la capa de tensión: el parche del preset de la orquesta, pero con la cola cortada para que las negras no se emborronen. */
	inline constexpr FMusicModalPreset kPulseTimpPreset = { { {1.00f, 0.60f, 1.00f}, {1.50f, 0.36f, 0.42f}, {1.98f, 0.26f, 0.26f}, {2.44f, 0.18f, 0.14f} }, 4 };
	/** Caja china seca para el contratiempo del puente. */
	inline constexpr FMusicModalPreset kClavePreset = { { {1.00f, 0.10f, 1.00f}, {2.50f, 0.05f, 0.40f}, {4.30f, 0.02f, 0.18f} }, 3 };

	/**
	 * Flauta de banda (piccolo suave): seno con dos armónicos, soplido al atacar, un «chiff» de altura que se corrige en
	 * ~30 ms y un vibrato que aparece a los ~0,25 s. No reinicia nada más que la fase.
	 */
	struct FFifeVoice
	{
		FMusicEnvAsr Env;
		float Phase = 0.f;
		float Inc = 0.f;
		float VibPhase = 0.f;
		float VibInc = 0.f;
		float Elapsed = 0.f;
		float InvRate = 1.f / 48000.f;
		float ChiffCents = 0.f;
		float ChiffCoef = 0.f;
		float BreathEnv = 0.f;
		float BreathCoef = 0.f;
		float Amp = 0.f;
		float PanL = 0.7071f;
		float PanR = 0.7071f;
		FMusicOnePole Air;
		FMusicRandom Rng{ 0xF1FE5u };

		bool IsActive() const { return Env.IsActive(); }
		float Level() const { return Env.Value * Amp; }

		void Trigger(float InFreqHz, float InAmp, float InLengthSeconds, float InPan, float InInvRate)
		{
			InvRate = InInvRate;
			const float SampleRate = 1.f / InInvRate;
			Phase = 0.f;
			Inc = std::min(InFreqHz * InInvRate, 0.16f);
			VibPhase = 0.f;
			VibInc = 5.0f * InInvRate;
			Elapsed = 0.f;
			ChiffCents = 28.f;
			ChiffCoef = 1.f - std::exp(-InInvRate / 0.03f);
			BreathEnv = 1.f;
			BreathCoef = 1.f - std::exp(-InInvRate / 0.04f);
			Air.SetHz(3000.f, InInvRate);
			TNMusic::MusicPanGains(InPan, PanL, PanR);
			Amp = InAmp;
			Env.Trigger(0.02f, std::max(0.02f, InLengthSeconds - 0.07f), 0.09f, SampleRate);
		}

		void Render(float* InOutL, float* InOutR, int32_t InNumFrames)
		{
			for (int32_t i = 0; i < InNumFrames; ++i)
			{
				const float E = Env.Process();
				Elapsed += InvRate;
				VibPhase += VibInc;
				if (VibPhase >= 1.f) { VibPhase -= 1.f; }
				const float VibDepth = 0.0035f * TNMusic::MusicClamp01((Elapsed - 0.25f) / 0.35f);
				ChiffCents -= ChiffCents * ChiffCoef;
				const float Pitch = 1.f + VibDepth * std::sin(VibPhase * TNMusic::MusicTwoPi) + ChiffCents * 0.0005778f;
				const float P = Phase * TNMusic::MusicTwoPi;
				const float Tone = std::sin(P) + 0.30f * std::sin(2.f * P) + 0.09f * std::sin(3.f * P);
				const float Breath = Air.High(Rng.Bipolar()) * BreathEnv * 0.10f;
				BreathEnv -= BreathEnv * BreathCoef;
				const float S = Amp * E * (Tone * 0.78f + Breath);
				InOutL[i] += S * PanL;
				InOutR[i] += S * PanR;
				Phase += Inc * Pitch;
				if (Phase >= 1.f) { Phase -= 1.f; }
			}
			Air.Flush();
			TNMusic::MusicFlush(BreathEnv);
		}
	};

	/**
	 * Sala pequeña estéreo (Freeverb: 4 filtros de peine con amortiguación y 2 de paso total por canal). Búferes de tamaño
	 * fijo (sin asignaciones); las longitudes se escalan a la frecuencia de muestreo.
	 */
	class FSmallRoom
	{
	public:
		void Init(float InRate)
		{
			const float Scale = InRate / 44100.f;
			static constexpr int32_t CombTunings[NumCombs] = { 1116, 1188, 1277, 1356 };
			static constexpr int32_t AllpassTunings[NumAllpass] = { 556, 441 };
			static constexpr int32_t Spread = 23;
			for (int32_t k = 0; k < NumCombs; ++k)
			{
				CombL[k].Configure(static_cast<int32_t>(static_cast<float>(CombTunings[k]) * Scale));
				CombR[k].Configure(static_cast<int32_t>(static_cast<float>(CombTunings[k] + Spread) * Scale));
			}
			for (int32_t k = 0; k < NumAllpass; ++k)
			{
				AllL[k].Configure(static_cast<int32_t>(static_cast<float>(AllpassTunings[k]) * Scale));
				AllR[k].Configure(static_cast<int32_t>(static_cast<float>(AllpassTunings[k] + Spread) * Scale));
			}
			Clear();
		}

		void Clear()
		{
			for (int32_t k = 0; k < NumCombs; ++k) { CombL[k].Clear(); CombR[k].Clear(); }
			for (int32_t k = 0; k < NumAllpass; ++k) { AllL[k].Clear(); AllR[k].Clear(); }
		}

		/** Entra una muestra mono; salen las dos cuerdas de la cola (sin la señal seca). */
		void Process(float InSample, float& OutL, float& OutR)
		{
			const float In = InSample * 0.25f;
			float SumL = 0.f;
			float SumR = 0.f;
			for (int32_t k = 0; k < NumCombs; ++k)
			{
				SumL += CombL[k].Process(In);
				SumR += CombR[k].Process(In);
			}
			for (int32_t k = 0; k < NumAllpass; ++k)
			{
				SumL = AllL[k].Process(SumL);
				SumR = AllR[k].Process(SumR);
			}
			OutL = SumL;
			OutR = SumR;
		}

	private:
		static constexpr int32_t NumCombs = 4;
		static constexpr int32_t NumAllpass = 2;
		static constexpr int32_t MaxLen = 4096;
		static constexpr float kFeedback = 0.80f;
		static constexpr float kDamp = 0.30f;

		struct FComb
		{
			float Buf[MaxLen] = {};
			int32_t Len = 1;
			int32_t Idx = 0;
			float Store = 0.f;

			void Configure(int32_t InLen) { Len = std::clamp(InLen, 8, MaxLen - 1); Idx = 0; }
			void Clear() { for (float& V : Buf) { V = 0.f; } Idx = 0; Store = 0.f; }
			float Process(float InX)
			{
				const float Out = Buf[Idx];
				Store = Out * (1.f - kDamp) + Store * kDamp;
				TNMusic::MusicFlush(Store);
				Buf[Idx] = InX + Store * kFeedback;
				if (++Idx >= Len) { Idx = 0; }
				return Out;
			}
		};

		struct FAllpass
		{
			float Buf[MaxLen] = {};
			int32_t Len = 1;
			int32_t Idx = 0;

			void Configure(int32_t InLen) { Len = std::clamp(InLen, 8, MaxLen - 1); Idx = 0; }
			void Clear() { for (float& V : Buf) { V = 0.f; } Idx = 0; }
			float Process(float InX)
			{
				const float Delayed = Buf[Idx];
				const float Out = Delayed - InX;
				Buf[Idx] = InX + Delayed * 0.5f;
				if (++Idx >= Len) { Idx = 0; }
				return Out;
			}
		};

		FComb CombL[NumCombs];
		FComb CombR[NumCombs];
		FAllpass AllL[NumAllpass];
		FAllpass AllR[NumAllpass];
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Composición. Todo en semicorcheas (16 por compás). Los semitonos van desde la tónica (Mi bemol) y cada instrumento
	// tiene su nota MIDI de referencia en el motor.
	// ─────────────────────────────────────────────────────────────────────────

	/** Nota de melodía: paso dentro de la sección (0..127), duración en semicorcheas, semitono y fuerza (0..100). */
	struct FNote
	{
		uint8_t Step;
		uint8_t Len;
		int8_t Semi;
		uint8_t Vel;
	};

	/** Paso de bajo (se repite cada compás): paso, tono del acorde (0 fundamental, 1 tercera, 2 quinta, 3 octava o séptima), octava y fuerza. */
	struct FBassStep
	{
		uint8_t Step;
		int8_t Tone;
		int8_t Oct;
		uint8_t Vel;
	};

	namespace Score
	{
		// Grados sobre Mi bemol mayor: I = Mib, IV = Lab, V = Sib, vi = Do menor.
		inline constexpr int8_t I = 0, IV = 5, V = 7, Vi = 9;

		inline constexpr FMusicChordSpec ChordsA[kBarsPerSection] = {
			{ I, EChordType::Maj }, { I, EChordType::Maj }, { IV, EChordType::Maj }, { I, EChordType::Maj },
			{ I, EChordType::Maj }, { IV, EChordType::Maj }, { V, EChordType::Dom7 }, { I, EChordType::Maj },
		};
		// A': el compás 4 baja al relativo menor (la melodía sigue igual: la nota larga es su séptima).
		inline constexpr FMusicChordSpec ChordsA2[kBarsPerSection] = {
			{ I, EChordType::Maj }, { I, EChordType::Maj }, { IV, EChordType::Maj }, { Vi, EChordType::Min },
			{ I, EChordType::Maj }, { IV, EChordType::Maj }, { V, EChordType::Dom7 }, { I, EChordType::Maj },
		};
		// Puente: vi - IV - I - V7 | vi - IV - V7 - V7 (deja preparada la vuelta a la tónica).
		inline constexpr FMusicChordSpec ChordsB[kBarsPerSection] = {
			{ Vi, EChordType::Min }, { IV, EChordType::Maj }, { I, EChordType::Maj }, { V, EChordType::Dom7 },
			{ Vi, EChordType::Min }, { IV, EChordType::Maj }, { V, EChordType::Dom7 }, { V, EChordType::Dom7 },
		};

		// Tema de A (8 compases). Frase de pregunta (1-4) terminada en la quinta, y de respuesta (5-8) que resuelve en la
		// tónica: arpegios de corneta (Mib-Sol-Sib), notas de paso por grados conjuntos y la nota larga del final. Todas
		// las notas de los tiempos fuertes son del acorde (comprobado en el arnés).
		inline constexpr FNote MelodyA[] = {
			// Compás 1 (Mib)
			{ 0, 3, 19, 88 }, { 3, 1, 16, 70 }, { 4, 4, 19, 80 }, { 8, 4, 24, 92 }, { 12, 4, 19, 72 },
			// Compás 2 (Mib)
			{ 16, 3, 16, 80 }, { 19, 1, 17, 66 }, { 20, 4, 19, 78 }, { 24, 8, 16, 84 },
			// Compás 3 (Lab)
			{ 32, 3, 17, 86 }, { 35, 1, 21, 70 }, { 36, 4, 24, 92 }, { 40, 4, 21, 78 }, { 44, 4, 17, 72 },
			// Compás 4 (Mib)
			{ 48, 3, 16, 80 }, { 51, 1, 14, 62 }, { 52, 4, 16, 76 }, { 56, 8, 19, 88 },
			// Compás 5 (Mib): vuelve el motivo del compás 1
			{ 64, 3, 19, 88 }, { 67, 1, 16, 70 }, { 68, 4, 19, 80 }, { 72, 4, 24, 92 }, { 76, 4, 19, 72 },
			// Compás 6 (Lab)
			{ 80, 3, 21, 86 }, { 83, 1, 17, 68 }, { 84, 4, 21, 80 }, { 88, 4, 24, 92 }, { 92, 4, 21, 74 },
			// Compás 7 (Sib7): arpegio descendente del dominante
			{ 96, 3, 23, 86 }, { 99, 1, 19, 68 }, { 100, 4, 23, 80 }, { 104, 4, 17, 76 }, { 108, 4, 14, 72 },
			// Compás 8 (Mib): cierre ascendente
			{ 112, 2, 16, 80 }, { 114, 2, 19, 84 }, { 116, 8, 24, 96 },
		};

		// Tema del puente (8 compases): corcheas en arco, más fluido y marinero.
		inline constexpr FNote MelodyB[] = {
			// Compás 1 (Do menor)
			{ 0, 2, 16, 76 }, { 2, 2, 21, 80 }, { 4, 4, 24, 90 }, { 8, 2, 23, 74 }, { 10, 2, 21, 72 }, { 12, 4, 19, 78 },
			// Compás 2 (Lab)
			{ 16, 2, 17, 76 }, { 18, 2, 21, 80 }, { 20, 4, 24, 90 }, { 24, 2, 21, 74 }, { 26, 2, 17, 72 }, { 28, 4, 16, 78 },
			// Compás 3 (Mib)
			{ 32, 2, 16, 76 }, { 34, 2, 19, 80 }, { 36, 4, 24, 92 }, { 40, 2, 19, 74 }, { 42, 2, 21, 72 }, { 44, 4, 23, 82 },
			// Compás 4 (Sib7)
			{ 48, 4, 23, 86 }, { 52, 4, 19, 78 }, { 56, 4, 17, 74 }, { 60, 4, 14, 70 },
			// Compás 5 (Do menor)
			{ 64, 2, 16, 76 }, { 66, 2, 21, 80 }, { 68, 4, 24, 90 }, { 72, 2, 23, 74 }, { 74, 2, 21, 72 }, { 76, 4, 19, 78 },
			// Compás 6 (Lab)
			{ 80, 2, 17, 76 }, { 82, 2, 21, 80 }, { 84, 4, 24, 90 }, { 88, 2, 21, 74 }, { 90, 2, 17, 72 }, { 92, 4, 16, 78 },
			// Compás 7 (Sib7)
			{ 96, 2, 23, 80 }, { 98, 2, 26, 84 }, { 100, 4, 23, 80 }, { 104, 2, 19, 74 }, { 106, 2, 23, 78 }, { 108, 4, 17, 76 },
			// Compás 8 (Sib7): sube hacia la anacrusa del tema
			{ 112, 4, 19, 84 }, { 116, 4, 14, 70 }, { 120, 4, 23, 82 }, { 124, 2, 21, 74 }, { 126, 2, 19, 78 },
		};

		// Bajos: el «oom-pah» de marcha (negras, quinta en el contratiempo), la versión saltarina (A') y la del puente.
		inline constexpr FBassStep BassMarch[] = { { 0, 0, 0, 100 }, { 4, 2, 0, 62 }, { 8, 0, 0, 88 }, { 12, 2, 0, 62 }, { 14, 0, 1, 38 } };
		inline constexpr FBassStep BassBounce[] = { { 0, 0, 0, 100 }, { 3, 0, 1, 42 }, { 6, 2, 0, 72 }, { 8, 0, 0, 86 }, { 11, 0, 1, 42 }, { 14, 2, 0, 60 } };
		inline constexpr FBassStep BassHalf[] = { { 0, 0, 0, 100 }, { 6, 2, 0, 58 }, { 8, 0, 0, 82 }, { 12, 2, 0, 50 } };

		struct FSectionData
		{
			const FMusicChordSpec* Chords;
			const FNote* Melody;
			int32_t NumMelody;
			const FBassStep* Bass;
			int32_t NumBass;
		};

		inline const FSectionData& GetSection(int32_t InIndex)
		{
			static const FSectionData Table[kNumSections] = {
				{ ChordsA, MelodyA, static_cast<int32_t>(std::size(MelodyA)), BassMarch, static_cast<int32_t>(std::size(BassMarch)) },
				{ ChordsA2, MelodyA, static_cast<int32_t>(std::size(MelodyA)), BassBounce, static_cast<int32_t>(std::size(BassBounce)) },
				{ ChordsB, MelodyB, static_cast<int32_t>(std::size(MelodyB)), BassHalf, static_cast<int32_t>(std::size(BassHalf)) },
				{ ChordsA, MelodyA, static_cast<int32_t>(std::size(MelodyA)), BassMarch, static_cast<int32_t>(std::size(BassMarch)) },
			};
			return Table[std::clamp(InIndex, 0, kNumSections - 1)];
		}

		// Shaker en corcheas con acento en los «y» (como el de la tienda) y caja de marcha con notas fantasma.
		inline constexpr float ShakerPattern[kStepsPerBar] = {
			0.50f, 0.f, 0.30f, 0.f, 0.42f, 0.f, 0.30f, 0.f, 0.48f, 0.f, 0.30f, 0.f, 0.42f, 0.f, 0.32f, 0.f
		};
		inline constexpr float SnarePattern[kStepsPerBar] = {
			0.44f, 0.f, 0.f, 0.20f, 0.60f, 0.f, 0.22f, 0.f, 0.44f, 0.f, 0.f, 0.20f, 0.60f, 0.f, 0.22f, 0.f
		};
		// Tresillo de congas: paso, alta (true) o grave, fuerza.
		struct FConga
		{
			uint8_t Step;
			bool bHigh;
			uint8_t Vel;
		};
		inline constexpr FConga Congas[] = { { 3, true, 46 }, { 6, true, 40 }, { 10, false, 50 }, { 14, true, 36 } };
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Motor
	// ─────────────────────────────────────────────────────────────────────────

	class FEngine
	{
	public:
		void Init(float InRate)
		{
			Rate = std::max(8000.f, InRate);
			InvRate = 1.f / Rate;
			Limiter.Init(Rate);
			Limiter.Threshold = kLimiterCeiling;
			Room.Init(Rate);
			bRunning = false;
			FadePos = 0.f;
			Volume = 0.f;
			LastRestartSerial = 0u;
		}

		/** true mientras el motor calcula algo (sonando o en el fundido de salida). */
		bool IsRunning() const { return bRunning; }

		void Render(float* InOut, int32_t InNumFrames, int32_t InOutChannels, FRaceMusicParams& InParams)
		{
			const int32_t Channels = std::max(1, InOutChannels);
			int32_t Done = 0;
			while (Done < InNumFrames)
			{
				const int32_t N = std::min(TNMusic::MusicBlockFrames, InNumFrames - Done);
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

		/** Diagnóstico (arnés): voces robadas mientras aún sonaban. */
		int32_t GetVoiceStealCount() const
		{
			return LeadVoices.StealCount + FifeVoices.StealCount + CompVoices.StealCount + BassVoices.StealCount + CongaVoices.StealCount
				+ TimpVoices.StealCount + ShakerVoices.StealCount + SnareVoices.StealCount + BrassVoices.StealCount + CymbalVoices.StealCount
				+ OrnVoices.StealCount + BellVoices.StealCount;
		}

	private:
		// Ganancia de cada voz con fuerza 1, calibradas en el arnés fuera del motor (Tools/RaceMusic) para que la mezcla
		// base quede muy por debajo de la de los temas de la tienda y de fin de partida (es un fondo).
		static constexpr float kLeadTrim = 0.19f;
		static constexpr float kFifeTrim = 0.115f;
		static constexpr float kCompTrim = 0.33f;
		static constexpr float kBassTrim = 0.50f;
		static constexpr float kCongaTrim = 0.34f;
		static constexpr float kShakerGain = 0.50f;
		static constexpr float kSnareTrim = 0.26f;
		static constexpr float kBrassTrim = 0.12f;
		static constexpr float kBellTrim = 0.16f;
		static constexpr float kOrnTrim = 0.24f;
		static constexpr float kTimpTrim = 0.40f;
		static constexpr float kCymbalTrim = 0.16f;
		static constexpr float kClaveTrim = 0.30f;
		/** Un único dial de volumen general de la pieza (fondo). */
		static constexpr float kOutputTrim = 0.44f;
		static constexpr float kReverbSend = 0.55f;
		/** Toda la capa de tensión a la vez (para ajustarla respecto a la base con un solo número). */
		static constexpr float kTensionGain = 0.80f;
		/** Techo del limitador (lineal): -13 dBFS. Es un fondo: ni un golpe de acento se pasa de aquí. */
		static constexpr float kLimiterCeiling = 0.22f;

		// Notas MIDI de referencia de cada instrumento (semitonos desde Mi bemol).
		static constexpr int32_t kLeadBaseMidi = 63;    // Mib4: la melodía escrita en 12..28 es Mib5..Sol6
		static constexpr int32_t kChordBaseMidi = 63;   // marimbas en Mib4..
		static constexpr int32_t kBassBaseMidi = 39;    // Mib2
		static constexpr int32_t kBrassBaseMidi = 51;   // Mib3
		static constexpr int32_t kOrnBaseMidi = 75;     // Mib5 (kalimba de tensión)
		static constexpr int32_t kTimpBaseMidi = 39;    // Mib2

		enum class ELead : uint8_t { SteelPan, Fife, Both };

		struct FTremolo
		{
			bool bActive = false;
			FMusicModalVoice* Voice = nullptr;
			uint32_t StrikeId = 0;
			int64_t NextStep = 0;
			int64_t EndStep = 0;
			float Vel = 0.f;
		};

		/** Foto del paso que se está programando. */
		struct FCtx
		{
			int64_t G = 0;
			int32_t Section = 0;
			int32_t BarInSection = 0;
			int32_t StepInBar = 0;
			int32_t StepInSection = 0;
			int32_t Pass = 0;
			uint32_t Layers = ELayer::All;
			float T = 0.f;
			const FMusicChordSpec* Chord = nullptr;
		};

		void StartPiece(const FRaceMusicParams& InParams)
		{
			bRunning = true;
			FadePos = 0.f;
			StepPos = -static_cast<double>(kIntroSteps);
			NextStep = -static_cast<int64_t>(kIntroSteps);
			LeadVoices.Clear();
			FifeVoices.Clear();
			CompVoices.Clear();
			BassVoices.Clear();
			CongaVoices.Clear();
			TimpVoices.Clear();
			ShakerVoices.Clear();
			SnareVoices.Clear();
			BrassVoices.Clear();
			CymbalVoices.Clear();
			OrnVoices.Clear();
			BellVoices.Clear();
			LeadVoices.Configure(6, true);
			FifeVoices.Configure(6, true);
			CompVoices.Configure(8, true);
			BassVoices.Configure(4, true);
			CongaVoices.Configure(4, true);
			TimpVoices.Configure(4, true);
			ShakerVoices.Configure(4, true);
			SnareVoices.Configure(6, true);
			BrassVoices.Configure(4, true);
			CymbalVoices.Configure(2, true);
			OrnVoices.Configure(6, true);
			BellVoices.Configure(2, true);
			for (FTremolo& Trem : Tremolos) { Trem = FTremolo(); }
			Room.Clear();
			LpL = FMusicOnePole();
			LpR = FMusicOnePole();
			SendHigh = FMusicOnePole();
			SendHigh.SetHz(220.f, InvRate);
			Human.SetSeed(0xBEAC4u);
			// Parte de los objetivos actuales, sin rampas heredadas de la vez anterior.
			TensionSm = TNMusic::MusicClamp01(FRaceMusicParams::Get(InParams.Tension));
			DuckSm = TNMusic::MusicClamp01(FRaceMusicParams::Get(InParams.Duck));
			PrevGain = 0.f;
		}

		void RenderBlock(int32_t InN, FRaceMusicParams& InParams)
		{
			const float Dt = static_cast<float>(InN) * InvRate;
			const bool bWant = InParams.bPlay.load(std::memory_order_relaxed);
			const uint32_t Serial = InParams.RestartSerial.load(std::memory_order_relaxed);
			if (Serial != LastRestartSerial)
			{
				LastRestartSerial = Serial;
				if (bRunning && bWant) { StartPiece(InParams); }
			}
			if (!bRunning)
			{
				if (!bWant)
				{
					Silence(InN);
					InParams.bDebugRunning.store(false, std::memory_order_relaxed);
					return;
				}
				StartPiece(InParams);
			}

			// Fundido de entrada y de salida (curva suave): al llegar a cero sin querer sonar, el motor se para.
			const float FadeSeconds = std::clamp(FRaceMusicParams::Get(InParams.FadeSeconds), 0.05f, 30.f);
			FadePos = std::clamp(FadePos + (bWant ? Dt : -Dt) / FadeSeconds, 0.f, 1.f);
			if (!bWant && FadePos <= 0.f)
			{
				bRunning = false;
				Silence(InN);
				InParams.bDebugRunning.store(false, std::memory_order_relaxed);
				return;
			}
			const float MasterGain = FadePos * FadePos * (3.f - 2.f * FadePos);

			// Parámetros suavizados.
			Volume += (std::clamp(FRaceMusicParams::Get(InParams.Volume), 0.f, 1.5f) - Volume) * TNMusic::MusicTimeCoef(0.25f, Dt);
			const float TensionTarget = TNMusic::MusicClamp01(FRaceMusicParams::Get(InParams.Tension));
			TensionSm += (TensionTarget - TensionSm) * TNMusic::MusicTimeCoef(TensionTarget > TensionSm ? 2.0f : 3.0f, Dt);
			const float DuckTarget = TNMusic::MusicClamp01(FRaceMusicParams::Get(InParams.Duck));
			DuckSm += (DuckTarget - DuckSm) * TNMusic::MusicTimeCoef(DuckTarget > DuckSm ? 0.3f : 0.9f, Dt);

			// Secuenciador: el tempo sube un poco con la tensión.
			const float Bpm = kBaseBpm * (1.f + kTensionTempoBoost * TensionSm);
			StepPos += static_cast<double>(Dt) * static_cast<double>(Bpm) / 60.0 * 4.0;
			const int64_t TargetStep = static_cast<int64_t>(std::floor(StepPos));
			const uint32_t Layers = InParams.LayerMask.load(std::memory_order_relaxed);
			while (NextStep <= TargetStep)
			{
				TriggerStep(NextStep, Layers, TensionSm, Bpm);
				++NextStep;
			}

			// Voces.
			for (int32_t i = 0; i < InN; ++i) { LocalL[i] = 0.f; LocalR[i] = 0.f; }
			LeadVoices.Render(LocalL, LocalR, InN);
			FifeVoices.Render(LocalL, LocalR, InN);
			CompVoices.Render(LocalL, LocalR, InN);
			BassVoices.Render(LocalL, LocalR, InN);
			CongaVoices.Render(LocalL, LocalR, InN);
			TimpVoices.Render(LocalL, LocalR, InN);
			ShakerVoices.Render(LocalL, LocalR, InN);
			SnareVoices.Render(LocalL, LocalR, InN);
			BrassVoices.Render(LocalL, LocalR, InN);
			CymbalVoices.Render(LocalL, LocalR, InN);
			OrnVoices.Render(LocalL, LocalR, InN);
			BellVoices.Render(LocalL, LocalR, InN);

			// «Ducking»: ganancia en dB y paso bajo que se cierra.
			const float DuckGain = std::pow(10.f, kDuckDb * DuckSm / 20.f);
			const float CutoffHz = kDuckOpenHz * std::pow(kDuckClosedHz / kDuckOpenHz, DuckSm);
			LpL.SetHz(CutoffHz, InvRate);
			LpR.K = LpL.K;
			const float Gain = kOutputTrim * MasterGain * Volume * DuckGain;
			const float DGain = (Gain - PrevGain) / static_cast<float>(InN);
			float G = PrevGain;
			PrevGain = Gain;
			for (int32_t i = 0; i < InN; ++i)
			{
				G += DGain;
				const float DryL = LocalL[i];
				const float DryR = LocalR[i];
				float WetL = 0.f;
				float WetR = 0.f;
				Room.Process(SendHigh.High(0.5f * (DryL + DryR)) * kReverbSend, WetL, WetR);
				float OutL = LpL.Low(DryL + WetL) * G;
				float OutR = LpR.Low(DryR + WetR) * G;
				Limiter.Process(OutL, OutR);
				MixL[i] = OutL;
				MixR[i] = OutR;
			}
			LpL.Flush();
			LpR.Flush();
			SendHigh.Flush();

			const int64_t Cur = std::max<int64_t>(NextStep - 1, -1);
			InParams.bDebugRunning.store(true, std::memory_order_relaxed);
			InParams.DebugBar.store(TargetStep < 0 ? -1 : static_cast<int32_t>((Cur >> 4) % kBarsPerLoop), std::memory_order_relaxed);
			InParams.DebugPass.store(TargetStep < 0 ? 0 : static_cast<int32_t>((Cur / kStepsPerLoop) & 1), std::memory_order_relaxed);
			FRaceMusicParams::Set(InParams.DebugTension, TensionSm);
			FRaceMusicParams::Set(InParams.DebugDuck, DuckSm);
		}

		void Silence(int32_t InN)
		{
			for (int32_t i = 0; i < InN; ++i) { MixL[i] = 0.f; MixR[i] = 0.f; }
		}

		// ── Programación de cada semicorchea ────────────────────────────────

		float Jitter() { return 1.f + 0.06f * Human.Bipolar(); }

		static float Vel01(uint8_t InVel) { return static_cast<float>(InVel) * 0.01f; }

		float StepSeconds(float InBpm) const { return 60.f / (InBpm * 4.f); }

		void TriggerStep(int64_t InG, uint32_t InLayers, float InTension, float InBpm)
		{
			UpdateTremolos(InG);
			if (InG < 0)
			{
				TriggerIntro(static_cast<int32_t>(InG + kIntroSteps), InLayers, InBpm);
				return;
			}
			FCtx Ctx;
			Ctx.G = InG;
			Ctx.StepInBar = static_cast<int32_t>(InG & 15);
			const int32_t BarInLoop = static_cast<int32_t>((InG >> 4) % kBarsPerLoop);
			Ctx.Section = BarInLoop / kBarsPerSection;
			Ctx.BarInSection = BarInLoop % kBarsPerSection;
			Ctx.StepInSection = Ctx.BarInSection * kStepsPerBar + Ctx.StepInBar;
			Ctx.Pass = static_cast<int32_t>((InG / kStepsPerLoop) & 1);
			Ctx.Layers = InLayers;
			Ctx.T = InTension;
			Ctx.Chord = &Score::GetSection(Ctx.Section).Chords[Ctx.BarInSection];

			if (InLayers & ELayer::Melody) { TriggerLead(Ctx, InBpm); }
			if (InLayers & ELayer::Harmony) { TriggerComp(Ctx); }
			if (InLayers & ELayer::Rhythm) { TriggerRhythm(Ctx); }
			if (InLayers & ELayer::Brass) { TriggerBrass(Ctx, InBpm); }
			if ((InLayers & ELayer::Tension) && InTension > 0.05f) { TriggerTension(Ctx); }
		}

		/** Introducción: redoble que crece en el primer compás y toque de corneta en el segundo. */
		void TriggerIntro(int32_t InStep, uint32_t InLayers, float InBpm)
		{
			const float StepSec = StepSeconds(InBpm);
			if (InLayers & ELayer::Rhythm)
			{
				if (InStep < kStepsPerBar + 12)
				{
					const float Progress = static_cast<float>(InStep) / static_cast<float>(kStepsPerBar + 12);
					const float Vel = (0.24f + 0.42f * Progress) * ((InStep & 1) ? 0.72f : 1.f) * Jitter();
					SnareVoices.Allocate().Trigger(Vel * kSnareTrim, 0.10f, 0.22f, (InStep & 1) ? 0.18f : 0.06f, InvRate);
				}
				if (InStep == kStepsPerBar)
				{
					TimpVoices.Allocate().Trigger(kPulseTimpPreset, TNMusic::MusicMidiToHz(static_cast<float>(kTimpBaseMidi)), 0.6f * kTimpTrim, -0.2f, InvRate);
				}
			}
			if (InLayers & ELayer::Brass)
			{
				// Toque de corneta: Sib3 - Mib4 - Sol4 y Sib4 largo (rompe justo al entrar el tema).
				static constexpr int8_t CallSemis[4] = { 7, 12, 16, 19 };
				static constexpr int32_t CallSteps[4] = { 16, 18, 20, 22 };
				for (int32_t k = 0; k < 4; ++k)
				{
					if (InStep == CallSteps[k])
					{
						const float Len = (k == 3 ? 9.f : 1.8f) * StepSec;
						const float Hz = TNMusic::MusicMidiToHz(static_cast<float>(kBrassBaseMidi + CallSemis[k]));
						BrassVoices.Allocate().Trigger(Hz, 0.70f * kBrassTrim, 0.75f, Len, 0.f, k == 3 ? 0.5f : 0.f, InvRate);
					}
				}
			}
		}

		ELead LeadFor(int32_t InSection, int32_t InPass) const
		{
			switch (InSection)
			{
			case 0: return InPass == 0 ? ELead::SteelPan : ELead::Fife;
			case 1: return InPass == 0 ? ELead::Fife : ELead::SteelPan;
			case 2: return ELead::SteelPan;
			default: return InPass == 0 ? ELead::Both : ELead::SteelPan;
			}
		}

		void TriggerLead(const FCtx& InCtx, float InBpm)
		{
			const Score::FSectionData& Section = Score::GetSection(InCtx.Section);
			const ELead Lead = LeadFor(InCtx.Section, InCtx.Pass);
			const float StepSec = StepSeconds(InBpm);
			for (int32_t k = 0; k < Section.NumMelody; ++k)
			{
				const FNote& Note = Section.Melody[k];
				if (Note.Step != InCtx.StepInSection) { continue; }
				const float Hz = TNMusic::MusicMidiToHz(static_cast<float>(kLeadBaseMidi + Note.Semi));
				const float Vel = Vel01(Note.Vel) * Jitter();
				const float LenSec = static_cast<float>(Note.Len) * StepSec;
				if (Lead == ELead::SteelPan || Lead == ELead::Both)
				{
					const float Amp = Vel * kLeadTrim * (Lead == ELead::Both ? 0.85f : 1.f);
					FMusicModalVoice& Voice = LeadVoices.Allocate();
					Voice.Trigger(TNMusic::GetModalPreset(EModalPreset::SteelPan), Hz, Amp, -0.15f, InvRate);
					if (Note.Len >= 6)
					{
						// Trémolo del panista: golpes seguidos sobre la misma nota, sin reiniciarla.
						StartTremolo(Voice, InCtx.G + 1, InCtx.G + Note.Len - 1, Amp * 0.5f);
					}
				}
				if (Lead == ELead::Fife || Lead == ELead::Both)
				{
					FifeVoices.Allocate().Trigger(Hz, Vel * kFifeTrim * (Lead == ELead::Both ? 0.7f : 1.f), LenSec, 0.25f, InvRate);
				}
			}
		}

		void StartTremolo(FMusicModalVoice& InVoice, int64_t InFirstStep, int64_t InEndStep, float InVel)
		{
			FTremolo* Free = &Tremolos[0];
			for (FTremolo& Trem : Tremolos)
			{
				if (!Trem.bActive) { Free = &Trem; break; }
			}
			Free->bActive = InEndStep > InFirstStep;
			Free->Voice = &InVoice;
			Free->StrikeId = InVoice.StrikeId;
			Free->NextStep = InFirstStep;
			Free->EndStep = InEndStep;
			Free->Vel = InVel;
		}

		void UpdateTremolos(int64_t InG)
		{
			for (FTremolo& Trem : Tremolos)
			{
				if (!Trem.bActive) { continue; }
				if (InG > Trem.EndStep || !Trem.Voice || Trem.Voice->StrikeId != Trem.StrikeId) { Trem.bActive = false; continue; }
				if (InG >= Trem.NextStep)
				{
					Trem.Voice->Restrike(Trem.Vel * Jitter());
					Trem.Vel *= 0.86f;
					Trem.NextStep = InG + 1;
				}
			}
		}

		/** Marimbas: dos tonos del acorde a contratiempo («pah» del oom-pah), arpegio en A', golpes sueltos en el puente. */
		void TriggerComp(const FCtx& InCtx)
		{
			const int32_t S = InCtx.StepInBar;
			auto Hit = [&](int32_t InTone, float InVel, float InPan, bool bInKalimba)
			{
				const int32_t Semi = TNMusic::MusicChordTone(*InCtx.Chord, InTone);
				const float Hz = TNMusic::MusicMidiToHz(static_cast<float>(kChordBaseMidi + Semi));
				const int32_t Preset = bInKalimba ? EModalPreset::Kalimba : EModalPreset::Marimba;
				CompVoices.Allocate().Trigger(TNMusic::GetModalPreset(Preset), Hz, InVel * kCompTrim * Jitter(), InPan, InvRate);
			};
			switch (InCtx.Section)
			{
			case 1:
			{
				// A': arpegio de corcheas (fundamental, tercera, quinta, octava y de vuelta).
				if ((S & 1) == 0)
				{
					static constexpr int8_t Arp[8] = { 0, 1, 2, 3, 2, 1, 2, 1 };
					Hit(Arp[S >> 1], (S == 0 ? 0.62f : 0.50f), (S & 2) ? 0.30f : -0.30f, (S & 2) != 0);
				}
				break;
			}
			case 2:
			{
				// Puente: contratiempos escasos (segundo y tercer «y») para dejar sitio a la melodía.
				if (S == 2 || S == 10) { Hit(1, 0.52f, -0.3f, false); Hit(2, 0.46f, 0.3f, false); }
				break;
			}
			default:
			{
				// «Pah» del oom-pah: tercera y quinta juntas en los contratiempos.
				if (S == 2 || S == 6 || S == 10 || S == 14)
				{
					const float Vel = (S == 6 || S == 14) ? 0.56f : 0.50f;
					Hit(1, Vel, -0.3f, false);
					Hit(2, Vel * 0.9f, 0.3f, false);
				}
				break;
			}
			}
		}

		void TriggerRhythm(const FCtx& InCtx)
		{
			const int32_t S = InCtx.StepInBar;
			const Score::FSectionData& Section = Score::GetSection(InCtx.Section);
			const float TensionBoost = 1.f + 0.5f * InCtx.T;

			// Bajo.
			for (int32_t k = 0; k < Section.NumBass; ++k)
			{
				const FBassStep& Step = Section.Bass[k];
				if (Step.Step != S) { continue; }
				const int32_t Semi = TNMusic::MusicChordTone(*InCtx.Chord, Step.Tone) + Step.Oct * 12;
				const float Hz = TNMusic::MusicMidiToHz(static_cast<float>(kBassBaseMidi + Semi));
				BassVoices.Allocate().Trigger(TNMusic::GetModalPreset(EModalPreset::PizzicatoBass), Hz, Vel01(Step.Vel) * kBassTrim * Jitter(), 0.f, InvRate);
			}

			// Shaker.
			const float ShakerVel = Score::ShakerPattern[S];
			if (ShakerVel > 1e-4f)
			{
				ShakerVoices.Allocate().Trigger(6500.f, 3200.f, 0.055f, ShakerVel * kShakerGain * Jitter(), InvRate);
			}

			// Caja de marcha: en el puente cede el sitio a una caja china en los tiempos 2 y 4.
			if (InCtx.Section == 2)
			{
				if (S == 4 || S == 12)
				{
					CongaVoices.Allocate().Trigger(kClavePreset, TNMusic::MusicMidiToHz(88.f), 0.7f * kClaveTrim * Jitter(), 0.3f, InvRate);
				}
			}
			else
			{
				const float SnareVel = Score::SnarePattern[S];
				if (SnareVel > 1e-4f)
				{
					SnareVoices.Allocate().Trigger(SnareVel * kSnareTrim * TensionBoost * Jitter(), 0.16f, 0.45f, 0.12f, InvRate);
				}
			}

			// Congas en tresillo.
			for (const Score::FConga& Conga : Score::Congas)
			{
				if (Conga.Step != S) { continue; }
				const float Hz = Conga.bHigh ? 260.f : 165.f;
				CongaVoices.Allocate().Trigger(kCongaPreset, Hz, Vel01(Conga.Vel) * kCongaTrim * Jitter(), Conga.bHigh ? 0.5f : -0.5f, InvRate);
			}

			// Rellenos: la última negra del compás 4 y media medida del compás 8 (redoble que crece), y platillo al empezar cada sección.
			if (InCtx.BarInSection == 3 && S >= 12)
			{
				static constexpr float Fill[4] = { 0.30f, 0.38f, 0.48f, 0.62f };
				SnareVoices.Allocate().Trigger(Fill[S - 12] * kSnareTrim * TensionBoost * Jitter(), 0.11f, 0.22f, (S & 1) ? 0.18f : 0.06f, InvRate);
			}
			if (InCtx.BarInSection == 7 && S >= 8)
			{
				const float Progress = static_cast<float>(S - 8) / 7.f;
				SnareVoices.Allocate().Trigger((0.25f + 0.50f * Progress) * kSnareTrim * TensionBoost * Jitter(), 0.11f, 0.22f, (S & 1) ? 0.18f : 0.06f, InvRate);
			}
			if (InCtx.BarInSection == 0 && S == 0 && InCtx.Section != 2)
			{
				CymbalVoices.Allocate().Trigger(0.6f * kCymbalTrim, 1.8f, InvRate);
			}

			// Campana del barco al empezar el puente: «ding-ding».
			if (InCtx.Section == 2 && InCtx.BarInSection == 0 && (S == 0 || S == 3))
			{
				BellVoices.Allocate().Trigger(kShipBellPreset, TNMusic::MusicMidiToHz(79.f), (S == 0 ? 1.f : 0.72f) * kBellTrim, 0.35f, InvRate);
			}
		}

		/** Toque de corneta al final de A' y de A'' (Sib3 - Mib4 - Sol4 - Sib4, justo antes de que vuelva el tema). */
		void TriggerBrass(const FCtx& InCtx, float InBpm)
		{
			if (!((InCtx.Section == 1 || InCtx.Section == 3) && InCtx.BarInSection == 7)) { return; }
			static constexpr int8_t CallSemis[4] = { 7, 12, 16, 19 };
			static constexpr int32_t CallSteps[4] = { 8, 10, 12, 14 };
			const float StepSec = StepSeconds(InBpm);
			for (int32_t k = 0; k < 4; ++k)
			{
				if (InCtx.StepInBar != CallSteps[k]) { continue; }
				const float Hz = TNMusic::MusicMidiToHz(static_cast<float>(kBrassBaseMidi + CallSemis[k]));
				BrassVoices.Allocate().Trigger(Hz, 0.62f * kBrassTrim, 0.70f, 1.9f * StepSec, k < 2 ? -0.15f : 0.15f, 0.f, InvRate);
			}
		}

		/** Capa de tensión: cuanto más alta, más densa. Con menos de 0,05 no se llama siquiera. */
		void TriggerTension(const FCtx& InCtx)
		{
			const float T = InCtx.T;
			const int32_t S = InCtx.StepInBar;

			// Redoble continuo de caja en semicorcheas (acento en el tiempo, después el «y», después los débiles).
			{
				static constexpr float Accent[4] = { 1.0f, 0.42f, 0.72f, 0.42f };
				const float Vel = (0.10f + 0.50f * T) * Accent[S & 3] * kTensionGain * Jitter();
				SnareVoices.Allocate().Trigger(Vel * kSnareTrim, 0.10f, 0.22f, (S & 1) ? 0.20f : 0.04f, InvRate);
			}

			// Timbal a negras (más fuerte en el primer tiempo).
			if (T > 0.22f && (S & 3) == 0)
			{
				const float Vel = TNMusic::MusicClamp01((T - 0.15f) / 0.85f) * (S == 0 ? 1.f : 0.68f) * Jitter();
				const float Hz = TNMusic::MusicMidiToHz(static_cast<float>(kTimpBaseMidi + InCtx.Chord->RootSemitone));
				TimpVoices.Allocate().Trigger(kPulseTimpPreset, Hz, Vel * kTimpTrim * kTensionGain, -0.2f, InvRate);
			}

			// Bajo a corcheas en la octava de arriba (pulso nervioso).
			if (T > 0.14f && (S & 3) == 2)
			{
				const int32_t Semi = InCtx.Chord->RootSemitone + 12;
				const float Hz = TNMusic::MusicMidiToHz(static_cast<float>(kBassBaseMidi + Semi));
				BassVoices.Allocate().Trigger(TNMusic::GetModalPreset(EModalPreset::PizzicatoBass), Hz, (0.20f + 0.46f * T) * kBassTrim * kTensionGain * Jitter(), 0.f, InvRate);
			}

			// Ostinato de kalimba en tresillo (3 + 3 + 2): fundamental, quinta y octava del acorde.
			if (T > 0.38f && (S == 0 || S == 3 || S == 6 || S == 8 || S == 11 || S == 14))
			{
				static constexpr int8_t Tones[8] = { 0, 2, 3, 2, 0, 2, 3, 2 };
				const int32_t Idx = (S == 0) ? 0 : (S == 3) ? 1 : (S == 6) ? 2 : (S == 8) ? 3 : (S == 11) ? 4 : 5;
				const int32_t Semi = TNMusic::MusicChordTone(*InCtx.Chord, Tones[Idx]);
				const float Hz = TNMusic::MusicMidiToHz(static_cast<float>(kOrnBaseMidi + Semi));
				OrnVoices.Allocate().Trigger(TNMusic::GetModalPreset(EModalPreset::Kalimba), Hz, (T - 0.25f) * 1.05f * kOrnTrim * kTensionGain * Jitter(), (Idx & 1) ? 0.4f : -0.1f, InvRate);
			}

			// Shaker en semicorcheas.
			if (T > 0.30f && (S & 1) == 1)
			{
				ShakerVoices.Allocate().Trigger(6500.f, 3200.f, 0.04f, (0.16f + 0.14f * T) * kShakerGain * kTensionGain * Jitter(), InvRate);
			}

			// Metales: acorde corto en el primer tiempo de los compases pares.
			if (T > 0.62f && S == 0 && (InCtx.BarInSection & 1) == 1)
			{
				const float Vel = TNMusic::MusicClamp01((T - 0.5f) * 2.f) * kTensionGain;
				const int32_t Root = InCtx.Chord->RootSemitone;
				const float Hz1 = TNMusic::MusicMidiToHz(static_cast<float>(kBrassBaseMidi + Root));
				const float Hz2 = TNMusic::MusicMidiToHz(static_cast<float>(kBrassBaseMidi + Root + 7));
				BrassVoices.Allocate().Trigger(Hz1, Vel * kBrassTrim, 0.85f, 0.22f, -0.15f, 0.f, InvRate);
				BrassVoices.Allocate().Trigger(Hz2, Vel * kBrassTrim * 0.9f, 0.85f, 0.22f, 0.15f, 0.f, InvRate);
			}
		}

		// ── Estado ───────────────────────────────────────────────────────────

		float Rate = 48000.f;
		float InvRate = 1.f / 48000.f;
		bool bRunning = false;
		float FadePos = 0.f;
		float Volume = 0.f;
		float TensionSm = 0.f;
		float DuckSm = 0.f;
		float PrevGain = 0.f;
		uint32_t LastRestartSerial = 0u;
		double StepPos = 0.0;
		int64_t NextStep = 0;
		FMusicRandom Human{ 0xBEAC4u };
		FTremolo Tremolos[3];

		TMusicVoicePool<FMusicModalVoice, 6> LeadVoices;
		TMusicVoicePool<FFifeVoice, 6> FifeVoices;
		TMusicVoicePool<FMusicModalVoice, 8> CompVoices;
		TMusicVoicePool<FMusicModalVoice, 4> BassVoices;
		TMusicVoicePool<FMusicModalVoice, 4> CongaVoices;
		TMusicVoicePool<FMusicModalVoice, 4> TimpVoices;
		TMusicVoicePool<FMusicNoiseHit, 4> ShakerVoices;
		TMusicVoicePool<FMusicSnareVoice, 6> SnareVoices;
		TMusicVoicePool<FMusicBrassVoice, 4> BrassVoices;
		TMusicVoicePool<FMusicCymbalVoice, 2> CymbalVoices;
		TMusicVoicePool<FMusicModalVoice, 6> OrnVoices;
		TMusicVoicePool<FMusicModalVoice, 2> BellVoices;

		FSmallRoom Room;
		FMusicOnePole LpL;
		FMusicOnePole LpR;
		FMusicOnePole SendHigh;
		FMusicLimiter Limiter;
		float LocalL[TNMusic::MusicBlockFrames] = {};
		float LocalR[TNMusic::MusicBlockFrames] = {};
		float MixL[TNMusic::MusicBlockFrames] = {};
		float MixR[TNMusic::MusicBlockFrames] = {};
	};
}
