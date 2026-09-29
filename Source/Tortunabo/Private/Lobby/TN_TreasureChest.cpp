#include "Lobby/TN_TreasureChest.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"
#include "TN_CastleKit.h"

namespace TNTreasureChestDetail
{
	using FBuffers = TNProcMesh::FTNProcMeshBuffers;

	// ── Medidas (cm, locales del cofre: origen en el suelo en el centro, +X es el frente, Z arriba) ──
	/** Medio fondo, medio ancho y alto de la caja (la tapa va encima). */
	constexpr double ChestHalfD = 50.0;
	constexpr double ChestHalfW = 78.0;
	constexpr double ChestBodyH = 64.0;
	/** Grueso de las paredes. */
	constexpr double ChestWallT = 7.0;
	/** Tapa de medio cañón: radio (vuela 3 cm por delante y por detrás), medio largo (vuela 3 por los lados) y grueso. */
	constexpr double LidRadius = ChestHalfD + 3.0;
	constexpr double LidSide = ChestHalfW + 3.0;
	constexpr double LidThick = 6.0;
	constexpr int32 LidSegments = 8;
	/** Flejes de hierro: a qué distancia del centro van y su medio ancho. */
	constexpr double StrapY = ChestHalfW * 0.56;
	constexpr double StrapHalf = 6.0;
	/** Alto total con la tapa cerrada. */
	constexpr double ChestTopZ = ChestBodyH + LidRadius;
	/** Cama de monedas de dentro. */
	constexpr double CoinBedZ = ChestBodyH - 18.0;
	/** Radio de la huella redonda (aviso, escaneo y chispitas de «aquí se puede rebuscar»). */
	constexpr float FootprintR = 78.f;

	// ── Tapa ──
	/** Mientras se rebusca se entreabre de LidPryStartDeg a LidPryEndDeg (grados) según el progreso. */
	constexpr float LidPryStartDeg = 18.f;
	constexpr float LidPryEndDeg = 55.f;
	/** Al salir el objeto salta a LidPopDeg y aguanta LidPopSeconds antes de cerrarse. */
	constexpr float LidPopDeg = 108.f;
	constexpr double LidPopSeconds = 1.2;
	constexpr float LidMaxDeg = 122.f;
	/** Muelle de la tapa (rebota un poco). */
	constexpr float LidStiffness = 160.f;
	constexpr float LidDamping = 13.f;
	/** Velocidad (grados/s) a partir de la que el golpe contra la caja suena. */
	constexpr float LidThumpSpeed = 70.f;
	/** Luz de dentro con la tapa bien abierta (lúmenes). */
	constexpr float GlowLumens = 1600.f;

	// ── Objeto que sale ──
	/** Cae en la tarima: entre estos cm por delante del frente y hasta LandSide a cada lado. */
	constexpr float LandFrontMin = 30.f;
	constexpr float LandFrontMax = 58.f;
	constexpr float LandSide = 45.f;

	/** Montones de monedas de dentro: centro, radio de la base y alto sobre la cama. */
	struct FCoinPile
	{
		double X;
		double Y;
		double Radius;
		double Height;
	};
	const FCoinPile CoinPiles[] = { { -8.0, -22.0, 30.0, 14.0 }, { 10.0, 26.0, 26.0, 11.0 }, { 20.0, -2.0, 16.0, 7.0 } };

	/** Alto de los montones sobre la cama de monedas en (X, Y) (conos). */
	double PileHeightAt(double X, double Y)
	{
		double Best = 0.0;
		for (const FCoinPile& Pile : CoinPiles)
		{
			const double Dist = FVector2D::Distance(FVector2D(X, Y), FVector2D(Pile.X, Pile.Y));
			Best = FMath::Max(Best, Pile.Height * (1.0 - Dist / Pile.Radius));
		}
		return Best;
	}

	/** Caja entre dos esquinas dadas en cualquier orden. */
	void AddSlab(FBuffers& B, double X0, double X1, double Y0, double Y1, double Z0, double Z1, const FLinearColor& Color)
	{
		TNCastleKit::AddAxisBox(B, FVector(FMath::Min(X0, X1), FMath::Min(Y0, Y1), FMath::Min(Z0, Z1)),
			FVector(FMath::Max(X0, X1), FMath::Max(Y0, Y1), FMath::Max(Z0, Z1)), Color);
	}

	/** Remache: cabeza baja de seis lados en P, que sale hacia Out. */
	void AddRivet(FBuffers& B, const FVector& P, const FVector& Out, const FLinearColor& Color)
	{
		TNProcMesh::TNProcAddCylinder(B, P - Out * 0.5, P + Out * 2.2, 2.7, 2.0, 6, Color);
	}

