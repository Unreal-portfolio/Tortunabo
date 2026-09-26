#pragma once

#include "CoreMinimal.h"
#include <atomic>

/**
 * Motor DSP del ambiente sintetizado (UTN_AmbientSynthComponent): viento, oleaje, agua corriente y borboteo, aves,
 * cigarras, grillos, ranas, lava y campanas generados en tiempo real a partir de ruido y osciladores, sin assets.
 * Calidad de boceto: hace de ambiente mientras audio prepara los suyos (UTN_AmbienceDataAsset los sustituye).
 *
 * Hilos: FSharedParams lo escribe el hilo de juego (atómicos relajados) y lo lee el hilo de render de audio una vez
 * por bloque de control. Todo lo demás es estado del generador y solo lo toca el hilo de audio: sin asignaciones,
 * sin UObjects, sin logs ni bloqueos; el azar sale de un xorshift propio por capa (determinista con la semilla).
 * C++ puro (solo CoreMinimal) para poder probarlo fuera del motor.
 */
namespace TNAmbientDSP
{
	constexpr float Pi = 3.14159265f;
	constexpr float TwoPi = 6.28318531f;

	/** Muestras por bloque de control: parámetros, envolventes lentas y coeficientes se recalculan a este ritmo. */
	constexpr int32 BlockFrames = 32;

	/** Sub-bloque de las voces de canto: frecuencia y envolvente se interpolan en línea recta dentro. */
	constexpr int32 VoiceSubFrames = 16;

	/** Capas del paisaje sonoro (mismo orden que ETNAmbientLayer). */
	namespace Layer
	{
		constexpr int32 Wind = 0;
		constexpr int32 Surf = 1;
		constexpr int32 Stream = 2;
		constexpr int32 Birds = 3;
		constexpr int32 Cicadas = 4;
		constexpr int32 Crickets = 5;
		constexpr int32 Frogs = 6;
		constexpr int32 Lava = 7;
		constexpr int32 Bells = 8;
		constexpr int32 Count = 9;
	}

	/** Especies de aves (mismo orden que ETNAmbientBird). */
	namespace Bird
	{
		constexpr int32 Whistle = 0;     ///< Silbidos tropicales con glissandos.
		constexpr int32 Trill = 1;       ///< Trino rápido de pajarillo.
		constexpr int32 Macaw = 2;       ///< Graznido áspero de guacamayo.
		constexpr int32 Oropendola = 3;  ///< Gorgoteo líquido que sube y cae.
		constexpr int32 Gull = 4;        ///< Gaviota: «kiau» en serie, como una risa.
		constexpr int32 Heron = 5;       ///< Garza: graznido grave y ronco.
		constexpr int32 Piper = 6;       ///< Andarríos: «pip» agudos seguidos.
		constexpr int32 Crow = 7;        ///< Graja: «cra» nasal.
		constexpr int32 Eagle = 8;       ///< Águila lejana: chillido largo que cae.
		constexpr int32 Dove = 9;        ///< Paloma: arrullo grave.
		constexpr int32 Sparrow = 10;    ///< Gorrión: pío corto e irregular.
		constexpr int32 Hen = 11;        ///< Gallina lejana: «co-co-co... coooc».
		constexpr int32 Count = 12;
	}

	/** Llamadas de la capa de ranas (se eligen con pesos fijos, no por preajuste). */
	namespace Frog
	{
		constexpr int32 Croak = Bird::Count;       ///< «Cruac» pulsado.
		constexpr int32 Peeper = Bird::Count + 1;  ///< Ranita arborícola: «pii» que sube.
		constexpr int32 Bull = Bird::Count + 2;    ///< Rana toro: «rrum» muy grave.
		constexpr int32 Count = 3;
	}

	/** Qué genera una instancia: el paisaje 2D por capas o una fuente puntual 3D (mono). */
	namespace Kind
	{
		constexpr int32 Soundscape = 0;
		constexpr int32 Waterfall = 1;
		constexpr int32 Geyser = 2;
		constexpr int32 LavaPool = 3;
		constexpr int32 Count = 4;
	}

	/** Preajustes: los 8 biomas en el orden de ETNProcBiome y el genérico (sin mapa procedural). */
	namespace Preset
	{
		constexpr int32 Jungle = 0;
		constexpr int32 Beach = 1;
		constexpr int32 Desert = 2;
		constexpr int32 Volcanic = 3;
		constexpr int32 Water = 4;
		constexpr int32 Rocky = 5;
		constexpr int32 Mangrove = 6;
		constexpr int32 Human = 7;
		constexpr int32 Generic = 8;
		constexpr int32 Count = 9;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Parámetros compartidos y preajustes
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Lo que el hilo de juego fija y el de audio lee (una vez por bloque). Solo atómicos: sin colas, bloqueos ni memoria
	 * que liberar en el hilo de audio. Los niveles son objetivos: el motor los alcanza suavizados.
	 */
	struct FSharedParams
	{
		/** Volumen objetivo de cada capa (0 = apagada; 1 = presencia plena). */
		std::atomic<float> Layers[Layer::Count];
		/** Peso de cada especie de ave al elegir quién canta. */
		std::atomic<float> Birds[Bird::Count];
		/** Llamadas de aves por segundo. */
		std::atomic<float> BirdRate{ 0.5f };
		/** 0 = cerca y claras; 1 = lejanas y apagadas. */
		std::atomic<float> BirdDistance{ 0.4f };
		/** Viento: profundidad de las ráfagas, silbido de banda estrecha y brillo (0..1). */
		std::atomic<float> WindGust{ 0.4f };
		std::atomic<float> WindWhistle{ 0.f };
		std::atomic<float> WindBright{ 0.4f };
		/** Oleaje: 0 = chapoteo de laguna; 1 = rompientes de mar abierto. */
		std::atomic<float> SurfSize{ 0.5f };
		/** Tormenta (0..1): más ráfagas y truenos. */
		std::atomic<float> Storm{ 0.f };
		/** Cierre (0..1): cueva, bajo techo o bajo el agua; paso bajo general. */
		std::atomic<float> Enclosure{ 0.f };
		/** Fuentes 3D: fuerza del chorro del géiser o de la cascada (0..1; se sigue en ~50 ms). */
		std::atomic<float> Intensity{ 1.f };
		/** Ganancia final. */
		std::atomic<float> Master{ 1.f };
		/** Segundos del suavizado de los volúmenes de capa. */
		std::atomic<float> SmoothSeconds{ 1.5f };
		/** Fijados antes de arrancar: el generador los lee al crearse. */
		std::atomic<int32> SourceKind{ Kind::Soundscape };
		std::atomic<uint32> Seed{ 1u };

		FSharedParams()
		{
			for (std::atomic<float>& Value : Layers) { Value.store(0.f, std::memory_order_relaxed); }
			for (std::atomic<float>& Value : Birds) { Value.store(0.f, std::memory_order_relaxed); }
		}

		static float Get(const std::atomic<float>& Value) { return Value.load(std::memory_order_relaxed); }
		static void Set(std::atomic<float>& Value, float NewValue) { Value.store(NewValue, std::memory_order_relaxed); }
	};

	/** Lo que suena con un bioma al 100 % (el paisaje sonoro mezcla varios con los pesos de bioma). */
	struct FBiomePreset
	{
		float Layers[Layer::Count] = {};
		float Birds[Bird::Count] = {};
		float BirdRate = 0.f;
		float BirdDistance = 0.4f;
		float WindGust = 0.4f;
		float WindWhistle = 0.f;
		float WindBright = 0.4f;
		/** Tamaño del oleaje si hay agua cerca (el paisaje sonoro lo acerca a 1 junto al mar abierto). */
		float SurfSize = 0.5f;
	};

	inline FBiomePreset MakePreset(int32 Index)
	{
		FBiomePreset Out;
		float* Lv = Out.Layers;
		float* Bw = Out.Birds;
		switch (Index)
		{
		case Preset::Jungle:
			// Selva: aves tropicales variadas (silbidos, trinos, guacamayos, oropéndolas), cigarras, ranas y algo de
			// grillos; el dosel frena el viento.
			Lv[Layer::Wind] = 0.22f; Lv[Layer::Stream] = 0.04f; Lv[Layer::Birds] = 0.9f; Lv[Layer::Cicadas] = 0.55f;
			Lv[Layer::Crickets] = 0.18f; Lv[Layer::Frogs] = 0.35f;
			Bw[Bird::Whistle] = 1.f; Bw[Bird::Trill] = 0.8f; Bw[Bird::Macaw] = 0.65f; Bw[Bird::Oropendola] = 0.55f; Bw[Bird::Dove] = 0.12f;
			Out.BirdRate = 1.3f; Out.BirdDistance = 0.3f; Out.WindGust = 0.3f; Out.WindBright = 0.2f;
			break;
		case Preset::Beach:
			// Playa: gaviotas y andarríos, brisa marina; el oleaje depende del agua que haya cerca.
			Lv[Layer::Wind] = 0.45f; Lv[Layer::Surf] = 0.85f; Lv[Layer::Birds] = 0.6f; Lv[Layer::Crickets] = 0.04f;
			Bw[Bird::Gull] = 1.f; Bw[Bird::Piper] = 0.35f; Bw[Bird::Whistle] = 0.05f;
			Out.BirdRate = 0.45f; Out.BirdDistance = 0.35f; Out.WindGust = 0.55f; Out.WindBright = 0.5f; Out.SurfSize = 1.f;
			break;
		case Preset::Desert:
			// Desierto: viento con silbido que barre, cigarras de calor y pocas aves, lejanas.
			Lv[Layer::Wind] = 0.85f; Lv[Layer::Birds] = 0.3f; Lv[Layer::Cicadas] = 0.45f; Lv[Layer::Crickets] = 0.06f;
			Bw[Bird::Whistle] = 0.4f; Bw[Bird::Eagle] = 0.3f; Bw[Bird::Crow] = 0.2f;
			Out.BirdRate = 0.12f; Out.BirdDistance = 0.75f; Out.WindGust = 0.7f; Out.WindWhistle = 0.8f; Out.WindBright = 0.6f;
			break;
		case Preset::Volcanic:
			// Volcán: rumor grave de lava con crepitar y burbujas, viento caliente, casi sin aves.
			Lv[Layer::Wind] = 0.45f; Lv[Layer::Birds] = 0.12f; Lv[Layer::Lava] = 0.8f;
			Bw[Bird::Crow] = 0.3f; Bw[Bird::Eagle] = 0.1f;
			Out.BirdRate = 0.06f; Out.BirdDistance = 0.8f; Out.WindGust = 0.5f; Out.WindWhistle = 0.2f; Out.WindBright = 0.4f;
			break;
		case Preset::Water:
			// Agua con isletas: chapoteo de laguna, gaviotas, garzas y andarríos, alguna rana.
			Lv[Layer::Wind] = 0.45f; Lv[Layer::Surf] = 0.8f; Lv[Layer::Stream] = 0.05f; Lv[Layer::Birds] = 0.6f;
			Lv[Layer::Crickets] = 0.04f; Lv[Layer::Frogs] = 0.2f;
			Bw[Bird::Gull] = 0.5f; Bw[Bird::Heron] = 0.3f; Bw[Bird::Piper] = 0.5f; Bw[Bird::Whistle] = 0.1f;
			Out.BirdRate = 0.4f; Out.BirdDistance = 0.45f; Out.WindGust = 0.5f; Out.WindBright = 0.5f; Out.SurfSize = 0.35f;
			break;
		case Preset::Rocky:
			// Acantilados: viento fuerte con algo de silbido, grajas y un águila lejana.
			Lv[Layer::Wind] = 0.7f; Lv[Layer::Birds] = 0.35f; Lv[Layer::Crickets] = 0.06f;
			Bw[Bird::Crow] = 1.f; Bw[Bird::Eagle] = 0.35f; Bw[Bird::Whistle] = 0.1f;
			Out.BirdRate = 0.3f; Out.BirdDistance = 0.5f; Out.WindGust = 0.8f; Out.WindWhistle = 0.3f; Out.WindBright = 0.7f;
			break;
		case Preset::Mangrove:
			// Manglar: agua quieta que borbotea, garzas y pájaros de agua, ranas y grillos, algo de cigarra.
			Lv[Layer::Wind] = 0.2f; Lv[Layer::Surf] = 0.25f; Lv[Layer::Stream] = 0.25f; Lv[Layer::Birds] = 0.6f;
			Lv[Layer::Cicadas] = 0.15f; Lv[Layer::Crickets] = 0.5f; Lv[Layer::Frogs] = 0.7f;
			Bw[Bird::Heron] = 0.7f; Bw[Bird::Piper] = 0.8f; Bw[Bird::Whistle] = 0.3f; Bw[Bird::Oropendola] = 0.2f;
			Out.BirdRate = 0.7f; Out.BirdDistance = 0.35f; Out.WindGust = 0.25f; Out.WindBright = 0.25f; Out.SurfSize = 0.1f;
			break;
		case Preset::Human:
			// Zona humana: palomas, gorriones y gallinas lejanas; campanillas de viento y alguna campana lejana.
			Lv[Layer::Wind] = 0.45f; Lv[Layer::Birds] = 0.85f; Lv[Layer::Crickets] = 0.15f; Lv[Layer::Bells] = 0.8f;
			Bw[Bird::Dove] = 1.f; Bw[Bird::Sparrow] = 1.f; Bw[Bird::Hen] = 0.5f; Bw[Bird::Crow] = 0.1f;
			Out.BirdRate = 0.8f; Out.BirdDistance = 0.35f; Out.WindGust = 0.4f; Out.WindBright = 0.4f;
			break;
		default:
			// Genérico (sin mapa procedural): brisa suave, pocos pájaros y algún grillo.
			Lv[Layer::Wind] = 0.5f; Lv[Layer::Birds] = 0.55f; Lv[Layer::Crickets] = 0.08f;
			Bw[Bird::Whistle] = 0.3f; Bw[Bird::Sparrow] = 0.4f; Bw[Bird::Dove] = 0.3f; Bw[Bird::Crow] = 0.1f;
			Out.BirdRate = 0.3f; Out.BirdDistance = 0.55f; Out.WindGust = 0.4f; Out.WindBright = 0.4f;
			break;
		}
		return Out;
	}

