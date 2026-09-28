#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapTerrain.h"

/**
 * Detalle adaptativo del terreno (lógica pura, sin motor).
 *
 * El mallado grueso (VertexSpacing, 1,5 m) dibuja bien las laderas lisas, pero en los cambios bruscos de
 * pendiente (pie y borde de los taludes, crestas, bocas de cueva) se aparta de la función del terreno y, en
 * diagonal a la rejilla, los dibuja en escalera. Donde la malla se aparta más de NearError (cerca de los
 * caminos) o de FarError (lejos), el quad se parte en N x N (0,5 m con N = 3). Sus vecinos hacen de costura:
 * un abanico desde su centro que recoge los puntos del lado partido, así no quedan grietas. Cada punto de un
 * lado se calcula solo con los dos vértices de ese lado, igual desde los dos quads (y teselas) que lo comparten.
 *
 * La malla de cada tesela (BuildTileMesh) y la altura dibujada en cualquier punto (SurfaceAt, para colocar
 * cosas sobre el suelo) salen de los mismos datos: lo que se ve, lo que colisiona y lo que se consulta coinciden.
 */
namespace TNProcMap
{
	struct FTerrainDetailParams
	{
		/** Subdivisiones por lado de un quad con detalle (3: de 1,5 m a 0,5 m); 1 = sin detalle. */
		int32 N = 3;
		/** Error (cm) de la malla gruesa desde el que se parte un quad cerca de los caminos. */
		double NearError = 15.0;
		/** Ídem lejos de los caminos (crestas de las montañas). */
		double FarError = 50.0;
		/** Distancia (cm) al borde del camino hasta la que cuenta NearError. */
		double NearDistance = 6000.0;
	};