	/** Gema tallada (pabellón hacia abajo y corona con tabla) con el centro de la cintura en C. */
	void AddGem(FBuffers& B, const FVector& C, double Size, const FLinearColor& Color)
	{
		const FVector Up(0.0, 0.0, 1.0);
		TNProcMesh::TNProcAddCylinder(B, C - Up * (Size * 0.9), C, Size * 0.08, Size, 6, Color * 0.8f);
		TNProcMesh::TNProcAddCylinder(B, C, C + Up * (Size * 0.45), Size, Size * 0.55, 6, Color);
	}

	/** Punto de la tapa (locales de la tapa: origen en la bisagra) a Rad del eje del medio cañón, en el ángulo Theta y a Y. */
	FVector LidArcPoint(double Rad, double Theta, double Y)
	{
		return FVector(ChestHalfD + Rad * FMath::Cos(Theta), Y, Rad * FMath::Sin(Theta));
	}

	/**
	 * Caja del cofre: paredes de madera oscura por dentro y tablones por fuera, zócalo, borde dorado, cantoneras,
	 * flejes de hierro con remaches, cerradura, asas y, dentro, la cama de monedas con montones, monedas sueltas, gemas
	 * y una copa.
	 */
	void BuildChestBody(FBuffers& B)
	{
		const FVector Up(0.0, 0.0, 1.0);
		const FLinearColor WoodA = TNCastleKit::Pal(0xB5794A);
		const FLinearColor WoodB = TNCastleKit::Pal(0x9E6639);
		const FLinearColor WoodDark = TNCastleKit::Pal(0x6E4526);
		const FLinearColor WoodIn = TNCastleKit::Pal(0x4A2E1A);
		const FLinearColor Iron = TNCastleKit::Pal(0x3B3F4A);
		const FLinearColor Gold = TNCastleKit::Pal(0xFFCB3D);
		const FLinearColor GoldDeep = TNCastleKit::Pal(0xE0A12A);
		const FLinearColor Keyhole = TNCastleKit::Pal(0x24160C);
		const FLinearColor CoinA = TNCastleKit::Pal(0xFFD447);
		const FLinearColor CoinB = TNCastleKit::Pal(0xF2B632);
		constexpr double DIn = ChestHalfD - ChestWallT;
		constexpr double WIn = ChestHalfW - ChestWallT;

		// Paredes (por dentro, madera oscura; por fuera las tapan los tablones) y zócalo.
		AddSlab(B, DIn, ChestHalfD, -ChestHalfW, ChestHalfW, 0.0, ChestBodyH, WoodIn);
		AddSlab(B, -ChestHalfD, -DIn, -ChestHalfW, ChestHalfW, 0.0, ChestBodyH, WoodIn);
		AddSlab(B, -DIn, DIn, WIn, ChestHalfW, 0.0, ChestBodyH, WoodIn);
		AddSlab(B, -DIn, DIn, -ChestHalfW, -WIn, 0.0, ChestBodyH, WoodIn);
		AddSlab(B, -ChestHalfD - 4.0, ChestHalfD + 4.0, -ChestHalfW - 4.0, ChestHalfW + 4.0, 0.0, 8.0, WoodDark);

		// Tablones por fuera: tres hileras con una junta oscura entre ellas, en las cuatro caras.
		constexpr int32 Rows = 3;
		const double RowH = (ChestBodyH - 15.0) / Rows;
		for (int32 r = 0; r < Rows; ++r)
		{
			const double Z0 = r == 0 ? 7.0 : 8.0 + RowH * r + 0.8;
			const double Z1 = r == Rows - 1 ? ChestBodyH - 6.0 : 8.0 + RowH * (r + 1) - 0.8;
			AddSlab(B, ChestHalfD - 1.0, ChestHalfD + 2.0, -ChestHalfW + 1.0, ChestHalfW - 1.0, Z0, Z1, (r % 2) ? WoodA : WoodB);
			AddSlab(B, -ChestHalfD - 2.0, -ChestHalfD + 1.0, -ChestHalfW + 1.0, ChestHalfW - 1.0, Z0, Z1, (r % 2) ? WoodB : WoodA);
			AddSlab(B, -ChestHalfD + 1.0, ChestHalfD - 1.0, ChestHalfW - 1.0, ChestHalfW + 2.0, Z0, Z1, (r % 2) ? WoodB : WoodA);
			AddSlab(B, -ChestHalfD + 1.0, ChestHalfD - 1.0, -ChestHalfW - 2.0, -ChestHalfW + 1.0, Z0, Z1, (r % 2) ? WoodA : WoodB);
		}

		// Borde dorado arriba (un poco por encima de las paredes: sin parpadeo entre caras).
		const double BandZ0 = ChestBodyH - 7.0;
		const double BandZ1 = ChestBodyH + 0.5;
		AddSlab(B, ChestHalfD - 1.0, ChestHalfD + 3.5, -ChestHalfW - 3.5, ChestHalfW + 3.5, BandZ0, BandZ1, GoldDeep);
		AddSlab(B, -ChestHalfD - 3.5, -ChestHalfD + 1.0, -ChestHalfW - 3.5, ChestHalfW + 3.5, BandZ0, BandZ1, GoldDeep);
		AddSlab(B, -ChestHalfD + 1.0, ChestHalfD - 1.0, ChestHalfW - 1.0, ChestHalfW + 3.5, BandZ0, BandZ1, GoldDeep);
		AddSlab(B, -ChestHalfD + 1.0, ChestHalfD - 1.0, -ChestHalfW - 3.5, -ChestHalfW + 1.0, BandZ0, BandZ1, GoldDeep);

		// Cantoneras doradas en las cuatro esquinas, con dos remaches por cara, y flejes de hierro delante y detrás.
		for (const double Sx : { -1.0, 1.0 })
		{
			for (const double Sy : { -1.0, 1.0 })
			{
				AddSlab(B, Sx * (ChestHalfD - 5.0), Sx * (ChestHalfD + 4.5), Sy * (ChestHalfW - 5.0), Sy * (ChestHalfW + 4.5), 6.0, ChestBodyH - 5.5, Gold);
				for (const double RivetZ : { 20.0, 42.0 })
				{
					AddRivet(B, FVector(Sx * (ChestHalfD + 4.5), Sy * (ChestHalfW - 0.5), RivetZ), FVector(Sx, 0.0, 0.0), GoldDeep);
					AddRivet(B, FVector(Sx * (ChestHalfD - 0.5), Sy * (ChestHalfW + 4.5), RivetZ), FVector(0.0, Sy, 0.0), GoldDeep);
				}
				AddSlab(B, Sx * (ChestHalfD + 1.0), Sx * (ChestHalfD + 4.0), Sy * StrapY - StrapHalf, Sy * StrapY + StrapHalf, 7.5, ChestBodyH - 6.5, Iron);
				for (const double RivetZ : { 18.0, 33.0, 48.0 })
				{
					AddRivet(B, FVector(Sx * (ChestHalfD + 4.0), Sy * StrapY, RivetZ), FVector(Sx, 0.0, 0.0), Gold);
				}
			}
		}

		// Lados: fleje en medio y un asa de hierro colgando de dos pletinas.
		for (const double Sy : { -1.0, 1.0 })
		{
			AddSlab(B, -StrapHalf, StrapHalf, Sy * (ChestHalfW + 1.0), Sy * (ChestHalfW + 4.0), 7.5, ChestBodyH - 6.5, Iron);
			for (const double RivetZ : { 16.0, 48.0 })
			{
				AddRivet(B, FVector(0.0, Sy * (ChestHalfW + 4.0), RivetZ), FVector(0.0, Sy, 0.0), Gold);
			}
			const double HandleZ = ChestBodyH * 0.55;
			const double Near = Sy * (ChestHalfW + 4.0);
			const double Far = Sy * (ChestHalfW + 11.0);
			for (const double PlateX : { -13.0, 13.0 })
			{
				AddSlab(B, PlateX - 3.5, PlateX + 3.5, Sy * (ChestHalfW + 1.0), Sy * (ChestHalfW + 5.5), HandleZ - 4.5, HandleZ + 4.5, Iron);
			}
			B.AddBeam(FVector(-13.0, Near, HandleZ), FVector(-13.0, Far, HandleZ - 7.0), 1.8, Iron);
			B.AddBeam(FVector(-13.0, Far, HandleZ - 7.0), FVector(13.0, Far, HandleZ - 7.0), 1.8, Iron);
			B.AddBeam(FVector(13.0, Far, HandleZ - 7.0), FVector(13.0, Near, HandleZ), 1.8, Iron);
		}

		// Cerradura: chapa dorada con el ojo de la llave (por debajo del pasador de la tapa) y cuatro remaches.
		AddSlab(B, ChestHalfD + 1.5, ChestHalfD + 5.5, -13.0, 13.0, ChestBodyH - 34.0, ChestBodyH - 5.0, Gold);
		TNProcMesh::TNProcAddCylinder(B, FVector(ChestHalfD + 5.3, 0.0, ChestBodyH - 25.0), FVector(ChestHalfD + 6.3, 0.0, ChestBodyH - 25.0), 3.4, 3.4, 8, Keyhole);
		AddSlab(B, ChestHalfD + 5.3, ChestHalfD + 6.3, -1.5, 1.5, ChestBodyH - 31.0, ChestBodyH - 25.0, Keyhole);
		for (const double PlateY : { -9.5, 9.5 })
		{
			for (const double PlateZ : { ChestBodyH - 30.0, ChestBodyH - 9.0 })
			{
				AddRivet(B, FVector(ChestHalfD + 5.5, PlateY, PlateZ), FVector(1.0, 0.0, 0.0), GoldDeep);
			}
		}

		// El tesoro: cama de monedas, montones, monedas sueltas, gemas y una copa.
		AddSlab(B, -DIn - 0.5, DIn + 0.5, -WIn - 0.5, WIn + 0.5, 1.0, CoinBedZ, CoinB);
		for (const FCoinPile& Pile : CoinPiles)
		{
			TNProcMesh::TNProcAddCylinder(B, FVector(Pile.X, Pile.Y, CoinBedZ - 0.5), FVector(Pile.X, Pile.Y, CoinBedZ + Pile.Height), Pile.Radius, Pile.Radius * 0.2, 10, CoinA);
		}
		for (int32 c = 0; c < 16; ++c)
		{
			const double CoinX = (DIn - 7.0) * TNProcMesh::TNProcHashNoise(c, 1, 41u);
			const double CoinY = (WIn - 7.0) * TNProcMesh::TNProcHashNoise(c, 2, 41u);
			const double Tilt = 0.45 * TNProcMesh::TNProcHashNoise(c, 3, 41u);
			const double Spin = UE_DOUBLE_PI * TNProcMesh::TNProcHashNoise(c, 4, 41u);
			const FVector Axis = FVector(FMath::Cos(Spin) * Tilt, FMath::Sin(Spin) * Tilt, 1.0).GetSafeNormal();
			const FVector CoinAt(CoinX, CoinY, CoinBedZ + PileHeightAt(CoinX, CoinY) + 0.6);
			TNProcMesh::TNProcAddCylinder(B, CoinAt, CoinAt + Axis * 1.4, 5.5, 5.5, 10, (c % 2) ? CoinA : CoinB);
		}
		AddGem(B, FVector(-22.0, 34.0, CoinBedZ + 7.0), 6.5, TNCastleKit::Pal(0xE63946));
		AddGem(B, FVector(24.0, -36.0, CoinBedZ + 6.0), 6.0, TNCastleKit::Pal(0x2EC4B6));
		AddGem(B, FVector(-8.0, -22.0, CoinBedZ + 16.0), 7.0, TNCastleKit::Pal(0x9B5DE5));
		AddGem(B, FVector(12.0, 26.0, CoinBedZ + 12.5), 5.0, TNCastleKit::Pal(0x4CC9F0));
		const FVector Cup(-26.0, -40.0, CoinBedZ);
		TNProcMesh::TNProcAddCylinder(B, Cup, Cup + Up * 1.8, 6.0, 5.0, 8, Gold);
		TNProcMesh::TNProcAddCylinder(B, Cup + Up * 1.8, Cup + Up * 9.0, 1.6, 1.6, 6, Gold);
		TNProcMesh::TNProcAddCylinder(B, Cup + Up * 9.0, Cup + Up * 18.0, 3.0, 7.0, 10, GoldDeep);
	}

