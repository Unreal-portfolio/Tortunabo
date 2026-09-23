#pragma once

#include "CoreMinimal.h"
#include "World/TN_GridPathDecisions.h"
#include "World/TN_GridTerrainDecisions.h"
#include "World/TN_TerrainModuleAsset.h"
#include "Math/RandomStream.h"

/**
 * Módulos de terreno como funciones PURAS: sin UWorld, sin actores, sin estado.
 * ATN_TerrainModuleTile y ATN_GridMapGenerator delegan aquí; los tests cubren
 * exactamente lo que se juega.
 *
 * Un módulo es un heightfield cuadrado (UTN_TerrainModuleAsset) que ocupa una celda del
 * grid. Todos comparten el mismo perfil de borde, de modo que casan entre sí con
 * cualquier rotación. Aquí se decide:
 *   - qué salidas tiene cada topología y cómo rotan (máscara de lados),
 *   - qué rotación necesita un módulo para ofrecer unas salidas dadas,
 *   - la altura interpolada en un punto del módulo y la malla de la celda.
 */
namespace TNTerrainModule
{
	constexpr int32 NumTopologies = 6;

	/** Bit del lado, según la numeración de TNGridLogic (Norte 0, Este 1, Sur 2, Oeste 3). */
	constexpr uint8 SideBit(int32 Side) { return static_cast<uint8>(1 << Side); }

	constexpr uint8 MaskNorth = SideBit(TNGridLogic::SideNorth);
	constexpr uint8 MaskEast  = SideBit(TNGridLogic::SideEast);
	constexpr uint8 MaskSouth = SideBit(TNGridLogic::SideSouth);
	constexpr uint8 MaskWest  = SideBit(TNGridLogic::SideWest);

	/** Salidas de la topología sin rotar. */
	inline uint8 ExitMask(ETNTerrainModuleTopology Topology)
	{
		switch (Topology)
		{
			case ETNTerrainModuleTopology::Straight:   return MaskSouth | MaskNorth;
			case ETNTerrainModuleTopology::CurveLeft:  return MaskSouth | MaskWest;
			case ETNTerrainModuleTopology::CurveRight: return MaskSouth | MaskEast;
			case ETNTerrainModuleTopology::TLeft:      return MaskSouth | MaskNorth | MaskWest;
			case ETNTerrainModuleTopology::TRight:     return MaskSouth | MaskNorth | MaskEast;
			default:                                   return MaskSouth | MaskNorth | MaskEast | MaskWest;
		}
	}

	/** Máscara tras girar el módulo YawSteps cuartos de vuelta (yaw +90° lleva Norte a Este). */
	inline uint8 RotateExitMask(uint8 Mask, int32 YawSteps)
	{
		const int32 Steps = ((YawSteps % TNGridLogic::NumSides) + TNGridLogic::NumSides) % TNGridLogic::NumSides;
		uint8 Rotated = 0;
		for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
		{
			if (Mask & SideBit(Side))
			{
				Rotated |= SideBit((Side + Steps) % TNGridLogic::NumSides);
			}
		}
		return Rotated;
	}

	/** Salidas que necesita una celda de camino ya clasificada (recta o giro, con su yaw). */
	inline uint8 RequiredExitMask(const TNGridLogic::FTNGridCell& Cell)
	{
		const uint8 Canonical = (Cell.Type == TNGridLogic::ETNGridTileType::Turn)
			? (MaskSouth | MaskEast)
			: (MaskSouth | MaskNorth);
		return RotateExitMask(Canonical, Cell.YawSteps);
	}

	/**
	 * Primer YawSteps (0..3) con el que la topología ofrece EXACTAMENTE las salidas pedidas,
	 * o INDEX_NONE. Con topologías simétricas (recta, cruz) hay varias soluciones; se toma
	 * la de menor giro, así que el resultado es determinista.
	 */
	inline int32 YawStepsForExits(ETNTerrainModuleTopology Topology, uint8 RequiredMask)
	{
		const uint8 Mask = ExitMask(Topology);
		for (int32 Steps = 0; Steps < TNGridLogic::NumSides; ++Steps)
		{
			if (RotateExitMask(Mask, Steps) == RequiredMask) { return Steps; }
		}
		return INDEX_NONE;
	}

