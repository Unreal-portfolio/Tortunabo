#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "../HUD/TN_HUDArt.h"

/**
 * Premios de la pantalla del puesto al llegar al agua (UTN_RaceArrivalWidget), dibujados en código con el estilo de
 * pegatina del HUD Tortunavy (TN_HUDArt.h: funciones de distancia con antialias, borde crema troquelado y sombra): coronas
 * de oro, plata y bronce para el podio y, del cuarto al octavo, premios cada vez más humillantes: un cubo de playa del
 * revés (por corona), media concha rota, un flotador pinchado, un calcetín mojado lleno de arena y un alga de peluca. Cada
 * textura se dibuja una vez y queda en la caché de TNHUDArt (la pantalla de la carrera las dibuja de antemano mientras se
 * juega, una por fotograma).
 */
namespace TNRaceArrivalArt
{
	/** Premio de cada puesto (el valor es el puesto menos uno; del octavo en adelante, el alga). */
	enum class EPrize : uint8
	{
		GoldCrown,
		SilverCrown,
		BronzeCrown,
		Bucket,
		BrokenShell,
		Floatie,
		Sock,
		Seaweed,
		Count
	};

	inline EPrize PrizeForPlace(int32 Place)
	{
		return static_cast<EPrize>(FMath::Clamp(Place, 1, static_cast<int32>(EPrize::Count)) - 1);
	}