	/**
	 * Tapa de medio cañón (locales de la tapa: origen en la bisagra, +X hacia el frente): tablones por fuera y madera
	 * oscura por dentro, testeros macizos con filo dorado, dos flejes de hierro con remaches, labio dorado delante,
	 * pasador sobre la cerradura, bisagras y una vieira dorada de emblema.
	 */
	void BuildChestLid(FBuffers& B)
	{
		const FLinearColor WoodA = TNCastleKit::Pal(0xB5794A);
		const FLinearColor WoodB = TNCastleKit::Pal(0x9E6639);
		const FLinearColor WoodDark = TNCastleKit::Pal(0x6E4526);
		const FLinearColor WoodIn = TNCastleKit::Pal(0x4A2E1A);
		const FLinearColor Iron = TNCastleKit::Pal(0x3B3F4A);
		const FLinearColor Gold = TNCastleKit::Pal(0xFFCB3D);
		const FLinearColor GoldDeep = TNCastleKit::Pal(0xE0A12A);
		constexpr double RIn = LidRadius - LidThick;

		for (int32 k = 0; k < LidSegments; ++k)
		{
			const double Th0 = UE_DOUBLE_PI * k / LidSegments;
			const double Th1 = UE_DOUBLE_PI * (k + 1) / LidSegments;
			const double ThM = (Th0 + Th1) * 0.5;
			const FVector Out(FMath::Cos(ThM), 0.0, FMath::Sin(ThM));
			// Un tablón por tramo, por fuera; por dentro, madera oscura.
			B.AddQuad(LidArcPoint(LidRadius, Th0, -LidSide), LidArcPoint(LidRadius, Th1, -LidSide), LidArcPoint(LidRadius, Th1, LidSide),
				LidArcPoint(LidRadius, Th0, LidSide), Out, (k % 2) ? WoodA : WoodB);
			B.AddQuad(LidArcPoint(RIn, Th0, -LidSide), LidArcPoint(RIn, Th1, -LidSide), LidArcPoint(RIn, Th1, LidSide), LidArcPoint(RIn, Th0, LidSide),
				-Out, WoodIn);
			for (const double Sy : { -1.0, 1.0 })
			{
				// Testeros: medio disco macizo a cada lado (tablón por fuera, madera oscura por dentro) con filo dorado.
				const FVector SideN(0.0, Sy, 0.0);
				const FVector Hub(ChestHalfD, Sy * LidSide, 0.0);
				const FVector E0 = LidArcPoint(LidRadius, Th0, Sy * LidSide);
				const FVector E1 = LidArcPoint(LidRadius, Th1, Sy * LidSide);
				B.AddTri(Hub, E0, E1, SideN, WoodB);
				B.AddTri(Hub, E0, E1, -SideN, WoodIn);
				B.AddBeam(LidArcPoint(LidRadius + 0.5, Th0, Sy * (LidSide - 1.5)), LidArcPoint(LidRadius + 0.5, Th1, Sy * (LidSide - 1.5)), 2.6, Gold);
				// Fleje de hierro por encima del tablón (cara de fuera y los dos cantos).
				const double Y0 = Sy * StrapY - StrapHalf;
				const double Y1 = Sy * StrapY + StrapHalf;
				B.AddQuad(LidArcPoint(LidRadius + 2.5, Th0, Y0), LidArcPoint(LidRadius + 2.5, Th1, Y0), LidArcPoint(LidRadius + 2.5, Th1, Y1),
					LidArcPoint(LidRadius + 2.5, Th0, Y1), Out, Iron);
				B.AddQuad(LidArcPoint(LidRadius - 0.5, Th0, Y0), LidArcPoint(LidRadius - 0.5, Th1, Y0), LidArcPoint(LidRadius + 2.5, Th1, Y0),
					LidArcPoint(LidRadius + 2.5, Th0, Y0), FVector(0.0, -1.0, 0.0), Iron);
				B.AddQuad(LidArcPoint(LidRadius - 0.5, Th0, Y1), LidArcPoint(LidRadius - 0.5, Th1, Y1), LidArcPoint(LidRadius + 2.5, Th1, Y1),
					LidArcPoint(LidRadius + 2.5, Th0, Y1), FVector(0.0, 1.0, 0.0), Iron);
			}
		}

		// Cantos de abajo (delante y detrás), entre la cara de fuera y la de dentro: se ven con la tapa abierta.
		for (const double Edge : { 0.0, UE_DOUBLE_PI })
		{
			B.AddQuad(LidArcPoint(RIn, Edge, -LidSide), LidArcPoint(LidRadius, Edge, -LidSide), LidArcPoint(LidRadius, Edge, LidSide),
				LidArcPoint(RIn, Edge, LidSide), FVector(0.0, 0.0, -1.0), WoodDark);
		}

		// Remaches de los flejes (en las aristas de los tablones, sobre la chapa) y bisagras detrás.
		for (const double Sy : { -1.0, 1.0 })
		{
			for (const int32 Step : { 1, 4, 7 })
			{
				const double Th = UE_DOUBLE_PI * Step / LidSegments;
				AddRivet(B, LidArcPoint(LidRadius + 2.5, Th, Sy * StrapY), FVector(FMath::Cos(Th), 0.0, FMath::Sin(Th)), Gold);
			}
			TNProcMesh::TNProcAddCylinder(B, FVector(-1.5, Sy * StrapY - 9.0, 0.5), FVector(-1.5, Sy * StrapY + 9.0, 0.5), 3.6, 3.6, 8, Iron);
		}

		// Labio dorado por el canto de delante y pasador que baja sobre la chapa de la cerradura.
		const double FrontX = ChestHalfD + LidRadius;
		AddSlab(B, FrontX - 3.0, FrontX + 1.5, -LidSide - 1.0, LidSide + 1.0, 0.0, 7.0, Gold);
		AddSlab(B, FrontX + 1.0, 2.0 * ChestHalfD + 8.5, -6.5, 6.5, -19.0, 4.0, Gold);
		AddRivet(B, FVector(2.0 * ChestHalfD + 8.5, 0.0, -3.0), FVector(1.0, 0.0, 0.0), GoldDeep);

		// Emblema: vieira dorada en el segundo tablón de delante (una cara plana), por encima del pasador.
		{
			const double ThA = UE_DOUBLE_PI / LidSegments;
			const double ThB = 2.0 * UE_DOUBLE_PI / LidSegments;
			const FVector PA = LidArcPoint(LidRadius, ThA, 0.0);
			const FVector PB = LidArcPoint(LidRadius, ThB, 0.0);
			const double ThC = (ThA + ThB) * 0.5;
			const FVector Along = (PB - PA).GetSafeNormal();
			TNCastleKit::AddScallop(B, (PA + PB) * 0.5 - Along * 3.0, FVector(FMath::Cos(ThC), 0.0, FMath::Sin(ThC)), Along, 10.5, Gold);
		}
	}
}

