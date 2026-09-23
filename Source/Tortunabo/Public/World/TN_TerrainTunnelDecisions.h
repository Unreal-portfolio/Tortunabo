#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "World/TN_TerrainModuleAsset.h"
#include "World/TN_TerrainModuleDecisions.h"

/**
 * Túnel de roca como función PURA: una bóveda que se atraviesa por dentro, en vez de la
 * losa del arco (que solo se pasa por debajo). Sección en forma de C: por fuera, un
 * bloque de techo plano a DeckHeight (caminable desde la rampa) con los costados hundidos
 * en las paredes del pasillo; por dentro, un arco de medio punto rebajado que llega al
 * suelo por los dos lados. La sección se extruye a lo largo del pasillo (Width) con
 * irregularidad determinista y bocas de roca en los dos extremos.
 */
namespace TNTerrainTunnel
{
	struct FTunnelShape
	{
		/** Estaciones a lo largo del pasillo y puntos por perfil (exterior e interior). */
		int32 Stations = 12;
		int32 ProfilePoints = 20;
		/** Distancia del borde del hueco a la pared del pasillo (el hueco es algo más estrecho). */
		double InnerMargin = 1100.0;
		double MinInnerHalfWidth = 400.0;
		/** Cuánto se entierra la base bajo el suelo. */
		double BaseBuried = 150.0;
		/** Irregularidad de costados, bóveda y bocas (fracción). */
		double Roughness = 0.07;
	};

	/** Perfil exterior (N puntos): base derecha, sube, techo plano, baja a base izquierda. */
	inline TArray<FVector2D> OuterProfile(double HalfOuter, double Top, double Base, int32 N)
	{
		// Recorrido por longitud de arco del rectángulo abierto por abajo.
		const double Side = Top - Base;
		const double Total = 2.0 * Side + 2.0 * HalfOuter;
		TArray<FVector2D> Points;
		for (int32 K = 0; K < N; ++K)
		{
			const double S = Total * K / (N - 1);
			if (S <= Side) { Points.Add(FVector2D(HalfOuter, Base + S)); }
			else if (S <= Side + 2.0 * HalfOuter) { Points.Add(FVector2D(HalfOuter - (S - Side), Top)); }
			else { Points.Add(FVector2D(-HalfOuter, Top - (S - Side - 2.0 * HalfOuter))); }
		}
		return Points;
	}

	/** Perfil interior (N puntos): del pie derecho del hueco al izquierdo por la bóveda. */
	inline TArray<FVector2D> InnerProfile(double HalfInner, double Apex, double Base, int32 N)
	{
		TArray<FVector2D> Points;
		for (int32 K = 0; K < N; ++K)
		{
			const double Theta = PI * K / (N - 1);
			// PI es float: sin(PI) sale negativo por redondeo y Pow daría NaN.
			const double Rise = FMath::Pow(FMath::Max(FMath::Sin(Theta), 0.0), 0.7);
			Points.Add(FVector2D(HalfInner * FMath::Cos(Theta), Base + (Apex - Base) * Rise));
		}
		return Points;
	}

