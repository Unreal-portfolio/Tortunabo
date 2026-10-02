// Mallas de la torreta del buggy (#435). Ver TN_BuggyTurretMesh.h.

#include "TN_BuggyTurretMesh.h"
#include "Vehicles/TN_BuggyTurretComponent.h"

namespace TNBuggyTurretMesh
{
	namespace
	{
		using TNProcMesh::FTNProcMeshBuffers;

		/** Color sRGB 0xRRGGBB (TNProcRuntimeMesh::MakeStaticMesh lo pasa a lineal), paleta de SM_TN_BuggyBody. */
		FLinearColor Srgb(uint32 Hex)
		{
			return FLinearColor(((Hex >> 16) & 0xFF) / 255.f, ((Hex >> 8) & 0xFF) / 255.f, (Hex & 0xFF) / 255.f, 1.f);
		}

		const FLinearColor Yellow = Srgb(0xF6C445);
		const FLinearColor Cream = Srgb(0xF4EFE2);
		const FLinearColor Paint = Srgb(0xE4572E);
		const FLinearColor Trim = Srgb(0x80878C);
		const FLinearColor Dark = Srgb(0x2B2833);

		constexpr int32 RingSegments = 28;
		constexpr int32 TubeSides = 6;
		constexpr int32 BarrelSides = 12;

		const FVector AxisX(1.0, 0.0, 0.0);

		/** Toro horizontal de radio Radius y tubo Tube centrado en Center. */
		void AddTorus(FTNProcMeshBuffers& Out, const FVector& Center, double Radius, double Tube, const FLinearColor& Color)
		{
			TArray<TArray<FVector>> Rings;
			for (int32 Segment = 0; Segment <= RingSegments; ++Segment)
			{
				const double Angle = UE_DOUBLE_TWO_PI * (Segment % RingSegments) / RingSegments;
				const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
				TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
				for (int32 Side = 0; Side < TubeSides; ++Side)
				{
					const double Around = UE_DOUBLE_TWO_PI * Side / TubeSides;
					Ring.Add(Center + Dir * (Radius + Tube * FMath::Cos(Around)) + FVector::UpVector * (Tube * FMath::Sin(Around)));
				}
			}
			Out.AddSweep(Rings, true, Color);
		}

		/** Anillo de Sides puntos de radio R alrededor del eje X, en X. */
		TArray<FVector> RingAroundX(double X, double Y, double R)
		{
			TArray<FVector> Ring;
			for (int32 Side = 0; Side < BarrelSides; ++Side)
			{
				const double Angle = UE_DOUBLE_TWO_PI * Side / BarrelSides;
				Ring.Add(FVector(X, Y + R * FMath::Cos(Angle), R * FMath::Sin(Angle)));
			}
			return Ring;
		}

		/** Une dos anillos con quads que miran hacia Inward ? el eje : fuera. */
		void Bridge(FTNProcMeshBuffers& Out, const TArray<FVector>& A, const TArray<FVector>& B, double AxisY, bool bInward,
			const FLinearColor& Color)
		{
			for (int32 Side = 0; Side < A.Num(); ++Side)
			{
				const int32 Next = (Side + 1) % A.Num();
				const FVector Mid = (A[Side] + A[Next]) * 0.5;
				const FVector Radial(0.0, Mid.Y - AxisY, Mid.Z);
				FVector Hint = bInward ? -Radial : Radial;
				// Corona plana (los dos anillos a la misma X): mira hacia delante.
				if (FMath::IsNearlyEqual(A[Side].X, B[Side].X))
				{
					Hint = AxisX;
				}
				Out.AddQuad(A[Side], A[Next], B[Next], B[Side], Hint, Color);
			}
		}
	}

	void BuildRing(FTNProcMeshBuffers& Out)
	{
		AddTorus(Out, FVector(0.0, 0.0, RingZ), RingRadius, RingTube, Trim);
		// Tirantes a las barandillas laterales, con su abrazadera.
		for (const double Side : { -1.0, 1.0 })
		{
			Out.AddBeam(FVector(0.0, Side * (RingRadius + RingTube * 0.5), RingZ), FVector(0.0, Side * (RailY - 2.0), RailZ), 1.6, Trim);
			Out.AddBox(FVector(0.0, Side * RailY, RailZ), AxisX, FVector(3.0, 5.2, 5.2), Paint);
		}
	}