	inline const FBiomePreset& GetPreset(int32 Index)
	{
		static const FBiomePreset Table[Preset::Count] = {
			MakePreset(0), MakePreset(1), MakePreset(2), MakePreset(3), MakePreset(4),
			MakePreset(5), MakePreset(6), MakePreset(7), MakePreset(8) };
		return Table[FMath::Clamp(Index, 0, Preset::Count - 1)];
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Utilidades
	// ─────────────────────────────────────────────────────────────────────────

	/** xorshift32 por instancia: barato, determinista, sin estado global. */
	struct FRandom
	{
		uint32 State = 0x9E3779B9u;

		void SetSeed(uint32 InSeed)
		{
			State = InSeed != 0u ? InSeed : 0x9E3779B9u;
			// Unas vueltas para que semillas parecidas no den secuencias parecidas.
			for (int32 k = 0; k < 6; ++k) { NextU(); }
		}

		uint32 NextU()
		{
			uint32 X = State;
			X ^= X << 13;
			X ^= X >> 17;
			X ^= X << 5;
			State = X;
			return X;
		}

		/** [0, 1). */
		float Unit() { return static_cast<float>(NextU() >> 8) * (1.f / 16777216.f); }
		/** [-1, 1). */
		float Bipolar() { return Unit() * 2.f - 1.f; }
		float Range(float A, float B) { return A + (B - A) * Unit(); }
		bool Chance(float P) { return Unit() < P; }
		int32 Int(int32 A, int32 B) { return A + static_cast<int32>(Unit() * static_cast<float>(B - A + 1)); }
		/** Espera exponencial de media Mean: llegadas al azar como las de un proceso de Poisson. */
		float Exponential(float Mean) { return -Mean * FMath::Loge(1.f - Unit() * 0.999f); }

		/** Índice al azar con pesos; -1 si todos son 0. */
		int32 Weighted(const float* Weights, int32 Num)
		{
			float Sum = 0.f;
			for (int32 k = 0; k < Num; ++k) { Sum += FMath::Max(0.f, Weights[k]); }
			if (Sum <= 1e-6f) { return -1; }
			float Pick = Unit() * Sum;
			for (int32 k = 0; k < Num; ++k)
			{
				Pick -= FMath::Max(0.f, Weights[k]);
				if (Pick < 0.f) { return k; }
			}
			return Num - 1;
		}
	};

	/** Semilla derivada para cada capa (que no compartan secuencia). */
	inline uint32 MixSeed(uint32 Seed, uint32 Salt)
	{
		uint32 H = Seed ^ (Salt * 0x9E3779B9u);
		H ^= H >> 16;
		H *= 0x85EBCA6Bu;
		H ^= H >> 13;
		H *= 0xC2B2AE35u;
		H ^= H >> 16;
		return H != 0u ? H : 0x1234567u;
	}

	/** sin(2π·P) para P en [0, 1): parábola con corrección (error < 0,1 %), sin llamadas a la librería. */
	inline float FastSin01(float P)
	{
		const float U = 2.f * P - 1.f;
		const float Y = 4.f * U * (1.f - FMath::Abs(U));
		return -(0.225f * (Y * FMath::Abs(Y) - Y) + Y);
	}

	/** Saturación suave (aproximación racional de tanh, ±1 como mucho). */
	inline float SoftClip(float X)
	{
		const float C = FMath::Clamp(X, -3.f, 3.f);
		return C * (27.f + C * C) / (27.f + 9.f * C * C);
	}

	inline float SmoothStep01(float X)
	{
		const float C = FMath::Clamp(X, 0.f, 1.f);
		return C * C * (3.f - 2.f * C);
	}

	/** Paneo de potencia constante: Pan en [-1, 1]. */
	inline void PanGains(float Pan, float& OutL, float& OutR)
	{
		const float Angle = (FMath::Clamp(Pan, -1.f, 1.f) + 1.f) * (Pi * 0.25f);
		OutL = FMath::Cos(Angle);
		OutR = FMath::Sin(Angle);
	}

	/** Coeficiente de un filtro de un polo para una frecuencia de corte. */
	inline float OnePoleCoef(float Hz, float InvRate)
	{
		return 1.f - FMath::Exp(-TwoPi * FMath::Max(1.f, Hz) * InvRate);
	}

	/** Coeficiente de acercamiento exponencial con constante de tiempo Seconds en un paso de Dt segundos. */
	inline float TimeCoef(float Seconds, float Dt)
	{
		return Seconds <= 1e-4f ? 1.f : 1.f - FMath::Exp(-Dt / Seconds);
	}

	/** Filtro de un polo (paso bajo y, restando, paso alto). */
	struct FOnePole
	{
		float Z = 0.f;
		float K = 1.f;

		void SetHz(float Hz, float InvRate) { K = OnePoleCoef(Hz, InvRate); }
		float Low(float X) { Z += K * (X - Z); return Z; }
		float High(float X) { Z += K * (X - Z); return X - Z; }
		void Flush() { if (FMath::Abs(Z) < 1e-15f) { Z = 0.f; } }
	};

	/** Filtro de estado variable TPT (Zavalishin/Simper): estable aunque se module el corte. */
	struct FSvf
	{
		float Ic1 = 0.f;
		float Ic2 = 0.f;
		float G = 0.f;
		float K = 1.f;
		float A1 = 1.f;
		float A2 = 0.f;
		float A3 = 0.f;
		float Lp = 0.f;
		float Bp = 0.f;

		void Set(float Hz, float Q, float InvRate)
		{
			const float Norm = FMath::Clamp(Hz * InvRate, 1e-5f, 0.45f);
			G = FMath::Tan(Pi * Norm);
			K = 1.f / FMath::Max(0.05f, Q);
			A1 = 1.f / (1.f + G * (G + K));
			A2 = G * A1;
			A3 = G * A2;
		}

		void CopyCoefs(const FSvf& Other)
		{
			G = Other.G;
			K = Other.K;
			A1 = Other.A1;
			A2 = Other.A2;
			A3 = Other.A3;
		}

		void Reset() { Ic1 = Ic2 = Lp = Bp = 0.f; }

		void Tick(float In)
		{
			const float V3 = In - Ic2;
			const float V1 = A1 * Ic1 + A2 * V3;
			const float V2 = Ic2 + A2 * Ic1 + A3 * V3;
			Ic1 = 2.f * V1 - Ic1;
			Ic2 = 2.f * V2 - Ic2;
			Lp = V2;
			Bp = V1;
		}

		float Low(float X) { Tick(X); return Lp; }
		/** Paso banda normalizado: 0 dB en el centro. */
		float Band(float X) { Tick(X); return Bp * K; }
		float High(float X) { Tick(X); return X - K * Bp - Lp; }

		void Flush()
		{
			if (FMath::Abs(Ic1) < 1e-15f) { Ic1 = 0.f; }
			if (FMath::Abs(Ic2) < 1e-15f) { Ic2 = 0.f; }
		}
	};

	/** Ruido rosa (filtro «económico» de Paul Kellet sobre ruido blanco). */
	struct FPink
	{
		float B0 = 0.f;
		float B1 = 0.f;
		float B2 = 0.f;

		float Next(float White)
		{
			B0 = 0.99765f * B0 + White * 0.0990460f;
			B1 = 0.96300f * B1 + White * 0.2965164f;
			B2 = 0.57000f * B2 + White * 1.0526913f;
			return (B0 + B1 + B2 + White * 0.1848f) * 0.22f;
		}
	};

	/**
	 * Ruido marrón: integrador con fuga sobre ruido blanco, sin lo subsónico (por debajo de ~25 Hz a 48 kHz): no se
	 * oye y se comería el margen del limitador.
	 */
	struct FBrown
	{
		float Z = 0.f;
		float Dc = 0.f;

		float Next(float White)
		{
			Z = Z * 0.996f + White * 0.06f;
			Dc += 0.0033f * (Z - Dc);
			return Z - Dc;
		}
	};

	/** Modo resonante (seno amortiguado por recurrencia de dos polos): barras de campanilla y parciales de campana. */
	struct FModal
	{
		float Y1 = 0.f;
		float Y2 = 0.f;
		float B1 = 0.f;
		float B2 = 0.f;
		float GainL = 0.f;
		float GainR = 0.f;
		bool bActive = false;

		void Strike(float Hz, float T60, float Amp, float InGainL, float InGainR, float InvRate)
		{
			const float W = TwoPi * FMath::Min(Hz * InvRate, 0.45f);
			const float Radius = FMath::Exp(-6.9078f * InvRate / FMath::Max(0.01f, T60));
			B1 = 2.f * Radius * FMath::Cos(W);
			B2 = -Radius * Radius;
			Y1 = Amp * FMath::Sin(W);
			Y2 = 0.f;
			GainL = InGainL;
			GainR = InGainR;
			bActive = true;
		}

		float Tick()
		{
			const float Y = B1 * Y1 + B2 * Y2;
			Y2 = Y1;
			Y1 = Y;
			return Y;
		}

		void CheckDone()
		{
			if (FMath::Abs(Y1) + FMath::Abs(Y2) < 1e-6f)
			{
				bActive = false;
				Y1 = Y2 = 0.f;
			}
		}
	};

	/** Burbujas: senos cortos cuyo tono sube mientras se apagan (el «plip» del agua; graves, el «blorp» de la lava). */
	struct FBubbles
	{
		static constexpr int32 MaxVoices = 10;

		struct FVoice
		{
			float Phase = 0.f;
			float Inc = 0.f;
			float IncMul = 1.f;
			float Amp = 0.f;
			float Decay = 0.f;
			float PanL = 0.7f;
			float PanR = 0.7f;
		};

		FVoice Voices[MaxVoices];
		int32 Next = 0;
		float InvRate = 1.f / 48000.f;

		void Init(float InRate) { InvRate = 1.f / InRate; }

		/** Hz inicial, subida total del tono (factor), duración (s, hasta -60 dB), amplitud y paneo (-1..1). */
		void Trigger(float Hz, float Rise, float Seconds, float InAmp, float Pan)
		{
			FVoice& V = Voices[Next];
			Next = (Next + 1) % MaxVoices;
			const float Samples = FMath::Max(1.f, Seconds / InvRate);
			V.Phase = 0.f;
			V.Inc = FMath::Min(Hz * InvRate, 0.45f);
			V.IncMul = FMath::Pow(FMath::Max(1.f, Rise), 1.f / Samples);
			V.Decay = FMath::Exp(-6.9f / Samples);
			V.Amp = InAmp;
			PanGains(Pan, V.PanL, V.PanR);
		}

		void Render(float* L, float* R, int32 N)
		{
			for (FVoice& V : Voices)
			{
				if (V.Amp < 1e-4f) { continue; }
				for (int32 i = 0; i < N; ++i)
				{
					const float S = FastSin01(V.Phase) * V.Amp;
					V.Phase += V.Inc;
					if (V.Phase >= 1.f) { V.Phase -= 1.f; }
					V.Inc = FMath::Min(V.Inc * V.IncMul, 0.45f);
					V.Amp *= V.Decay;
					L[i] += S * V.PanL;
					R[i] += S * V.PanR;
				}
			}
		}

		void RenderMono(float* M, int32 N)
		{
			for (FVoice& V : Voices)
			{
				if (V.Amp < 1e-4f) { continue; }
				for (int32 i = 0; i < N; ++i)
				{
					M[i] += FastSin01(V.Phase) * V.Amp;
					V.Phase += V.Inc;
					if (V.Phase >= 1.f) { V.Phase -= 1.f; }
					V.Inc = FMath::Min(V.Inc * V.IncMul, 0.45f);
					V.Amp *= V.Decay;
				}
			}
		}
	};

	/** Valores de control ya suavizados que las capas usan en un bloque. */
	struct FControl
	{
		float Birds[Bird::Count] = {};
		float BirdRate = 0.f;
		float BirdDistance = 0.4f;
		float WindGust = 0.4f;
		float WindWhistle = 0.f;
		float WindBright = 0.4f;
		float SurfSize = 0.5f;
		float Storm = 0.f;
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Viento
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Viento: ruido rosa por un paso bajo resonante cuyo corte y volumen siguen ráfagas lentas al azar, un rumor de
	 * ruido marrón muy grave y un silbido de banda estrecha que barre despacio (desierto, cumbres). Con tormenta, las
	 * ráfagas son más frecuentes y fuertes y suenan truenos lejanos. Ruido distinto en cada canal.
	 */
	struct FWind
	{
		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FPink PinkL;
		FPink PinkR;
		FBrown BrownL;
		FBrown BrownR;
		FSvf BodyL;
		FSvf BodyR;
		FOnePole RumbleL;
		FOnePole RumbleR;
		FSvf Whistle;
		float Gust = 0.4f;
		float GustTarget = 0.4f;
		float GustTimer = 0.f;
		float Flutter = 0.f;
		float FlutterTarget = 0.f;
		float FlutterTimer = 0.f;
		float Sweep = 0.f;
		float PrevBody = 0.f;
		float PrevRumble = 0.f;
		float PrevWhistle = 0.f;
		float PrevWhistlePan = 0.f;
		// Truenos (solo con tormenta).
		float ThunderTimer = 6.f;
		float ThunderEnv = 0.f;
		float ThunderPeak = 0.f;
		float ThunderRise = 0.f;
		float ThunderFall = 0.f;
		bool bThunderRising = false;
		float ThunderRoll = 1.f;
		float ThunderRollTarget = 1.f;
		float ThunderRollTimer = 0.f;
		float PrevThunder = 0.f;
		float ThunderPanL = 0.7f;
		float ThunderPanR = 0.7f;
		FBrown ThunderBrown;
		FOnePole ThunderLow1;
		FOnePole ThunderLow2;

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			RumbleL.SetHz(90.f, InvRate);
			RumbleR.SetHz(90.f, InvRate);
			ThunderLow1.SetHz(110.f, InvRate);
			ThunderLow2.SetHz(260.f, InvRate);
			Sweep = Rng.Unit();
			Gust = GustTarget = Rng.Range(0.3f, 0.6f);
			ThunderTimer = Rng.Range(4.f, 9.f);
		}

		void Render(float* L, float* R, int32 N, const FControl& C)
		{
			const float Dt = static_cast<float>(N) * InvRate;

			// Ráfagas: un objetivo al azar cada 1,2-4 s al que se acerca despacio (con tormenta, más a menudo y más fuertes).
			GustTimer -= Dt;
			if (GustTimer <= 0.f)
			{
				GustTarget = FMath::Pow(Rng.Unit(), 0.9f - 0.5f * C.Storm);
				GustTimer = Rng.Range(1.2f, 4.f) * (1.f - 0.45f * C.Storm);
			}
			Gust += (GustTarget - Gust) * TimeCoef(0.9f, Dt);
			FlutterTimer -= Dt;
			if (FlutterTimer <= 0.f)
			{
				FlutterTarget = Rng.Bipolar();
				FlutterTimer = Rng.Range(0.12f, 0.45f);
			}
			Flutter += (FlutterTarget - Flutter) * TimeCoef(0.15f, Dt);

			const float Depth = FMath::Clamp(C.WindGust, 0.f, 1.f);
			const float Force = FMath::Clamp(Gust + 0.2f * Flutter * Depth, 0.f, 1.2f);
			const float BodyGain = FMath::Lerp(0.8f, 0.12f + 1.4f * Force, Depth);
			const float Cut = 160.f + 650.f * C.WindBright + (350.f + 1500.f * C.WindBright) * Force * (0.35f + 0.65f * Depth);
			BodyL.Set(Cut, 0.8f + 0.9f * Force * Depth, InvRate);
			BodyR.CopyCoefs(BodyL);
			const float RumbleGain = (0.3f + 0.7f * Force) * (0.6f + 0.4f * Depth) * 2.f;

			// Silbido: banda estrecha que barre despacio y sube con la ráfaga, paseándose de un lado a otro.
			Sweep += Dt * (0.035f + 0.08f * Force);
			if (Sweep >= 1.f) { Sweep -= 1.f; }
			const float WhistleHz = FMath::Lerp(520.f, 1350.f, 0.5f + 0.5f * FastSin01(Sweep)) * (0.85f + 0.35f * Force);
			Whistle.Set(WhistleHz, 15.f, InvRate);
			const float WhistleGain = C.WindWhistle * (0.2f + 0.8f * Force * Force) * 12.f;
			const float WhistlePan = 0.75f * FastSin01(FMath::Frac(Sweep * 3.f));

			// Truenos: con tormenta, cada 7-20 s; suben en 0,1-0,5 s, retumban y se apagan en varios segundos.
			if (C.Storm > 0.05f)
			{
				ThunderTimer -= Dt;
				if (ThunderTimer <= 0.f && ThunderEnv < 0.05f)
				{
					ThunderPeak = Rng.Range(0.45f, 1.f) * FMath::Min(1.f, C.Storm);
					bThunderRising = true;
					ThunderRise = TimeCoef(Rng.Range(0.08f, 0.3f), Dt);
					ThunderFall = TimeCoef(Rng.Range(0.7f, 1.5f), Dt);
					PanGains(Rng.Range(-0.7f, 0.7f), ThunderPanL, ThunderPanR);
					ThunderTimer = Rng.Range(7.f, 20.f) / (0.4f + C.Storm);
				}
			}
			if (bThunderRising)
			{
				ThunderEnv += (ThunderPeak * 1.1f - ThunderEnv) * ThunderRise;
				if (ThunderEnv >= ThunderPeak) { bThunderRising = false; }
			}
			else
			{
				ThunderEnv -= ThunderEnv * ThunderFall;
				if (ThunderEnv < 1e-5f) { ThunderEnv = 0.f; }
			}
			ThunderRollTimer -= Dt;
			if (ThunderRollTimer <= 0.f)
			{
				ThunderRollTarget = Rng.Range(0.3f, 1.f);
				ThunderRollTimer = Rng.Range(0.08f, 0.35f);
			}
			ThunderRoll += (ThunderRollTarget - ThunderRoll) * TimeCoef(0.1f, Dt);
			const float ThunderGain = ThunderEnv * ThunderRoll * 16.f;
			const bool bThunder = ThunderGain > 1e-5f || PrevThunder > 1e-5f;

			const float InvN = 1.f / static_cast<float>(N);
			float Gb = PrevBody;
			float Gr = PrevRumble;
			float Gw = PrevWhistle;
			float Pw = PrevWhistlePan;
			float Gt = PrevThunder;
			const float DGb = (BodyGain - PrevBody) * InvN;
			const float DGr = (RumbleGain - PrevRumble) * InvN;
			const float DGw = (WhistleGain - PrevWhistle) * InvN;
			const float DPw = (WhistlePan - PrevWhistlePan) * InvN;
			const float DGt = (ThunderGain - PrevThunder) * InvN;
			for (int32 i = 0; i < N; ++i)
			{
				Gb += DGb;
				Gr += DGr;
				Gw += DGw;
				Pw += DPw;
				Gt += DGt;
				const float NL = Rng.Bipolar();
				const float NR = Rng.Bipolar();
				const float BL = BodyL.Low(PinkL.Next(NL));
				const float BR = BodyR.Low(PinkR.Next(NR));
				const float RL = RumbleL.Low(BrownL.Next(NL));
				const float RR = RumbleR.Low(BrownR.Next(NR));
				const float Wh = Whistle.Band(0.5f * (NL + NR)) * Gw;
				L[i] += Gb * BL + Gr * RL + Wh * (0.5f - 0.5f * Pw);
				R[i] += Gb * BR + Gr * RR + Wh * (0.5f + 0.5f * Pw);
				if (bThunder)
				{
					const float Th = ThunderLow2.Low(ThunderLow1.Low(ThunderBrown.Next(Rng.Bipolar()))) * Gt;
					L[i] += Th * ThunderPanL;
					R[i] += Th * ThunderPanR;
				}
			}
			PrevBody = BodyGain;
			PrevRumble = RumbleGain;
			PrevWhistle = WhistleGain;
			PrevWhistlePan = WhistlePan;
			PrevThunder = ThunderGain;
			BodyL.Flush();
			BodyR.Flush();
			Whistle.Flush();
			RumbleL.Flush();
			RumbleR.Flush();
			ThunderLow1.Flush();
			ThunderLow2.Flush();
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Oleaje
	// ─────────────────────────────────────────────────────────────────────────

	/** Una ola: sube (ruido grave que crece), rompe (golpe de ruido cuyo corte cae) y deja espuma aguda que chisporrotea. */
	struct FSurfWave
	{
		float Time = 0.f;
		float Period = 7.f;
		float PeakAt = 3.5f;
		float Amp = 1.f;
		float Scale = 1.f;
		float PanL = 0.7f;
		float PanR = 0.7f;
		bool bBroken = false;
		float Swell = 0.f;
		float Crash = 0.f;
		float CrashTarget = 0.f;
		float Foam = 0.f;
		float FoamTarget = 0.f;
		float PrevSwell = 0.f;
		float PrevCrash = 0.f;
		float PrevFoam = 0.f;
		float Fizz = 0.f;
		FPink PinkL;
		FPink PinkR;
		FSvf SwellL;
		FSvf SwellR;
		FSvf CrashL;
		FSvf CrashR;
		FOnePole FoamL1;
		FOnePole FoamL2;
		FOnePole FoamR1;
		FOnePole FoamR2;
	};

	/**
	 * Oleaje: dos olas desfasadas (la principal rompe cada 5-9 s en mar abierto; en una laguna, chapoteos cortos de
	 * 1,6-3 s) y un fondo continuo de mar lejano. Cada ola tiene su posición en el estéreo y su propio ruido por canal.
	 */
	struct FSurf
	{
		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FSurfWave Waves[2];
		FPink BedL;
		FPink BedR;
		FOnePole BedLowL;
		FOnePole BedLowR;
		float PrevBed = 0.f;
		float FizzDecay = 0.99f;

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			BedLowL.SetHz(320.f, InvRate);
			BedLowR.SetHz(320.f, InvRate);
			FizzDecay = FMath::Exp(-InvRate / 0.0015f);
			for (int32 w = 0; w < 2; ++w)
			{
				FSurfWave& W = Waves[w];
				W.FoamL1.SetHz(2800.f, InvRate);
				W.FoamL2.SetHz(2800.f, InvRate);
				W.FoamR1.SetHz(2800.f, InvRate);
				W.FoamR2.SetHz(2800.f, InvRate);
				StartWave(W, 0.5f, w == 0 ? 1.f : 0.45f);
				// Arranque desfasado: la segunda ola va a mitad de su ciclo.
				W.Time = w == 0 ? Rng.Range(0.f, 0.3f) * W.Period : 0.5f * W.Period;
			}
		}

		void StartWave(FSurfWave& W, float Size, float InScale)
		{
			const float Calm = Rng.Range(1.6f, 3.2f);
			const float Open = Rng.Range(5.f, 9.f);
			W.Period = FMath::Lerp(Calm, Open, Size);
			W.PeakAt = W.Period * Rng.Range(0.42f, 0.6f);
			W.Amp = Rng.Range(0.55f, 1.f);
			W.Scale = InScale;
			W.Time = 0.f;
			W.bBroken = false;
			PanGains(Rng.Range(-0.65f, 0.65f), W.PanL, W.PanR);
		}

		void Render(float* L, float* R, int32 N, const FControl& C)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			const float Size = FMath::Clamp(C.SurfSize, 0.f, 1.f);
			const float InvN = 1.f / static_cast<float>(N);

			for (int32 w = 0; w < 2; ++w)
			{
				FSurfWave& W = Waves[w];
				W.Time += Dt;
				if (W.Time >= W.Period) { StartWave(W, Size, w == 0 ? 1.f : 0.45f); }
				// Subida hasta que rompe; luego la masa de agua se deshace.
				if (W.Time < W.PeakAt)
				{
					const float S = W.Time / W.PeakAt;
					W.Swell = W.Amp * S * S * (3.f - 2.f * S);
				}
				else
				{
					W.Swell -= W.Swell * TimeCoef(0.5f + 0.6f * Size, Dt);
				}
				if (!W.bBroken && W.Time >= W.PeakAt)
				{
					W.bBroken = true;
					W.CrashTarget = W.Amp;
					W.FoamTarget = W.Amp;
				}
				// El golpe entra en ~0,1 s (no de sopetón) y se apaga mientras su corte cae.
				W.Crash += (W.CrashTarget - W.Crash) * TimeCoef(FMath::Lerp(0.05f, 0.1f, Size), Dt);
				W.CrashTarget -= W.CrashTarget * TimeCoef(FMath::Lerp(0.16f, 0.55f, Size), Dt);
				W.Foam += (W.FoamTarget - W.Foam) * TimeCoef(0.15f, Dt);
				W.FoamTarget -= W.FoamTarget * TimeCoef(FMath::Lerp(0.6f, 2.2f, Size), Dt);
				if (W.Crash < 1e-5f && W.CrashTarget < 1e-5f) { W.Crash = W.CrashTarget = 0.f; }
				if (W.Foam < 1e-5f && W.FoamTarget < 1e-5f) { W.Foam = W.FoamTarget = 0.f; }

				const float Norm = 1.f / FMath::Max(0.1f, W.Amp);
				W.SwellL.Set(200.f + 700.f * W.Swell * Norm, 0.7f, InvRate);
				W.SwellR.CopyCoefs(W.SwellL);
				W.CrashL.Set(450.f + FMath::Lerp(1300.f, 4200.f, Size) * W.Crash * Norm, 0.75f, InvRate);
				W.CrashR.CopyCoefs(W.CrashL);

				const float SwellGain = W.Swell * W.Scale * FMath::Lerp(0.5f, 1.f, Size) * 1.6f;
				const float CrashGain = W.Crash * W.Scale * FMath::Lerp(0.35f, 1.f, Size) * 1.2f;
				const float FoamGain = W.Foam * W.Scale * FMath::Lerp(0.3f, 0.6f, Size) * 1.4f;
				const float FizzProb = FMath::Min(1.f, W.Foam) * 320.f * InvRate;
				float Gs = W.PrevSwell;
				float Gc = W.PrevCrash;
				float Gf = W.PrevFoam;
				const float DGs = (SwellGain - W.PrevSwell) * InvN;
				const float DGc = (CrashGain - W.PrevCrash) * InvN;
				const float DGf = (FoamGain - W.PrevFoam) * InvN;
				W.PrevSwell = SwellGain;
				W.PrevCrash = CrashGain;
				W.PrevFoam = FoamGain;
				if (SwellGain + CrashGain + FoamGain + Gs + Gc + Gf < 1e-6f) { continue; }

				for (int32 i = 0; i < N; ++i)
				{
					Gs += DGs;
					Gc += DGc;
					Gf += DGf;
					const float NL = Rng.Bipolar();
					const float NR = Rng.Bipolar();
					if (Rng.Unit() < FizzProb) { W.Fizz = Rng.Unit(); }
					W.Fizz *= FizzDecay;
					const float Sparkle = 0.45f + 1.6f * W.Fizz;
					const float WaveL = Gs * W.SwellL.Low(W.PinkL.Next(NL)) + Gc * W.CrashL.Low(NL) + Gf * Sparkle * W.FoamL2.High(W.FoamL1.High(NL));
					const float WaveR = Gs * W.SwellR.Low(W.PinkR.Next(NR)) + Gc * W.CrashR.Low(NR) + Gf * Sparkle * W.FoamR2.High(W.FoamR1.High(NR));
					L[i] += WaveL * W.PanL;
					R[i] += WaveR * W.PanR;
				}
				W.SwellL.Flush();
				W.SwellR.Flush();
				W.CrashL.Flush();
				W.CrashR.Flush();
			}

			// Fondo: mar lejano continuo, algo más presente con olas grandes.
			const float BedGain = FMath::Lerp(0.25f, 0.5f, Size);
			float Gb = PrevBed;
			const float DGb = (BedGain - PrevBed) * InvN;
			PrevBed = BedGain;
			for (int32 i = 0; i < N; ++i)
			{
				Gb += DGb;
				L[i] += Gb * BedLowL.Low(BedL.Next(Rng.Bipolar()));
				R[i] += Gb * BedLowR.Low(BedR.Next(Rng.Bipolar()));
			}
			BedLowL.Flush();
			BedLowR.Flush();
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Agua corriente (arroyo, río) y borboteo
	// ─────────────────────────────────────────────────────────────────────────

	/** Agua corriente: ruido de banda media-alta constante con leve modulación, un cuerpo grave y burbujas al azar. */
	struct FStream
	{
		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FSvf BandL;
		FSvf BandR;
		FOnePole BodyL;
		FOnePole BodyR;
		float Mod = 1.f;
		float ModTarget = 1.f;
		float ModTimer = 0.f;
		float PrevMod = 1.f;
		FBubbles Bubbles;
		float BubbleTimer = 0.f;

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			BandL.Set(2300.f, 0.9f, InvRate);
			BandR.CopyCoefs(BandL);
			BodyL.SetHz(500.f, InvRate);
			BodyR.SetHz(500.f, InvRate);
			Bubbles.Init(InRate);
		}

		void Render(float* L, float* R, int32 N)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			ModTimer -= Dt;
			if (ModTimer <= 0.f)
			{
				ModTarget = Rng.Range(0.8f, 1.15f);
				ModTimer = Rng.Range(0.3f, 1.2f);
			}
			Mod += (ModTarget - Mod) * TimeCoef(0.35f, Dt);

			// Borboteo: unas 28 burbujas por segundo, las pequeñas más agudas y cortas; mandan sobre el siseo.
			BubbleTimer -= Dt;
			for (int32 Guard = 0; BubbleTimer <= 0.f && Guard < 6; ++Guard)
			{
				const float Octaves = Rng.Unit() * 2.4f;
				const float Hz = 380.f * FMath::Pow(2.f, Octaves);
				const float Seconds = FMath::Lerp(0.07f, 0.02f, Octaves / 2.4f);
				Bubbles.Trigger(Hz, Rng.Range(1.3f, 2.f), Seconds, Rng.Range(0.1f, 0.4f), Rng.Range(-0.8f, 0.8f));
				BubbleTimer += Rng.Exponential(1.f / 28.f);
			}

			const float InvN = 1.f / static_cast<float>(N);
			float Gm = PrevMod;
			const float DGm = (Mod - PrevMod) * InvN;
			PrevMod = Mod;
			for (int32 i = 0; i < N; ++i)
			{
				Gm += DGm;
				const float NL = Rng.Bipolar();
				const float NR = Rng.Bipolar();
				L[i] += Gm * (0.6f * BandL.Band(NL) + 0.3f * BodyL.Low(NL));
				R[i] += Gm * (0.6f * BandR.Band(NR) + 0.3f * BodyR.Low(NR));
			}
			Bubbles.Render(L, R, N);
			BandL.Flush();
			BandR.Flush();
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Cantos: aves y ranas
	// ─────────────────────────────────────────────────────────────────────────

	/** Una sílaba de un canto: glissando con arco, timbre por modulación de fase, ruido, formante, vibrato y trémolo. */
	struct FSyllable
	{
		/** Segundos desde el inicio de la llamada y duración. */
		float Start = 0.f;
		float Dur = 0.1f;
		/** Frecuencia inicial y ln(final / inicial). */
		float F0 = 1000.f;
		float LogRatio = 0.f;
		/** Joroba del tono en el centro (fracción; + sube y baja, - al revés). */
		float Arc = 0.f;
		float Amp = 1.f;
		/** Moduladora / portadora e índice (en vueltas: 0,05 casi puro, 0,6 áspero). */
		float Ratio = 1.f;
		float Index = 0.f;
		/** Ruido mezclado (0..1): aspereza de graznidos. */
		float Noise = 0.f;
		float VibHz = 0.f;
		float VibDepth = 0.f;
		/** Trémolo: pulsos de amplitud (ranas). */
		float TremHz = 0.f;
		float TremDepth = 0.f;
		/** Fracción de la sílaba que dura el ataque y la caída. */
		float Attack = 0.12f;
		float Release = 0.35f;
	};

	/** Una voz que canta una llamada (secuencia de sílabas) con su paneo, distancia y formante. */
	struct FCallVoice
	{
		static constexpr int32 MaxSyllables = 20;

		FSyllable Syl[MaxSyllables];
		int32 NumSyl = 0;
		int32 Cur = 0;
		float Time = 0.f;
		float Phase = 0.f;
		float ModPhase = 0.f;
		float VibPhase = 0.f;
		float TremPhase = 0.f;
		float PanL = 0.7f;
		float PanR = 0.7f;
		float Gain = 1.f;
		float FormantComp = 1.f;
		float LastInc = 0.f;
		float LastAmp = 0.f;
		bool bFormant = false;
		bool bActive = false;
		FSvf Formant;
		FOnePole Air;
	};

	/** Añade una sílaba (si cabe) y la devuelve para retocarla. */
	inline FSyllable& AddSyl(FCallVoice& V, float InStart, float InDur, float InF0, float InF1, float InAmp)
	{
		const int32 Idx = FMath::Min(V.NumSyl, FCallVoice::MaxSyllables - 1);
		V.NumSyl = FMath::Min(V.NumSyl + 1, FCallVoice::MaxSyllables);
		FSyllable& S = V.Syl[Idx];
		S = FSyllable();
		S.Start = InStart;
		S.Dur = FMath::Max(0.01f, InDur);
		S.F0 = FMath::Max(20.f, InF0);
		S.LogRatio = FMath::Loge(FMath::Max(20.f, InF1) / S.F0);
		S.Amp = InAmp;
		return S;
	}

	inline void SetFormant(FCallVoice& V, float Hz, float Q, float InvRate)
	{
		V.bFormant = true;
		V.Formant.Set(Hz, Q, InvRate);
		V.Formant.Reset();
		V.FormantComp = 1.f + 0.45f * Q;
	}

	/**
	 * Rellena la voz con una llamada de la especie. Ajusta la distancia mínima de algunas (águila, gallina: siempre
	 * lejos) y devuelve su sonoridad relativa.
	 */
	inline float BuildCall(FCallVoice& V, int32 Species, FRandom& Rng, float& InOutDistance, float InvRate)
	{
		V.NumSyl = 0;
		V.bFormant = false;
		float T = 0.f;
		float Loud = 1.f;
		switch (Species)
		{
		case Bird::Whistle:
		{
			const float Base = Rng.Range(1700.f, 3000.f);
			const int32 Pattern = Rng.Int(0, 2);
			if (Pattern == 0)
			{
				// Serie que baja.
				const int32 Notes = Rng.Int(3, 5);
				float Hz = Base * 1.2f;
				for (int32 k = 0; k < Notes; ++k)
				{
					const float D = Rng.Range(0.1f, 0.2f);
					FSyllable& S = AddSyl(V, T, D, Hz, Hz * Rng.Range(0.88f, 0.97f), Rng.Range(0.7f, 1.f));
					S.Ratio = 2.f;
					S.Index = Rng.Range(0.01f, 0.04f);
					S.VibHz = Rng.Range(0.f, 25.f);
					S.VibDepth = 0.01f;
					T += D + Rng.Range(0.05f, 0.14f);
					Hz *= Rng.Range(0.86f, 0.95f);
				}
			}
			else if (Pattern == 1)
			{
				// Nota ligada que sube y, a veces, una corta que baja.
				const float D = Rng.Range(0.25f, 0.45f);
				FSyllable& S = AddSyl(V, 0.f, D, Base, Base * Rng.Range(1.35f, 1.7f), 1.f);
				S.Index = 0.02f;
				S.Attack = 0.2f;
				T = D + 0.06f;
				if (Rng.Chance(0.6f))
				{
					FSyllable& S2 = AddSyl(V, T, 0.12f, Base * 1.5f, Base * 1.1f, 0.8f);
					S2.Index = 0.02f;
				}
			}
			else
			{
				// Silbido de piropo: sube, pausa, y baja con joroba.
				FSyllable& S = AddSyl(V, 0.f, 0.2f, Base * 0.8f, Base * 1.5f, 0.9f);
				S.Arc = 0.08f;
				S.Index = 0.02f;
				FSyllable& S2 = AddSyl(V, 0.32f, 0.42f, Base * 0.95f, Base * 0.7f, 1.f);
				S2.Arc = 0.35f;
				S2.Index = 0.02f;
			}
			Loud = 0.75f;
			break;
		}
		case Bird::Trill:
		{
			const int32 Notes = Rng.Int(8, 18);
			const float Base = Rng.Range(4200.f, 6000.f);
			for (int32 k = 0; k < Notes; ++k)
			{
				const float D = Rng.Range(0.028f, 0.045f);
				const float Hz = Base * Rng.Range(0.97f, 1.03f);
				const float Shape = FastSin01(0.5f * static_cast<float>(k) / static_cast<float>(FMath::Max(1, Notes - 1)));
				FSyllable& S = AddSyl(V, T, D, Hz, Hz * 0.62f, 0.5f + 0.5f * Shape);
				S.Index = 0.05f;
				S.Attack = 0.08f;
				S.Release = 0.5f;
				T += D + Rng.Range(0.018f, 0.03f);
			}
			Loud = 0.55f;
			break;
		}
		case Bird::Macaw:
		{
			const int32 Squawks = Rng.Int(1, 3);
			for (int32 k = 0; k < Squawks; ++k)
			{
				const float D = Rng.Range(0.22f, 0.45f);
				const float Hz = Rng.Range(650.f, 1000.f);
				FSyllable& S = AddSyl(V, T, D, Hz, Hz * Rng.Range(0.75f, 0.9f), Rng.Range(0.8f, 1.f));
				S.Arc = 0.18f;
				S.Ratio = 1.49f;
				S.Index = Rng.Range(0.55f, 0.8f);
				S.Noise = 0.3f;
				S.VibHz = Rng.Range(28.f, 42.f);
				S.VibDepth = 0.03f;
				S.Attack = 0.06f;
				T += D + Rng.Range(0.12f, 0.35f);
			}
			SetFormant(V, Rng.Range(1600.f, 2100.f), 1.8f, InvRate);
			Loud = 1.f;
			break;
		}
		case Bird::Oropendola:
		{
			const int32 Phrases = Rng.Int(1, 2);
			for (int32 k = 0; k < Phrases; ++k)
			{
				FSyllable& A = AddSyl(V, T, 0.06f, 500.f, 1100.f, 0.6f);
				A.Index = 0.08f;
				FSyllable& B = AddSyl(V, T + 0.09f, 0.06f, 700.f, 1500.f, 0.7f);
				B.Index = 0.08f;
				FSyllable& Up = AddSyl(V, T + 0.19f, 0.22f, 900.f, Rng.Range(2900.f, 3500.f), 1.f);
				Up.Index = 0.15f;
				Up.Attack = 0.2f;
				Up.Release = 0.1f;
				FSyllable& Down = AddSyl(V, T + 0.41f, 0.14f, 2600.f, 700.f, 0.8f);
				Down.Index = 0.1f;
				Down.Attack = 0.05f;
				T += 0.55f + Rng.Range(0.5f, 0.9f);
			}
			Loud = 0.8f;
			break;
		}
		case Bird::Gull:
		{
			const int32 Calls = Rng.Int(2, 6);
			for (int32 k = 0; k < Calls; ++k)
			{
				const float Fade = 1.f - 0.06f * static_cast<float>(k);
				const float D = Rng.Range(0.22f, 0.38f) * Fade;
				const float Hz = Rng.Range(850.f, 1050.f) * (1.f - 0.04f * static_cast<float>(k));
				FSyllable& S = AddSyl(V, T, D, Hz, Hz * 0.72f, Rng.Range(0.75f, 1.f));
				S.Arc = 0.28f;
				S.Index = Rng.Range(0.3f, 0.45f);
				S.Noise = 0.08f;
				S.Attack = 0.08f;
				S.Release = 0.45f;
				T += D + Rng.Range(0.07f, 0.16f);
			}
			SetFormant(V, 1700.f, 1.4f, InvRate);
			Loud = 0.9f;
			break;
		}
		case Bird::Heron:
		{
			const int32 Croaks = Rng.Int(1, 2);
			for (int32 k = 0; k < Croaks; ++k)
			{
				const float D = Rng.Range(0.35f, 0.6f);
				const float Hz = Rng.Range(260.f, 360.f);
				FSyllable& S = AddSyl(V, T, D, Hz, Hz * 0.8f, 1.f);
				S.Arc = 0.1f;
				S.Index = Rng.Range(0.6f, 0.85f);
				S.Noise = 0.45f;
				S.VibHz = 48.f;
				S.VibDepth = 0.06f;
				S.Attack = 0.05f;
				T += D + 0.4f;
			}
			SetFormant(V, Rng.Range(800.f, 1000.f), 1.3f, InvRate);
			Loud = 0.9f;
			break;
		}
		case Bird::Piper:
		{
			const int32 Pips = Rng.Int(3, 7);
			const float Base = Rng.Range(2600.f, 3300.f);
			for (int32 k = 0; k < Pips; ++k)
			{
				const float D = Rng.Range(0.045f, 0.07f);
				FSyllable& S = AddSyl(V, T, D, Base, Base * 1.12f, Rng.Range(0.8f, 1.f));
				S.Index = 0.04f;
				S.Attack = 0.15f;
				T += D + Rng.Range(0.08f, 0.14f);
			}
			Loud = 0.55f;
			break;
		}
		case Bird::Crow:
		{
			const int32 Caws = Rng.Int(2, 4);
			for (int32 k = 0; k < Caws; ++k)
			{
				const float D = Rng.Range(0.25f, 0.38f);
				const float Hz = Rng.Range(520.f, 720.f);
				FSyllable& S = AddSyl(V, T, D, Hz, Hz * 0.86f, 1.f);
				S.Arc = 0.08f;
				S.Index = Rng.Range(0.45f, 0.65f);
				S.Noise = 0.3f;
				S.VibHz = 32.f;
				S.VibDepth = 0.04f;
				S.Attack = 0.06f;
				T += D + Rng.Range(0.25f, 0.5f);
			}
			SetFormant(V, Rng.Range(1100.f, 1400.f), 1.6f, InvRate);
			Loud = 0.95f;
			break;
		}
		case Bird::Eagle:
		{
			const int32 Screams = Rng.Int(1, 2);
			for (int32 k = 0; k < Screams; ++k)
			{
				const float D = Rng.Range(0.7f, 1.2f);
				const float Hz = Rng.Range(2700.f, 3300.f);
				FSyllable& S = AddSyl(V, T, D, Hz, Hz * 0.62f, 1.f);
				S.Arc = 0.12f;
				S.Index = 0.08f;
				S.Noise = 0.08f;
				S.VibHz = 11.f;
				S.VibDepth = 0.025f;
				S.Attack = 0.05f;
				S.Release = 0.6f;
				T += D + 0.6f;
			}
			InOutDistance = FMath::Max(InOutDistance, 0.75f);
			Loud = 0.7f;
			break;
		}
		case Bird::Dove:
		{
			const float Base = Rng.Range(430.f, 540.f);
			const float Durs[4] = { 0.24f, 0.42f, 0.28f, 0.3f };
			const float Pitch[4] = { 1.f, 1.06f, 0.97f, 0.95f };
			const int32 Coos = Rng.Int(3, 4);
			for (int32 k = 0; k < Coos; ++k)
			{
				const float Hz = Base * Pitch[k];
				FSyllable& S = AddSyl(V, T, Durs[k], Hz, Hz * 0.9f, k == 1 ? 1.f : 0.8f);
				S.Arc = 0.08f;
				S.Index = 0.03f;
				S.VibHz = 7.f;
				S.VibDepth = 0.012f;
				S.Attack = 0.3f;
				S.Release = 0.4f;
				T += Durs[k] + Rng.Range(0.06f, 0.1f);
			}
			Loud = 0.6f;
			break;
		}
		case Bird::Sparrow:
		{
			const int32 Chirps = Rng.Int(3, 8);
			for (int32 k = 0; k < Chirps; ++k)
			{
				const float D = Rng.Range(0.04f, 0.09f);
				const float Hz = Rng.Range(3600.f, 5000.f);
				FSyllable& S = AddSyl(V, T, D, Hz, Hz * Rng.Range(0.8f, 1.2f), Rng.Range(0.6f, 1.f));
				S.Arc = Rng.Range(0.1f, 0.3f);
				S.Index = 0.08f;
				S.Attack = 0.1f;
				T += D + Rng.Range(0.05f, 0.22f);
			}
			Loud = 0.5f;
			break;
		}
		case Bird::Hen:
		{
			const int32 Boks = Rng.Int(3, 5);
			const float Base = Rng.Range(360.f, 440.f);
			for (int32 k = 0; k < Boks; ++k)
			{
				const float D = Rng.Range(0.07f, 0.11f);
				FSyllable& S = AddSyl(V, T, D, Base, Base * 0.93f, Rng.Range(0.7f, 1.f));
				S.Ratio = 2.f;
				S.Index = 0.35f;
				S.Noise = 0.25f;
				S.Attack = 0.1f;
				T += D + Rng.Range(0.1f, 0.18f);
			}
			if (Rng.Chance(0.5f))
			{
				FSyllable& S = AddSyl(V, T + 0.05f, 0.42f, Base * 1.2f, Base * 1.55f, 1.f);
				S.Arc = 0.2f;
				S.Ratio = 2.f;
				S.Index = 0.45f;
				S.Noise = 0.25f;
			}
			SetFormant(V, 950.f, 2.f, InvRate);
			InOutDistance = FMath::Max(InOutDistance, 0.5f);
			Loud = 0.6f;
			break;
		}
		case Frog::Croak:
		{
			const int32 Repeats = Rng.Int(1, 3);
			const float Base = Rng.Range(110.f, 170.f);
			for (int32 r = 0; r < Repeats; ++r)
			{
				for (int32 k = 0; k < 2; ++k)
				{
					const float D = Rng.Range(0.07f, 0.1f);
					FSyllable& S = AddSyl(V, T, D, Base, Base * 0.92f, k == 0 ? 0.8f : 1.f);
					S.Index = Rng.Range(0.6f, 0.9f);
					S.Noise = 0.1f;
					S.TremHz = Rng.Range(28.f, 42.f);
					S.TremDepth = 0.85f;
					S.Attack = 0.1f;
					S.Release = 0.3f;
					T += D + Rng.Range(0.03f, 0.05f);
				}
				T += Rng.Range(0.25f, 0.4f);
			}
			SetFormant(V, Rng.Range(650.f, 1000.f), 4.f, InvRate);
			Loud = 0.8f;
			break;
		}
		case Frog::Peeper:
		{
			const int32 Peeps = Rng.Int(1, 3);
			const float Base = Rng.Range(2600.f, 3200.f);
			for (int32 k = 0; k < Peeps; ++k)
			{
				const float D = Rng.Range(0.08f, 0.12f);
				FSyllable& S = AddSyl(V, T, D, Base, Base * 1.15f, 1.f);
				S.Index = 0.03f;
				S.Attack = 0.2f;
				T += D + Rng.Range(0.25f, 0.45f);
			}
			Loud = 0.45f;
			break;
		}
		default:
		{
			// Rana toro: «rrum» muy grave y pulsado.
			const int32 Calls = Rng.Int(1, 2);
			for (int32 k = 0; k < Calls; ++k)
			{
				const float D = Rng.Range(0.4f, 0.7f);
				const float Hz = Rng.Range(55.f, 80.f);
				FSyllable& S = AddSyl(V, T, D, Hz, Hz * 0.95f, 1.f);
				S.Index = 0.8f;
				S.Noise = 0.05f;
				S.TremHz = Rng.Range(16.f, 22.f);
				S.TremDepth = 0.6f;
				S.Attack = 0.15f;
				T += D + 0.5f;
			}
			SetFormant(V, Rng.Range(260.f, 380.f), 3.f, InvRate);
			Loud = 1.f;
			break;
		}
		}
		return Loud;
	}

	inline float SylFreq(const FSyllable& S, float U)
	{
		return S.F0 * FMath::Exp(S.LogRatio * U) * (1.f + S.Arc * FastSin01(0.5f * U));
	}

	inline float SylEnv(const FSyllable& S, float U)
	{
		return S.Amp * SmoothStep01(U / FMath::Max(0.01f, S.Attack)) * SmoothStep01((1.f - U) / FMath::Max(0.01f, S.Release));
	}

	/**
	 * Capa de cantos: llamadas al azar (Poisson) de especies elegidas por pesos, con unas pocas voces fijas (la más
	 * vieja se reutiliza). A veces contesta otra de la misma especie desde el otro lado.
	 */
	struct FCallLayer
	{
		static constexpr int32 MaxVoices = 6;

		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FCallVoice Voices[MaxVoices];
		float Timer = 0.5f;
		int32 AnswerSpecies = -1;
		float AnswerPan = 0.f;

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			Timer = Rng.Range(0.3f, 1.5f);
		}

		void Schedule(float Dt, float CallsPerSecond, const float* Weights, int32 FirstSpecies, int32 NumSpecies, float Distance)
		{
			Timer -= Dt;
			if (Timer > 0.f) { return; }
			int32 Species = -1;
			float Pan = 0.f;
			if (AnswerSpecies >= 0)
			{
				Species = AnswerSpecies;
				Pan = AnswerPan;
				AnswerSpecies = -1;
			}
			else
			{
				const int32 Pick = Rng.Weighted(Weights, NumSpecies);
				Species = Pick >= 0 ? FirstSpecies + Pick : -1;
				Pan = Rng.Range(-0.85f, 0.85f);
				if (Species >= 0 && Rng.Chance(0.3f))
				{
					AnswerSpecies = Species;
					AnswerPan = FMath::Clamp(-Pan * Rng.Range(0.5f, 1.f) + Rng.Range(-0.2f, 0.2f), -0.9f, 0.9f);
				}
			}
			if (Species >= 0 && CallsPerSecond > 0.001f)
			{
				Trigger(Species, Pan, Distance);
			}
			Timer = AnswerSpecies >= 0 ? Rng.Range(0.35f, 1.1f)
				: FMath::Clamp(Rng.Exponential(1.f / FMath::Max(0.01f, CallsPerSecond)), 0.12f, 40.f);
		}

		void Trigger(int32 Species, float Pan, float Distance)
		{
			int32 Best = 0;
			float Oldest = -1.f;
			for (int32 k = 0; k < MaxVoices; ++k)
			{
				if (!Voices[k].bActive)
				{
					Best = k;
					break;
				}
				if (Voices[k].Time > Oldest)
				{
					Oldest = Voices[k].Time;
					Best = k;
				}
			}
			FCallVoice& V = Voices[Best];
			float Dist = FMath::Clamp(Distance + Rng.Range(-0.2f, 0.2f), 0.f, 1.f);
			const float Loud = BuildCall(V, Species, Rng, Dist, InvRate);
			V.Cur = 0;
			V.Time = 0.f;
			// Moduladora en fase con la portadora: con relación 1:1 la banda lateral n = -1 cae en 0 Hz y, con un desfase,
			// cada sílaba llevaría continua (golpes graves); en fase la onda es impar y no la tiene.
			V.Phase = Rng.Unit();
			V.ModPhase = V.Phase;
			V.VibPhase = 0.f;
			V.TremPhase = 0.f;
			V.LastAmp = 0.f;
			V.LastInc = 0.f;
			V.Gain = Loud * Rng.Range(0.6f, 1.f) * FMath::Lerp(1.f, 0.2f, Dist);
			V.Air.SetHz(FMath::Lerp(11000.f, 2000.f, Dist), InvRate);
			V.Air.Z = 0.f;
			PanGains(Pan, V.PanL, V.PanR);
			V.bActive = V.NumSyl > 0;
		}

		void Render(float* L, float* R, int32 N)
		{
			for (FCallVoice& V : Voices)
			{
				if (!V.bActive) { continue; }
				for (int32 s0 = 0; s0 < N; s0 += VoiceSubFrames)
				{
					const int32 M = FMath::Min(VoiceSubFrames, N - s0);
					const float SubDt = static_cast<float>(M) * InvRate;
					while (V.Cur < V.NumSyl && V.Time >= V.Syl[V.Cur].Start + V.Syl[V.Cur].Dur) { ++V.Cur; }
					if (V.Cur >= V.NumSyl)
					{
						V.bActive = false;
						break;
					}
					const FSyllable& S = V.Syl[V.Cur];
					const float T0 = V.Time;
					const float T1 = V.Time + SubDt;
					V.Time = T1;
					if (T1 <= S.Start)
					{
						// Hueco entre sílabas.
						V.LastAmp = 0.f;
						continue;
					}
					const float U1 = FMath::Clamp((T1 - S.Start) / S.Dur, 0.f, 1.f);
					V.VibPhase = FMath::Frac(V.VibPhase + S.VibHz * SubDt);
					V.TremPhase = FMath::Frac(V.TremPhase + S.TremHz * SubDt);
					const float Vib = 1.f + S.VibDepth * FastSin01(V.VibPhase);
					const float Trem = 1.f - S.TremDepth * (0.5f + 0.5f * FastSin01(V.TremPhase));
					const float Inc1 = FMath::Min(SylFreq(S, U1) * Vib * InvRate, 0.45f);
					const float Amp1 = SylEnv(S, U1) * Trem * V.Gain;
					const bool bFresh = T0 <= S.Start || V.LastAmp <= 0.f;
					float Inc = bFresh ? FMath::Min(SylFreq(S, 0.f) * InvRate, 0.45f) : V.LastInc;
					float Amp = V.LastAmp;
					const float InvM = 1.f / static_cast<float>(M);
					const float DInc = (Inc1 - Inc) * InvM;
					const float DAmp = (Amp1 - Amp) * InvM;
					for (int32 i = 0; i < M; ++i)
					{
						Inc += DInc;
						Amp += DAmp;
						const float Pm = V.Phase + S.Index * FastSin01(V.ModPhase);
						float X = FastSin01(Pm - FMath::FloorToFloat(Pm));
						if (S.Noise > 0.f) { X += S.Noise * (Rng.Bipolar() - X); }
						if (V.bFormant) { X = V.Formant.Band(X) * V.FormantComp; }
						X = V.Air.Low(X) * Amp;
						L[s0 + i] += X * V.PanL;
						R[s0 + i] += X * V.PanR;
						V.Phase += Inc;
						if (V.Phase >= 1.f) { V.Phase -= 1.f; }
						V.ModPhase += Inc * S.Ratio;
						if (V.ModPhase >= 1.f) { V.ModPhase -= FMath::FloorToFloat(V.ModPhase); }
					}
					V.LastInc = Inc1;
					V.LastAmp = Amp1;
				}
				V.Formant.Flush();
				V.Air.Flush();
			}
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Insectos
	// ─────────────────────────────────────────────────────────────────────────

	/** Un grupo de cigarras: banda de ruido aguda y estrecha pulsada muy rápido, que sube, se sostiene y calla en ciclos. */
	struct FCicadaGroup
	{
		FSvf Band;
		FSvf Band2;
		float Pulse = 0.f;
		float PulseInc = 0.f;
		float Trem = 0.f;
		float TremInc = 0.f;
		float TremDepth = 0.f;
		float Env = 0.f;
		float PrevEnv = 0.f;
		float Target = 0.f;
		float StageTime = 0.f;
		int32 Stage = 3;
		float PanL = 0.7f;
		float PanR = 0.7f;
	};

	/** Cigarras: tres grupos a su aire (selva y desierto). */
	struct FCicadas
	{
		static constexpr int32 NumGroups = 3;

		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FCicadaGroup Groups[NumGroups];

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			for (FCicadaGroup& G : Groups)
			{
				Retune(G);
				G.Stage = Rng.Int(0, 3);
				G.StageTime = Rng.Range(0.5f, 4.f);
				PanGains(Rng.Range(-0.8f, 0.8f), G.PanL, G.PanR);
			}
		}

		void Retune(FCicadaGroup& G)
		{
			// Dos etapas de banda en cascada: zumbido con cuerpo de tono y faldas empinadas (no un siseo ancho).
			G.Band.Set(Rng.Range(4200.f, 6500.f), 6.f, InvRate);
			G.Band2.CopyCoefs(G.Band);
			G.PulseInc = Rng.Range(140.f, 230.f) * InvRate;
			G.TremInc = Rng.Chance(0.6f) ? Rng.Range(5.f, 12.f) * InvRate : 0.f;
			G.TremDepth = G.TremInc > 0.f ? Rng.Range(0.3f, 0.6f) : 0.f;
			G.Target = Rng.Range(0.45f, 1.f);
		}

		void Render(float* L, float* R, int32 N)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			const float InvN = 1.f / static_cast<float>(N);
			for (FCicadaGroup& G : Groups)
			{
				G.StageTime -= Dt;
				if (G.StageTime <= 0.f)
				{
					G.Stage = (G.Stage + 1) % 4;
					if (G.Stage == 0) { Retune(G); }
					G.StageTime = G.Stage == 0 ? Rng.Range(1.5f, 3.f) : (G.Stage == 1 ? Rng.Range(2.f, 7.f) : (G.Stage == 2 ? Rng.Range(1.5f, 3.f) : Rng.Range(1.f, 6.f)));
				}
				const float Goal = G.Stage <= 1 ? G.Target : 0.f;
				G.Env += (Goal - G.Env) * TimeCoef(G.Stage == 0 ? 0.9f : 0.7f, Dt);
				if (G.Env < 1e-4f && G.PrevEnv < 1e-4f)
				{
					G.PrevEnv = G.Env;
					continue;
				}
				float Ge = G.PrevEnv;
				const float DGe = (G.Env - G.PrevEnv) * InvN;
				G.PrevEnv = G.Env;
				for (int32 i = 0; i < N; ++i)
				{
					Ge += DGe;
					const float Buzz = G.Band2.Band(G.Band.Band(Rng.Bipolar())) * 4.f;
					const float P = 0.5f + 0.5f * FastSin01(G.Pulse);
					const float P2 = P * P;
					const float Tr = 1.f - G.TremDepth * (0.5f + 0.5f * FastSin01(G.Trem));
					const float X = Buzz * (0.12f + 0.88f * P2 * P2) * Tr * Ge;
					L[i] += X * G.PanL;
					R[i] += X * G.PanR;
					G.Pulse += G.PulseInc;
					if (G.Pulse >= 1.f) { G.Pulse -= 1.f; }
					G.Trem += G.TremInc;
					if (G.Trem >= 1.f) { G.Trem -= 1.f; }
				}
				G.Band.Flush();
				G.Band2.Flush();
			}
		}
	};

	/** Un grillo: tono agudo en grupos de 3-5 pulsos que se repiten, con descansos de vez en cuando. */
	struct FCricket
	{
		float Phase = 0.f;
		float Inc = 0.f;
		float T = 0.f;
		float Period = 0.5f;
		float PulseT = 0.f;
		int32 PulseIdx = 0;
		int32 Pulses = 4;
		float PulseLen = 0.015f;
		float InvPulseLen = 66.f;
		float PulseSpan = 0.027f;
		float Amp = 1.f;
		float Rest = 0.f;
		float PanL = 0.7f;
		float PanR = 0.7f;
	};

	/** Grillos: cuatro, cada uno con su tono y su ritmo (manglar y noche). */
	struct FCrickets
	{
		static constexpr int32 NumCrickets = 4;

		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FCricket Crickets[NumCrickets];

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			for (FCricket& K : Crickets)
			{
				K.Inc = Rng.Range(4100.f, 4900.f) * InvRate;
				K.Period = Rng.Range(0.32f, 0.8f);
				K.Pulses = Rng.Int(3, 5);
				K.PulseLen = Rng.Range(0.012f, 0.018f);
				K.InvPulseLen = 1.f / K.PulseLen;
				K.PulseSpan = K.PulseLen + Rng.Range(0.009f, 0.014f);
				K.Amp = Rng.Range(0.5f, 1.f);
				K.T = Rng.Range(0.f, K.Period);
				PanGains(Rng.Range(-0.8f, 0.8f), K.PanL, K.PanR);
			}
		}

		void Render(float* L, float* R, int32 N)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			for (FCricket& K : Crickets)
			{
				if (K.Rest > 0.f)
				{
					K.Rest -= Dt;
					continue;
				}
				for (int32 i = 0; i < N; ++i)
				{
					K.T += InvRate;
					if (K.T >= K.Period)
					{
						K.T -= K.Period;
						K.PulseT = 0.f;
						K.PulseIdx = 0;
						if (Rng.Chance(0.03f))
						{
							K.Rest = Rng.Range(0.8f, 3.f);
							break;
						}
					}
					K.PulseT += InvRate;
					if (K.PulseT >= K.PulseSpan)
					{
						K.PulseT -= K.PulseSpan;
						++K.PulseIdx;
					}
					K.Phase += K.Inc;
					if (K.Phase >= 1.f) { K.Phase -= 1.f; }
					if (K.PulseIdx < K.Pulses && K.PulseT < K.PulseLen)
					{
						const float Gate = FastSin01(0.5f * K.PulseT * K.InvPulseLen);
						const float X = FastSin01(K.Phase) * Gate * K.Amp;
						L[i] += X * K.PanL;
						R[i] += X * K.PanR;
					}
				}
			}
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Lava
	// ─────────────────────────────────────────────────────────────────────────

	/** Lava: rumor grave por oleadas, crepitar (clics al azar filtrados) y burbujas gordas que revientan. */
	struct FLava
	{
		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FBrown BrownL;
		FBrown BrownR;
		FOnePole LowL1;
		FOnePole LowL2;
		FOnePole LowR1;
		FOnePole LowR2;
		float Surge = 1.f;
		float SurgeTarget = 1.f;
		float SurgeTimer = 0.f;
		float PrevSurge = 1.f;
		float Click = 0.f;
		float ClickDecay = 0.99f;
		float ClickPanL = 0.7f;
		float ClickPanR = 0.7f;
		FOnePole ClickHigh;
		FBubbles Blorps;
		float BlorpTimer = 1.f;

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			LowL1.SetHz(70.f, InvRate);
			LowL2.SetHz(120.f, InvRate);
			LowR1.SetHz(70.f, InvRate);
			LowR2.SetHz(120.f, InvRate);
			ClickHigh.SetHz(1200.f, InvRate);
			Blorps.Init(InRate);
			BlorpTimer = Rng.Range(0.3f, 1.5f);
		}

		void Render(float* L, float* R, int32 N)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			SurgeTimer -= Dt;
			if (SurgeTimer <= 0.f)
			{
				SurgeTarget = Rng.Range(0.55f, 1.25f);
				SurgeTimer = Rng.Range(0.8f, 3.f);
			}
			Surge += (SurgeTarget - Surge) * TimeCoef(0.8f, Dt);
			const float CrackleProb = 22.f * Surge * InvRate;

			BlorpTimer -= Dt;
			for (int32 Guard = 0; BlorpTimer <= 0.f && Guard < 3; ++Guard)
			{
				Blorps.Trigger(Rng.Range(55.f, 150.f), Rng.Range(1.3f, 1.9f), Rng.Range(0.12f, 0.3f), Rng.Range(0.25f, 0.6f), Rng.Range(-0.7f, 0.7f));
				BlorpTimer += Rng.Exponential(1.f / 0.9f);
			}

			const float InvN = 1.f / static_cast<float>(N);
			float Gs = PrevSurge;
			const float DGs = (Surge - PrevSurge) * InvN;
			PrevSurge = Surge;
			for (int32 i = 0; i < N; ++i)
			{
				Gs += DGs;
				const float NL = Rng.Bipolar();
				const float NR = Rng.Bipolar();
				const float RumL = LowL2.Low(LowL1.Low(BrownL.Next(NL))) * 1.5f * Gs;
				const float RumR = LowR2.Low(LowR1.Low(BrownR.Next(NR))) * 1.5f * Gs;
				if (Rng.Unit() < CrackleProb)
				{
					const float Size = Rng.Unit();
					Click = 0.04f + Size * Size * Size;
					ClickDecay = FMath::Exp(-InvRate / Rng.Range(0.0012f, 0.006f));
					PanGains(Rng.Range(-0.8f, 0.8f), ClickPanL, ClickPanR);
				}
				const float Crack = ClickHigh.High(NL * Click) * 1.5f;
				Click *= ClickDecay;
				L[i] += RumL + Crack * ClickPanL;
				R[i] += RumR + Crack * ClickPanR;
			}
			Blorps.Render(L, R, N);
			LowL1.Flush();
			LowL2.Flush();
			LowR1.Flush();
			LowR2.Flush();
			ClickHigh.Flush();
			if (Click < 1e-6f) { Click = 0.f; }
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Campanas (zona humana)
	// ─────────────────────────────────────────────────────────────────────────

	/** Campanillas de viento (barras pentatónicas en racimos) y, de vez en cuando, una campana lejana que tañe 1-3 veces. */
	struct FBells
	{
		static constexpr int32 MaxModes = 32;

		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FModal Modes[MaxModes];
		int32 NextMode = 0;
		float ChimeTimer = 3.f;
		int32 ChimeLeft = 0;
		float ChimeGap = 0.f;
		float ChimePan = 0.f;
		float BellTimer = 20.f;
		int32 BellLeft = 0;
		float BellGap = 0.f;
		float BellPan = 0.f;
		float BellHz = 300.f;

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			ChimeTimer = Rng.Range(1.f, 5.f);
			BellTimer = Rng.Range(8.f, 25.f);
		}

		void StrikeMode(float Hz, float T60, float Amp, float Pan)
		{
			float GainL = 0.7f;
			float GainR = 0.7f;
			PanGains(Pan, GainL, GainR);
			Modes[NextMode].Strike(Hz, T60, Amp, GainL, GainR, InvRate);
			NextMode = (NextMode + 1) % MaxModes;
		}

		void StrikeChime()
		{
			static const float Scale[5] = { 1.f, 1.125f, 1.25f, 1.5f, 1.6667f };
			const float Hz = 1320.f * Scale[Rng.Int(0, 4)] * (Rng.Chance(0.3f) ? 2.f : 1.f);
			const float Amp = Rng.Range(0.25f, 0.7f);
			const float Pan = ChimePan + Rng.Range(-0.15f, 0.15f);
			// Barra libre-libre: parciales inarmónicos 1 : 2,76 : 5,40.
			StrikeMode(Hz, 2.2f, Amp, Pan);
			StrikeMode(Hz * 2.756f, 1.1f, Amp * 0.35f, Pan);
			StrikeMode(Hz * 5.404f, 0.5f, Amp * 0.12f, Pan);
		}

		void TollBell()
		{
			// Campana de tercera menor: hum, prima, tercera, quinta, nominal y parciales altos; lejana (se apagan los agudos).
			static const float Ratios[8] = { 0.5f, 1.f, 1.2f, 1.5f, 2.f, 2.5f, 2.67f, 3.f };
			static const float Amps[8] = { 0.45f, 0.7f, 0.55f, 0.25f, 0.6f, 0.22f, 0.12f, 0.15f };
			static const float Decays[8] = { 7.f, 4.5f, 3.5f, 2.5f, 3.f, 1.8f, 1.4f, 1.2f };
			for (int32 k = 0; k < 8; ++k)
			{
				const float Hz = BellHz * Ratios[k];
				const float Tilt = 1.f / FMath::Sqrt(1.f + (Hz / 1800.f) * (Hz / 1800.f));
				StrikeMode(Hz, Decays[k], Amps[k] * Tilt * 0.8f, BellPan);
			}
		}

		void Render(float* L, float* R, int32 N)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			ChimeTimer -= Dt;
			if (ChimeLeft == 0 && ChimeTimer <= 0.f)
			{
				ChimeLeft = Rng.Int(1, 4);
				ChimeGap = 0.f;
				ChimePan = Rng.Range(-0.5f, 0.5f);
				ChimeTimer = Rng.Range(2.5f, 9.f);
			}
			if (ChimeLeft > 0)
			{
				ChimeGap -= Dt;
				if (ChimeGap <= 0.f)
				{
					StrikeChime();
					--ChimeLeft;
					ChimeGap = Rng.Range(0.08f, 0.45f);
				}
			}
			BellTimer -= Dt;
			if (BellLeft == 0 && BellTimer <= 0.f)
			{
				BellLeft = Rng.Int(1, 3);
				BellGap = 0.f;
				BellPan = Rng.Range(-0.6f, 0.6f);
				BellHz = Rng.Range(260.f, 340.f);
				BellTimer = Rng.Range(25.f, 60.f);
			}
			if (BellLeft > 0)
			{
				BellGap -= Dt;
				if (BellGap <= 0.f)
				{
					TollBell();
					--BellLeft;
					BellGap = Rng.Range(2.2f, 3.f);
				}
			}
			for (FModal& Mode : Modes)
			{
				if (!Mode.bActive) { continue; }
				for (int32 i = 0; i < N; ++i)
				{
					const float Y = Mode.Tick();
					L[i] += Y * Mode.GainL;
					R[i] += Y * Mode.GainR;
				}
				Mode.CheckDone();
			}
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Fuentes puntuales 3D (mono)
	// ─────────────────────────────────────────────────────────────────────────

	/** Cascada: fragor de ruido rosa, siseo de salpicadura, retumbo grave y algo de burbujeo en la poza. */
	struct FWaterfall
	{
		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FPink Pink;
		FSvf Roar;
		FSvf Hiss;
		FBrown Brown;
		FOnePole Low1;
		FOnePole Low2;
		float Mod = 1.f;
		float ModTarget = 1.f;
		float ModTimer = 0.f;
		float PrevMod = 1.f;
		float LastIntensity = -1.f;
		FBubbles Bubbles;
		float BubbleTimer = 0.f;

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			Hiss.Set(3500.f, 0.7f, InvRate);
			Low1.SetHz(100.f, InvRate);
			Low2.SetHz(160.f, InvRate);
			Bubbles.Init(InRate);
		}

		void Render(float* M, int32 N, float Intensity)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			const float I = FMath::Clamp(Intensity, 0.f, 1.5f);
			if (FMath::Abs(I - LastIntensity) > 0.01f)
			{
				LastIntensity = I;
				Roar.Set(900.f + 2200.f * I, 0.6f, InvRate);
			}
			ModTimer -= Dt;
			if (ModTimer <= 0.f)
			{
				ModTarget = Rng.Range(0.85f, 1.12f);
				ModTimer = Rng.Range(0.2f, 0.9f);
			}
			Mod += (ModTarget - Mod) * TimeCoef(0.25f, Dt);
			BubbleTimer -= Dt;
			for (int32 Guard = 0; BubbleTimer <= 0.f && Guard < 6; ++Guard)
			{
				Bubbles.Trigger(Rng.Range(300.f, 1600.f), Rng.Range(1.3f, 2.f), Rng.Range(0.02f, 0.06f), Rng.Range(0.04f, 0.12f), 0.f);
				BubbleTimer += Rng.Exponential(1.f / 30.f);
			}
			const float InvN = 1.f / static_cast<float>(N);
			float Gm = PrevMod;
			const float DGm = (Mod - PrevMod) * InvN;
			PrevMod = Mod;
			const float Body = 0.4f + 0.6f * I;
			for (int32 i = 0; i < N; ++i)
			{
				Gm += DGm;
				const float W = Rng.Bipolar();
				const float Rumble = Low2.Low(Low1.Low(Brown.Next(W))) * 2.f;
				M[i] += Gm * Body * (1.3f * Roar.Low(Pink.Next(W)) + 0.5f * Hiss.Band(W) + Rumble);
			}
			Bubbles.RenderMono(M, N);
			Roar.Flush();
			Hiss.Flush();
			Low1.Flush();
			Low2.Flush();
		}
	};