	/**
	 * Todos los YawSteps (0..3) con los que la topología ofrece AL MENOS las salidas
	 * pedidas. Las salidas sobrantes se tapan con un muro de basura (ver
	 * TN_TerrainModuleWallDecisions.h), así que una T o una cruz sirven para una recta.
	 */
	inline TArray<int32> YawStepsCoveringExits(ETNTerrainModuleTopology Topology, uint8 RequiredMask)
	{
		TArray<int32> Result;
		const uint8 Mask = ExitMask(Topology);
		for (int32 Steps = 0; Steps < TNGridLogic::NumSides; ++Steps)
		{
			if ((RotateExitMask(Mask, Steps) & RequiredMask) == RequiredMask) { Result.Add(Steps); }
		}
		return Result;
	}

	/**
	 * Lados (en espacio LOCAL del módulo, sin rotar) cuya boca hay que tapar: las salidas
	 * que el módulo abre y que no conectan con la celda anterior ni con la siguiente del
	 * camino. Las bocas de entrada y salida del grid (Sur del inicio, Norte del final)
	 * también se tapan: no hay nada más allá del borde.
	 * @param ConnectedMask Lados en espacio de MUNDO que sí conectan con el camino.
	 */
	inline uint8 BlockedExitsLocal(ETNTerrainModuleTopology Topology, int32 YawSteps, uint8 ConnectedMask)
	{
		const uint8 OpenWorld = RotateExitMask(ExitMask(Topology), YawSteps);
		const uint8 BlockedWorld = OpenWorld & static_cast<uint8>(~ConnectedMask);
		return RotateExitMask(BlockedWorld, -YawSteps);
	}

	/** Lados de una celda del camino que conectan con sus vecinas de camino (espacio de mundo). */
	inline uint8 ConnectedExitMask(const TArray<TNGridLogic::FTNGridCell>& Cells, int32 Index)
	{
		uint8 Mask = 0;
		if (!Cells.IsValidIndex(Index)) { return Mask; }
		if (Index > 0)
		{
			Mask |= SideBit(TNGridLogic::SideTowards(Cells[Index].Coord, Cells[Index - 1].Coord));
		}
		if (Index + 1 < Cells.Num())
		{
			Mask |= SideBit(TNGridLogic::SideTowards(Cells[Index].Coord, Cells[Index + 1].Coord));
		}
		return Mask;
	}

	/** Tipo de borde que queda en el lado de mundo WorldSide al girar el módulo YawSteps
	 *  cuartos (misma convención que RotateExitMask: el lado local s pasa a s + YawSteps). */
	inline ETNTerrainEdge WorldSideEdge(const UTN_TerrainModuleAsset& Asset, int32 YawSteps, int32 WorldSide)
	{
		const int32 N = TNGridLogic::NumSides;
		return Asset.GetSideEdge(((WorldSide - YawSteps) % N + N) % N);
	}

	/** El módulo girado ofrece exactamente los tipos de borde pedidos, lado a lado
	 *  (Wanted: NumSides valores en el orden de TNGridLogic). */
	inline bool EdgesMatch(const UTN_TerrainModuleAsset& Asset, int32 YawSteps, const ETNTerrainEdge* Wanted)
	{
		for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
		{
			if (WorldSideEdge(Asset, YawSteps, Side) != Wanted[Side]) { return false; }
		}
		return true;
	}

	/** Vista sobre el heightfield de un módulo, con su tamaño en el mundo. */
	struct FModuleField
	{
		int32 Resolution = 0;
		double Size = 0.0;
		TArrayView<const uint16> Heights;
		double HeightScale = 0.25;
		int32 HeightZero = 32768;

		bool IsValid() const
		{
			return Resolution >= 2 && Size > 0.0 && Heights.Num() == Resolution * Resolution;
		}