	/** Malla de una tesela del terreno (espacio del mapa). */
	struct FTerrainTileMesh
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		/** Máscara del camino (0..255) de cada vértice. */
		TArray<uint8> Masks;
	};

	/**
	 * Altura en (Tx, Ty) de [0, 1]^2 dentro de un quad con esquinas H00=(0,0) H10=(1,0) H01=(0,1) H11=(1,1),
	 * con los mismos dos triángulos que la malla (diagonal de SplitAlongAD).
	 */
	inline double TerrainQuadHeight(double H00, double H10, double H01, double H11, double Tx, double Ty)
	{
		if (SplitAlongAD(H00, H10, H01, H11))
		{
			return Tx >= Ty ? H00 + Tx * (H10 - H00) + Ty * (H11 - H10) : H00 + Ty * (H01 - H00) + Tx * (H11 - H01);
		}
		return Tx + Ty <= 1.0 ? H00 + Tx * (H10 - H00) + Ty * (H01 - H00) : H11 + (1.0 - Tx) * (H01 - H11) + (1.0 - Ty) * (H10 - H11);
	}

	class FTerrainDetail
	{
	public:
		/** Quad normal (dos triángulos), partido en N x N, o costura (abanico; en los bits bajos, sus lados partidos). */
		static constexpr uint8 Plain = 0;
		static constexpr uint8 Fine = 1;
		static constexpr uint8 SeamBit = 0x10;
		/** Lados de un quad: 0 abajo (y), 1 derecha (x + 1), 2 arriba (y + 1), 3 izquierda (x). */
		static constexpr int32 NumSides = 4;

		int32 N = 1;
		/** Quads del mallado grueso (vértices: (QX + 1) x (QY + 1)). */
		int32 QX = 0;
		int32 QY = 0;
		FVector2D Origin = FVector2D::ZeroVector;
		double Spacing = 150.0;
		/** Tipo de cada quad (x + y * QX); vacío si no hay detalle. */
		TArray<uint8> Kind;
		/** Desplazamiento de sus datos en Pool (-1 en los normales). */
		TArray<int32> Offset;
		/**
		 * Quad partido: (N + 1)^2 alturas, fila a fila (j * (N + 1) + i), esquinas incluidas. Costura: su centro y
		 * los N - 1 puntos de cada lado (1 + Lado * (N - 1) + k - 1), aunque solo valen los de sus lados partidos.
		 */
		TArray<float> Pool;
		TArray<uint8> PoolMask;

		bool IsActive() const { return N > 1 && QX > 0 && QY > 0 && Kind.Num() == QX * QY; }

		uint8 KindAt(int32 x, int32 y) const
		{
			return IsActive() && x >= 0 && y >= 0 && x < QX && y < QY ? Kind[y * QX + x] : Plain;
		}

		static bool IsSeam(uint8 K) { return (K & SeamBit) != 0; }

		int32 SeamSlot(int32 Side, int32 k) const { return 1 + Side * (N - 1) + (k - 1); }

		/** Punto (fx, fy) del mallado fino (N por quad) en el mapa. */
		FVector2D FinePos(double fx, double fy) const { return Origin + FVector2D(fx * Spacing / N, fy * Spacing / N); }

		/**
		 * Altura y máscara guardadas de un punto del mallado fino que no es vértice grueso (en un lado partido o
		 * dentro de un quad partido); false si no existe.
		 */
		bool FinePoint(int32 gfx, int32 gfy, float& OutH, uint8& OutMask) const
		{
			if (!IsActive()) { return false; }
			const int32 x = gfx / N, y = gfy / N, i = gfx % N, j = gfy % N;
			auto Read = [&](int32 q, int32 Slot) { OutH = Pool[Offset[q] + Slot]; OutMask = PoolMask[Offset[q] + Slot]; return true; };
			if (i != 0 && j != 0)
			{
				return KindAt(x, y) == Fine ? Read(y * QX + x, j * (N + 1) + i) : false;
			}
			if (j == 0 && i != 0)
			{
				// Lado horizontal de la fila y, entre los quads (x, y - 1) y (x, y).
				const uint8 Up = KindAt(x, y), Down = KindAt(x, y - 1);
				if (Up == Fine) { return Read(y * QX + x, i); }
				if (Down == Fine) { return Read((y - 1) * QX + x, N * (N + 1) + i); }
				if (IsSeam(Up) && (Up & (1 << 0))) { return Read(y * QX + x, SeamSlot(0, i)); }
				if (IsSeam(Down) && (Down & (1 << 2))) { return Read((y - 1) * QX + x, SeamSlot(2, i)); }
				return false;
			}
			if (i == 0 && j != 0)
			{
				// Lado vertical de la columna x, entre los quads (x - 1, y) y (x, y).
				const uint8 Right = KindAt(x, y), Left = KindAt(x - 1, y);
				if (Right == Fine) { return Read(y * QX + x, j * (N + 1)); }
				if (Left == Fine) { return Read(y * QX + x - 1, j * (N + 1) + N); }
				if (IsSeam(Right) && (Right & (1 << 3))) { return Read(y * QX + x, SeamSlot(3, j)); }
				if (IsSeam(Left) && (Left & (1 << 1))) { return Read(y * QX + x - 1, SeamSlot(1, j)); }
				return false;
			}
			return false;
		}

		/**
		 * Contorno de una costura en sentido antihorario (coordenadas locales en N-ésimos del quad): esquinas y los
		 * puntos de sus lados partidos. Devuelve cuántos.
		 */
		int32 SeamOutline(uint8 K, int32 (&OutI)[4 + 4 * 8], int32 (&OutJ)[4 + 4 * 8]) const
		{
			int32 Num = 0;
			auto Push = [&](int32 i, int32 j) { OutI[Num] = i; OutJ[Num] = j; ++Num; };
			Push(0, 0);
			if (K & (1 << 0)) { for (int32 k = 1; k < N; ++k) { Push(k, 0); } }
			Push(N, 0);
			if (K & (1 << 1)) { for (int32 k = 1; k < N; ++k) { Push(N, k); } }
			Push(N, N);
			if (K & (1 << 2)) { for (int32 k = N - 1; k >= 1; --k) { Push(k, N); } }
			Push(0, N);
			if (K & (1 << 3)) { for (int32 k = N - 1; k >= 1; --k) { Push(0, k); } }
			return Num;
		}

		/** Altura guardada del punto (i, j) (en N-ésimos) del contorno de la costura q. */
		double SeamOutlineHeight(int32 q, int32 x, int32 y, int32 i, int32 j, const TArray<float>& H) const
		{
			const int32 NX = QX + 1;
			if ((i == 0 || i == N) && (j == 0 || j == N)) { return H[(y + j / N) * NX + x + i / N]; }
			if (j == 0) { return Pool[Offset[q] + SeamSlot(0, i)]; }
			if (i == N) { return Pool[Offset[q] + SeamSlot(1, j)]; }
			if (j == N) { return Pool[Offset[q] + SeamSlot(2, i)]; }
			return Pool[Offset[q] + SeamSlot(3, j)];
		}

		/**
		 * Altura de la superficie dibujada en (Lx, Ly), coordenadas del mallado grueso (en quads): la misma que la
		 * malla y su colisión.
		 */
		double SurfaceAt(double Lx, double Ly, const TArray<float>& H) const
		{
			const int32 NX = QX + 1;
			const double Fx = FMath::Clamp(Lx, 0.0, static_cast<double>(QX) - 1e-9);
			const double Fy = FMath::Clamp(Ly, 0.0, static_cast<double>(QY) - 1e-9);
			const int32 x = FMath::Clamp(FMath::FloorToInt(Fx), 0, QX - 1);
			const int32 y = FMath::Clamp(FMath::FloorToInt(Fy), 0, QY - 1);
			const double U = Fx - x, V = Fy - y;
			const uint8 K = KindAt(x, y);
			const int32 q = y * QX + x;
			if (K == Fine)
			{
				const float* F = &Pool[Offset[q]];
				const double Su = U * N, Sv = V * N;
				const int32 i = FMath::Clamp(FMath::FloorToInt(Su), 0, N - 1);
				const int32 j = FMath::Clamp(FMath::FloorToInt(Sv), 0, N - 1);
				const int32 W = N + 1;
				return TerrainQuadHeight(F[j * W + i], F[j * W + i + 1], F[(j + 1) * W + i], F[(j + 1) * W + i + 1], Su - i, Sv - j);
			}
			if (IsSeam(K))
			{
				// Abanico desde el centro: el triángulo del contorno que contiene (U, V).
				int32 Ci[4 + 4 * 8], Cj[4 + 4 * 8];
				const int32 Num = SeamOutline(K, Ci, Cj);
				const double Mc = Pool[Offset[q]];
				for (int32 k = 0; k < Num; ++k)
				{
					const int32 k1 = (k + 1) % Num;
					const double Ax = static_cast<double>(Ci[k]) / N - 0.5, Ay = static_cast<double>(Cj[k]) / N - 0.5;
					const double Bx = static_cast<double>(Ci[k1]) / N - 0.5, By = static_cast<double>(Cj[k1]) / N - 0.5;
					const double Px = U - 0.5, Py = V - 0.5;
					// P = a * A + b * B (desde el centro); dentro si a, b >= 0 y a + b <= 1.
					const double Det = Ax * By - Ay * Bx;
					if (FMath::Abs(Det) < 1e-12) { continue; }
					const double a = (Px * By - Py * Bx) / Det;
					const double b = (Ax * Py - Ay * Px) / Det;
					if (a >= -1e-9 && b >= -1e-9 && a + b <= 1.0 + 1e-9)
					{
						const double Ha = SeamOutlineHeight(q, x, y, Ci[k], Cj[k], H);
						const double Hb = SeamOutlineHeight(q, x, y, Ci[k1], Cj[k1], H);
						return Mc + a * (Ha - Mc) + b * (Hb - Mc);
					}
				}
			}
			return TerrainQuadHeight(H[y * NX + x], H[y * NX + x + 1], H[(y + 1) * NX + x], H[(y + 1) * NX + x + 1], U, V);
		}

		/** Normal de la superficie dibujada en (Lx, Ly) por diferencias centrales a E quads a cada lado. */
		FVector SurfaceNormal(double Lx, double Ly, double E, const TArray<float>& H) const
		{
			const double Hx = SurfaceAt(Lx + E, Ly, H) - SurfaceAt(Lx - E, Ly, H);
			const double Hy = SurfaceAt(Lx, Ly + E, H) - SurfaceAt(Lx, Ly - E, H);
			return FVector(-Hx, -Hy, 2.0 * E * Spacing).GetSafeNormal();
		}

		/**
		 * Malla de la tesela (Tx, Ty) de TileQuads x TileQuads quads: primero sus vértices gruesos, fila a fila,
		 * y luego los finos que haga falta. Sin detalle, la malla de siempre (dos triángulos por quad).
		 */
		void BuildTileMesh(int32 Tx, int32 Ty, int32 TileQuads, const TArray<float>& H, const TArray<uint8>& Mask, FTerrainTileMesh& Out) const
		{
			const int32 NX = QX + 1;
			const int32 NY = QY + 1;
			const int32 Side = TileQuads + 1;
			const int32 Nd = IsActive() ? N : 1;
			Out.Verts.Reset();
			Out.Tris.Reset();
			Out.Normals.Reset();
			Out.Masks.Reset();
			Out.Verts.Reserve(Side * Side);
			Out.Normals.Reserve(Side * Side);
			Out.Masks.Reserve(Side * Side);
			Out.Tris.Reserve(TileQuads * TileQuads * 6);
			auto Hc = [&](int32 X, int32 Y) { return static_cast<double>(H[FMath::Clamp(Y, 0, NY - 1) * NX + FMath::Clamp(X, 0, NX - 1)]); };
			// Un vértice grueso que toca un cuadrado con detalle lleva la normal fina, como sus vecinos finos.
			auto TouchesDetail = [&](int32 GX, int32 GY)
			{
				return Nd > 1 && (KindAt(GX - 1, GY - 1) != Plain || KindAt(GX, GY - 1) != Plain || KindAt(GX - 1, GY) != Plain || KindAt(GX, GY) != Plain);
			};
			for (int32 y = 0; y <= TileQuads; ++y)
			{
				for (int32 x = 0; x <= TileQuads; ++x)
				{
					const int32 GX = Tx * TileQuads + x;
					const int32 GY = Ty * TileQuads + y;
					const FVector2D P = Origin + FVector2D(GX * Spacing, GY * Spacing);
					Out.Verts.Add(FVector(P.X, P.Y, Hc(GX, GY)));
					if (TouchesDetail(GX, GY))
					{
						Out.Normals.Add(SurfaceNormal(GX, GY, 1.0 / Nd, H));
					}
					else
					{
						const double Dx = (Hc(GX + 1, GY) - Hc(GX - 1, GY)) / (2.0 * Spacing);
						const double Dy = (Hc(GX, GY + 1) - Hc(GX, GY - 1)) / (2.0 * Spacing);
						Out.Normals.Add(FVector(-Dx, -Dy, 1.0).GetSafeNormal());
					}
					Out.Masks.Add(Mask[FMath::Clamp(GY, 0, NY - 1) * NX + FMath::Clamp(GX, 0, NX - 1)]);
				}
			}
			// Vértices finos de la tesela (índice en su mallado fino local; -1 si aún no está).
			const int32 FS = TileQuads * Nd + 1;
			TArray<int32> FineIndex;
			if (Nd > 1) { FineIndex.Init(INDEX_NONE, FS * FS); }
			auto Vertex = [&](int32 lfx, int32 lfy) -> int32
			{
				if (lfx % Nd == 0 && lfy % Nd == 0) { return (lfy / Nd) * Side + lfx / Nd; }
				int32& Slot = FineIndex[lfy * FS + lfx];
				if (Slot != INDEX_NONE) { return Slot; }
				const int32 gfx = Tx * TileQuads * Nd + lfx;
				const int32 gfy = Ty * TileQuads * Nd + lfy;
				float Hf = 0.0f;
				uint8 Mf = 0;
				FinePoint(gfx, gfy, Hf, Mf);
				Slot = Out.Verts.Num();
				const FVector2D P = FinePos(gfx, gfy);
				Out.Verts.Add(FVector(P.X, P.Y, Hf));
				Out.Normals.Add(SurfaceNormal(static_cast<double>(gfx) / Nd, static_cast<double>(gfy) / Nd, 1.0 / Nd, H));
				Out.Masks.Add(Mf);
				return Slot;
			};
			// A=(x,y), B=(x+1,y), C=(x,y+1), D=(x+1,y+1). La cara frontal de UE es (C-A)x(B-A):
			// (A,C,B)+(B,C,D) o (A,C,D)+(A,D,B) miran hacia +Z. La diagonal sigue la curva de nivel.
			auto Quad = [&](int32 A, int32 B, int32 C, int32 D)
			{
				if (SplitAlongAD(Out.Verts[A].Z, Out.Verts[B].Z, Out.Verts[C].Z, Out.Verts[D].Z))
				{
					Out.Tris.Add(A); Out.Tris.Add(C); Out.Tris.Add(D);
					Out.Tris.Add(A); Out.Tris.Add(D); Out.Tris.Add(B);
				}
				else
				{
					Out.Tris.Add(A); Out.Tris.Add(C); Out.Tris.Add(B);
					Out.Tris.Add(B); Out.Tris.Add(C); Out.Tris.Add(D);
				}
			};
			for (int32 ly = 0; ly < TileQuads; ++ly)
			{
				for (int32 lx = 0; lx < TileQuads; ++lx)
				{
					const int32 gx = Tx * TileQuads + lx;
					const int32 gy = Ty * TileQuads + ly;
					const uint8 K = KindAt(gx, gy);
					if (K == Fine)
					{
						for (int32 j = 0; j < Nd; ++j)
						{
							for (int32 i = 0; i < Nd; ++i)
							{
								Quad(Vertex(lx * Nd + i, ly * Nd + j), Vertex(lx * Nd + i + 1, ly * Nd + j),
									Vertex(lx * Nd + i, ly * Nd + j + 1), Vertex(lx * Nd + i + 1, ly * Nd + j + 1));
							}
						}
						continue;
					}
					if (IsSeam(K))
					{
						int32 Ci[4 + 4 * 8], Cj[4 + 4 * 8];
						const int32 Num = SeamOutline(K, Ci, Cj);
						int32 Ring[4 + 4 * 8];
						for (int32 k = 0; k < Num; ++k) { Ring[k] = Vertex(lx * Nd + Ci[k], ly * Nd + Cj[k]); }
						// Centro del abanico (solo de este quad).
						const int32 q = gy * QX + gx;
						const int32 M = Out.Verts.Num();
						const FVector2D P = Origin + FVector2D((gx + 0.5) * Spacing, (gy + 0.5) * Spacing);
						Out.Verts.Add(FVector(P.X, P.Y, Pool[Offset[q]]));
						Out.Normals.Add(SurfaceNormal(gx + 0.5, gy + 0.5, 1.0 / Nd, H));
						Out.Masks.Add(PoolMask[Offset[q]]);
						// Contorno antihorario visto desde arriba: (M, siguiente, actual) mira hacia +Z.
						for (int32 k = 0; k < Num; ++k)
						{
							Out.Tris.Add(M); Out.Tris.Add(Ring[(k + 1) % Num]); Out.Tris.Add(Ring[k]);
						}
						continue;
					}
					const int32 A = ly * Side + lx;
					Quad(A, A + 1, A + Side, A + Side + 1);
				}
			}
		}

		int32 CountKind(bool bSeam) const
		{
			int32 C = 0;
			for (const uint8 K : Kind) { C += bSeam ? (IsSeam(K) ? 1 : 0) : (K == Fine ? 1 : 0); }
			return C;
		}
	};

	/**
	 * Decide qué quads llevan detalle y calcula sus alturas. H y EdgeDist: alturas y distancia al borde del camino
	 * (FTerrainBuilder::ExportPathEdgeDistance) del mallado grueso de TB. Parallel(Num, Body) ejecuta Body(i) para
	 * i en [0, Num) (en el juego, ParallelFor).
	 */
	template <typename FParallel>
	void BuildTerrainDetail(FTerrainDetail& Out, const FTerrainBuilder& TB, const TArray<float>& H, const TArray<float>& EdgeDist,
		int32 QX, int32 QY, const FTerrainDetailParams& Prm, const FParallel& Parallel)
	{
		Out = FTerrainDetail();
		Out.N = FMath::Clamp(Prm.N, 1, 8);
		Out.QX = QX;
		Out.QY = QY;
		Out.Origin = TB.Origin;
		Out.Spacing = TB.Spacing;
		if (Out.N <= 1 || QX < 1 || QY < 1) { return; }
		const int32 N = Out.N;
		const int32 NX = QX + 1;
		const int32 NQ = QX * QY;
		const FVector2D Origin = TB.Origin;
		const double Spacing = TB.Spacing;
		auto Hc = [&](int32 x, int32 y) { return static_cast<double>(H[FMath::Clamp(y, 0, QY) * NX + FMath::Clamp(x, 0, QX)]); };
		// Curvatura (segunda diferencia) en un vértice: filtro barato antes de medir el error de verdad.
		auto K2 = [&](int32 x, int32 y)
		{
			const double c = 2.0 * Hc(x, y);
			double k = FMath::Abs(Hc(x - 1, y) - c + Hc(x + 1, y));
			k = FMath::Max(k, FMath::Abs(Hc(x, y - 1) - c + Hc(x, y + 1)));
			k = FMath::Max(k, 0.5 * FMath::Abs(Hc(x - 1, y - 1) - c + Hc(x + 1, y + 1)));
			return FMath::Max(k, 0.5 * FMath::Abs(Hc(x - 1, y + 1) - c + Hc(x + 1, y - 1)));
		};

		// 1. Candidatos: 1 lejos de los caminos, 2 cerca.
		TArray<uint8> Cand;
		Cand.Init(0, NQ);
		Parallel(QY, [&](int32 y)
		{
			for (int32 x = 0; x < QX; ++x)
			{
				const int32 A = y * NX + x;
				const float Ed = FMath::Min(FMath::Min(EdgeDist[A], EdgeDist[A + 1]), FMath::Min(EdgeDist[A + NX], EdgeDist[A + NX + 1]));
				const bool bNear = Ed < Prm.NearDistance;
				const double K = FMath::Max(FMath::Max(K2(x, y), K2(x + 1, y)), FMath::Max(K2(x, y + 1), K2(x + 1, y + 1)));
				if (K > (bNear ? Prm.NearError : Prm.FarError)) { Cand[y * QX + x] = bNear ? 2 : 1; }
			}
		});

		// 2. Error de la malla gruesa en los puntos medios de los lados y en el centro de cada candidato.
		constexpr float Unset = 1e30f;
		TArray<float> HMid, VMid;
		HMid.Init(Unset, QX * (QY + 1));
		VMid.Init(Unset, (QX + 1) * QY);
		Parallel(QY + 1, [&](int32 y)
		{
			for (int32 x = 0; x < QX; ++x)
			{
				if (!((y < QY && Cand[y * QX + x]) || (y > 0 && Cand[(y - 1) * QX + x]))) { continue; }
				const int32 Src[2] = { y * NX + x, y * NX + x + 1 };
				uint8 M = 0;
				HMid[y * QX + x] = static_cast<float>(TB.HeightAtPoint(Origin + FVector2D((x + 0.5) * Spacing, y * Spacing), Src, 2, M));
			}
		});
		Parallel(QY, [&](int32 y)
		{
			for (int32 x = 0; x <= QX; ++x)
			{
				if (!((x < QX && Cand[y * QX + x]) || (x > 0 && Cand[y * QX + x - 1]))) { continue; }
				const int32 Src[2] = { y * NX + x, (y + 1) * NX + x };
				uint8 M = 0;
				VMid[y * (QX + 1) + x] = static_cast<float>(TB.HeightAtPoint(Origin + FVector2D(x * Spacing, (y + 0.5) * Spacing), Src, 2, M));
			}
		});
		Out.Kind.Init(FTerrainDetail::Plain, NQ);
		Parallel(QY, [&](int32 y)
		{
			for (int32 x = 0; x < QX; ++x)
			{
				const int32 q = y * QX + x;
				if (!Cand[q]) { continue; }
				const int32 A = y * NX + x;
				const int32 Src[4] = { A, A + 1, A + NX, A + NX + 1 };
				const double Ha = H[A], Hb = H[A + 1], Hcc = H[A + NX], Hd = H[A + NX + 1];
				uint8 M = 0;
				const double Mid = TB.HeightAtPoint(Origin + FVector2D((x + 0.5) * Spacing, (y + 0.5) * Spacing), Src, 4, M);
				double E = FMath::Abs(Mid - (SplitAlongAD(Ha, Hb, Hcc, Hd) ? 0.5 * (Ha + Hd) : 0.5 * (Hb + Hcc)));
				E = FMath::Max(E, FMath::Abs(HMid[y * QX + x] - 0.5 * (Ha + Hb)));
				E = FMath::Max(E, FMath::Abs(HMid[(y + 1) * QX + x] - 0.5 * (Hcc + Hd)));
				E = FMath::Max(E, FMath::Abs(VMid[y * (QX + 1) + x] - 0.5 * (Ha + Hcc)));
				E = FMath::Max(E, FMath::Abs(VMid[y * (QX + 1) + x + 1] - 0.5 * (Hb + Hd)));
				if (E > (Cand[q] == 2 ? Prm.NearError : Prm.FarError)) { Out.Kind[q] = FTerrainDetail::Fine; }
			}
		});
		HMid.Empty();
		VMid.Empty();
		Cand.Empty();

		// 3. Costuras: quads normales con algún vecino partido (en sus bits, qué lados).
		TArray<uint8> Seam;
		Seam.Init(0, NQ);
		Parallel(QY, [&](int32 y)
		{
			for (int32 x = 0; x < QX; ++x)
			{
				const int32 q = y * QX + x;
				if (Out.Kind[q] != FTerrainDetail::Plain) { continue; }
				uint8 Bits = 0;
				if (y > 0 && Out.Kind[q - QX] == FTerrainDetail::Fine) { Bits |= 1 << 0; }
				if (x + 1 < QX && Out.Kind[q + 1] == FTerrainDetail::Fine) { Bits |= 1 << 1; }
				if (y + 1 < QY && Out.Kind[q + QX] == FTerrainDetail::Fine) { Bits |= 1 << 2; }
				if (x > 0 && Out.Kind[q - 1] == FTerrainDetail::Fine) { Bits |= 1 << 3; }
				if (Bits) { Seam[q] = FTerrainDetail::SeamBit | Bits; }
			}
		});
		for (int32 q = 0; q < NQ; ++q) { if (Seam[q]) { Out.Kind[q] = Seam[q]; } }
		Seam.Empty();

		// 4. Sitio de cada quad con datos.
		Out.Offset.Init(INDEX_NONE, NQ);
		int32 Total = 0;
		for (int32 q = 0; q < NQ; ++q)
		{
			const uint8 K = Out.Kind[q];
			if (K == FTerrainDetail::Fine) { Out.Offset[q] = Total; Total += (N + 1) * (N + 1); }
			else if (FTerrainDetail::IsSeam(K)) { Out.Offset[q] = Total; Total += 1 + FTerrainDetail::NumSides * (N - 1); }
		}
		Out.Pool.Init(0.0f, Total);
		Out.PoolMask.Init(0, Total);

		// 5. Alturas. Un punto de un lado, solo con los dos vértices de ese lado: igual desde sus dos quads.
		auto EdgeH = [&](int32 ex, int32 ey, int32 k, float& OutH, uint8& OutM)
		{
			const int32 Src[2] = { ey * NX + ex, ey * NX + ex + 1 };
			OutH = static_cast<float>(TB.HeightAtPoint(Out.FinePos(ex * N + k, ey * N), Src, 2, OutM));
		};
		auto EdgeV = [&](int32 ex, int32 ey, int32 k, float& OutH, uint8& OutM)
		{
			const int32 Src[2] = { ey * NX + ex, (ey + 1) * NX + ex };
			OutH = static_cast<float>(TB.HeightAtPoint(Out.FinePos(ex * N, ey * N + k), Src, 2, OutM));
		};
		Parallel(QY, [&](int32 y)
		{
			for (int32 x = 0; x < QX; ++x)
			{
				const int32 q = y * QX + x;
				const uint8 K = Out.Kind[q];
				if (K == FTerrainDetail::Plain) { continue; }
				float* P = &Out.Pool[Out.Offset[q]];
				uint8* Pm = &Out.PoolMask[Out.Offset[q]];
				const int32 A = y * NX + x;
				const int32 Src[4] = { A, A + 1, A + NX, A + NX + 1 };
				if (K == FTerrainDetail::Fine)
				{
					const int32 W = N + 1;
					P[0] = H[A]; P[N] = H[A + 1]; P[N * W] = H[A + NX]; P[N * W + N] = H[A + NX + 1];
					for (int32 k = 1; k < N; ++k)
					{
						EdgeH(x, y, k, P[k], Pm[k]);
						EdgeH(x, y + 1, k, P[N * W + k], Pm[N * W + k]);
						EdgeV(x, y, k, P[k * W], Pm[k * W]);
						EdgeV(x + 1, y, k, P[k * W + N], Pm[k * W + N]);
					}
					for (int32 j = 1; j < N; ++j)
					{
						for (int32 i = 1; i < N; ++i)
						{
							P[j * W + i] = static_cast<float>(TB.HeightAtPoint(Out.FinePos(x * N + i, y * N + j), Src, 4, Pm[j * W + i]));
						}
					}
					continue;
				}
				P[0] = static_cast<float>(TB.HeightAtPoint(Origin + FVector2D((x + 0.5) * Spacing, (y + 0.5) * Spacing), Src, 4, Pm[0]));
				for (int32 k = 1; k < N; ++k)
				{
					if (K & (1 << 0)) { EdgeH(x, y, k, P[Out.SeamSlot(0, k)], Pm[Out.SeamSlot(0, k)]); }
					if (K & (1 << 1)) { EdgeV(x + 1, y, k, P[Out.SeamSlot(1, k)], Pm[Out.SeamSlot(1, k)]); }
					if (K & (1 << 2)) { EdgeH(x, y + 1, k, P[Out.SeamSlot(2, k)], Pm[Out.SeamSlot(2, k)]); }
					if (K & (1 << 3)) { EdgeV(x, y, k, P[Out.SeamSlot(3, k)], Pm[Out.SeamSlot(3, k)]); }
				}
			}
		});
	}
}