	/**
	 * Géiser: rugido que sube y baja con la intensidad (el pulso del chorro), siseo arriba con el chorro fuerte, retumbo
	 * grave y, cuando el chorro está bajo, borboteo de burbujas gordas.
	 */
	struct FGeyser
	{
		float InvRate = 1.f / 48000.f;
		FRandom Rng;
		FPink Pink;
		FSvf Roar;
		FOnePole HissA;
		FOnePole HissB;
		FBrown Brown;
		FOnePole Low1;
		FOnePole Low2;
		FBubbles Bubbles;
		float BubbleTimer = 0.f;
		float PrevRoar = 0.f;
		float PrevHiss = 0.f;
		float PrevLow = 0.f;

		void Init(float InRate, uint32 InSeed)
		{
			InvRate = 1.f / InRate;
			Rng.SetSeed(InSeed);
			HissA.SetHz(2500.f, InvRate);
			HissB.SetHz(2500.f, InvRate);
			Low1.SetHz(80.f, InvRate);
			Low2.SetHz(140.f, InvRate);
			Bubbles.Init(InRate);
		}

		void Render(float* M, int32 N, float Intensity)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			const float I = FMath::Clamp(Intensity, 0.f, 1.5f);
			// En reposo apenas un rumor y el borboteo; con el chorro, rugido brillante ~15 dB más fuerte.
			Roar.Set(280.f + 4200.f * FMath::Pow(I, 1.2f), 0.65f, InvRate);
			const float RoarGain = (0.04f + 1.f * I) * 1.4f;
			const float HissGain = 0.8f * I * I;
			const float LowGain = (0.12f + 0.88f * I) * 2.f;