		double HeightAt(int32 I, int32 J) const
		{
			const int32 Row = FMath::Clamp(I, 0, Resolution - 1);
			const int32 Col = FMath::Clamp(J, 0, Resolution - 1);
			return (static_cast<double>(Heights[Row * Resolution + Col]) - HeightZero) * HeightScale;
		}

		/** Separación entre vértices, en uu. */
		double Step() const { return Size / (Resolution - 1); }
	};

	inline FModuleField MakeField(const UTN_TerrainModuleAsset& Asset, double Size)
	{
		FModuleField Field;
		Field.Resolution = Asset.Resolution;
		Field.Size = Size;
		Field.Heights = Asset.Heights;
		Field.HeightScale = Asset.HeightScale;
		Field.HeightZero = Asset.HeightZero;
		return Field;
	}

	/** Altura interpolada (bilineal) en un punto local al módulo, con origen en su centro. */
	inline double SampleHeight(const FModuleField& Field, const FVector2D& Local)
	{
		const double Step = Field.Step();
		const double FI = FMath::Clamp(Local.X / Step + (Field.Resolution - 1) * 0.5, 0.0, Field.Resolution - 1.0);
		const double FJ = FMath::Clamp(Local.Y / Step + (Field.Resolution - 1) * 0.5, 0.0, Field.Resolution - 1.0);
		const int32 I0 = FMath::Min(FMath::FloorToInt32(FI), Field.Resolution - 2);
		const int32 J0 = FMath::Min(FMath::FloorToInt32(FJ), Field.Resolution - 2);
		const double AI = FI - I0;
		const double AJ = FJ - J0;
		return FMath::Lerp(
			FMath::Lerp(Field.HeightAt(I0, J0), Field.HeightAt(I0 + 1, J0), AI),
			FMath::Lerp(Field.HeightAt(I0, J0 + 1), Field.HeightAt(I0 + 1, J0 + 1), AI),
			AJ);
	}

	/** Alturas crudas de un lado, recorrido en el sentido en que crece el otro eje. */
	inline TArray<uint16> EdgeHeights(const FModuleField& Field, int32 Side)
	{
		TArray<uint16> Edge;
		Edge.Reserve(Field.Resolution);
		const int32 Last = Field.Resolution - 1;
		for (int32 K = 0; K < Field.Resolution; ++K)
		{
			int32 I = 0;
			int32 J = 0;
			switch (Side)
			{
				case TNGridLogic::SideNorth: I = Last; J = K; break;
				case TNGridLogic::SideSouth: I = 0;    J = K; break;
				case TNGridLogic::SideEast:  I = K;    J = Last; break;
				default:                     I = K;    J = 0; break;
			}
			Edge.Add(Field.Heights[I * Field.Resolution + J]);
		}
		return Edge;
	}

	/** Los cuatro lados son idénticos entre sí y simétricos respecto a su punto medio. */
	inline bool HasCanonicalBorder(const FModuleField& Field)
	{
		if (!Field.IsValid()) { return false; }
		const TArray<uint16> North = EdgeHeights(Field, TNGridLogic::SideNorth);
		for (int32 K = 0; K < North.Num(); ++K)
		{
			if (North[K] != North[North.Num() - 1 - K]) { return false; }
		}
		for (int32 Side = 1; Side < TNGridLogic::NumSides; ++Side)
		{
			if (EdgeHeights(Field, Side) != North) { return false; }
		}
		return true;
	}

	/** Dos módulos casan por cualquier lado: mismo borde canónico en ambos. */
	inline bool BordersMatch(const FModuleField& A, const FModuleField& B)
	{
		return HasCanonicalBorder(A) && HasCanonicalBorder(B)
			&& EdgeHeights(A, TNGridLogic::SideNorth) == EdgeHeights(B, TNGridLogic::SideNorth);
	}