	/** Gira el punto (x, y) Degrees alrededor de (Cx, Cy): para medir distancias de una figura torcida sin torcerlas. */
	inline FVector2f Turn(float x, float y, float Cx, float Cy, float Degrees)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		const float C = FMath::Cos(A);
		const float S = FMath::Sin(A);
		const float Dx = x - Cx;
		const float Dy = y - Cy;
		return FVector2f(Cx + Dx * C - Dy * S, Cy + Dx * S + Dy * C);
	}

	/** Unión suave de dos distancias (radio K px): las piezas se funden sin esquina. */
	inline float SmoothUnion(float A, float B, float K)
	{
		const float H = FMath::Clamp(0.5f + 0.5f * (B - A) / K, 0.f, 1.f);
		return FMath::Lerp(B, A, H) - K * H * (1.f - H);
	}

	/** Medidas de los dibujos (px de la textura), fuera de las lambdas para que no haya que capturarlas. */
	namespace Dims
	{
		/** Media concha: inclinación y centro del giro. */
		constexpr float ShellTilt = -14.f;
		constexpr float ShellPivotX = 128.f;
		constexpr float ShellPivotY = 140.f;
		/** Flotador: centro y radios de su elipse. */
		constexpr float RingCx = 128.f;
		constexpr float RingCy = 136.f;
		constexpr float RingRx = 104.f;
		constexpr float RingRy = 78.f;
		/** Alga: tiras que cuelgan. */
		constexpr int32 NumStrands = 9;
	}

	/** Pseudoazar fijo en [0, 1) para colocar granos, motas y gotas (el mismo dibujo en cada ejecución). */
	inline float Hash01(int32 A, int32 B)
	{
		uint32 H = static_cast<uint32>(A) * 374761393u + static_cast<uint32>(B) * 668265263u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= H >> 16;
		return static_cast<float>(H & 0xFFFFu) / 65536.f;
	}

	/**
	 * Corona del podio: 0 de oro con cinco puntas y gemas (coral, turquesa y un aro de piedras en la banda), 1 de plata con
	 * tres puntas y una gema turquesa, 2 de bronce con tres puntas, una doblada, una abolladura, una grieta y los huecos de
	 * las gemas vacíos (la corona es pequeña; el orgullo, no). 256 × 208.
	 */
	inline UTexture2D* Crown(int32 Metal)
	{
		const int32 Kind = FMath::Clamp(Metal, 0, 2);
		static const TCHAR* Keys[] = { TEXT("RaceArrivalCrownGold"), TEXT("RaceArrivalCrownSilver"), TEXT("RaceArrivalCrownBronze") };
		return TNHUDArt::Cached(Keys[Kind], [Kind]
		{
			using namespace TNHUDArt;
			FPainter P(256, 208);
			struct FMetal { FLinearColor Light, Dark, Edge, Tip; };
			const FMetal Metals[] = {
				{ Hex(0xFFE27A), Hex(0xE8A92E), Hex(0x8A5A10), Hex(0xFFF1B8) },
				{ Hex(0xF7FAFD), Hex(0xA7B3C2), Hex(0x4E5A6B), Hex(0xFFFFFF) },
				{ Hex(0xF4B784), Hex(0xB4672F), Hex(0x6A3812), Hex(0xFFD8B5) } };
			const FMetal& M = Metals[Kind];
			const int32 NumPeaks = Kind == 0 ? 5 : 3;

			// Puntas de izquierda a derecha, la del centro la más alta (en la de bronce, la de la derecha doblada).
			TArray<FVector2f> Peaks;
			for (int32 i = 0; i < NumPeaks; ++i)
			{
				const float T = static_cast<float>(i) / static_cast<float>(NumPeaks - 1);
				const float Middle = 1.f - FMath::Abs(2.f * T - 1.f);
				FVector2f Peak(30.f + 196.f * T, 72.f - 44.f * Middle);
				if (Kind == 2 && i == NumPeaks - 1)
				{
					Peak += FVector2f(-14.f, 26.f);
				}
				Peaks.Add(Peak);
			}
			TArray<FVector2f> Shape;
			Shape.Add(FVector2f(38.f, 172.f));
			for (int32 i = 0; i < NumPeaks; ++i)
			{
				Shape.Add(Peaks[i]);
				if (i + 1 < NumPeaks)
				{
					Shape.Add(FVector2f(0.5f * (Peaks[i].X + Peaks[i + 1].X), 120.f));
				}
			}
			Shape.Add(FVector2f(218.f, 172.f));
			const float TipR = Kind == 0 ? 10.f : 9.f;
			auto Band = [](float x, float y) { return Box(x, y, 128.f, 164.f, 100.f, 20.f, 9.f); };
			auto Body = [&Shape, &Band](float x, float y) { return FMath::Min(Polygon(x, y, Shape), Band(x, y)); };
			auto Tips = [&Peaks, TipR](float x, float y)
			{
				float D = TNumericLimits<float>::Max();
				for (const FVector2f& Peak : Peaks)
				{
					D = FMath::Min(D, Circle(x, y, Peak.X, Peak.Y, TipR));
				}
				return D;
			};
			auto All = [&Body, &Tips](float x, float y) { return FMath::Min(Body(x, y), Tips(x, y)); };

			P.Sticker(All, 7.f, 6.f, FVector2f(3.f, 6.f));
			P.Fill([&All](float x, float y) { return All(x, y) - 3.f; }, M.Edge);
			P.Layer(Body, [&M](float, float y) { return Mix(M.Light, M.Dark, (y - 30.f) / 150.f); });
			// Banda: algo más oscura, con dos filos.
			P.Layer(Band, [&M](float, float y) { return Mix(Mix(M.Light, M.Dark, 0.35f), M.Dark, (y - 146.f) / 36.f); });
			P.Fill([&Band](float x, float y) { return FMath::Max(FMath::Abs(y - 147.f) - 1.4f, Band(x, y)); }, FLinearColor(M.Edge.R, M.Edge.G, M.Edge.B, 0.55f));
			P.Fill([&Band](float x, float y) { return FMath::Max(FMath::Abs(y - 179.f) - 1.4f, Band(x, y)); }, FLinearColor(M.Edge.R, M.Edge.G, M.Edge.B, 0.55f));
			P.Fill(Tips, M.Tip);
			P.Fill([&Peaks, TipR](float x, float y) { return Circle(x, y, Peaks[0].X - 3.f, Peaks[0].Y - 3.f, TipR * 0.35f); }, Hex(0xFFFFFF, 0.8f));

			if (Kind == 0)
			{
				// Oro: gema coral grande en el centro, dos turquesa a los lados y un aro de piedras en la banda.
				P.Fill([](float x, float y) { return Ellipse(x, y, 128.f, 108.f, 15.f, 19.f) - 2.5f; }, M.Edge);
				P.Layer([](float x, float y) { return Ellipse(x, y, 128.f, 108.f, 15.f, 19.f); },
					[](float, float y) { return Mix(Hex(0xFF9A80), CoralDeep, (y - 90.f) / 36.f); });
				for (const float Gx : { 80.f, 176.f })
				{
					P.Fill([Gx](float x, float y) { return Circle(x, y, Gx, 118.f, 10.f) - 2.f; }, M.Edge);
					P.Layer([Gx](float x, float y) { return Circle(x, y, Gx, 118.f, 10.f); }, [](float, float y) { return Mix(SeaLight, Sea, (y - 108.f) / 20.f); });
					P.Fill([Gx](float x, float y) { return Circle(x, y, Gx - 3.f, 115.f, 3.f); }, Hex(0xFFFFFF, 0.8f));
				}
				for (int32 k = 0; k < 5; ++k)
				{
					const float Bx = 60.f + 34.f * k;
					const FLinearColor Stone = (k % 2 == 0) ? CoralC : Sea;
					P.Fill([Bx](float x, float y) { return Circle(x, y, Bx, 164.f, 6.5f); }, Stone);
					P.Fill([Bx](float x, float y) { return Circle(x, y, Bx - 2.f, 162.f, 2.f); }, Hex(0xFFFFFF, 0.75f));
				}
				P.Fill([](float x, float y) { return Circle(x, y, 123.f, 101.f, 4.f); }, Hex(0xFFFFFF, 0.85f));
			}
			else if (Kind == 1)
			{
				// Plata: una gema turquesa y tres piedrecitas en la banda.
				P.Fill([](float x, float y) { return Circle(x, y, 128.f, 110.f, 13.f) - 2.5f; }, M.Edge);
				P.Layer([](float x, float y) { return Circle(x, y, 128.f, 110.f, 13.f); }, [](float, float y) { return Mix(SeaLight, Sea, (y - 97.f) / 26.f); });
				P.Fill([](float x, float y) { return Circle(x, y, 124.f, 105.f, 4.f); }, Hex(0xFFFFFF, 0.85f));
				for (int32 k = 0; k < 3; ++k)
				{
					const float Bx = 88.f + 40.f * k;
					P.Fill([Bx](float x, float y) { return Circle(x, y, Bx, 164.f, 6.f); }, Sea);
				}
			}
			else
			{
				// Bronce: los huecos de las gemas vacíos, una abolladura y una grieta.
				for (int32 k = 0; k < 3; ++k)
				{
					const float Bx = 88.f + 40.f * k;
					P.Fill([Bx](float x, float y) { return Circle(x, y, Bx, 164.f, 6.f); }, FLinearColor(M.Edge.R, M.Edge.G, M.Edge.B, 0.7f));
				}
				P.Fill([&Body](float x, float y) { return FMath::Max(Ellipse(x, y, 160.f, 132.f, 18.f, 11.f), Body(x, y) + 2.f); }, Hex(0x5A2E0E, 0.35f));
				const TArray<FVector2f> CrackLine = { { 96.f, 156.f }, { 104.f, 138.f }, { 97.f, 124.f }, { 108.f, 104.f }, { 102.f, 90.f } };
				for (int32 s = 0; s + 1 < CrackLine.Num(); ++s)
				{
					const FVector2f A = CrackLine[s];
					const FVector2f B = CrackLine[s + 1];
					P.Fill([A, B](float x, float y) { return Segment(x, y, A.X, A.Y, B.X, B.Y, 1.5f); }, M.Edge);
				}
			}
			// Brillo en diagonal a la izquierda (solo dentro de la corona).
			P.Fill([&Body](float x, float y) { return FMath::Max(Segment(x, y, 62.f, 150.f, 88.f, 78.f, 5.f), Body(x, y) + 3.f); }, Hex(0xFFFFFF, 0.38f));
			static const TCHAR* Names[] = { TEXT("TN_Race_ArrivalCrownGold"), TEXT("TN_Race_ArrivalCrownSilver"), TEXT("TN_Race_ArrivalCrownBronze") };
			return P.ToTexture(Names[Kind]);
		});
	}

	/** 4.º: cubo de playa del revés (como corona), turquesa con el borde amarillo, asa coral, estrella de mar y arena. 256 × 256. */
	inline UTexture2D* Bucket()
	{
		return TNHUDArt::Cached(TEXT("RaceArrivalBucket"), []
		{
			using namespace TNHUDArt;
			FPainter P(256, 256);
			const TArray<FVector2f> Pail = { { 72.f, 62.f }, { 184.f, 62.f }, { 208.f, 194.f }, { 48.f, 194.f } };
			auto Body = [&Pail](float x, float y) { return Polygon(x, y, Pail) - 6.f; };
			auto RimSdf = [](float x, float y) { return Box(x, y, 128.f, 204.f, 96.f, 15.f, 7.f); };
			auto Top = [](float x, float y) { return Ellipse(x, y, 128.f, 58.f, 60.f, 12.f); };
			auto Handle = [](float x, float y) { return FMath::Max(FMath::Abs(Ellipse(x, y, 128.f, 206.f, 100.f, 34.f)) - 4.f, 206.f - y); };
			auto Knobs = [](float x, float y) { return FMath::Min(Circle(x, y, 30.f, 206.f, 8.f), Circle(x, y, 226.f, 206.f, 8.f)); };
			auto All = [&](float x, float y) { return FMath::Min(FMath::Min(FMath::Min(Body(x, y), RimSdf(x, y)), FMath::Min(Top(x, y), Handle(x, y))), Knobs(x, y)); };

			P.Sticker(All, 7.f, 6.f, FVector2f(3.f, 6.f));
			P.Fill([&Handle](float x, float y) { return Handle(x, y) - 2.f; }, Hex(0x9A2A1C));
			P.Fill(Handle, CoralC);
			P.Fill([&Body](float x, float y) { return Body(x, y) - 2.5f; }, Hex(0x0E4F72));
			P.Layer(Body, [](float x, float y)
			{
				// Turquesa con volumen: más claro a la izquierda y arriba.
				const FLinearColor Base = Mix(SeaLight, Sea, (y - 60.f) / 140.f);
				return Mix(Base, Hex(0x137FA8), FMath::Clamp((x - 96.f) / 120.f, 0.f, 1.f) * 0.6f);
			});
			// Dos franjas claras de plástico.
			P.Fill([&Body](float x, float y) { return FMath::Max(FMath::Abs(y - 96.f) - 5.f, Body(x, y) + 3.f); }, Hex(0xFFFFFF, 0.28f));
			P.Fill([&Body](float x, float y) { return FMath::Max(FMath::Abs(y - 164.f) - 4.f, Body(x, y) + 3.f); }, Hex(0xFFFFFF, 0.2f));
			P.Fill([&Top](float x, float y) { return Top(x, y) - 2.f; }, Hex(0x0E4F72));
			P.Layer(Top, [](float, float y) { return Mix(Foam, SeaLight, (y - 46.f) / 24.f); });
			P.Fill([&RimSdf](float x, float y) { return RimSdf(x, y) - 2.5f; }, Hex(0x9A6A08));
			P.Layer(RimSdf, [](float, float y) { return Mix(Hex(0xFFE58A), Hex(0xF2B13A), (y - 189.f) / 30.f); });
			P.Fill([&RimSdf](float x, float y) { return FMath::Max(FMath::Abs(y - 197.f) - 1.2f, RimSdf(x, y) + 2.f); }, Hex(0xFFFFFF, 0.5f));
			P.Fill(Knobs, Hex(0xF2B13A));
			// Estrella de mar pegada.
			const TArray<FVector2f> Star = StarPoints(126.f, 130.f, 30.f, 0.44f, 5);
			P.Fill([&Star](float x, float y) { return Polygon(x, y, Star) - 2.5f; }, CoralDeep);
			P.Fill([&Star](float x, float y) { return Polygon(x, y, Star) + 0.5f; }, CoralLight);
			for (int32 k = 0; k < 5; ++k)
			{
				const float A = -HALF_PI + 2.f * PI * k / 5.f;
				const float Dx = 126.f + 14.f * FMath::Cos(A);
				const float Dy = 130.f + 14.f * FMath::Sin(A);
				P.Fill([Dx, Dy](float x, float y) { return Circle(x, y, Dx, Dy, 2.4f); }, Hex(0xFFF2D4, 0.9f));
			}
			// Arena pegada junto al borde y granos que caen.
			for (const FVector2f& Clump : { FVector2f(70.f, 186.f), FVector2f(170.f, 184.f), FVector2f(196.f, 176.f) })
			{
				P.Fill([Clump](float x, float y) { return Ellipse(x, y, Clump.X, Clump.Y, 13.f, 7.f); }, SandC);
			}
			for (int32 k = 0; k < 14; ++k)
			{
				const float Gx = 58.f + 140.f * Hash01(k, 11);
				const float Gy = 226.f + 26.f * Hash01(k, 29);
				const float Gr = 1.8f + 2.2f * Hash01(k, 47);
				P.Fill([Gx, Gy, Gr](float x, float y) { return Circle(x, y, Gx, Gy, Gr); }, (k % 3 == 0) ? WetSand : SandC);
			}
			return P.ToTexture(TEXT("TN_Race_ArrivalBucket"));
		});
	}

	/** 5.º: media concha rota, sosa y descolorida, con un mordisco, una grieta y el trocito que se le ha caído. 256 × 256. */
	inline UTexture2D* BrokenShell()
	{
		return TNHUDArt::Cached(TEXT("RaceArrivalBrokenShell"), []
		{
			using namespace TNHUDArt;
			FPainter P(256, 256);
			auto Local = [](float x, float y) { return Turn(x, y, Dims::ShellPivotX, Dims::ShellPivotY, Dims::ShellTilt); };
			auto Fan = [&Local](float x, float y)
			{
				const FVector2f L = Local(x, y);
				return FMath::Max(Circle(L.X, L.Y, 128.f, 120.f, 88.f), L.Y - 184.f);
			};
			auto Ears = [&Local](float x, float y)
			{
				const FVector2f L = Local(x, y);
				return Box(L.X, L.Y, 128.f, 192.f, 40.f, 18.f, 8.f);
			};
			// Corte en zigzag algo inclinado: se queda la mitad de la izquierda, con un mordisco arriba.
			auto Cut = [&Local](float x, float y)
			{
				const FVector2f L = Local(x, y);
				const float Teeth = 9.f * (2.f * FMath::Abs(FMath::Frac(L.Y / 24.f) - 0.5f) - 0.5f);
				return L.X - (140.f + 0.12f * (L.Y - 128.f) + Teeth);
			};
			auto Bite = [&Local](float x, float y)
			{
				const FVector2f L = Local(x, y);
				return -Circle(L.X, L.Y, 60.f, 66.f, 17.f);
			};
			auto Piece = [&](float x, float y) { return FMath::Max(FMath::Max(FMath::Min(Fan(x, y), Ears(x, y)), Cut(x, y)), Bite(x, y)); };
			const TArray<FVector2f> Chip = { { 186.f, 150.f }, { 216.f, 160.f }, { 200.f, 188.f } };
			auto ChipSdf = [&Chip](float x, float y) { return Polygon(x, y, Chip) - 3.f; };
			auto All = [&](float x, float y) { return FMath::Min(Piece(x, y), ChipSdf(x, y)); };

			const FLinearColor TopC = Hex(0xEADFCB);
			const FLinearColor BottomC = Hex(0xB9A487);
			P.Sticker(All, 7.f, 6.f, FVector2f(3.f, 6.f));
			P.Fill([&All](float x, float y) { return All(x, y) - 2.5f; }, Hex(0x6E5A40));
			P.Layer(Piece, [&TopC, &BottomC](float, float y) { return Mix(TopC, BottomC, (y - 40.f) / 170.f); });
			P.Layer(ChipSdf, [&TopC, &BottomC](float, float y) { return Mix(TopC, BottomC, (y - 150.f) / 40.f); });
			// Costillas desde la charnela (solo en lo que queda).
			for (int32 k = -4; k <= 4; ++k)
			{
				const float A = k * 0.3f;
				const float Ex = 128.f + FMath::Sin(A) * 100.f;
				const float Ey = 192.f - FMath::Cos(A) * 100.f;
				P.Fill([&, Ex, Ey](float x, float y)
				{
					const FVector2f L = Local(x, y);
					return FMath::Max(Segment(L.X, L.Y, 128.f, 192.f, Ex, Ey, 3.f), Piece(x, y) + 2.f);
				}, Hex(0x7E6A50, 0.45f));
			}
			// Grieta desde el corte y un brillo apagado.
			const TArray<FVector2f> CrackLine = { { 134.f, 74.f }, { 114.f, 94.f }, { 120.f, 114.f }, { 102.f, 138.f } };
			for (int32 s = 0; s + 1 < CrackLine.Num(); ++s)
			{
				const FVector2f A = CrackLine[s];
				const FVector2f B = CrackLine[s + 1];
				P.Fill([&, A, B](float x, float y)
				{
					const FVector2f L = Local(x, y);
					return FMath::Max(Segment(L.X, L.Y, A.X, A.Y, B.X, B.Y, 1.8f), Piece(x, y) + 1.f);
				}, Hex(0x5A4630));
			}
			P.Fill([&](float x, float y)
			{
				const FVector2f L = Local(x, y);
				return FMath::Max(Ellipse(L.X, L.Y, 92.f, 86.f, 14.f, 9.f), Piece(x, y) + 2.f);
			}, Hex(0xFFFFFF, 0.3f));
			return P.ToTexture(TEXT("TN_Race_ArrivalBrokenShell"));
		});
	}

	/**
	 * 6.º: flotador pinchado, a rayas coral y crema, arrugado y desinflado por la derecha, con un parche, la válvula y el
	 * aire que se escapa. 256 × 256.
	 */
	inline UTexture2D* Floatie()
	{
		return TNHUDArt::Cached(TEXT("RaceArrivalFloatie"), []
		{
			using namespace TNHUDArt;
			FPainter P(256, 256);
			using Dims::RingCx;
			using Dims::RingCy;
			using Dims::RingRx;
			using Dims::RingRy;
			// Radio normalizado y ángulo en la elipse del flotador (visto un poco desde arriba).
			auto Polar = [](float x, float y, float& OutN, float& OutA)
			{
				const float Nx = (x - RingCx) / RingRx;
				const float Ny = (y - RingCy) / RingRy;
				OutN = FMath::Sqrt(Nx * Nx + Ny * Ny);
				OutA = FMath::Atan2(Ny, Nx);
			};
			auto Ring = [&Polar](float x, float y)
			{
				float N = 0.f;
				float A = 0.f;
				Polar(x, y, N, A);
				// Desinflado y arrugado por la derecha; el agujero se agranda por ahí.
				const float Right = FMath::Max(0.f, FMath::Cos(A));
				const float Outer = 1.f - 0.16f * FMath::Pow(Right, 1.5f) + 0.035f * FMath::Sin(10.f * A) * Right;
				const float Inner = 0.46f + 0.1f * Right;
				return FMath::Max(N - Outer, Inner - N) * RingRy;
			};
			auto At = [](float N, float A) { return FVector2f(RingCx + RingRx * N * FMath::Cos(A), RingCy + RingRy * N * FMath::Sin(A)); };
			const FVector2f PatchAt = At(0.72f, 0.3f);
			auto Patch = [PatchAt](float x, float y)
			{
				const FVector2f L = Turn(x, y, PatchAt.X, PatchAt.Y, -30.f);
				return Box(L.X, L.Y, PatchAt.X, PatchAt.Y, 17.f, 10.f, 3.f);
			};
			auto All = [&Ring, &Patch](float x, float y) { return FMath::Min(Ring(x, y), Patch(x, y)); };

			P.Sticker(All, 7.f, 6.f, FVector2f(3.f, 6.f));
			P.Fill([&Ring](float x, float y) { return Ring(x, y) - 2.5f; }, Hex(0x8A2A1C));
			P.Layer(Ring, [&Polar](float x, float y)
			{
				float N = 0.f;
				float A = 0.f;
				Polar(x, y, N, A);
				// Ocho gajos alternos; luz arriba a la izquierda y sombra abajo a la derecha; lo desinflado, más apagado.
				const int32 Segment8 = FMath::FloorToInt((A + PI) / (PI / 4.f));
				FLinearColor C = (Segment8 % 2 == 0) ? CoralC : Cream;
				const float Nx = (x - RingCx) / RingRx;
				const float Ny = (y - RingCy) / RingRy;
				C = Mix(C, FLinearColor::White, 0.3f * FMath::Max(0.f, -0.6f * Nx - 0.8f * Ny));
				C = Mix(C, Hex(0x5A3A36), 0.28f * FMath::Max(0.f, 0.5f * Nx + 0.8f * Ny));
				return Mix(C, Hex(0xB8AAA2), 0.35f * FMath::Max(0.f, FMath::Cos(A)));
			});
			// Arrugas del lado desinflado.
			for (int32 k = 0; k < 5; ++k)
			{
				const float A = -0.55f + 0.27f * k;
				const FVector2f From = At(0.62f + 0.04f * (k % 2), A);
				const FVector2f To = At(0.8f, A + 0.05f);
				P.Fill([From, To, &Ring](float x, float y) { return FMath::Max(Segment(x, y, From.X, From.Y, To.X, To.Y, 1.6f), Ring(x, y) + 2.f); }, Hex(0x5A2A20, 0.4f));
			}
			// Parche con dos puntadas en cruz.
			P.Fill([&Patch](float x, float y) { return Patch(x, y) - 2.f; }, Hex(0x8A6A3A));
			P.Fill(Patch, Hex(0xF2E2B8));
			P.Fill([PatchAt](float x, float y)
			{
				return FMath::Min(Segment(x, y, PatchAt.X - 8.f, PatchAt.Y - 6.f, PatchAt.X + 8.f, PatchAt.Y + 6.f, 1.4f),
					Segment(x, y, PatchAt.X - 8.f, PatchAt.Y + 6.f, PatchAt.X + 8.f, PatchAt.Y - 6.f, 1.4f));
			}, Hex(0x8A6A3A));
			// Válvula arriba a la izquierda.
			const FVector2f Valve = At(0.76f, -2.3f);
			P.Fill([Valve](float x, float y) { return Box(x, y, Valve.X, Valve.Y, 9.f, 6.f, 2.f) - 2.f; }, Hex(0x8A2A1C));
			P.Fill([Valve](float x, float y) { return Box(x, y, Valve.X, Valve.Y, 9.f, 6.f, 2.f); }, Cream);
			// El pinchazo y el aire que se escapa («psss»).
			const FVector2f Hole = At(0.83f, -0.3f);
			P.Fill([Hole](float x, float y) { return Circle(x, y, Hole.X, Hole.Y, 4.f); }, Hex(0x3A1A14));
			for (int32 k = 0; k < 3; ++k)
			{
				const float R = 13.f + 9.f * k;
				P.Fill([Hole, R](float x, float y) { return Arc(x, y, Hole.X, Hole.Y, R, -0.7f, 0.5f, 2.f); }, Hex(0xBFEFFA, 0.9f - 0.2f * k));
			}
			return P.ToTexture(TEXT("TN_Race_ArrivalFloatie"));
		});
	}

	/**
	 * 7.º: calcetín mojado lleno de arena, gris de agua de mar, con sus rayas desteñidas, talón y punta más oscuros, un
	 * agujero, granos de arena, gotas que caen y el tufillo que sube. 256 × 256.
	 */
	inline UTexture2D* Sock()
	{
		return TNHUDArt::Cached(TEXT("RaceArrivalSock"), []
		{
			using namespace TNHUDArt;
			FPainter P(256, 256);
			// La caña empieza dentro del puño (su punta redonda no asoma por encima: el calcetín está abierto por arriba).
			auto Leg = [](float x, float y) { return Segment(x, y, 104.f, 90.f, 104.f, 150.f, 36.f); };
			auto Foot = [](float x, float y) { return Segment(x, y, 114.f, 170.f, 188.f, 184.f, 32.f); };
			auto Cuff = [](float x, float y) { return Box(x, y, 104.f, 68.f, 40.f, 14.f, 8.f); };
			auto Body = [&Leg, &Foot](float x, float y) { return SmoothUnion(Leg(x, y), Foot(x, y), 18.f); };
			auto All = [&Body, &Cuff](float x, float y) { return FMath::Min(Body(x, y), Cuff(x, y)); };

			P.Sticker(All, 7.f, 6.f, FVector2f(3.f, 6.f));
			P.Fill([&All](float x, float y) { return All(x, y) - 2.5f; }, Hex(0x4A4438));
			P.Layer(Body, [](float, float y) { return Mix(Hex(0xD6D0C2), Hex(0x9C9282), (y - 60.f) / 160.f); });
			// Rayas desteñidas en la caña.
			P.Fill([&Leg](float x, float y) { return FMath::Max(FMath::Abs(y - 100.f) - 6.f, Leg(x, y) + 2.f); }, Hex(0xE07A62, 0.7f));
			P.Fill([&Leg](float x, float y) { return FMath::Max(FMath::Abs(y - 124.f) - 6.f, Leg(x, y) + 2.f); }, Hex(0x4F8FB8, 0.65f));
			// Talón y punta, más gastados.
			P.Fill([&Body](float x, float y) { return FMath::Max(Circle(x, y, 94.f, 182.f, 24.f), Body(x, y) + 2.f); }, Hex(0x857B6C));
			P.Fill([&Body](float x, float y) { return FMath::Max(Circle(x, y, 200.f, 188.f, 24.f), Body(x, y) + 2.f); }, Hex(0x857B6C));
			P.Fill([](float x, float y) { return Ellipse(x, y, 208.f, 180.f, 7.f, 5.f); }, Ink);
			// Puño de canalé.
			P.Fill([&Cuff](float x, float y) { return Cuff(x, y) - 2.f; }, Hex(0x4A4438));
			P.Fill(Cuff, Hex(0xE8E2D4));
			P.Fill([&Cuff](float x, float y)
			{
				const float Rib = FMath::Abs(FMath::Frac((x - 64.f) / 9.f) - 0.5f) * 9.f - 1.2f;
				return FMath::Max(Rib, Cuff(x, y) + 3.f);
			}, Hex(0xB8B0A0, 0.8f));
			// Brillos de mojado.
			P.Fill([&Body](float x, float y) { return FMath::Max(Ellipse(x, y, 88.f, 138.f, 7.f, 16.f), Body(x, y) + 3.f); }, Hex(0xFFFFFF, 0.35f));
			P.Fill([&Body](float x, float y) { return FMath::Max(Ellipse(x, y, 150.f, 170.f, 14.f, 5.f), Body(x, y) + 3.f); }, Hex(0xFFFFFF, 0.3f));
			// Arena pegada por todas partes.
			for (int32 k = 0; k < 70; ++k)
			{
				const float Gx = 70.f + 150.f * Hash01(k, 3);
				const float Gy = 84.f + 132.f * Hash01(k, 17);
				if (Body(Gx, Gy) > -4.f)
				{
					continue;
				}
				const float Gr = 1.5f + 1.9f * Hash01(k, 31);
				P.Fill([Gx, Gy, Gr](float x, float y) { return Circle(x, y, Gx, Gy, Gr); }, (k % 3 == 0) ? WetSand : SandC);
			}
			// Gotas que caen.
			for (const FVector3f& DropAt : { FVector3f(206.f, 236.f, 6.f), FVector3f(166.f, 244.f, 4.5f), FVector3f(90.f, 232.f, 5.5f) })
			{
				P.Layer([DropAt](float x, float y) { return Drop(x, y, DropAt.X, DropAt.Y, DropAt.Z); },
					[DropAt](float, float y) { return Mix(Hex(0xBFEFFA), Sea, (y - (DropAt.Y - 2.f * DropAt.Z)) / (3.f * DropAt.Z)); });
			}
			// El tufillo: tres líneas onduladas verdosas que suben del puño.
			for (int32 k = 0; k < 3; ++k)
			{
				const float X0 = 80.f + 24.f * k;
				P.Fill([X0, k](float x, float y)
				{
					const float Wave = X0 + 5.f * FMath::Sin(y * 0.3f + k * 1.7f);
					return FMath::Max(FMath::Abs(x - Wave) - 1.8f, FMath::Abs(y - 30.f) - 20.f);
				}, Hex(0x9ACD5A, 0.85f));
			}
			return P.ToTexture(TEXT("TN_Race_ArrivalSock"));
		});
	}

	/**
	 * 8.º: alga de peluca, una mata de alga verde oscura con las tiras colgando por los lados, vesículas, una conchita
	 * enganchada y gotas que caen. 256 × 256.
	 */
	inline UTexture2D* Seaweed()
	{
		return TNHUDArt::Cached(TEXT("RaceArrivalSeaweed"), []
		{
			using namespace TNHUDArt;
			FPainter P(256, 256);
			using Dims::NumStrands;
			auto Cap = [](float x, float y) { return FMath::Max(Ellipse(x, y, 128.f, 94.f, 86.f, 58.f), y - 120.f); };
			// Centro y grosor de cada tira a la altura y; se abren en abanico y ondulan.
			auto StrandEnd = [](int32 i) { return 190.f + 40.f * Hash01(i, 5); };
			auto StrandX = [](int32 i, float y) { return 50.f + 19.5f * i + 7.f * FMath::Sin(y * 0.07f + i * 1.3f) + (i - 4) * 0.06f * (y - 100.f); };
			auto Strand = [&StrandEnd, &StrandX](int32 i, float x, float y)
			{
				const float End = StrandEnd(i);
				const float Yc = FMath::Clamp(y, 92.f, End);
				const float T = (Yc - 92.f) / (End - 92.f);
				const float W = 11.f - 5.f * T;
				const float Dx = x - StrandX(i, Yc);
				const float Dy = y - Yc;
				return FMath::Sqrt(Dx * Dx + Dy * Dy) - W;
			};
			auto Strands = [&Strand](float x, float y)
			{
				float D = TNumericLimits<float>::Max();
				for (int32 i = 0; i < NumStrands; ++i)
				{
					D = FMath::Min(D, Strand(i, x, y));
				}
				return D;
			};
			auto All = [&Cap, &Strands](float x, float y) { return FMath::Min(Cap(x, y), Strands(x, y)); };

			P.Sticker(All, 7.f, 6.f, FVector2f(3.f, 6.f));
			P.Fill([&All](float x, float y) { return All(x, y) - 2.5f; }, Hex(0x12301C));
			for (int32 i = 0; i < NumStrands; ++i)
			{
				const FLinearColor Light = (i % 2) ? Hex(0x5DAA55) : Hex(0x3E8A45);
				P.Layer([&Strand, i](float x, float y) { return Strand(i, x, y); }, [Light](float, float y) { return Mix(Light, Hex(0x1F5230), (y - 100.f) / 130.f); });
				// Una vena más clara por el centro de la tira.
				P.Fill([&Strand, &StrandX, i](float x, float y) { return FMath::Max(FMath::Abs(x - StrandX(i, y)) - 1.2f, Strand(i, x, y) + 3.f); }, Hex(0x8FD07A, 0.5f));
			}
			P.Layer(Cap, [](float x, float y)
			{
				const FLinearColor C = Mix(Hex(0x4C9A4C), Hex(0x245C33), (y - 40.f) / 80.f);
				return Mix(C, Hex(0x1C4A28), 0.25f * FMath::Abs(FMath::Sin(x * 0.11f + y * 0.05f)));
			});
			// Vesículas (bolitas de aire del alga).
			for (int32 k = 0; k < 7; ++k)
			{
				const int32 i = (k * 3 + 1) % NumStrands;
				const float By = 140.f + 60.f * Hash01(k, 9);
				const float Bx = StrandX(i, By);
				P.Fill([Bx, By](float x, float y) { return Ellipse(x, y, Bx, By, 6.f, 8.f) - 1.5f; }, Hex(0x2E5A20));
				P.Fill([Bx, By](float x, float y) { return Ellipse(x, y, Bx, By, 6.f, 8.f); }, Hex(0x9CC255));
			}
			// Conchita enganchada en lo alto y un brillo de mojado.
			P.Fill([](float x, float y) { return Circle(x, y, 162.f, 70.f, 12.f) - 2.f; }, Hex(0xC8412E, 0.8f));
			P.Fill([](float x, float y) { return Circle(x, y, 162.f, 70.f, 12.f); }, Hex(0xFFD4BA));
			for (int32 k = -1; k <= 1; ++k)
			{
				const float Ex = 162.f + 9.f * k;
				P.Fill([Ex](float x, float y) { return Segment(x, y, 162.f, 80.f, Ex, 60.f, 1.2f); }, Hex(0xC8412E, 0.6f));
			}
			P.Fill([&Cap](float x, float y) { return FMath::Max(Ellipse(x, y, 96.f, 64.f, 22.f, 8.f), Cap(x, y) + 3.f); }, Hex(0xFFFFFF, 0.3f));
			for (const FVector3f& DropAt : { FVector3f(70.f, 240.f, 5.f), FVector3f(150.f, 248.f, 4.f), FVector3f(198.f, 236.f, 5.f) })
			{
				P.Layer([DropAt](float x, float y) { return Drop(x, y, DropAt.X, DropAt.Y, DropAt.Z); },
					[DropAt](float, float y) { return Mix(Hex(0xBFEFFA), Sea, (y - (DropAt.Y - 2.f * DropAt.Z)) / (3.f * DropAt.Z)); });
			}
			return P.ToTexture(TEXT("TN_Race_ArrivalSeaweed"));
		});
	}

	/** La textura del premio. */
	inline UTexture2D* PrizeTexture(EPrize Prize)
	{
		switch (Prize)
		{
		case EPrize::GoldCrown: return Crown(0);
		case EPrize::SilverCrown: return Crown(1);
		case EPrize::BronzeCrown: return Crown(2);
		case EPrize::Bucket: return Bucket();
		case EPrize::BrokenShell: return BrokenShell();
		case EPrize::Floatie: return Floatie();
		case EPrize::Sock: return Sock();
		default: return Seaweed();
		}
	}

	/**
	 * Tamaño (px a 1080 p) con el que se enseña cada premio: la corona de oro enorme, la de plata más pequeña, la de bronce
	 * enana; los demás, normales (lo humillante es el premio, no el tamaño).
	 */
	inline FVector2D PrizeSize(EPrize Prize)
	{
		switch (Prize)
		{
		case EPrize::GoldCrown: return FVector2D(520.0, 422.0);
		case EPrize::SilverCrown: return FVector2D(330.0, 268.0);
		case EPrize::BronzeCrown: return FVector2D(168.0, 136.0);
		default: return FVector2D(320.0, 320.0);
		}
	}
}