			// Borboteo con el chorro bajo: burbujas gordas.
			const float Gurgle = FMath::Clamp(1.f - I * 2.5f, 0.f, 1.f);
			BubbleTimer -= Dt;
			for (int32 Guard = 0; BubbleTimer <= 0.f && Guard < 4; ++Guard)
			{
				if (Gurgle > 0.05f)
				{
					Bubbles.Trigger(Rng.Range(140.f, 650.f), Rng.Range(1.4f, 2.2f), Rng.Range(0.05f, 0.14f), Rng.Range(0.2f, 0.5f) * Gurgle, 0.f);
				}
				BubbleTimer += Rng.Exponential(1.f / 16.f);
			}

			const float InvN = 1.f / static_cast<float>(N);
			float Gr = PrevRoar;
			float Gh = PrevHiss;
			float Gl = PrevLow;
			const float DGr = (RoarGain - PrevRoar) * InvN;
			const float DGh = (HissGain - PrevHiss) * InvN;
			const float DGl = (LowGain - PrevLow) * InvN;
			PrevRoar = RoarGain;
			PrevHiss = HissGain;
			PrevLow = LowGain;
			for (int32 i = 0; i < N; ++i)
			{
				Gr += DGr;
				Gh += DGh;
				Gl += DGl;
				const float W = Rng.Bipolar();
				M[i] += Gr * Roar.Low(Pink.Next(W)) + Gh * HissB.High(HissA.High(W)) + Gl * Low2.Low(Low1.Low(Brown.Next(W)));
			}
			Bubbles.RenderMono(M, N);
			Roar.Flush();
			HissA.Flush();
			HissB.Flush();
			Low1.Flush();
			Low2.Flush();
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Salida y motor completo
	// ─────────────────────────────────────────────────────────────────────────