	/**
	 * Malla del túnel en espacio local del módulo. Bridge: Center, Yaw (eje de pared a
	 * pared), Length (de pared a pared, con los costados enterrados), Width (longitud a lo
	 * largo del pasillo), Thickness (grosor del techo) y DeckHeight (cota del techo).
	 * FloorHeight: cota del suelo del pasillo bajo el centro.
	 */
	inline TNGridTerrain::FTileMesh BuildTunnelMesh(const FTNTerrainModuleBridge& Bridge, double FloorHeight, int32 Seed,
		const TNTerrainModule::FModuleColors& Colors, const FTunnelShape& Shape = FTunnelShape())
	{
		TNGridTerrain::FTileMesh Mesh;
		const int32 S = FMath::Max(Shape.Stations, 2);
		const int32 N = FMath::Max(Shape.ProfilePoints, 6);
		const double HalfOuter = Bridge.Length * 0.5;
		const double HalfInner = FMath::Max(HalfOuter - Shape.InnerMargin, Shape.MinInnerHalfWidth);
		const double Base = FloorHeight - Shape.BaseBuried;
		const double Apex = Bridge.DeckHeight - Bridge.Thickness;
		if (Bridge.Length <= 0.f || Bridge.Width <= 0.f || Apex <= FloorHeight || HalfInner >= HalfOuter) { return Mesh; }

		const double YawRad = FMath::DegreesToRadians(static_cast<double>(Bridge.Yaw));
		const FVector2D Across(FMath::Cos(YawRad), FMath::Sin(YawRad));   // de pared a pared
		const FVector2D Along(-Across.Y, Across.X);                        // a lo largo del pasillo
		const TArray<FVector2D> Outer = OuterProfile(HalfOuter, Bridge.DeckHeight, Base, N);
		const TArray<FVector2D> Inner = InnerProfile(HalfInner, Apex, Base, N);

		FRandomStream Stream(Seed);
		const double PhaseA = Stream.FRandRange(0.0, 2.0 * PI);
		const double PhaseB = Stream.FRandRange(0.0, 2.0 * PI);
		auto Bump = [&](double U, double K)
		{
			return 1.0 + Shape.Roughness * (0.6 * FMath::Sin(U * 0.0019 + K * 0.9 + PhaseA) + 0.4 * FMath::Sin(U * 0.0047 - K * 1.7 + PhaseB));
		};

		// Vértices: por estación, N del perfil exterior y luego N del interior.
		TArray<FVector> AxisPoints;
		for (int32 St = 0; St <= S; ++St)
		{
			const double U = -Bridge.Width * 0.5 + Bridge.Width * St / S;
			const FVector2D Mid = FVector2D(Bridge.Center) + Along * U;
			AxisPoints.Add(FVector(Mid.X, Mid.Y, (Base + Apex) * 0.5));
			// Las bocas se abren un poco hacia fuera, como la entrada de una cueva.
			const double Portal = 1.0 + 0.12 * FMath::Pow(FMath::Abs(U) / (Bridge.Width * 0.5), 4.0);
			for (int32 K = 0; K < N; ++K)
			{
				const bool bTop = FMath::IsNearlyEqual(Outer[K].Y, Bridge.DeckHeight);
				const double Y = Outer[K].X * (bTop ? 1.0 : Bump(U, K));
				const FVector2D P = Mid + Across * Y;
				Mesh.Vertices.Add(FVector(P.X, P.Y, Outer[K].Y));   // techo liso: se camina por encima
			}
			for (int32 K = 0; K < N; ++K)
			{
				const double Scale = Portal * (2.0 - Bump(U, K + 31));
				const FVector2D P = Mid + Across * (Inner[K].X * Scale);
				Mesh.Vertices.Add(FVector(P.X, P.Y, Inner[K].Y));
			}
		}

		auto AddTriangle = [&](int32 A, int32 B, int32 C, const FVector& Outward)
		{
			const FVector Cross = FVector::CrossProduct(Mesh.Vertices[B] - Mesh.Vertices[A], Mesh.Vertices[C] - Mesh.Vertices[A]);
			if (FVector::DotProduct(Cross, Outward) > 0.0) { Swap(B, C); }
			Mesh.Triangles.Append({ A, B, C });
		};
		const int32 Ring = 2 * N;
		for (int32 St = 0; St < S; ++St)
		{
			for (int32 K = 0; K + 1 < N; ++K)
			{
				// Cara exterior: mira lejos del eje del hueco.
				const int32 A = St * Ring + K, B = A + 1, C = A + Ring, D = C + 1;
				const FVector MidO = (Mesh.Vertices[A] + Mesh.Vertices[D]) * 0.5;
				const FVector OutO = MidO - (AxisPoints[St] + AxisPoints[St + 1]) * 0.5;
				AddTriangle(A, B, C, OutO);
				AddTriangle(B, D, C, OutO);
				// Cara interior (la bóveda): mira hacia el hueco.
				const int32 E = St * Ring + N + K, F = E + 1, G = E + Ring, H = G + 1;
				const FVector MidI = (Mesh.Vertices[E] + Mesh.Vertices[H]) * 0.5;
				const FVector InI = (AxisPoints[St] + AxisPoints[St + 1]) * 0.5 - MidI;
				AddTriangle(E, F, G, InI);
				AddTriangle(F, H, G, InI);
			}
		}
		// Bocas: franja entre el perfil exterior y el interior en cada extremo.
		const FVector AlongDir(Along.X, Along.Y, 0.0);
		for (int32 End = 0; End < 2; ++End)
		{
			const int32 St = End == 0 ? 0 : S;
			const FVector Facing = End == 0 ? -AlongDir : AlongDir;
			for (int32 K = 0; K + 1 < N; ++K)
			{
				const int32 A = St * Ring + K, B = A + 1, C = St * Ring + N + K, D = C + 1;
				AddTriangle(A, B, C, Facing);
				AddTriangle(B, D, C, Facing);
			}
		}

		TNTerrainModule::FinishRockMesh(Mesh, Colors);
		return Mesh;
	}