ATN_TreasureChest::ATN_TreasureChest()
{
	using namespace TNTreasureChestDetail;
	PromptText = NSLOCTEXT("Tortunabo", "TreasureChestPrompt", "Mantén para rebuscar en el cofre");
	// Más difícil que un decorado del mapa, pero siempre hay premio y se puede repetir tras un respiro.
	SearchSeconds = 5.f;
	LootChance = 1.f;
	bRepeatable = true;
	RepeatCooldown = 2.5f;
	MaxLootLying = 6;
	// Suena a chismes y monedas más que a arena.
	RummagePitch = 1.4f;

	// Mallas sin colisión (la pone ChestBlock); las rellena BuildChestMeshes.
	ChestBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestBody"));
	ChestBody->SetupAttachment(SceneRoot);
	ChestBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChestBody->SetCanEverAffectNavigation(false);

	ChestLid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestLid"));
	ChestLid->SetupAttachment(SceneRoot);
	ChestLid->SetRelativeLocation(FVector(-ChestHalfD, 0.0, ChestBodyH));
	ChestLid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChestLid->SetCanEverAffectNavigation(false);

	// Caja de colisión del cofre cerrado (estática: la traza del sitio donde cae el objeto no la busca delante).
	ChestBlock = CreateDefaultSubobject<UBoxComponent>(TEXT("ChestBlock"));
	ChestBlock->SetupAttachment(SceneRoot);
	ChestBlock->InitBoxExtent(FVector(ChestHalfD + 4.0, ChestHalfW + 4.0, ChestTopZ * 0.5));
	ChestBlock->SetRelativeLocation(FVector(0.0, 0.0, ChestTopZ * 0.5));
	ChestBlock->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ChestBlock->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	ChestBlock->SetCanEverAffectNavigation(false);
	ChestBlock->SetHiddenInGame(true);

	// Brillo dorado de dentro, sin sombras; apagado con la tapa cerrada.
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(SceneRoot);
	GlowLight->SetRelativeLocation(FVector(0.0, 0.0, ChestBodyH + 6.0));
	GlowLight->SetIntensityUnits(ELightUnits::Lumens);
	GlowLight->SetIntensity(0.f);
	GlowLight->SetAttenuationRadius(420.f);
	GlowLight->SetLightColor(FLinearColor(1.f, 0.74f, 0.32f));
	GlowLight->SetCastShadows(false);
	GlowLight->SetVisibility(false);
}

