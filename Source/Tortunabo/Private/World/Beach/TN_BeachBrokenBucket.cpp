#include "World/Beach/TN_BeachBrokenBucket.h"
#include "Core/TN_Log.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "TN_BeachTrapKit.h"

/**
 * Geometría del cubo tumbado: un tronco de cono a lo largo de X (boca en -X, culo en +X) apoyado en la arena por su
 * generatriz de abajo (cada sección toca el suelo, así que el eje sube un poco hacia la boca). Pared de fuera y de dentro,
 * reborde enrollado y nervios, culo con un agujero dentado, grietas, asa, pegatina, suelo de arena y rampas.
 */
namespace TNBeachBucketDetail
{
	using FBuffers = TNBeachTrapKit::FBuffers;
	using FHulls = TNBeachTrapKit::FHulls;

	constexpr int32 AroundSeg = 28;
	constexpr int32 AlongSeg = 10;
	constexpr int32 HullSeg = 16;
	constexpr int32 PlateHullSeg = 12;
	/** Lo que se hunde en la arena. */
	constexpr double Sink = 6.0;

	struct FBucketDims
	{
		double Len = 504.0;
		double Rm = 238.0;
		double Rb = 182.0;
		double Wall = 26.0;
		double FloorZ = 69.0;
		double Rh = 135.0;
		double Xm = -252.0;
		double Xb = 252.0;
		double RampIn = 170.0;
		double RampOut = 150.0;

		double RadiusAt(double X) const { return FMath::Lerp(Rm, Rb, FMath::Clamp((X - Xm) / Len, 0.0, 1.0)); }
		double AxisZ(double X) const { return RadiusAt(X) - Sink; }
		FVector Axis(double X) const { return FVector(X, 0.0, AxisZ(X)); }

		/** Punto a Rad del eje en la sección X; Theta = 0 arriba, pi abajo. */
		FVector At(double X, double Rad, double Theta) const
		{
			return FVector(X, Rad * FMath::Sin(Theta), AxisZ(X) + Rad * FMath::Cos(Theta));
		}

		/** Semiancho del suelo de arena en X (cuerda de la pared de dentro a la altura del suelo). */
		double FloorHalfWidth(double X) const
		{
			const double Ri = RadiusAt(X) - Wall;
			const double Dz = AxisZ(X) - FloorZ;
			return FMath::Sqrt(FMath::Max(0.0, Ri * Ri - Dz * Dz));
		}

		/** Semiancho del agujero del culo a la altura del suelo. */
		double HoleHalfWidth() const
		{
			const double Dz = AxisZ(Xb) - FloorZ;
			return FMath::Sqrt(FMath::Max(0.0, Rh * Rh - Dz * Dz));
		}
	};

	double ThetaOf(int32 K, int32 Count)
	{
		return TNPlaygroundKit::KitTwoPi * K / Count;
	}

	/** Radio del borde dentado del agujero en el segmento K (dientes hacia fuera y algunos hacia dentro). */
	double HoleRadius(const FBucketDims& D, int32 K, uint32 Seed)
	{
		const double Tooth = (K % 2 == 0) ? 0.1 + 0.08 * TNPlaygroundKit::Hash01(K, 1, Seed) : -0.07 * TNPlaygroundKit::Hash01(K, 2, Seed);
		return D.Rh * (1.0 + Tooth);
	}

