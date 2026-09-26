#pragma once

#include "CoreMinimal.h"
#include "TN_ProcMapMeshKit.h"

/**
 * Meta de la playa final (TN_ProcMapGenerator_Build.cpp): un neumático gigante en arco sobre la línea,
 * al estilo del puente Dunlop de los circuitos, con el nombre del juego en los flancos, pasarela a
 * cuadros con el cartel de META y un rótulo colgante; banderas a cuadros en la cima, boyas que marcan
 * la línea en toda la boca de la playa, banderines entre el arco y la arena y banderolas a lo largo de
 * la playa. Los rótulos son de bloques (tipografía de 5x7) con contorno oscuro, legibles por las dos
 * caras. Marco local: X hacia el mar (sentido del camino), Y a la izquierda, Z arriba; origen en el
 * centro de la línea a la cota del mar.
 */
namespace TNFinishMesh
{
	using namespace TNProcMesh;
	using TNProcMap::LerpD;

	struct FTNFinishParams
	{
		/** Radio interior del arco (media luz). */
		double Radius = 1250.0;
		/** Semiancho de la boca de la playa en la línea (hasta los brazos). */
		double MouthHalf = 4000.0;
		/** Cota del fondo en la línea. */
		double FloorZ = -60.0;
		uint32 Seed = 0;
	};

	namespace FinishColors
	{
		const FLinearColor Rubber(0.035f, 0.035f, 0.04f);
		const FLinearColor Tread(0.06f, 0.06f, 0.065f);
		const FLinearColor Brand(1.0f, 0.78f, 0.05f);
		const FLinearColor Ink(0.02f, 0.02f, 0.025f);
		const FLinearColor White(0.92f, 0.92f, 0.9f);
		const FLinearColor Red(0.78f, 0.07f, 0.05f);
		const FLinearColor Rope(0.85f, 0.8f, 0.62f);
		const FLinearColor Pole(0.2f, 0.2f, 0.22f);
	}