void ATN_TreasureChest::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildChestMeshes();
}

void ATN_TreasureChest::BeginPlay()
{
	using namespace TNTreasureChestDetail;
	if (HasAuthority())
	{
		// Huella redonda que abarca la caja y color del «polvo»: oro (monedas y destellos en vez de tierra).
		SetupSpot(FootprintR, 0.f, static_cast<float>(ChestTopZ), FLinearColor(1.f, 0.8f, 0.36f));
	}
	Super::BeginPlay();
	// Las mallas son RF_DuplicateTransient: la copia del PIE llega sin ellas.
	if (ChestBody && !ChestBody->GetStaticMesh())
	{
		BuildChestMeshes();
	}
}

void ATN_TreasureChest::BuildChestMeshes()
{
	using namespace TNTreasureChestDetail;
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !ChestBody || !ChestLid)
	{
		return;
	}
	UMaterialInterface* Mat = TNCastleKit::VertexColorMaterial();
	FBuffers BodyBuffers;
	BuildChestBody(BodyBuffers);
	ChestBody->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, BodyBuffers, Mat));
	FBuffers LidBuffers;
	BuildChestLid(LidBuffers);
	ChestLid->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, LidBuffers, Mat));
}

// ── Reglas del cofre ─────────────────────────────────────────────────────────