	void BuildBucket(FBuffers& B, FHulls& Hulls, const FBucketDims& D, uint32 Seed)
	{
		const FLinearColor Toy = TNPlaygroundKit::ToyColor(static_cast<int32>(Seed % 7u), 0.2f);
		const FLinearColor Base = TNPlaygroundKit::Mix(Toy, TNPlaygroundKit::Rgb(0xFFFFFF, 0.2f), 0.22);
		const FLinearColor Sun = TNPlaygroundKit::Mix(Base, TNPlaygroundKit::Rgb(0xFFFFFF, 0.2f), 0.35);
		const FLinearColor Inside = TNPlaygroundKit::Shade(Base, 0.62);
		const FLinearColor Band = TNPlaygroundKit::Shade(Base, 0.82);
		const FLinearColor CrackCol = TNPlaygroundKit::Rgb(0x3A3232);
		const FVector AxisX = FVector::ForwardVector;
		const double InnerEnd = D.Xb - D.Wall;

		// Paredes de fuera y de dentro.
		for (int32 i = 0; i < AlongSeg; ++i)
		{
			const double X0 = D.Xm + D.Len * i / AlongSeg;
			const double X1 = D.Xm + D.Len * (i + 1) / AlongSeg;
			const double Xi0 = D.Xm + (InnerEnd - D.Xm) * i / AlongSeg;
			const double Xi1 = D.Xm + (InnerEnd - D.Xm) * (i + 1) / AlongSeg;
			for (int32 k = 0; k < AroundSeg; ++k)
			{
				const double T0 = ThetaOf(k, AroundSeg);
				const double T1 = ThetaOf(k + 1, AroundSeg);
				const double Tm = 0.5 * (T0 + T1);
				const FVector Radial(0.0, FMath::Sin(Tm), FMath::Cos(Tm));
				const FLinearColor Outer = i == 1 ? Band : (FMath::Cos(Tm) > 0.55 ? Sun : Base);
				B.AddQuad(D.At(X0, D.RadiusAt(X0), T0), D.At(X0, D.RadiusAt(X0), T1), D.At(X1, D.RadiusAt(X1), T1), D.At(X1, D.RadiusAt(X1), T0), Radial, Outer);
				B.AddQuad(D.At(Xi0, D.RadiusAt(Xi0) - D.Wall, T0), D.At(Xi0, D.RadiusAt(Xi0) - D.Wall, T1), D.At(Xi1, D.RadiusAt(Xi1) - D.Wall, T1),
					D.At(Xi1, D.RadiusAt(Xi1) - D.Wall, T0), -Radial, Inside);
			}
		}
		// Canto de la boca, reborde enrollado y dos nervios.
		TNPlaygroundKit::AddAnnulus(B, D.Axis(D.Xm), -AxisX, D.Rm - D.Wall, D.Rm, AroundSeg, Band);
		TNPlaygroundKit::AddFrustum(B, D.Axis(D.Xm - 4.0), D.Axis(D.Xm + 16.0), D.Rm + 7.0, D.RadiusAt(D.Xm + 16.0) + 7.0, AroundSeg, Band, Band, false, false);
		TNPlaygroundKit::AddAnnulus(B, D.Axis(D.Xm - 4.0), -AxisX, D.Rm - D.Wall, D.Rm + 7.0, AroundSeg, Band);
		for (const double Frac : { 0.3, 0.55 })
		{
			const double Xr = D.Xm + D.Len * Frac;
			const double Rr = D.RadiusAt(Xr) + 3.5;
			TNPlaygroundKit::AddFrustum(B, D.Axis(Xr - 6.0), D.Axis(Xr + 6.0), Rr, Rr, AroundSeg, Band, Band, false, false);
		}

		// Culo con el agujero dentado: cara de fuera, cara de dentro y canto del agujero.
		for (int32 k = 0; k < AroundSeg; ++k)
		{
			const double T0 = ThetaOf(k, AroundSeg);
			const double T1 = ThetaOf(k + 1, AroundSeg);
			const double H0 = HoleRadius(D, k, Seed);
			const double H1 = HoleRadius(D, (k + 1) % AroundSeg, Seed);
			const double RinEnd = D.RadiusAt(InnerEnd) - D.Wall;
			B.AddQuad(D.At(D.Xb, H0, T0), D.At(D.Xb, H1, T1), D.At(D.Xb, D.Rb, T1), D.At(D.Xb, D.Rb, T0), AxisX, Base);
			B.AddQuad(D.At(InnerEnd, H0, T0), D.At(InnerEnd, H1, T1), D.At(InnerEnd, RinEnd, T1), D.At(InnerEnd, RinEnd, T0), -AxisX, Inside);
			const double Tm = 0.5 * (T0 + T1);
			B.AddQuad(D.At(D.Xb, H0, T0), D.At(D.Xb, H1, T1), D.At(InnerEnd, H1, T1), D.At(InnerEnd, H0, T0), -FVector(0.0, FMath::Sin(Tm), FMath::Cos(Tm)),
				TNPlaygroundKit::Rgb(0xF2F0EA, 0.1f));
		}
		// Grietas en zigzag que salen del agujero (y dos que siguen por la pared).
		for (int32 c = 0; c < 5; ++c)
		{
			const double Theta = TNPlaygroundKit::KitTwoPi * (c + 0.4 * TNPlaygroundKit::Hash01(c, 3, Seed)) / 5.0;
			FVector Prev = D.At(D.Xb + 0.8, D.Rh * 1.05, Theta);
			constexpr int32 Zig = 4;
			for (int32 z = 1; z <= Zig; ++z)
			{
				const double U = static_cast<double>(z) / Zig;
				const double Th = Theta + 0.08 * ((z % 2) ? 1.0 : -1.0);
				const FVector Next = D.At(D.Xb + 0.8, FMath::Lerp(D.Rh * 1.05, D.Rb * 0.98, U), Th);
				const FVector Side = FVector::CrossProduct((Next - Prev).GetSafeNormal(), AxisX).GetSafeNormal() * 2.2;
				B.AddQuad(Prev - Side, Next - Side, Next + Side, Prev + Side, AxisX, CrackCol);
				Prev = Next;
			}
			if (c < 2)
			{
				for (int32 z = 1; z <= Zig; ++z)
				{
					const double Xz = D.Xb - 22.0 * z;
					const double Th = Theta + 0.05 * ((z % 2) ? 1.0 : -1.0);
					const FVector Next = D.At(Xz, D.RadiusAt(Xz) + 0.8, Th);
					const FVector Radial(0.0, FMath::Sin(Th), FMath::Cos(Th));
					const FVector Side = FVector::CrossProduct((Next - Prev).GetSafeNormal(), Radial).GetSafeNormal() * 2.2;
					B.AddQuad(Prev - Side, Next - Side, Next + Side, Prev + Side, Radial, CrackCol);
					Prev = Next;
				}
			}
		}

		// Asa caída sobre el lomo, colgada de sus dos orejetas junto a la boca.
		const double LugX = D.Xm + 0.1 * D.Len;
		const double HandleR = D.RadiusAt(LugX) + 9.0;
		const FVector LugMid = D.Axis(LugX);
		const double Lean = FMath::DegreesToRadians(25.0);
		const FVector Over(FMath::Sin(Lean), 0.0, FMath::Cos(Lean));
		TArray<FVector> HandlePath;
		for (int32 s = 0; s <= 16; ++s)
		{
			const double Phi = TNPlaygroundKit::KitPi * s / 16.0;
			HandlePath.Add(LugMid + FVector(0.0, HandleR * FMath::Cos(Phi), 0.0) + Over * (HandleR * FMath::Sin(Phi)));
		}
		const FLinearColor HandleCol = TNPlaygroundKit::Rgb(0xFFF6E0, 0.25f);
		const TArray<double> HandleRadii = { 8.0 };
		const TArray<FLinearColor> HandleColors = { HandleCol };
		TNPlaygroundKit::AddTube(B, HandlePath, HandleRadii, 6, HandleColors, AxisX, false);
		for (const double Side : { -1.0, 1.0 })
		{
			const FVector Lug = LugMid + FVector(0.0, Side * (HandleR - 9.0), 0.0);
			TNPlaygroundKit::AddFrustum(B, Lug, Lug + FVector(0.0, Side * 16.0, 0.0), 14.0, 12.0, 10, Band, Band, false, true);
		}
		// Pegatina de estrella en el costado.
		const double StickerX = D.Xm + 0.66 * D.Len;
		const double StickerT = FMath::DegreesToRadians(62.0);
		TNPlaygroundKit::AddStarfish(B, D.At(StickerX, D.RadiusAt(StickerX) + 1.0, StickerT), FVector(0.0, FMath::Sin(StickerT), FMath::Cos(StickerT)), AxisX, 70.0, 3.0,
			TNPlaygroundKit::ToyColor(static_cast<int32>(Seed % 7u) + 2, 0.2f));

		// Suelo de arena por dentro (sigue por el agujero) con dunitas y unos guijarros.
		constexpr int32 FloorSteps = 12;
		const double FloorX0 = D.Xm - 2.0;
		const double FloorX1 = D.Xb + 2.0;
		for (int32 i = 0; i < FloorSteps; ++i)
		{
			const double Xa = FMath::Lerp(FloorX0, FloorX1, static_cast<double>(i) / FloorSteps);
			const double Xc = FMath::Lerp(FloorX0, FloorX1, static_cast<double>(i + 1) / FloorSteps);
			const double Wa = Xa > InnerEnd ? D.HoleHalfWidth() : FMath::Min(D.FloorHalfWidth(Xa), Xa > InnerEnd - 40.0 ? D.FloorHalfWidth(InnerEnd) : 1e9);
			const double Wc = Xc > InnerEnd ? D.HoleHalfWidth() : D.FloorHalfWidth(Xc);
			const FLinearColor Col = (i % 3 == 1) ? TNBeachTrapKit::SandSide() : TNBeachTrapKit::SandTop();
			B.AddQuad(FVector(Xa, -Wa, D.FloorZ + 0.5), FVector(Xc, -Wc, D.FloorZ + 0.5), FVector(Xc, Wc, D.FloorZ + 0.5), FVector(Xa, Wa, D.FloorZ + 0.5), FVector::UpVector, Col);
		}
		for (int32 s = 0; s < 4; ++s)
		{
			const double Xs = FMath::Lerp(D.Xm + 60.0, InnerEnd - 60.0, TNPlaygroundKit::Hash01(s, 4, Seed));
			const double Ys = (TNPlaygroundKit::Hash01(s, 5, Seed) - 0.5) * D.FloorHalfWidth(Xs) * 1.4;
			TNBeachTrapKit::AddPebble(B, FVector(Xs, Ys, D.FloorZ + 3.0), FMath::Lerp(7.0, 13.0, TNPlaygroundKit::Hash01(s, 6, Seed)), Seed + static_cast<uint32>(s),
				TNBeachTrapKit::RockTone(s));
		}
		TNPlaygroundKit::AddShellFan(B, FVector(D.Xm + 0.4 * D.Len, D.FloorHalfWidth(D.Xm + 0.4 * D.Len) * 0.5, D.FloorZ + 1.5), FVector::UpVector, FVector::ForwardVector, 18.0,
			TNBeachTrapKit::ShellTone(static_cast<int32>(Seed % 5u)));

		// Rampas: arena que entra por la boca y lengua que sale por el agujero.
		TNBeachTrapKit::AddSandRamp(B, &Hulls, FTransform(FVector(D.Xm - D.RampIn, 0.0, 0.0)), D.RampIn, D.FloorHalfWidth(D.Xm) * 0.85, 0.0, D.FloorZ, -25.0);
		TNBeachTrapKit::AddSandRamp(B, &Hulls, FTransform(FRotator(0.0, 180.0, 0.0), FVector(D.Xb + D.RampOut, 0.0, 0.0)), D.RampOut, D.HoleHalfWidth() * 0.95, 0.0,
			D.FloorZ, -25.0);

		// Colisión: pared en tramos, culo alrededor del agujero y suelo.
		for (int32 k = 0; k < HullSeg; ++k)
		{
			const double T0 = ThetaOf(k, HullSeg);
			const double T1 = ThetaOf(k + 1, HullSeg);
			TArray<FVector> Hull;
			for (const double Th : { T0, T1 })
			{
				Hull.Add(D.At(D.Xm, D.Rm, Th));
				Hull.Add(D.At(D.Xm, D.Rm - D.Wall, Th));
				Hull.Add(D.At(D.Xb, D.Rb, Th));
				Hull.Add(D.At(D.Xb, D.Rb - D.Wall, Th));
			}
			Hulls.Add(Hull);
		}
		for (int32 k = 0; k < PlateHullSeg; ++k)
		{
			const double T0 = ThetaOf(k, PlateHullSeg);
			const double T1 = ThetaOf(k + 1, PlateHullSeg);
			TArray<FVector> Hull;
			for (const double Th : { T0, T1 })
			{
				Hull.Add(D.At(InnerEnd, D.Rh, Th));
				Hull.Add(D.At(InnerEnd, D.Rb, Th));
				Hull.Add(D.At(D.Xb, D.Rh, Th));
				Hull.Add(D.At(D.Xb, D.Rb, Th));
			}
			Hulls.Add(Hull);
		}
		const double Wm = D.FloorHalfWidth(D.Xm);
		const double We = D.FloorHalfWidth(InnerEnd);
		Hulls.Add(TArray<FVector>{ FVector(D.Xm, -Wm, 0.0), FVector(D.Xm, Wm, 0.0), FVector(D.Xm, -Wm, D.FloorZ), FVector(D.Xm, Wm, D.FloorZ), FVector(InnerEnd, -We, 0.0),
			FVector(InnerEnd, We, 0.0), FVector(InnerEnd, -We, D.FloorZ), FVector(InnerEnd, We, D.FloorZ) });
		const double Wh = D.HoleHalfWidth();
		Hulls.Add(TNPlaygroundKit::HullAxisBox(FVector((InnerEnd + D.Xb) * 0.5 + 1.0, 0.0, D.FloorZ * 0.5), FVector((D.Xb - InnerEnd) * 0.5 + 1.0, Wh, D.FloorZ * 0.5)));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachBrokenBucket
// ─────────────────────────────────────────────────────────────────────────────

ATN_BeachBrokenBucket::ATN_BeachBrokenBucket()
{
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.f);

	BucketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BucketMesh"));
	BucketMesh->SetupAttachment(GetRootComponent());
	TNBeachTrapKit::ConfigureVisual(BucketMesh);

	BucketCollision = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BucketCollision"));
	BucketCollision->SetupAttachment(GetRootComponent());
	TNBeachTrapKit::ConfigureSolid(BucketCollision, true);
}

void ATN_BeachBrokenBucket::ApplySpec()
{
	using namespace TNBeachBucketDetail;
	const double Fit = TNBeachTrapKit::FitRadius(Spec.Element, Spec.SizeScale);
	const double K = FMath::Clamp(Fit / 430.0, 0.7, 1.15);
	const uint32 Seed = TNBeachTrapKit::SeedOf(Spec.Seed, 41u);
	FBucketDims D;
	D.Len = 504.0 * K;
	D.Rm = 238.0 * K;
	D.Rb = 182.0 * K;
	D.Wall = FMath::Clamp(26.0 * K, 18.0, 30.0);
	D.FloorZ = 0.38 * D.Rb;
	D.Rh = 0.74 * D.Rb;
	D.Xm = -0.5 * D.Len;
	D.Xb = 0.5 * D.Len;
	D.RampIn = 170.0 * K;
	D.RampOut = 150.0 * K;

	TNBeachTrapKit::FBuffers Bucket;
	TNBeachTrapKit::FHulls Hulls;
	BuildBucket(Bucket, Hulls, D, Seed);
	TNBeachTrapKit::SetMesh(BucketMesh, this, Bucket, TN_ART("Beach.BrokenBucket.Bucket"));
	BucketCollision->SetCollisionConvexMeshes(Hulls);
	UE_LOG(LogTortunabo, Verbose, TEXT("[Playa] Cubo %s: %.0f cm de largo, boca de %.0f, suelo a %.0f."), *GetName(), D.Len, D.Rm, D.FloorZ);
}