	/** Caja con ejes cualesquiera (ortonormales) y semilados Half. */
	inline void TNFinishBox(FTNProcMeshBuffers& M, const FVector& C, const FVector& Ax, const FVector& Ay, const FVector& Az, const FVector& Half, const FLinearColor& Color)
	{
		auto P = [&](double Sx, double Sy, double Sz) { return C + Ax * (Sx * Half.X) + Ay * (Sy * Half.Y) + Az * (Sz * Half.Z); };
		M.AddQuad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Az, Color);
		M.AddQuad(P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), -Az, Color);
		M.AddQuad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), Ax, Color);
		M.AddQuad(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), -Ax, Color);
		M.AddQuad(P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), Ay, Color);
		M.AddQuad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), -Ay, Color);
	}

	/** Quad visible por las dos caras (telas, banderines, carteles finos). */
	inline void TNFinishCloth(FTNProcMeshBuffers& M, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal, const FLinearColor& Color)
	{
		M.AddQuad(A, B, C, D, Normal, Color);
		M.AddQuad(A, B, C, D, -Normal, Color * 0.85f);
	}

	/**
	 * Tipografía de bloques de 5x7 (fila de arriba primero, bit 4 = columna izquierda) para los rótulos.
	 * '^' es la exclamación de apertura (¡). Ancho: 5 las letras, 1 las exclamaciones, 3 el espacio.
	 */
	inline bool TNFinishGlyph(char Ch, uint8 OutRows[7], int32& OutWidth)
	{
		struct FGlyph { char C; int32 W; uint8 R[7]; };
		static const FGlyph Glyphs[] = {
			{ 'A', 5, { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
			{ 'B', 5, { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
			{ 'E', 5, { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } },
			{ 'G', 5, { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } },
			{ 'L', 5, { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
			{ 'M', 5, { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } },
			{ 'N', 5, { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } },
			{ 'O', 5, { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
			{ 'R', 5, { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
			{ 'T', 5, { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
			{ 'U', 5, { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
			{ '!', 1, { 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01 } },
			{ '^', 1, { 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01 } },
			{ ' ', 3, { 0, 0, 0, 0, 0, 0, 0 } },
		};
		for (const FGlyph& G : Glyphs)
		{
			if (G.C != Ch) { continue; }
			for (int32 r = 0; r < 7; ++r) { OutRows[r] = G.R[r]; }
			OutWidth = G.W;
			return true;
		}
		return false;
	}

	/** Ancho de un texto en píxeles de la tipografía (con un píxel entre letras). */
	inline int32 TNFinishTextWidth(const char* Text)
	{
		int32 W = 0;
		int32 N = 0;
		for (const char* C = Text; *C; ++C)
		{
			uint8 Rows[7];
			int32 Gw = 0;
			if (TNFinishGlyph(*C, Rows, Gw)) { W += Gw; ++N; }
		}
		return W + FMath::Max(0, N - 1);
	}

	/**
	 * Una letra de bloques en el plano (Right, Up) con origen Origin en su esquina inferior izquierda,
	 * en relieve Depth hacia Normal: tramos horizontales de píxeles como cajas, con un contorno oscuro
	 * detrás (Outline cm más ancho por cada lado).
	 */
	inline void TNFinishGlyphAt(FTNProcMeshBuffers& M, char Ch, const FVector& Origin, const FVector& Right, const FVector& Up, const FVector& Normal,
		double Px, double Depth, double Outline, const FLinearColor& Fill, const FLinearColor& Edge)
	{
		uint8 Rows[7];
		int32 Gw = 0;
		if (!TNFinishGlyph(Ch, Rows, Gw)) { return; }
		for (int32 r = 0; r < 7; ++r)
		{
			const double Row = 6 - r;
			int32 c = 0;
			while (c < Gw)
			{
				const bool bOn = (Rows[r] >> (Gw - 1 - c)) & 1;
				if (!bOn) { ++c; continue; }
				int32 e = c;
				while (e + 1 < Gw && ((Rows[r] >> (Gw - 2 - e)) & 1)) { ++e; }
				const double Cx = (c + e + 1) * 0.5 * Px;
				const double Cy = (Row + 0.5) * Px;
				const double HalfW = (e - c + 1) * 0.5 * Px;
				const FVector Center = Origin + Right * Cx + Up * Cy;
				if (Outline > 0.0)
				{
					TNFinishBox(M, Center + Normal * (Depth * 0.35), Right, Up, Normal, FVector(HalfW + Outline, Px * 0.5 + Outline, Depth * 0.35), Edge);
				}
				TNFinishBox(M, Center + Normal * (Depth * 0.5), Right, Up, Normal, FVector(HalfW, Px * 0.5, Depth * 0.5), Fill);
				c = e + 1;
			}
		}
	}

	/** Texto recto centrado en Center (plano Right-Up, relieve hacia Normal). */
	inline void TNFinishText(FTNProcMeshBuffers& M, const char* Text, const FVector& Center, const FVector& Right, const FVector& Up, const FVector& Normal,
		double Px, double Depth, double Outline, const FLinearColor& Fill, const FLinearColor& Edge)
	{
		const double W = TNFinishTextWidth(Text) * Px;
		FVector Pen = Center - Right * (W * 0.5) - Up * (3.5 * Px);
		for (const char* C = Text; *C; ++C)
		{
			uint8 Rows[7];
			int32 Gw = 0;
			if (!TNFinishGlyph(*C, Rows, Gw)) { continue; }
			TNFinishGlyphAt(M, *C, Pen, Right, Up, Normal, Px, Depth, Outline, Fill, Edge);
			Pen += Right * ((Gw + 1) * Px);
		}
	}

	/**
	 * Texto en arco sobre un flanco del neumático: letras centradas en la cima, radio RText desde Center
	 * (plano YZ, desplazado a X = FaceX), leído de izquierda a derecha por quien mira desde ese lado.
	 */
	inline void TNFinishArcText(FTNProcMeshBuffers& M, const char* Text, const FVector& Center, double FaceX, double RText, double Px, double Depth,
		double Outline, const FLinearColor& Fill, const FLinearColor& Edge)
	{
		// UE es de mano izquierda: quien mira hacia +X tiene +Y a su derecha. Desde -X se lee hacia +Y
		// (el ángulo decrece); desde +X, hacia -Y (crece).
		const double Side = FaceX < 0.0 ? -1.0 : 1.0;
		const FVector Normal(FaceX < 0.0 ? -1.0 : 1.0, 0.0, 0.0);
		const double Span = TNFinishTextWidth(Text) * Px / RText;
		double Pen = 0.0;
		for (const char* C = Text; *C; ++C)
		{
			uint8 Rows[7];
			int32 Gw = 0;
			if (!TNFinishGlyph(*C, Rows, Gw)) { continue; }
			const double Mid = HALF_PI - Side * Span * 0.5 + Side * (Pen + Gw * 0.5 * Px) / RText;
			const FVector Radial(0.0, FMath::Cos(Mid), FMath::Sin(Mid));
			const FVector Tangent = FVector(0.0, -FMath::Sin(Mid), FMath::Cos(Mid)) * Side;
			const FVector Base = Center + FVector(FaceX, 0.0, 0.0) + Radial * (RText - 3.5 * Px) - Tangent * (Gw * 0.5 * Px);
			TNFinishGlyphAt(M, *C, Base, Tangent, Radial, Normal, Px, Depth, Outline, Fill, Edge);
			Pen += (Gw + 1) * Px;
		}
	}

	/** Bandera a cuadros ondeando: NX x NY casillas de Sq cm desde el mástil (Attach) hacia Along. */
	inline void TNFinishCheckerFlag(FTNProcMeshBuffers& M, const FVector& Attach, const FVector& Along, const FVector& Down, int32 NX, int32 NY, double Sq, double Wave, uint32 Seed)
	{
		const FVector Across = FVector::CrossProduct(Along, Down).GetSafeNormal();
		auto P = [&](int32 i, int32 j)
		{
			const double U = static_cast<double>(i) / NX;
			return Attach + Along * (i * Sq) + Down * (j * Sq) + Across * (Wave * U * FMath::Sin(U * 5.5 + (Seed & 7)));
		};
		for (int32 i = 0; i < NX; ++i)
		{
			for (int32 j = 0; j < NY; ++j)
			{
				const FLinearColor& Col = ((i + j) % 2) ? FinishColors::Ink : FinishColors::White;
				TNFinishCloth(M, P(i, j), P(i + 1, j), P(i + 1, j + 1), P(i, j + 1), Across, Col);
			}
		}
	}

	/** Boya de balizamiento: esfera de caras planas medio hundida. */
	inline void TNFinishBuoy(FTNProcMeshBuffers& M, const FVector& C, double R, const FLinearColor& Color)
	{
		TArray<double> Z, Rad;
		for (int32 k = 0; k <= 4; ++k)
		{
			const double A = -HALF_PI + PI * k / 4.0;
			Z.Add(R * FMath::Sin(A));
			Rad.Add(FMath::Max(1.0, R * FMath::Cos(A)));
		}
		TNProcAddLathe(M, C, Z, Rad, 0.0, 0u, Color, 7);
	}

	/**
	 * Banderola de playa (tipo pluma) en Base: mástil y vela curva que se abre hacia -X (el viento viene
	 * del mar), con una franja blanca cerca de la punta.
	 */
	inline void TNFinishFeatherFlag(FTNProcMeshBuffers& Solid, FTNProcMeshBuffers& Deco, const FVector& Base, double Height, const FLinearColor& Color, double Facing)
	{
		TNProcAddCylinder(Solid, Base - FVector(0.0, 0.0, 30.0), Base + FVector(0.0, 0.0, Height), 6.0, 4.0, 6, FinishColors::Pole);
		const int32 N = 9;
		const double Z0 = Height * 0.24;
		const double Z1 = Height * 0.97;
		const FVector Out(-1.0, 0.0, 0.0);
		auto Inner = [&](int32 k) { return Base + FVector(0.0, 0.0, LerpD(Z0, Z1, static_cast<double>(k) / N)); };
		auto Outer = [&](int32 k)
		{
			const double T = static_cast<double>(k) / N;
			const double W = (0.55 + 0.45 * FMath::Sin(PI * 0.8 * T)) * FMath::Sqrt(FMath::Max(0.0, 1.0 - FMath::Pow(T, 5.0))) * Height * 0.2;
			return Inner(k) + Out * W - FVector(0.0, 0.0, 0.08 * W) + FVector(0.0, Facing * 6.0 * FMath::Sin(T * 3.0), 0.0);
		};
		for (int32 k = 0; k < N; ++k)
		{
			const double T = (k + 0.5) / N;
			const FLinearColor Col = (T > 0.66 && T < 0.78) ? FinishColors::White : Color;
			TNFinishCloth(Deco, Inner(k), Outer(k), Outer(k + 1), Inner(k + 1), FVector(0.0, 1.0, 0.0), Col);
		}
	}

	/**
	 * Construye la meta. Solid va con colisión (neumático y mástiles); Deco sin ella (rótulos, telas,
	 * boyas). Ground(X, Y) da la cota del terreno y HalfWidthAt(X) el semiancho de la playa a la altura
	 * X del eje (negativo hacia la arena).
	 */
	template <typename FGround, typename FHalfWidth>
	void TNFinishBuild(FTNProcMeshBuffers& Solid, FTNProcMeshBuffers& Deco, const FTNFinishParams& P, FGround&& Ground, FHalfWidth&& HalfWidthAt)
	{
		using namespace FinishColors;
		const double Ri = P.Radius;
		const double Ro = Ri + 300.0;
		const double Tw = 150.0;
		const FVector Ctr(0.0, 0.0, P.FloorZ - 60.0);
		const double Zc = Ctr.Z;

		// ── Neumático: perfil con flanco plano (para el rótulo), hombros y banda de rodadura ──
		{
			const double Prof[][2] = {
				{ -Tw + 45.0, Ri }, { -Tw + 10.0, Ri + 25.0 }, { -Tw, Ri + 70.0 }, { -Tw, Ro - 70.0 }, { -Tw + 18.0, Ro - 18.0 }, { -Tw + 55.0, Ro },
				{ Tw - 55.0, Ro }, { Tw - 18.0, Ro - 18.0 }, { Tw, Ro - 70.0 }, { Tw, Ri + 70.0 }, { Tw - 10.0, Ri + 25.0 }, { Tw - 45.0, Ri } };
			const int32 NP = UE_ARRAY_COUNT(Prof);
			const int32 Steps = 64;
			const double A0 = FMath::DegreesToRadians(-9.0);
			const double A1 = FMath::DegreesToRadians(189.0);
			TArray<TArray<FVector>> Rings;
			for (int32 s = 0; s <= Steps; ++s)
			{
				const double A = LerpD(A0, A1, static_cast<double>(s) / Steps);
				TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
				for (int32 k = 0; k < NP; ++k)
				{
					Ring.Add(Ctr + FVector(Prof[k][0], Prof[k][1] * FMath::Cos(A), Prof[k][1] * FMath::Sin(A)));
				}
			}
			Solid.AddSweep(Rings, true, Rubber);

			// Rodadura: tacos en dos filas, en espiga.
			const int32 Blocks = 44;
			for (int32 b = 0; b < Blocks; ++b)
			{
				for (int32 Row = 0; Row < 2; ++Row)
				{
					const double A = FMath::DegreesToRadians(-4.0 + (188.0 * (b + 0.5 * Row + 0.25)) / Blocks);
					const FVector Radial(0.0, FMath::Cos(A), FMath::Sin(A));
					const FVector Tangent(0.0, -FMath::Sin(A), FMath::Cos(A));
					const double Skew = FMath::DegreesToRadians(Row == 0 ? 28.0 : -28.0);
					const FVector Tx = Tangent * FMath::Cos(Skew) + FVector(1.0, 0.0, 0.0) * FMath::Sin(Skew);
					const FVector Xx = FVector::CrossProduct(Radial, Tx).GetSafeNormal();
					const FVector C = Ctr + Radial * (Ro + 12.0) + FVector(Row == 0 ? -46.0 : 46.0, 0.0, 0.0);
					TNFinishBox(Solid, C, Tx, Xx, Radial, FVector(34.0, 30.0, 14.0), Tread);
				}
			}
		}

		// ── Flancos: dos aros amarillos y el nombre del juego, legible por delante y por detrás ──
		for (const double FaceX : { -Tw, Tw })
		{
			const double Out = FaceX < 0.0 ? -1.0 : 1.0;
			for (const double RingR : { Ri + 78.0, Ro - 82.0 })
			{
				const int32 Seg = 72;
				for (int32 s = 0; s < Seg; ++s)
				{
					const double A = FMath::DegreesToRadians(LerpD(-6.0, 186.0, static_cast<double>(s) / Seg));
					const double B = FMath::DegreesToRadians(LerpD(-6.0, 186.0, static_cast<double>(s + 1) / Seg));
					auto At = [&](double Ang, double R) { return Ctr + FVector(FaceX + Out * 2.0, R * FMath::Cos(Ang), R * FMath::Sin(Ang)); };
					Deco.AddQuad(At(A, RingR - 7.0), At(B, RingR - 7.0), At(B, RingR + 7.0), At(A, RingR + 7.0), FVector(Out, 0.0, 0.0), Brand);
				}
			}
			TNFinishArcText(Deco, "TORTUNABO", Ctr, FaceX, 0.5 * (Ri + Ro), 16.0, 7.0, 5.0, Brand, Ink);
		}

		// ── Pasarela a cuadros de lado a lado y cartel de META encima ──
		const auto HalfSpanAt = [&](double Z) { return FMath::Sqrt(FMath::Max(0.0, Ri * Ri - FMath::Square(Z - Zc))); };
		const double BeamZ0 = 540.0;
		const double BeamZ1 = 660.0;
		const double BeamHalf = HalfSpanAt(BeamZ0) + 30.0;
		TNFinishBox(Deco, FVector(0.0, 0.0, 0.5 * (BeamZ0 + BeamZ1)), FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0),
			FVector(40.0, BeamHalf, 0.5 * (BeamZ1 - BeamZ0)), Ink);
		{
			const double Sq = 0.5 * (BeamZ1 - BeamZ0);
			const int32 Cols = FMath::FloorToInt(2.0 * HalfSpanAt(BeamZ1) / Sq);
			const double Y0 = -0.5 * Cols * Sq;
			for (int32 i = 0; i < Cols; ++i)
			{
				for (int32 j = 0; j < 2; ++j)
				{
					if ((i + j) % 2) { continue; }
					for (const double Sx : { -41.5, 41.5 })
					{
						const FVector N(Sx < 0.0 ? -1.0 : 1.0, 0.0, 0.0);
						const double Ya = Y0 + i * Sq, Yb = Ya + Sq, Za = BeamZ0 + j * Sq, Zb = Za + Sq;
						Deco.AddQuad(FVector(Sx, Ya, Za), FVector(Sx, Yb, Za), FVector(Sx, Yb, Zb), FVector(Sx, Ya, Zb), N, White);
					}
				}
			}
		}
		{
			const double PanZ0 = BeamZ1;
			const double PanZ1 = 930.0;
			const double PanHalf = FMath::Min(620.0, HalfSpanAt(PanZ1) - 40.0);
			const FVector Mid(0.0, 0.0, 0.5 * (PanZ0 + PanZ1));
			TNFinishBox(Deco, Mid, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(22.0, PanHalf, 0.5 * (PanZ1 - PanZ0)), Red);
			// Marco amarillo.
			const double Fr = 16.0;
			for (const double Sx : { -1.0, 1.0 })
			{
				const FVector N(Sx, 0.0, 0.0);
				const double X = Sx * 24.0;
				TNFinishBox(Deco, FVector(X, 0.0, PanZ1 - Fr * 0.5), N, FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(3.0, PanHalf, Fr * 0.5), Brand);
				TNFinishBox(Deco, FVector(X, 0.0, PanZ0 + Fr * 0.5), N, FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(3.0, PanHalf, Fr * 0.5), Brand);
				TNFinishBox(Deco, FVector(X, PanHalf - Fr * 0.5, Mid.Z), N, FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(3.0, Fr * 0.5, 0.5 * (PanZ1 - PanZ0)), Brand);
				TNFinishBox(Deco, FVector(X, -PanHalf + Fr * 0.5, Mid.Z), N, FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(3.0, Fr * 0.5, 0.5 * (PanZ1 - PanZ0)), Brand);
				// META, legible desde cada lado (UE es de mano izquierda: mirando hacia +X, +Y queda a la derecha).
				const FVector Right(0.0, Sx < 0.0 ? 1.0 : -1.0, 0.0);
				TNFinishText(Deco, "META", FVector(Sx * 22.0, 0.0, Mid.Z), Right, FVector(0.0, 0.0, 1.0), N, 30.0, 4.0, 6.0, White, Ink);
			}
		}

		// ── Rótulo colgante bajo la pasarela ──
		{
			const double SignZ0 = 380.0;
			const double SignZ1 = 500.0;
			const double SignHalf = 350.0;
			TNFinishBox(Deco, FVector(0.0, 0.0, 0.5 * (SignZ0 + SignZ1)), FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0),
				FVector(8.0, SignHalf, 0.5 * (SignZ1 - SignZ0)), Brand);
			for (const double Y : { -SignHalf + 40.0, SignHalf - 40.0 })
			{
				Deco.AddBeam(FVector(0.0, Y, SignZ1), FVector(0.0, Y, BeamZ0), 2.5, Rope);
			}
			for (const double Sx : { -1.0, 1.0 })
			{
				const FVector Right(0.0, Sx < 0.0 ? 1.0 : -1.0, 0.0);
				TNFinishText(Deco, "^AL AGUA!", FVector(Sx * 8.0, 0.0, 0.5 * (SignZ0 + SignZ1)), Right, FVector(0.0, 0.0, 1.0), FVector(Sx, 0.0, 0.0),
					12.5, 5.0, 0.0, Ink, Ink);
			}
		}

		// ── Banderas a cuadros en la cima del neumático ──
		for (const double Deg : { 70.0, 110.0 })
		{
			const double A = FMath::DegreesToRadians(Deg);
			const FVector Radial(0.0, FMath::Cos(A), FMath::Sin(A));
			const FVector Foot = Ctr + Radial * (Ro + 20.0);
			const FVector Top = Foot + Radial * 420.0;
			TNProcAddCylinder(Solid, Foot - Radial * 40.0, Top, 7.0, 5.0, 6, Pole);
			TNProcAddLathe(Solid, Top, { 0.0, 10.0, 20.0 }, { 10.0, 12.0, 1.0 }, 0.0, 0u, Brand, 6);
			const FVector Along = FVector(0.0, Deg < 90.0 ? 1.0 : -1.0, 0.0);
			TNFinishCheckerFlag(Deco, Top - FVector(0.0, 0.0, 15.0), Along, FVector(0.0, 0.0, -1.0), 5, 4, 34.0, 26.0, P.Seed + static_cast<uint32>(Deg));
		}

		// ── Boyas de la línea en toda la boca (bajo el arco también) ──
		{
			const double Reach = P.MouthHalf - 120.0;
			const double Step = 170.0;
			FVector Prev;
			bool bPrev = false;
			int32 k = 0;
			for (double Y = -Reach; Y <= Reach; Y += Step, ++k)
			{
				const bool bTyre = FMath::Abs(Y) > Ri - 40.0 && FMath::Abs(Y) < Ro + 60.0;
				const bool bWet = Ground(0.0, Y) < -25.0;
				if (bTyre || !bWet) { bPrev = false; continue; }
				const FVector C(0.0, Y, 2.0);
				TNFinishBuoy(Deco, C, 21.0, (k % 2) ? White : Red);
				if (bPrev) { Deco.AddBeam(Prev + FVector(0.0, 0.0, 4.0), C + FVector(0.0, 0.0, 4.0), 2.2, Rope); }
				Prev = C;
				bPrev = true;
			}
		}

		// ── Banderines desde los hombros del neumático hasta los mástiles de la orilla ──
		static const FLinearColor Pennants[5] = { Red, Brand, FLinearColor(0.1f, 0.35f, 0.8f), FLinearColor(0.15f, 0.62f, 0.2f), White };
		for (const double Side : { -1.0, 1.0 })
		{
			const double A = FMath::DegreesToRadians(Side > 0.0 ? 38.0 : 142.0);
			const FVector From = Ctr + FVector(0.0, Ro * FMath::Cos(A), Ro * FMath::Sin(A));
			const double MastX = -1100.0;
			const double MastY = Side * FMath::Max(Ro + 400.0, HalfWidthAt(MastX) - 350.0);
			const double MastZ0 = Ground(MastX, MastY);
			const double MastH = 560.0;
			TNProcAddCylinder(Solid, FVector(MastX, MastY, MastZ0 - 40.0), FVector(MastX, MastY, MastZ0 + MastH), 9.0, 6.0, 6, Pole);
			const FVector To(MastX, MastY, MastZ0 + MastH - 20.0);
			const double Len = FVector::Dist(From, To);
			const int32 N = FMath::Max(4, FMath::FloorToInt(Len / 70.0));
			FVector PrevP = From;
			for (int32 i = 1; i <= N; ++i)
			{
				const double T = static_cast<double>(i) / N;
				const FVector Pt = FMath::Lerp(From, To, T) - FVector(0.0, 0.0, 140.0 * 4.0 * T * (1.0 - T));
				Deco.AddBeam(PrevP, Pt, 1.6, Rope);
				if (i < N)
				{
					const FVector Dir = (Pt - PrevP).GetSafeNormal();
					const FVector A0 = PrevP + Dir * 8.0;
					const FVector A1 = Pt - Dir * 8.0;
					const FVector Tip = (A0 + A1) * 0.5 - FVector(0.0, 0.0, 38.0);
					const FVector N0 = FVector::CrossProduct(A1 - A0, Tip - A0).GetSafeNormal();
					Deco.AddTri(A0, A1, Tip, N0, Pennants[i % 5]);
					Deco.AddTri(A0, A1, Tip, -N0, Pennants[i % 5] * 0.85f);
				}
				PrevP = Pt;
			}
		}

		// ── Banderolas a lo largo de la playa, al pie de los brazos ──
		{
			static const FLinearColor Flags[4] = { Red, Brand, FLinearColor(0.1f, 0.35f, 0.8f), FLinearColor(0.12f, 0.6f, 0.25f) };
			int32 n = 0;
			for (double X = -2300.0; X >= -5900.0; X -= 1200.0)
			{
				for (const double Side : { -1.0, 1.0 })
				{
					const double Y = Side * (HalfWidthAt(X) - 320.0);
					const double Z = Ground(X, Y);
					if (Z < 10.0) { continue; }
					TNFinishFeatherFlag(Solid, Deco, FVector(X, Y, Z), 470.0 + 40.0 * ((n * 7 + static_cast<int32>(P.Seed)) % 3), Flags[n % 4], Side);
					++n;
				}
			}
		}
	}
}