	/** Cómo se cubre la bóveda de una cueva (BuildCaveMesh). */
	struct FCaveShape
	{
		/** Tierra sobre el terreno que rodea la cueva: la colina asoma un poco por encima. */
		double Cover = 150.0;
		/** Grosor mínimo de tierra sobre la clave de la bóveda. */
		double MinRoof = 250.0;
		/** Joroba extra en el centro, sobre el pasillo (fracción de la media anchura). */
		double Dome = 0.18;
		/** Faldón: hasta dónde se extiende la colina más allá de las paredes, y cuánto se entierra. */
		double Skirt = 1200.0;
		double SkirtBuried = 120.0;
	};

	/**
	 * Cueva que atraviesa una colina: misma bóveda interior que BuildTunnelMesh, pero por
	 * fuera, en vez de un bloque de techo plano, una manta que copia el terreno que la
	 * rodea (GroundAt, espacio local del módulo), sube en joroba sobre el pasillo y se
	 * entierra en un faldón a ambos lados. Con los colores del terreno se lee como parte
	 * del relieve y no como una pieza puesta encima. Las bocas quedan como la entrada de
	 * una cueva en la ladera.
	 */
	inline TNGridTerrain::FTileMesh BuildCaveMesh(const FTNTerrainModuleBridge& Bridge, double FloorHeight, int32 Seed,
		const TNTerrainModule::FModuleColors& Colors, TFunctionRef<double(const FVector2D&)> GroundAt,
		const FTunnelShape& Shape = FTunnelShape(), const FCaveShape& Cave = FCaveShape())
	{
		TNGridTerrain::FTileMesh Mesh;
		const int32 S = FMath::Max(Shape.Stations, 2);
		const int32 N = FMath::Max(Shape.ProfilePoints, 6);
		const double HalfOuter = Bridge.Length * 0.5;
		const double HalfInner = FMath::Max(HalfOuter - Shape.InnerMargin, Shape.MinInnerHalfWidth);
		const double Base = FloorHeight - Shape.BaseBuried;
		const double Apex = Bridge.DeckHeight - Bridge.Thickness;
		if (Bridge.Length <= 0.f || Bridge.Width <= 0.f || Apex <= FloorHeight || HalfInner >= HalfOuter) { return Mesh; }

		const double YawRad = FMath::DegreesToRadians(static_cast<double>(Bridge.Yaw));
		const FVector2D Across(FMath::Cos(YawRad), FMath::Sin(YawRad));
		const FVector2D Along(-Across.Y, Across.X);
		const TArray<FVector2D> Inner = InnerProfile(HalfInner, Apex, Base, N);
		const double Reach = HalfOuter + Cave.Skirt;

		FRandomStream Stream(Seed);
		const double PhaseA = Stream.FRandRange(0.0, 2.0 * PI);
		const double PhaseB = Stream.FRandRange(0.0, 2.0 * PI);
		auto Bump = [&](double U, double K)
		{
			return 1.0 + Shape.Roughness * (0.6 * FMath::Sin(U * 0.0019 + K * 0.9 + PhaseA) + 0.4 * FMath::Sin(U * 0.0047 - K * 1.7 + PhaseB));
		};

		TArray<FVector> AxisPoints;
		for (int32 St = 0; St <= S; ++St)
		{
			const double U = -Bridge.Width * 0.5 + Bridge.Width * St / S;
			const FVector2D Mid = FVector2D(Bridge.Center) + Along * U;
			AxisPoints.Add(FVector(Mid.X, Mid.Y, (Base + Apex) * 0.5));
			const double Portal = 1.0 + 0.12 * FMath::Pow(FMath::Abs(U) / (Bridge.Width * 0.5), 4.0);
			// Manta exterior: de +Reach a -Reach a través del pasillo.
			for (int32 K = 0; K < N; ++K)
			{
				const double T = 1.0 - 2.0 * K / (N - 1.0);        // 1 .. -1
				const double X = Reach * T;
				const FVector2D P = Mid + Across * X;
				const double Ground = GroundAt(P);
				double Z;
				if (K == 0 || K == N - 1)
				{
					Z = Ground - Cave.SkirtBuried;                  // el faldón se entierra
				}
				else
				{
					const double Inside = FMath::Clamp(1.0 - FMath::Abs(X) / HalfOuter, 0.0, 1.0);
					const double Dome = Cave.Dome * HalfOuter * FMath::Sin(Inside * 0.5 * PI) * Bump(U, K) * 0.6;
					const double Roof = (Apex + Cave.MinRoof) * Inside + Ground * (1.0 - Inside);
					Z = FMath::Max(Ground + Cave.Cover, Roof + Dome);
				}
				Mesh.Vertices.Add(FVector(P.X, P.Y, Z));
			}
			for (int32 K = 0; K < N; ++K)
			{
				const double Scale = Portal * (2.0 - Bump(U, K + 31));
				const FVector2D P = Mid + Across * (Inner[K].X * Scale);
				Mesh.Vertices.Add(FVector(P.X, P.Y, Inner[K].Y));
			}
		}

		auto AddTriangle = [&](int32 A, int32 B, int32 C, const FVector& Outward)
		{
			const FVector Cross = FVector::CrossProduct(Mesh.Vertices[B] - Mesh.Vertices[A], Mesh.Vertices[C] - Mesh.Vertices[A]);
			if (FVector::DotProduct(Cross, Outward) > 0.0) { Swap(B, C); }
			Mesh.Triangles.Append({ A, B, C });
		};
		const int32 Ring = 2 * N;
		for (int32 St = 0; St < S; ++St)
		{
			for (int32 K = 0; K + 1 < N; ++K)
			{
				const int32 A = St * Ring + K, B = A + 1, C = A + Ring, D = C + 1;
				AddTriangle(A, B, C, FVector::UpVector);
				AddTriangle(B, D, C, FVector::UpVector);
				const int32 E = St * Ring + N + K, F = E + 1, G = E + Ring, H = G + 1;
				const FVector MidI = (Mesh.Vertices[E] + Mesh.Vertices[H]) * 0.5;
				const FVector InI = (AxisPoints[St] + AxisPoints[St + 1]) * 0.5 - MidI;
				AddTriangle(E, F, G, InI);
				AddTriangle(F, H, G, InI);
			}
		}
		// Bocas: la ladera entre la manta y la bóveda en cada extremo.
		const FVector AlongDir(Along.X, Along.Y, 0.0);
		for (int32 End = 0; End < 2; ++End)
		{
			const int32 St = End == 0 ? 0 : S;
			const FVector Facing = End == 0 ? -AlongDir : AlongDir;
			for (int32 K = 0; K + 1 < N; ++K)
			{
				const int32 A = St * Ring + K, B = A + 1, C = St * Ring + N + K, D = C + 1;
				AddTriangle(A, B, C, Facing);
				AddTriangle(B, D, C, Facing);
			}
		}

		TNTerrainModule::FinishRockMesh(Mesh, Colors);
		return Mesh;
	}
}