	/** Colores de vértice del módulo, en espacio lineal. */
	struct FModuleColors
	{
		FLinearColor Floor = FLinearColor(0.62f, 0.44f, 0.21f);
		FLinearColor Cliff = FLinearColor(0.075f, 0.065f, 0.06f);
		FLinearColor High = FLinearColor(0.045f, 0.04f, 0.035f);
		FLinearColor Wet = FLinearColor(0.09f, 0.065f, 0.035f);
		/** Cota del agua en uu; por debajo el suelo se pinta húmedo. */
		double WaterLevel = -400.0;
		/** Por encima de esta cota el suelo deja de ser camino y pasa a montón. */
		double HighGroundStart = 400.0;
		double HighGroundEnd = 900.0;
	};

	inline FLinearColor SampleModuleColor(const FModuleColors& Colors, double Height, const FVector& Normal)
	{
		const double Cliff = TNGridTerrain::SmoothStep(0.20, 0.55, 1.0 - Normal.Z);
		const double High = TNGridTerrain::SmoothStep(Colors.HighGroundStart, Colors.HighGroundEnd, Height);
		const double Wet = TNGridTerrain::SmoothStep(Colors.WaterLevel + 60.0, Colors.WaterLevel, Height);

		FLinearColor Color = FMath::Lerp(Colors.Floor, Colors.High, static_cast<float>(High));
		Color = FMath::Lerp(Color, Colors.Cliff, static_cast<float>(Cliff * 0.6));
		Color = FMath::Lerp(Color, Colors.Wet, static_cast<float>(Wet));
		Color.A = 1.f;
		return Color;
	}

	/** Normales suaves (media de las caras que comparten vértice) y color de vértice de una
	 *  malla de roca con triángulos en orden horario, la convención de Unreal. */
	inline void FinishRockMesh(TNGridTerrain::FTileMesh& Mesh, const FModuleColors& Colors)
	{
		Mesh.Normals.Init(FVector::ZeroVector, Mesh.Vertices.Num());
		for (int32 T = 0; T + 2 < Mesh.Triangles.Num(); T += 3)
		{
			const int32 A = Mesh.Triangles[T];
			const int32 B = Mesh.Triangles[T + 1];
			const int32 C = Mesh.Triangles[T + 2];
			// Orden horario: la normal hacia la cara visible es (C-A)x(B-A).
			const FVector Face = FVector::CrossProduct(Mesh.Vertices[C] - Mesh.Vertices[A], Mesh.Vertices[B] - Mesh.Vertices[A]);
			Mesh.Normals[A] += Face;
			Mesh.Normals[B] += Face;
			Mesh.Normals[C] += Face;
		}
		Mesh.Colors.Reset(Mesh.Vertices.Num());
		for (int32 V = 0; V < Mesh.Vertices.Num(); ++V)
		{
			Mesh.Normals[V] = Mesh.Normals[V].GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
			Mesh.Colors.Add(SampleModuleColor(Colors, Mesh.Vertices[V].Z, Mesh.Normals[V]));
		}
	}

	/** Parámetros de forma del arco de roca; los valores por defecto son los de juego. */
	struct FArchShape
	{
		/** Estaciones a lo largo del arco y puntos por sección. */
		int32 Stations = 24;
		int32 RingPoints = 16;
		/** Exponente de la superelipse de la sección: 4 = canto redondeado y techo casi plano. */
		double SectionPower = 4.0;
		/** Cuánto baja la panza en los apoyos respecto al centro (uu): el arco nace de la pared. */
		double RootDrop = 700.0;
		/** Ensanche de la sección en los apoyos (fracción del ancho). */
		double RootWiden = 0.35;
		/** Irregularidad de los costados y la panza (fracción del radio). */
		double Roughness = 0.08;
	};