float ATN_TreasureChest::GetHoldDuration() const
{
	// Siempre los suyos (5 s): tn.Search.Seconds es para los decorados del mapa.
	return SearchSeconds;
}

float ATN_TreasureChest::GetLuck() const
{
	// Siempre hay premio: tn.Search.Luck es para los decorados del mapa.
	return LootChance;
}

FVector ATN_TreasureChest::GetMouthPoint() const
{
	return GetActorTransform().TransformPosition(FVector(0.0, 0.0, TNTreasureChestDetail::ChestBodyH));
}

FVector ATN_TreasureChest::GetLootOrigin(const APawn* /*Pawn*/) const
{
	// De dentro del cofre, algo por delante del centro (la tapa se abre hacia atrás).
	return GetActorTransform().TransformPosition(FVector(12.0, 0.0, TNTreasureChestDetail::ChestBodyH + 8.0));
}

FVector ATN_TreasureChest::GetRummageOrigin(const APawn* Searcher) const
{
	// Del borde de la boca que da al que rebusca: monedas y chismes que saltan hacia él.
	const FVector Mouth = GetMouthPoint();
	FVector Toward = Searcher ? (Searcher->GetActorLocation() - Mouth).GetSafeNormal2D() : FVector::ZeroVector;
	if (Toward.IsNearlyZero())
	{
		Toward = GetActorForwardVector();
	}
	return Mouth + Toward * 25.0 + FVector(0.0, 0.0, 4.0);
}