	void BuildMount(FTNProcMeshBuffers& Out)
	{
		// Carro sobre el aro (a la derecha) con dos zapatas que lo abrazan.
		const double CarriageZ = RingZ + RingTube + 2.5;
		Out.AddBox(FVector(0.0, RingRadius, CarriageZ), AxisX, FVector(10.0, 4.5, 2.5), Dark);
		for (const double Side : { -1.0, 1.0 })
		{
			const double X = Side * 8.0;
			Out.AddBox(FVector(X, FMath::Sqrt(RingRadius * RingRadius - X * X), RingZ), AxisX, FVector(1.5, 4.0, 3.2), Dark);
		}
		// Horquilla del carro hasta el eje de cabeceo y cubo del eje (simétrico: no se nota que no cabecea).
		const double PlateBottom = CarriageZ + 2.5;
		const double PlateTop = HubRadius;
		Out.AddBox(FVector(0.0, YokeY, (PlateBottom + PlateTop) * 0.5), AxisX, FVector(8.0, 1.8, (PlateTop - PlateBottom) * 0.5), Yellow);
		// Pernos en la cara de fuera de la horquilla: se ve que el carro gira con la torreta.
		for (const double X : { -5.0, 5.0 })
		{
			Out.AddBox(FVector(X, YokeY + 2.3, PlateBottom + 3.0), AxisX, FVector(1.2, 0.6, 1.2), Dark);
		}
		TNProcMesh::TNProcAddCylinder(Out, FVector(0.0, HubInnerY, 0.0), FVector(0.0, HubOuterY, 0.0), HubRadius, HubRadius, 10, Dark);
	}

	void BuildGun(FTNProcMeshBuffers& Out)
	{
		const double SideY = UTN_BuggyTurretComponent::MuzzleSideCm;
		// Brazo lateral del cubo al cuerpo, por fuera de la cabeza de la artillera.
		Out.AddBox(FVector((BodyMinX + 2.0 - 3.0) * 0.5, 32.0, 0.0), AxisX, FVector((BodyMinX + 2.0 + 3.0) * 0.5, 2.0, 3.5), Trim);
		// Cuerpo, culata y tolva de cocos.
		const double BodyHalfX = (BodyMaxX - BodyMinX) * 0.5;
		Out.AddBox(FVector(BodyMinX + BodyHalfX, SideY, 0.0), AxisX, FVector(BodyHalfX, 8.0, 7.0), Yellow);
		TNProcMesh::TNProcAddCylinder(Out, FVector(BodyMinX, SideY, 0.0), FVector(BodyMinX - 3.0, SideY, 0.0), 4.0, 3.0, 8, Dark);
		TNProcMesh::TNProcAddCylinder(Out, FVector(BodyMinX + 9.0, SideY, 7.0), FVector(BodyMinX + 9.0, SideY, 14.0), 4.5, 6.5, 8, Paint);
		// Escudo frontal con su marco.
		const double ShieldMaxY = SideY + 17.0;
		const double ShieldMinZ = -13.0;
		const double ShieldMaxZ = 15.0;
		const FVector ShieldCenter(ShieldX, (ShieldMinY + ShieldMaxY) * 0.5, (ShieldMinZ + ShieldMaxZ) * 0.5);
		const FVector ShieldHalf(1.2, (ShieldMaxY - ShieldMinY) * 0.5, (ShieldMaxZ - ShieldMinZ) * 0.5);
		Out.AddBox(ShieldCenter, AxisX, ShieldHalf, Cream);
		const double RimX = ShieldX + ShieldHalf.X + 0.6;
		const FVector Corners[4] = { FVector(RimX, ShieldMinY, ShieldMinZ), FVector(RimX, ShieldMaxY, ShieldMinZ),
			FVector(RimX, ShieldMaxY, ShieldMaxZ), FVector(RimX, ShieldMinY, ShieldMaxZ) };
		for (int32 Edge = 0; Edge < 4; ++Edge)
		{
			Out.AddBeam(Corners[Edge], Corners[(Edge + 1) % 4], 1.0, Paint);
		}
	}

	void BuildBarrel(FTNProcMeshBuffers& Out)
	{
		const double SideY = UTN_BuggyTurretComponent::MuzzleSideCm;
		const double MouthX = UTN_BuggyTurretComponent::MuzzleDistanceCm;
		const double LipX = MouthX - 1.0;
		const double InnerRadius = MouthRadius - 1.3;
		const double BoreX = MouthX - 6.5;
		// Caña desde dentro del cuerpo, campana, labio, corona de la boca y ánima hacia dentro con su fondo.
		const TArray<FVector> Start = RingAroundX(BodyMaxX - 2.0, SideY, BarrelRadius);
		const TArray<FVector> BellStart = RingAroundX(BellStartX, SideY, BarrelRadius);
		const TArray<FVector> Lip = RingAroundX(LipX, SideY, MouthRadius);
		const TArray<FVector> Rim = RingAroundX(MouthX, SideY, MouthRadius);
		const TArray<FVector> Inner = RingAroundX(MouthX, SideY, InnerRadius);
		const TArray<FVector> Bore = RingAroundX(BoreX, SideY, BarrelRadius * 0.6);
		Bridge(Out, Start, BellStart, SideY, false, Yellow);
		Bridge(Out, BellStart, Lip, SideY, false, Yellow);
		Bridge(Out, Lip, Rim, SideY, false, Yellow);
		Bridge(Out, Rim, Inner, SideY, false, Yellow);
		Bridge(Out, Inner, Bore, SideY, true, Yellow);
		const FVector BoreCenter(BoreX, SideY, 0.0);
		for (int32 Side = 0; Side < Bore.Num(); ++Side)
		{
			Out.AddTri(BoreCenter, Bore[Side], Bore[(Side + 1) % Bore.Num()], AxisX, Dark);
		}
	}
}