	/**
	 * Arco natural de roca a partir de un "puente" del módulo, en espacio local del módulo.
	 * Techo plano a DeckHeight (caminable cuando la ruta alta llega a él), panza en arco que
	 * deja paso por debajo y que en los extremos baja RootDrop para enraizar en la pared.
	 * Sección de superelipse con irregularidad determinista por Seed. Malla cerrada, con
	 * caras hacia fuera (horario visto desde fuera, convención de Unreal).
	 */
	inline TNGridTerrain::FTileMesh BuildArchMesh(const FTNTerrainModuleBridge& Bridge, int32 Seed,
		const FModuleColors& Colors, const FArchShape& Shape = FArchShape())
	{
		TNGridTerrain::FTileMesh Mesh;
		const int32 N = FMath::Max(Shape.Stations, 2);
		const int32 M = FMath::Max(Shape.RingPoints, 4);
		if (Bridge.Length <= 0.f || Bridge.Width <= 0.f || Bridge.Thickness <= 0.f) { return Mesh; }

		const double YawRad = FMath::DegreesToRadians(static_cast<double>(Bridge.Yaw));
		const FVector2D Axis(FMath::Cos(YawRad), FMath::Sin(YawRad));
		const FVector2D Across(-Axis.Y, Axis.X);
		const FVector2D Center(Bridge.Center);
		const double HalfLength = Bridge.Length * 0.5;
		// Fases de ruido fijas por semilla: misma semilla, misma roca en todas las máquinas.
		FRandomStream Stream(Seed);
		const double PhaseA = Stream.FRandRange(0.0, 2.0 * PI);
		const double PhaseB = Stream.FRandRange(0.0, 2.0 * PI);
		auto Rough = [&](double U, double Theta)
		{
			return Shape.Roughness * (0.6 * FMath::Sin(U * 0.0021 + 3.0 * Theta + PhaseA)
				+ 0.4 * FMath::Sin(U * 0.0053 - 5.0 * Theta + PhaseB));
		};

		TArray<FVector> Centers;
		for (int32 S = 0; S <= N; ++S)
		{
			const double U = -HalfLength + Bridge.Length * S / N;
			const double A = FMath::Abs(U) / HalfLength;
			const double HalfWidth = Bridge.Width * 0.5 * (1.0 + Shape.RootWiden * A * A);
			const double Bottom = Bridge.DeckHeight - Bridge.Thickness - Shape.RootDrop * A * A * A;
			const double HalfHeight = (Bridge.DeckHeight - Bottom) * 0.5;
			const double MidZ = Bottom + HalfHeight;
			const FVector2D Mid2D = Center + Axis * U;
			Centers.Add(FVector(Mid2D.X, Mid2D.Y, MidZ));

			for (int32 R = 0; R < M; ++R)
			{
				const double Theta = 2.0 * PI * R / M;
				const double C = FMath::Cos(Theta);
				const double Sn = FMath::Sin(Theta);
				const double Exp = 2.0 / Shape.SectionPower;
				double Y = HalfWidth * FMath::Sign(C) * FMath::Pow(FMath::Abs(C), Exp);
				double Z = HalfHeight * FMath::Sign(Sn) * FMath::Pow(FMath::Abs(Sn), Exp);
				// El techo se queda liso para poder caminar; costados y panza, irregulares.
				if (Sn < 0.7)
				{
					const double Bump = 1.0 + Rough(U, Theta);
					Y *= Bump;
					Z = Sn < 0.0 ? Z * Bump : Z;
				}
				const FVector2D P = Mid2D + Across * Y;
				Mesh.Vertices.Add(FVector(P.X, P.Y, MidZ + Z));
			}
		}

		// Tapas en los extremos (quedan enterradas en la pared, pero cierran la colisión).
		const int32 CapStart = Mesh.Vertices.Add(Centers[0]);
		const int32 CapEnd = Mesh.Vertices.Add(Centers.Last());

		// Cada triángulo se orienta para que su cara visible mire hacia fuera de la línea
		// central: en Unreal (B-A)x(C-A) apunta al lado contrario de la cara visible.
		auto AddTriangle = [&](int32 A, int32 B, int32 C, const FVector& Outward)
		{
			const FVector Cross = FVector::CrossProduct(Mesh.Vertices[B] - Mesh.Vertices[A], Mesh.Vertices[C] - Mesh.Vertices[A]);
			if (FVector::DotProduct(Cross, Outward) > 0.0) { Swap(B, C); }
			Mesh.Triangles.Append({ A, B, C });
		};

		for (int32 S = 0; S < N; ++S)
		{
			for (int32 R = 0; R < M; ++R)
			{
				const int32 A = S * M + R;
				const int32 B = S * M + (R + 1) % M;
				const int32 C = (S + 1) * M + R;
				const int32 D = (S + 1) * M + (R + 1) % M;
				const FVector QuadMid = (Mesh.Vertices[A] + Mesh.Vertices[D]) * 0.5;
				const FVector Outward = QuadMid - (Centers[S] + Centers[S + 1]) * 0.5;
				AddTriangle(A, B, C, Outward);
				AddTriangle(B, D, C, Outward);
			}
		}
		const FVector AxisDir(Axis.X, Axis.Y, 0.0);
		for (int32 R = 0; R < M; ++R)
		{
			AddTriangle(CapStart, R, (R + 1) % M, -AxisDir);
			AddTriangle(CapEnd, N * M + R, N * M + (R + 1) % M, AxisDir);
		}

		FinishRockMesh(Mesh, Colors);
		return Mesh;
	}