	/** Limitador suave: envolvente de pico (ataque 1,5 ms, caída 200 ms) y saturación suave para lo que se escape. */
	struct FLimiter
	{
		float Env = 0.f;
		float AttackK = 0.f;
		float ReleaseK = 0.f;
		float Threshold = 0.7f;

		void Init(float InRate)
		{
			AttackK = 1.f - FMath::Exp(-1.f / (0.0015f * InRate));
			ReleaseK = 1.f - FMath::Exp(-1.f / (0.2f * InRate));
		}

		void Process(float& InOutL, float& InOutR)
		{
			const float Peak = FMath::Max(FMath::Abs(InOutL), FMath::Abs(InOutR));
			Env += (Peak > Env ? AttackK : ReleaseK) * (Peak - Env);
			if (Env < 1e-9f) { Env = 0.f; }
			const float Gain = Env > Threshold ? Threshold / Env : 1.f;
			InOutL = SoftClip(InOutL * Gain);
			InOutR = SoftClip(InOutR * Gain);
		}
	};

	/**
	 * Ganancia de cada capa con su nivel a 1, calibrada en un arnés fuera del motor (sin limitador): las continuas
	 * quedan entre -19 y -25 dBFS RMS (viento -20, oleaje -19, agua -21, cigarras -24, grillos -25, lava -19) y los
	 * cantos y campanadas con picos de -3 a -8 dBFS.
	 */
	inline float LayerNorm(int32 LayerIdx)
	{
		static const float Norm[Layer::Count] = { 0.22f, 0.54f, 0.66f, 0.6f, 0.64f, 0.25f, 0.45f, 0.43f, 0.25f };
		return Norm[FMath::Clamp(LayerIdx, 0, Layer::Count - 1)];
	}