FVector ATN_TreasureChest::FindLanding(const APawn* Pawn, const FVector& /*From*/) const
{
	using namespace TNTreasureChestDetail;
	const FVector Base = GetActorLocation();
	FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	if (Fwd.IsNearlyZero())
	{
		Fwd = FVector::ForwardVector;
	}
	const FVector Side(-Fwd.Y, Fwd.X, 0.0);
	// Siempre por delante del cofre, en la tarima (nunca fuera de la azotea), algo hacia el lado del que rebuscaba.
	double Lean = 0.0;
	if (Pawn)
	{
		Lean = FMath::Clamp(FVector::DotProduct(Pawn->GetActorLocation() - Base, Side) / 150.0, -1.0, 1.0) * 25.0;
	}
	const FVector Target = Base + Fwd * (ChestHalfD + FMath::FRandRange(LandFrontMin, LandFrontMax))
		+ Side * (Lean + FMath::FRandRange(-LandSide, LandSide));

	// El suelo de verdad (tarima o azotea): traza corta contra lo estático; si no hay nada, a la altura del cofre.
	const UWorld* World = GetWorld();
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TN_ChestLanding), false, this);
	if (Pawn)
	{
		Query.AddIgnoredActor(Pawn);
	}
	if (World && World->LineTraceSingleByObjectType(Hit, FVector(Target.X, Target.Y, Base.Z + 120.0), FVector(Target.X, Target.Y, Base.Z - 150.0),
		FCollisionObjectQueryParams(ECC_WorldStatic), Query))
	{
		return Hit.ImpactPoint + FVector(0.0, 0.0, 5.0);
	}
	return FVector(Target.X, Target.Y, Base.Z + 5.0);
}

// ── Tapa, brillo y sonidos (cada máquina con pantalla) ───────────────────────

void ATN_TreasureChest::OnSearchStateChanged(const FTNSearchSpotState& OldState)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FTNSearchSpotState& State = GetSearchState();
	if (State.Searcher && !OldState.Searcher)
	{
		// Alguien empieza a rebuscar: la tapa cruje al entreabrirse.
		PlaySearchSound(ETNSearchSound::LidCreak, FMath::FRandRange(0.92f, 1.08f), 0.9f, GetMouthPoint());
		CreakClock = FMath::FRandRange(1.f, 1.6f);
	}
	// La tapa se mueve con cada cambio: tick a cada fotograma hasta que se quede quieta (WantsFrameTick).
	SetActorTickInterval(0.f);
}

bool ATN_TreasureChest::WantsFrameTick() const
{
	return LidTarget > 0.f || LidAngle > 0.05f || FMath::Abs(LidSpeed) > 0.05f;
}

void ATN_TreasureChest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLid(DeltaSeconds);
	}
}