	/** Parámetros de forma del monolito; los valores por defecto son los de juego. */
	struct FMonolithShape
	{
		int32 Stations = 12;
		int32 RingPoints = 10;
		/** Radio de la cima respecto al del pie. */
		double TopRadiusRatio = 0.55;
		/** Irregularidad del contorno (fracción del radio). */
		double Roughness = 0.14;
	};

	/**
	 * Pilar de roca vertical (monolito) en espacio local del módulo: anillos apilados que
	 * se estrechan hacia la cima, con el eje inclinado Lean grados y contorno irregular
	 * determinista por Seed. Malla cerrada con tapas, caras hacia fuera.
	 */
	inline TNGridTerrain::FTileMesh BuildMonolithMesh(const FTNTerrainModuleMonolith& Monolith, int32 Seed,
		const FModuleColors& Colors, const FMonolithShape& Shape = FMonolithShape())
	{
		TNGridTerrain::FTileMesh Mesh;
		const int32 N = FMath::Max(Shape.Stations, 2);
		const int32 M = FMath::Max(Shape.RingPoints, 4);
		if (Monolith.Radius <= 0.f || Monolith.Height <= 0.f) { return Mesh; }

		const double YawRad = FMath::DegreesToRadians(static_cast<double>(Monolith.Yaw));
		const FVector2D LeanDir(FMath::Cos(YawRad), FMath::Sin(YawRad));
		const double LeanSlope = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(Monolith.Lean), 0.0, 30.0)));
		FRandomStream Stream(Seed);
		const double PhaseA = Stream.FRandRange(0.0, 2.0 * PI);
		const double PhaseB = Stream.FRandRange(0.0, 2.0 * PI);

		TArray<FVector> Centers;
		for (int32 S = 0; S <= N; ++S)
		{
			const double T = static_cast<double>(S) / N;
			const double Z = Monolith.BaseHeight + Monolith.Height * T;
			const FVector2D Mid = FVector2D(Monolith.Center) + LeanDir * (LeanSlope * Monolith.Height * T);
			const double Radius = Monolith.Radius * FMath::Lerp(1.0, Shape.TopRadiusRatio, T);
			Centers.Add(FVector(Mid.X, Mid.Y, Z));
			for (int32 R = 0; R < M; ++R)
			{
				const double Theta = 2.0 * PI * R / M;
				const double Bump = 1.0 + Shape.Roughness * (0.6 * FMath::Sin(3.0 * Theta + T * 4.0 + PhaseA)
					+ 0.4 * FMath::Sin(5.0 * Theta - T * 9.0 + PhaseB));
				Mesh.Vertices.Add(FVector(Mid.X + Radius * Bump * FMath::Cos(Theta),
					Mid.Y + Radius * Bump * FMath::Sin(Theta), Z));
			}
		}
		// Cima ligeramente abombada; el pie queda enterrado.
		const int32 CapBottom = Mesh.Vertices.Add(Centers[0]);
		const int32 CapTop = Mesh.Vertices.Add(Centers.Last() + FVector(0.0, 0.0, Monolith.Radius * 0.25));

		auto AddTriangle = [&](int32 A, int32 B, int32 C, const FVector& Outward)
		{
			const FVector Cross = FVector::CrossProduct(Mesh.Vertices[B] - Mesh.Vertices[A], Mesh.Vertices[C] - Mesh.Vertices[A]);
			if (FVector::DotProduct(Cross, Outward) > 0.0) { Swap(B, C); }
			Mesh.Triangles.Append({ A, B, C });
		};
		for (int32 S = 0; S < N; ++S)
		{
			for (int32 R = 0; R < M; ++R)
			{
				const int32 A = S * M + R;
				const int32 B = S * M + (R + 1) % M;
				const int32 C = (S + 1) * M + R;
				const int32 D = (S + 1) * M + (R + 1) % M;
				const FVector Outward = (Mesh.Vertices[A] + Mesh.Vertices[D]) * 0.5 - (Centers[S] + Centers[S + 1]) * 0.5;
				AddTriangle(A, B, C, Outward);
				AddTriangle(B, D, C, Outward);
			}
		}
		for (int32 R = 0; R < M; ++R)
		{
			AddTriangle(CapBottom, R, (R + 1) % M, -FVector::UpVector);
			AddTriangle(CapTop, N * M + R, N * M + (R + 1) % M, FVector::UpVector);
		}

		FinishRockMesh(Mesh, Colors);
		return Mesh;
	}

	/**
	 * Malla del módulo en su espacio local (origen en el centro, cara hacia +Z). Un vértice
	 * por muestra del heightfield; normales por diferencias centrales de las alturas, de
	 * modo que dos módulos vecinos, al compartir borde, comparten también posiciones.
	 */
	inline TNGridTerrain::FTileMesh BuildModuleMesh(const FModuleField& Field, const FModuleColors& Colors,
		const FModuleColors* BlendColors = nullptr, TArrayView<const uint16> BiomeMask = TArrayView<const uint16>())
	{
		// Módulo mixto: el byte alto de la máscara es el peso de la segunda paleta.
		const bool bBlend = BlendColors && BiomeMask.Num() == Field.Resolution * Field.Resolution;
		TNGridTerrain::FTileMesh Mesh;
		if (!Field.IsValid()) { return Mesh; }

		const int32 R = Field.Resolution;
		const double Step = Field.Step();
		const double Half = Field.Size * 0.5;
		Mesh.Vertices.Reserve(R * R);
		Mesh.Normals.Reserve(R * R);
		Mesh.Colors.Reserve(R * R);

		for (int32 I = 0; I < R; ++I)
		{
			for (int32 J = 0; J < R; ++J)
			{
				const double Height = Field.HeightAt(I, J);
				// En el borde la diferencia central se acorta a un lado: el vecino no está.
				const double SlopeX = (Field.HeightAt(I + 1, J) - Field.HeightAt(I - 1, J))
					/ (Step * ((I > 0 && I < R - 1) ? 2.0 : 1.0));
				const double SlopeY = (Field.HeightAt(I, J + 1) - Field.HeightAt(I, J - 1))
					/ (Step * ((J > 0 && J < R - 1) ? 2.0 : 1.0));
				const FVector Normal = FVector(-SlopeX, -SlopeY, 1.0).GetSafeNormal();

				Mesh.Vertices.Add(FVector(I * Step - Half, J * Step - Half, Height));
				Mesh.Normals.Add(Normal);
				FLinearColor Color = SampleModuleColor(Colors, Height, Normal);
				if (bBlend)
				{
					const float Weight = static_cast<float>(BiomeMask[I * R + J] >> 8) / 255.f;
					Color = FMath::Lerp(Color, SampleModuleColor(*BlendColors, Height, Normal), Weight);
				}
				Mesh.Colors.Add(Color);
			}
		}

		Mesh.Triangles = TNGridTerrain::BuildGridTriangles(R);
		return Mesh;
	}
}