	/** Recorte de salida de las fuentes 3D: pegados a ellas, cascada y géiser rondan -17 dBFS RMS y la lava -19 (picos de -3). */
	inline float PointTrim(int32 InKind)
	{
		return InKind == Kind::LavaPool ? 0.63f : 0.21f;
	}

	/** Motor completo de una instancia: paisaje 2D por capas o una fuente 3D, suavizado de parámetros y salida limitada. */
	class FEngine
	{
	public:
		void Init(float InRate, int32 InKind, uint32 InSeed)
		{
			Rate = FMath::Max(8000.f, InRate);
			InvRate = 1.f / Rate;
			SourceKind = FMath::Clamp(InKind, 0, Kind::Count - 1);
			Wind.Init(Rate, MixSeed(InSeed, 1u));
			Surf.Init(Rate, MixSeed(InSeed, 2u));
			Stream.Init(Rate, MixSeed(InSeed, 3u));
			Birds.Init(Rate, MixSeed(InSeed, 4u));
			Frogs.Init(Rate, MixSeed(InSeed, 5u));
			Cicadas.Init(Rate, MixSeed(InSeed, 6u));
			Crickets.Init(Rate, MixSeed(InSeed, 7u));
			Lava.Init(Rate, MixSeed(InSeed, 8u));
			Bells.Init(Rate, MixSeed(InSeed, 9u));
			Waterfall.Init(Rate, MixSeed(InSeed, 10u));
			Geyser.Init(Rate, MixSeed(InSeed, 11u));
			Limiter.Init(Rate);
			for (float& Value : Level) { Value = 0.f; }
			Master = 0.f;
			PrevMaster = 0.f;
			Enclosure = 0.f;
			Intensity = 0.f;
			bFirstBlock = true;
		}

