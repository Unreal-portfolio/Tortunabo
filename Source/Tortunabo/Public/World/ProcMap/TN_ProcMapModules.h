#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"

/**
 * Partición del mapa en módulos IRREGULARES y siempre conexos.
 *
 * Técnica: Dijkstra multi-fuente sobre un campo de coste con ruido. Cada módulo
 * crece desde una semilla colocada en una rejilla con jitter (así el tamaño medio
 * sigue siendo ModuleSize x ModuleSize y la vecindad parecida a la del grid), y el
 * ruido del coste deforma las fronteras. Todas las celdas heredan la etiqueta de
 * su predecesora en el árbol de caminos mínimos, así que cada región es conexa por
 * construcción; una pasada final elimina conexiones solo-diagonales para que la
 * conectividad valga también en 4-vecindad.
 */

namespace TNProcMap
{
	namespace ModulesDetail
	{
		/** Transformada de distancia chamfer (1, √2) en unidades de celda, in-place. */
		inline void ChamferDistance(TArray<float>& D, int32 W, int32 H)
		{
			const float Diag = 1.41421356f;
			for (int32 y = 0; y < H; ++y)
			{
				for (int32 x = 0; x < W; ++x)
				{
					float& V = D[y * W + x];
					if (x > 0) { V = FMath::Min(V, D[y * W + x - 1] + 1.0f); }
					if (y > 0)
					{
						V = FMath::Min(V, D[(y - 1) * W + x] + 1.0f);
						if (x > 0) { V = FMath::Min(V, D[(y - 1) * W + x - 1] + Diag); }
						if (x < W - 1) { V = FMath::Min(V, D[(y - 1) * W + x + 1] + Diag); }
					}
				}
			}
			for (int32 y = H - 1; y >= 0; --y)
			{
				for (int32 x = W - 1; x >= 0; --x)
				{
					float& V = D[y * W + x];
					if (x < W - 1) { V = FMath::Min(V, D[y * W + x + 1] + 1.0f); }
					if (y < H - 1)
					{
						V = FMath::Min(V, D[(y + 1) * W + x] + 1.0f);
						if (x < W - 1) { V = FMath::Min(V, D[(y + 1) * W + x + 1] + Diag); }
						if (x > 0) { V = FMath::Min(V, D[(y + 1) * W + x - 1] + Diag); }
					}
				}
			}
		}
	}