void ATN_TreasureChest::TickLid(float DeltaSeconds)
{
	using namespace TNTreasureChestDetail;
	const FTNSearchSpotState& State = GetSearchState();
	const double Now = ServerNow();
	LidClock += DeltaSeconds;
	ThumpCooldown = FMath::Max(0.f, ThumpCooldown - DeltaSeconds);

	// Adónde va la tapa, según el estado replicado (igual en todas las máquinas): entreabierta a tirones mientras se
	// rebusca (más cuanto más se lleva), abierta del todo un rato al salir el objeto y, si no, cerrada.
	const bool bSearching = State.Searcher != nullptr;
	float Progress = 0.f;
	float Target = 0.f;
	if (bSearching)
	{
		const double Hold = FMath::Max(0.2, static_cast<double>(GetHoldDuration()));
		Progress = FMath::Clamp(static_cast<float>((Now - static_cast<double>(State.SearchStart)) / Hold), 0.f, 1.f);
		Target = FMath::Lerp(LidPryStartDeg, LidPryEndDeg, Progress);
	}
	else if (State.SearchCount != 0 && State.Outcome != ETNSearchOutcome::None && Now - static_cast<double>(State.OutcomeTime) < LidPopSeconds)
	{
		Target = LidPopDeg;
	}
	LidTarget = Target;

	// Muelle con algo de rebote, en pasos cortos (estable aunque un fotograma tarde).
	float Remaining = FMath::Min(DeltaSeconds, 0.25f);
	while (Remaining > 0.f)
	{
		const float Step = FMath::Min(Remaining, 1.f / 90.f);
		Remaining -= Step;
		LidSpeed += (LidStiffness * (Target - LidAngle) - LidDamping * LidSpeed) * Step;
		LidAngle += LidSpeed * Step;
		if (LidAngle > LidMaxDeg)
		{
			LidAngle = LidMaxDeg;
			LidSpeed = FMath::Min(LidSpeed, 0.f);
		}
		if (LidAngle < 0.f)
		{
			// Cae sobre la caja: «¡clonc!» si viene con fuerza y un rebote corto.
			if (LidSpeed < -LidThumpSpeed && ThumpCooldown <= 0.f)
			{
				PlaySearchSound(ETNSearchSound::LidThump, FMath::FRandRange(0.94f, 1.06f), FMath::Clamp(-LidSpeed / 400.f, 0.35f, 1.f), GetMouthPoint());
				ThumpCooldown = 0.3f;
			}
			LidAngle = 0.f;
			LidSpeed = -LidSpeed * 0.18f;
			if (LidSpeed < 8.f)
			{
				LidSpeed = 0.f;
			}
		}
	}

	// Mientras se rebusca, la tapa pesa y se resiste: tiembla (más al principio).
	float Shown = LidAngle;
	if (bSearching && LidAngle > 5.f)
	{
		Shown += (2.2f + 2.f * (1.f - Progress)) * FMath::Sin(LidClock * 27.f) * (0.55f + 0.45f * FMath::Sin(LidClock * 4.3f));
	}
	if (ChestLid)
	{
		ChestLid->SetRelativeRotation(FRotator(Shown, 0.f, 0.f));
	}

	// Brillo dorado de dentro: sube con la tapa, parpadea un poco y aprieta al salir el objeto.
	if (GlowLight)
	{
		const float Open = FMath::Clamp(Shown / 40.f, 0.f, 1.f);
		const float Flicker = 1.f + 0.08f * FMath::Sin(LidClock * 13.f) + 0.05f * FMath::Sin(LidClock * 31.f + 1.3f);
		const float Lumens = GlowLumens * Open * Flicker * (Target >= LidPopDeg ? 1.4f : 1.f);
		const bool bLit = Lumens > 5.f;
		if (GlowLight->IsVisible() != bLit)
		{
			GlowLight->SetVisibility(bLit);
		}
		if (bLit)
		{
			GlowLight->SetIntensity(Lumens);
		}
	}

	// Destellos que suben de dentro con la tapa abierta.
	if (Shown > 12.f)
	{
		GlintClock -= DeltaSeconds;
		if (GlintClock <= 0.f)
		{
			GlintClock = FMath::FRandRange(0.14f, 0.3f);
			const FVector Glint = GetActorTransform().TransformPosition(FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-55.f, 55.f), ChestBodyH - 6.0));
			EmitSparkles(Glint, 1, FVector::UpVector, 1.4f);
		}
	}

	// Crujidos de vez en cuando mientras se fuerza la tapa.
	if (bSearching)
	{
		CreakClock -= DeltaSeconds;
		if (CreakClock <= 0.f)
		{
			CreakClock = FMath::FRandRange(0.9f, 1.6f);
			PlaySearchSound(ETNSearchSound::LidCreak, FMath::FRandRange(0.85f, 1.15f), FMath::FRandRange(0.35f, 0.55f), GetMouthPoint());
		}
	}
}