		/** Rellena NumFrames tramas intercaladas de OutChannels canales (1 = mono para 3D; 2 = estéreo; más, a cero). */
		void Render(float* Out, int32 NumFrames, int32 OutChannels, const FSharedParams& P)
		{
			const int32 Channels = FMath::Max(1, OutChannels);
			int32 Done = 0;
			while (Done < NumFrames)
			{
				const int32 N = FMath::Min(BlockFrames, NumFrames - Done);
				RenderBlock(N, P);
				float* Dst = Out + Done * Channels;
				for (int32 i = 0; i < N; ++i)
				{
					if (Channels == 1)
					{
						Dst[i] = 0.5f * (MixL[i] + MixR[i]);
					}
					else
					{
						Dst[i * Channels] = MixL[i];
						Dst[i * Channels + 1] = MixR[i];
						for (int32 c = 2; c < Channels; ++c) { Dst[i * Channels + c] = 0.f; }
					}
				}
				Done += N;
			}
		}

	private:
		void RenderBlock(int32 N, const FSharedParams& P)
		{
			const float Dt = static_cast<float>(N) * InvRate;
			const float Smooth = TimeCoef(FMath::Max(0.01f, FSharedParams::Get(P.SmoothSeconds)), Dt);
			// El primer bloque toma los parámetros de forma tal cual (los volúmenes sí entran en fundido).
			const float ShapeK = bFirstBlock ? 1.f : Smooth;
			bFirstBlock = false;
			for (int32 b = 0; b < Bird::Count; ++b)
			{
				Ctl.Birds[b] += (FSharedParams::Get(P.Birds[b]) - Ctl.Birds[b]) * ShapeK;
			}
			Ctl.BirdRate += (FSharedParams::Get(P.BirdRate) - Ctl.BirdRate) * ShapeK;
			Ctl.BirdDistance += (FSharedParams::Get(P.BirdDistance) - Ctl.BirdDistance) * ShapeK;
			Ctl.WindGust += (FSharedParams::Get(P.WindGust) - Ctl.WindGust) * ShapeK;
			Ctl.WindWhistle += (FSharedParams::Get(P.WindWhistle) - Ctl.WindWhistle) * ShapeK;
			Ctl.WindBright += (FSharedParams::Get(P.WindBright) - Ctl.WindBright) * ShapeK;
			Ctl.SurfSize += (FSharedParams::Get(P.SurfSize) - Ctl.SurfSize) * ShapeK;
			Ctl.Storm += (FSharedParams::Get(P.Storm) - Ctl.Storm) * ShapeK;
			Enclosure += (FMath::Clamp(FSharedParams::Get(P.Enclosure), 0.f, 1.f) - Enclosure) * Smooth;
			Intensity += (FSharedParams::Get(P.Intensity) - Intensity) * TimeCoef(0.05f, Dt);
			Master += (FMath::Max(0.f, FSharedParams::Get(P.Master)) - Master) * TimeCoef(0.3f, Dt);

			for (int32 i = 0; i < N; ++i)
			{
				MixL[i] = 0.f;
				MixR[i] = 0.f;
			}

			if (SourceKind == Kind::Soundscape)
			{
				const float InvN = 1.f / static_cast<float>(N);
				for (int32 LayerIdx = 0; LayerIdx < Layer::Count; ++LayerIdx)
				{
					const float Goal = FMath::Max(0.f, FSharedParams::Get(P.Layers[LayerIdx]));
					const float From = Level[LayerIdx];
					float To = From + (Goal - From) * Smooth;
					// Apagándose, por debajo de -60 dB se corta del todo: la capa deja de calcularse.
					if (To < 1e-3f && Goal < 1e-5f) { To = 0.f; }
					Level[LayerIdx] = To;
					if (From < 1e-5f && To < 1e-5f) { continue; }

					for (int32 i = 0; i < N; ++i)
					{
						TmpL[i] = 0.f;
						TmpR[i] = 0.f;
					}
					switch (LayerIdx)
					{
					case Layer::Wind:
						Wind.Render(TmpL, TmpR, N, Ctl);
						break;
					case Layer::Surf:
						Surf.Render(TmpL, TmpR, N, Ctl);
						break;
					case Layer::Stream:
						Stream.Render(TmpL, TmpR, N);
						break;
					case Layer::Birds:
						Birds.Schedule(Dt, Ctl.BirdRate, Ctl.Birds, 0, Bird::Count, Ctl.BirdDistance);
						Birds.Render(TmpL, TmpR, N);
						break;
					case Layer::Cicadas:
						Cicadas.Render(TmpL, TmpR, N);
						break;
					case Layer::Crickets:
						Crickets.Render(TmpL, TmpR, N);
						break;
					case Layer::Frogs:
					{
						static const float FrogWeights[Frog::Count] = { 0.5f, 0.3f, 0.2f };
						// Con la capa débil, menos llamadas (no solo más bajas).
						Frogs.Schedule(Dt, 1.6f * FMath::Min(1.f, 0.3f + To), FrogWeights, Frog::Croak, Frog::Count, 0.4f);
						Frogs.Render(TmpL, TmpR, N);
						break;
					}
					case Layer::Lava:
						Lava.Render(TmpL, TmpR, N);
						break;
					default:
						Bells.Render(TmpL, TmpR, N);
						break;
					}
					const float Norm = LayerNorm(LayerIdx);
					float G = From * Norm;
					const float DG = (To - From) * Norm * InvN;
					for (int32 i = 0; i < N; ++i)
					{
						G += DG;
						MixL[i] += TmpL[i] * G;
						MixR[i] += TmpR[i] * G;
					}
				}
			}
			else
			{
				for (int32 i = 0; i < N; ++i)
				{
					TmpL[i] = 0.f;
					TmpR[i] = 0.f;
				}
				if (SourceKind == Kind::Waterfall)
				{
					Waterfall.Render(TmpL, N, Intensity);
				}
				else if (SourceKind == Kind::Geyser)
				{
					Geyser.Render(TmpL, N, Intensity);
				}
				else
				{
					Lava.Render(TmpL, TmpR, N);
					for (int32 i = 0; i < N; ++i) { TmpL[i] = 0.5f * (TmpL[i] + TmpR[i]) * FMath::Clamp(Intensity, 0.f, 1.5f); }
				}
				const float Trim = PointTrim(SourceKind);
				for (int32 i = 0; i < N; ++i)
				{
					MixL[i] = TmpL[i] * Trim;
					MixR[i] = MixL[i];
				}
			}

			// Salida: cierre (cueva, bajo el agua) con paso bajo, volumen general y limitador suave.
			const bool bClosed = Enclosure > 0.01f;
			if (bClosed)
			{
				const float Hz = FMath::Lerp(16000.f, 600.f, FMath::Sqrt(Enclosure));
				EncL.SetHz(Hz, InvRate);
				EncR.K = EncL.K;
			}
			const float InvBlock = 1.f / static_cast<float>(N);
			float Gm = PrevMaster;
			const float DGm = (Master - PrevMaster) * InvBlock;
			PrevMaster = Master;
			for (int32 i = 0; i < N; ++i)
			{
				Gm += DGm;
				float OutL = MixL[i];
				float OutR = MixR[i];
				if (bClosed)
				{
					OutL = EncL.Low(OutL);
					OutR = EncR.Low(OutR);
				}
				else
				{
					EncL.Z = OutL;
					EncR.Z = OutR;
				}
				OutL *= Gm;
				OutR *= Gm;
				Limiter.Process(OutL, OutR);
				MixL[i] = OutL;
				MixR[i] = OutR;
			}
			EncL.Flush();
			EncR.Flush();
		}

		float Rate = 48000.f;
		float InvRate = 1.f / 48000.f;
		int32 SourceKind = Kind::Soundscape;
		float Level[Layer::Count] = {};
		float Master = 0.f;
		float PrevMaster = 0.f;
		float Enclosure = 0.f;
		float Intensity = 0.f;
		bool bFirstBlock = true;
		FControl Ctl;
		FWind Wind;
		FSurf Surf;
		FStream Stream;
		FCallLayer Birds;
		FCallLayer Frogs;
		FCicadas Cicadas;
		FCrickets Crickets;
		FLava Lava;
		FBells Bells;
		FWaterfall Waterfall;
		FGeyser Geyser;
		FOnePole EncL;
		FOnePole EncR;
		FLimiter Limiter;
		float MixL[BlockFrames] = {};
		float MixR[BlockFrames] = {};
		float TmpL[BlockFrames] = {};
		float TmpR[BlockFrames] = {};
	};
}