	/** Rellena WorldSize, raster, Modules (semillas, centroides, vecinos) y los campos de distancia. */
	inline void BuildModules(FLayout& L, FRng Rng)
	{
		const FGenParams& P = L.Params;
		const int32 N = FMath::Max(1, P.GridSize);
		L.WorldSize = static_cast<double>(N) * P.ModuleSize;
		L.RasterW = FMath::Max(4, FMath::RoundToInt(L.WorldSize / P.CellSize));
		L.RasterH = L.RasterW;
		const int32 W = L.RasterW;
		const int32 H = L.RasterH;
		const int32 NumCells = W * H;
		const int32 NumModules = N * N;

		// ── Semillas en rejilla con jitter ──────────────────────────────────
		L.Modules.Reset();
		L.Modules.SetNum(NumModules);
		TArray<double> Weight;
		Weight.SetNum(NumModules);
		const double Jitter = 0.22 * P.ModuleSize;
		for (int32 j = 0; j < N; ++j)
		{
			for (int32 i = 0; i < N; ++i)
			{
				FModule& M = L.Modules[j * N + i];
				M.Id = j * N + i;
				M.GridCoord = FIntPoint(i, j);
				M.Seed = FVector2D((i + 0.5) * P.ModuleSize + Rng.Range(-Jitter, Jitter),
					(j + 0.5) * P.ModuleSize + Rng.Range(-Jitter, Jitter));
				Weight[M.Id] = Rng.Range(0.9, 1.1);
			}
		}

		// ── Campo de coste con ruido ────────────────────────────────────────
		const uint32 NoiseSeed = P.Seed ^ 0x51EDu;
		TArray<float> Cost;
		Cost.SetNum(NumCells);
		for (int32 y = 0; y < H; ++y)
		{
			for (int32 x = 0; x < W; ++x)
			{
				const FVector2D C = L.CellCenter(x, y);
				const double Big = 0.5 + 0.5 * Fbm2(NoiseSeed, C.X / 16000.0, C.Y / 16000.0, 3);
				const double Small = 0.5 + 0.5 * Noise2(NoiseSeed + 17u, C.X / 5000.0, C.Y / 5000.0);
				Cost[y * W + x] = static_cast<float>(1.0 + 2.2 * Big * Big + 0.7 * Small);
			}
		}

		// ── Dijkstra multi-fuente (8-vecindad) ──────────────────────────────
		L.ModuleOfCell.Init(static_cast<int16>(INDEX_NONE), NumCells);
		TArray<double> Dist;
		Dist.Init(1e300, NumCells);
		FMinHeap Heap;
		Heap.Items.Reserve(NumCells);
		for (const FModule& M : L.Modules)
		{
			const FIntPoint C = L.CellOf(M.Seed);
			const int32 Idx = L.CellIndex(C.X, C.Y);
			if (L.ModuleOfCell[Idx] != INDEX_NONE) { continue; }
			L.ModuleOfCell[Idx] = static_cast<int16>(M.Id);
			Dist[Idx] = 0.0;
			Heap.Push(0.0, Idx);
		}

		static const int32 DX8[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		static const int32 DY8[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		static const double Len8[8] = { 1.0, 1.0, 1.0, 1.0, 1.41421356, 1.41421356, 1.41421356, 1.41421356 };

		while (!Heap.IsEmpty())
		{
			const FHeapItem Item = Heap.Pop();
			const int32 Idx = Item.Value;
			if (Item.Key > Dist[Idx]) { continue; }
			const int32 X = Idx % W;
			const int32 Y = Idx / W;
			const int16 Label = L.ModuleOfCell[Idx];
			for (int32 k = 0; k < 8; ++k)
			{
				const int32 NX = X + DX8[k];
				const int32 NY = Y + DY8[k];
				if (!L.CellInside(NX, NY)) { continue; }
				const int32 NIdx = NY * W + NX;
				const double Edge = Len8[k] * 0.5 * (static_cast<double>(Cost[Idx]) + static_cast<double>(Cost[NIdx])) * Weight[Label];
				const double ND = Item.Key + Edge;
				if (ND < Dist[NIdx])
				{
					Dist[NIdx] = ND;
					L.ModuleOfCell[NIdx] = Label;
					Heap.Push(ND, NIdx);
				}
			}
		}

		// ── Limpieza: cada módulo debe ser conexo en 4-vecindad ──────────────
		static const int32 DX4[4] = { 1, -1, 0, 0 };
		static const int32 DY4[4] = { 0, 0, 1, -1 };
		for (int32 Pass = 0; Pass < 6; ++Pass)
		{
			TArray<int32> Comp;
			Comp.Init(INDEX_NONE, NumCells);
			TArray<int32> CompSize;
			TArray<int16> CompLabel;
			TArray<int32> Stack;
			for (int32 Start = 0; Start < NumCells; ++Start)
			{
				if (Comp[Start] != INDEX_NONE) { continue; }
				const int32 CompId = CompSize.Num();
				const int16 Label = L.ModuleOfCell[Start];
				CompSize.Add(0);
				CompLabel.Add(Label);
				Stack.Reset();
				Stack.Add(Start);
				Comp[Start] = CompId;
				while (Stack.Num() > 0)
				{
					const int32 C = Stack.Pop();
					++CompSize[CompId];
					const int32 X = C % W;
					const int32 Y = C / W;
					for (int32 k = 0; k < 4; ++k)
					{
						const int32 NX = X + DX4[k];
						const int32 NY = Y + DY4[k];
						if (!L.CellInside(NX, NY)) { continue; }
						const int32 NIdx = NY * W + NX;
						if (Comp[NIdx] == INDEX_NONE && L.ModuleOfCell[NIdx] == Label)
						{
							Comp[NIdx] = CompId;
							Stack.Add(NIdx);
						}
					}
				}
			}

			// Componente principal de cada módulo = la más grande.
			TArray<int32> MainComp;
			MainComp.Init(INDEX_NONE, NumModules);
			for (int32 c = 0; c < CompSize.Num(); ++c)
			{
				const int32 Lb = CompLabel[c];
				if (Lb < 0) { continue; }
				if (MainComp[Lb] == INDEX_NONE || CompSize[c] > CompSize[MainComp[Lb]]) { MainComp[Lb] = c; }
			}

			bool bChanged = false;
			for (int32 Idx = 0; Idx < NumCells; ++Idx)
			{
				const int32 Lb = L.ModuleOfCell[Idx];
				if (Lb >= 0 && MainComp[Lb] == Comp[Idx]) { continue; }
				// Fragmento suelto: pasa al vecino 4-conexo más frecuente.
				const int32 X = Idx % W;
				const int32 Y = Idx / W;
				int32 Best = INDEX_NONE;
				int32 BestCount = 0;
				int32 Counts[4] = { 0, 0, 0, 0 };
				int32 Labels[4] = { -1, -1, -1, -1 };
				for (int32 k = 0; k < 4; ++k)
				{
					const int32 NX = X + DX4[k];
					const int32 NY = Y + DY4[k];
					if (!L.CellInside(NX, NY)) { continue; }
					const int32 NL = L.ModuleOfCell[NY * W + NX];
					if (NL == Lb || NL < 0) { continue; }
					for (int32 q = 0; q < 4; ++q)
					{
						if (Labels[q] == NL) { ++Counts[q]; break; }
						if (Labels[q] == -1) { Labels[q] = NL; Counts[q] = 1; break; }
					}
				}
				for (int32 q = 0; q < 4; ++q)
				{
					if (Labels[q] >= 0 && Counts[q] > BestCount) { BestCount = Counts[q]; Best = Labels[q]; }
				}
				if (Best != INDEX_NONE)
				{
					L.ModuleOfCell[Idx] = static_cast<int16>(Best);
					bChanged = true;
				}
			}
			if (!bChanged) { break; }
		}

		// ── Estadísticas por módulo ─────────────────────────────────────────
		TArray<FVector2D> Sum;
		Sum.Init(FVector2D::ZeroVector, NumModules);
		for (int32 Idx = 0; Idx < NumCells; ++Idx)
		{
			const int32 Lb = L.ModuleOfCell[Idx];
			if (Lb < 0) { continue; }
			++L.Modules[Lb].CellCount;
			Sum[Lb] += L.CellCenter(Idx % W, Idx / W);
		}
		for (FModule& M : L.Modules)
		{
			M.Centroid = M.CellCount > 0 ? Sum[M.Id] / static_cast<double>(M.CellCount) : M.Seed;
		}

		// ── Adyacencia (frontera compartida en 4-vecindad) ──────────────────
		TArray<int32> Shared;
		Shared.Init(0, NumModules * NumModules);
		TArray<FVector2D> MidSum;
		MidSum.Init(FVector2D::ZeroVector, NumModules * NumModules);
		for (int32 y = 0; y < H; ++y)
		{
			for (int32 x = 0; x < W; ++x)
			{
				const int32 A = L.ModuleOfCell[y * W + x];
				if (x + 1 < W)
				{
					const int32 B = L.ModuleOfCell[y * W + x + 1];
					if (A != B && A >= 0 && B >= 0)
					{
						const FVector2D Mid = (L.CellCenter(x, y) + L.CellCenter(x + 1, y)) * 0.5;
						++Shared[A * NumModules + B]; ++Shared[B * NumModules + A];
						MidSum[A * NumModules + B] += Mid; MidSum[B * NumModules + A] += Mid;
					}
				}
				if (y + 1 < H)
				{
					const int32 B = L.ModuleOfCell[(y + 1) * W + x];
					if (A != B && A >= 0 && B >= 0)
					{
						const FVector2D Mid = (L.CellCenter(x, y) + L.CellCenter(x, y + 1)) * 0.5;
						++Shared[A * NumModules + B]; ++Shared[B * NumModules + A];
						MidSum[A * NumModules + B] += Mid; MidSum[B * NumModules + A] += Mid;
					}
				}
			}
		}
		// Frontera mínima para poder colocar un portal con holgura (~70 m).
		const int32 MinShared = FMath::Max(6, FMath::RoundToInt(7000.0 / P.CellSize));
		for (FModule& M : L.Modules)
		{
			for (int32 Other = 0; Other < NumModules; ++Other)
			{
				const int32 Count = Shared[M.Id * NumModules + Other];
				if (Other == M.Id || Count < MinShared) { continue; }
				M.Neighbors.Add(Other);
				M.SharedBorder.Add(Count);
				M.BorderMid.Add(MidSum[M.Id * NumModules + Other] / static_cast<double>(Count));
			}
		}

		// ── Campos de distancia ─────────────────────────────────────────────
		const float Big = 1e9f;
		L.ModuleDist.Init(Big, NumCells);
		for (int32 y = 0; y < H; ++y)
		{
			for (int32 x = 0; x < W; ++x)
			{
				const int32 Lb = L.ModuleOfCell[y * W + x];
				for (int32 k = 0; k < 4; ++k)
				{
					const int32 NX = x + DX4[k];
					const int32 NY = y + DY4[k];
					if (L.CellInside(NX, NY) && L.ModuleOfCell[NY * W + NX] != Lb)
					{
						L.ModuleDist[y * W + x] = 0.5f;
						break;
					}
				}
			}
		}
		ModulesDetail::ChamferDistance(L.ModuleDist, W, H);

		// Bordes del mapa: sur/este/oeste llevan muro; norte lleva la costa.
		const double SideExtra = FMath::Max(0.0, P.MapEdgeClearance - 3000.0);
		const double NorthExtra = FMath::Max(0.0, P.CoastInset + 5000.0 - 3000.0);
		L.BorderDist.SetNum(NumCells);
		for (int32 y = 0; y < H; ++y)
		{
			for (int32 x = 0; x < W; ++x)
			{
				const int32 Idx = y * W + x;
				L.ModuleDist[Idx] *= static_cast<float>(P.CellSize);
				const FVector2D C = L.CellCenter(x, y);
				const double EdgeSides = FMath::Min(FMath::Min(C.X, L.WorldSize - C.X), C.Y) - SideExtra;
				const double EdgeNorth = (L.WorldSize - C.Y) - NorthExtra;
				const double Edge = FMath::Min(EdgeSides, EdgeNorth);
				L.BorderDist[Idx] = static_cast<float>(FMath::Min(static_cast<double>(L.ModuleDist[Idx]), Edge));
			}
		}
	}

	/** Celdas del módulo From que tocan al módulo To, como puntos de frontera exactos. */
	inline void CollectBorderPoints(const FLayout& L, int32 From, int32 To, TArray<FVector2D>& OutPoints, TArray<FVector2D>& OutDirs)
	{
		static const int32 DX4[4] = { 1, -1, 0, 0 };
		static const int32 DY4[4] = { 0, 0, 1, -1 };
		OutPoints.Reset();
		OutDirs.Reset();
		for (int32 y = 0; y < L.RasterH; ++y)
		{
			for (int32 x = 0; x < L.RasterW; ++x)
			{
				if (L.ModuleOfCell[L.CellIndex(x, y)] != From) { continue; }
				for (int32 k = 0; k < 4; ++k)
				{
					const int32 NX = x + DX4[k];
					const int32 NY = y + DY4[k];
					if (L.CellInside(NX, NY) && L.ModuleOfCell[L.CellIndex(NX, NY)] == To)
					{
						OutPoints.Add((L.CellCenter(x, y) + L.CellCenter(NX, NY)) * 0.5);
						OutDirs.Add(FVector2D(static_cast<double>(DX4[k]), static_cast<double>(DY4[k])));
					}
				}
			}
		}
	}
}
