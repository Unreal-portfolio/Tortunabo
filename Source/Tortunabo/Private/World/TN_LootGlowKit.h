#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#include "ProcMap/TN_ProcMapAmbientFX.h"

/**
 * Marca común de «aquí hay algo que coger» (Docs/Botin_Decorados.md, «Brillo de lo que se coge»): un anillo dorado de
 * guiones en el suelo que gira despacio, una columna de luz tenue y chispitas doradas que suben. La llevan los objetos
 * del suelo (UTN_PickupGlowComponent, en todos los modos) y la usan los decorados que se rebuscan (ATN_ProcSearchSpot:
 * sus chispitas y el anillo fijo que abarca su huella), para que se lea igual en el cooperativo, la carrera y el lobby.
 * El giro y la respiración del anillo (RingPose) son los mismos para unos y otros.
 *
 * Mallas construidas en ejecución una sola vez y compartidas por todos (fuera del recolector), con los materiales del
 * mapa procedural: M_ProcGlow (opaco, emisivo = color del vértice x 2: el anillo florece) y M_ProcFXSoft (translúcido
 * sin luz, opacidad del alfa del vértice: la columna). Si faltaran, no hay anillo ni columna (quedan chispitas y luz).
 */
namespace TNLootGlow
{
	/** Oro del anillo y de la luz, crema de los brillos del anillo y oro de las chispitas (el de siempre de los rebuscables). */
	inline FLinearColor Gold() { return FLinearColor(1.f, 0.72f, 0.24f); }
	inline FLinearColor Cream() { return FLinearColor(1.f, 0.94f, 0.72f); }
	inline FLinearColor SparkleColor() { return FLinearColor(1.f, 0.82f, 0.32f); }

	/** Radio (cm) de la malla del anillo con escala 1; radio de abajo y alto de la columna con escala 1. */
	constexpr float RingUnitRadius = 100.f;
	constexpr float BeamUnitRadius = 50.f;
	constexpr float BeamUnitHeight = 100.f;

	/** Radio (cm) con escala 1 en el que empiezan por dentro los guiones del anillo (la corona de fuera, 87-99; RingMesh). */
	constexpr float RingDashInnerRadius = 87.f;

	/** Giro del anillo (grados por segundo, al revés que el objeto), ritmo y hondura (fracción del radio) de su respiración. */
	constexpr float RingDegreesPerSecond = -24.f;
	constexpr float RingBreathRate = 2.4f;
	constexpr float RingBreathDepth = 0.045f;
	/** Lo que se levanta (cm) sobre el suelo, para que no parpadee contra él. */
	constexpr float RingLift = 2.5f;

	/**
	 * Pose del anillo, la misma en los objetos del suelo y en los rebuscables: a ras del suelo (GroundPoint, inclinado con él
	 * por GroundTilt), de radio Radius (cm) por Grow (0-1: crece al aparecer), girando despacio según Clock (s) y respirando
	 * (Breath, fracción del radio: RingBreathDepth * sin(Clock * RingBreathRate) de serie; los rebuscables lo cambian por
	 * un latido mientras alguien rebusca).
	 */
	inline FTransform RingPose(const FVector& GroundPoint, const FQuat& GroundTilt, float Radius, float Clock, float Grow, float Breath)
	{
		const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(Clock * RingDegreesPerSecond));
		return FTransform(GroundTilt * Spin, GroundPoint + GroundTilt.GetUpVector() * RingLift,
			FVector(Radius / RingUnitRadius * Grow * (1.f + Breath)));
	}

	/** M_ProcGlow (lo que brilla en las cuevas y las conchas de puntos). */
	inline UMaterialInterface* GlowMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcGlow.M_ProcGlow"), nullptr, LOAD_NoWarn);
		}
		return Cached.Get();
	}

	/**
	 * Anillo de radio 100 en el plano XY, a ras de Z = 0 y con las dos caras: seis guiones dorados con las puntas
	 * afiladas en la corona de fuera (87-99; es lo que deja ver el giro), un destello color crema en cada hueco y un aro
	 * fino crema por dentro (72-75). Una sola malla para todos.
	 */
	inline UStaticMesh* RingMesh()
	{
		static TWeakObjectPtr<UStaticMesh> Cached;
		if (Cached.IsValid()) { return Cached.Get(); }
		UMaterialInterface* Mat = GlowMaterial();
		if (!Mat) { return nullptr; }

		TNProcMesh::FTNProcMeshBuffers M;
		auto Polar = [](double Angle, double Radius)
		{
			return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);
		};
		auto TwoSided = [&M](const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color)
		{
			M.AddQuad(A, B, C, D, FVector::UpVector, Color);
			M.AddQuad(A, B, C, D, -FVector::UpVector, Color);
		};

		constexpr int32 Dashes = 6;
		constexpr int32 Steps = 8;
		constexpr double DashMid = 93.0;
		constexpr double DashHalfWidth = 6.0;
		const double Slot = UE_DOUBLE_TWO_PI / Dashes;
		const double Span = Slot * 0.62;
		for (int32 d = 0; d < Dashes; ++d)
		{
			const double Start = Slot * d;
			// Guion: más ancho en el centro y afilado en las puntas.
			for (int32 s = 0; s < Steps; ++s)
			{
				const double T0 = static_cast<double>(s) / Steps;
				const double T1 = static_cast<double>(s + 1) / Steps;
				const double W0 = DashHalfWidth * FMath::Sqrt(FMath::Sin(UE_DOUBLE_PI * T0));
				const double W1 = DashHalfWidth * FMath::Sqrt(FMath::Sin(UE_DOUBLE_PI * T1));
				const double A0 = Start + Span * T0;
				const double A1 = Start + Span * T1;
				TwoSided(Polar(A0, DashMid - W0), Polar(A0, DashMid + W0), Polar(A1, DashMid + W1), Polar(A1, DashMid - W1), Gold());
			}
			// Destello en el hueco: rombo pequeño, alargado hacia fuera.
			const double Gap = Start + Span + (Slot - Span) * 0.5;
			const FVector Center = Polar(Gap, DashMid);
			const FVector Radial(FMath::Cos(Gap), FMath::Sin(Gap), 0.0);
			const FVector Tangent(-Radial.Y, Radial.X, 0.0);
			TwoSided(Center - Radial * 6.5, Center + Tangent * 3.2, Center + Radial * 6.5, Center - Tangent * 3.2, Cream());
		}

		constexpr int32 CircleSegments = 40;
		for (int32 k = 0; k < CircleSegments; ++k)
		{
			const double A0 = UE_DOUBLE_TWO_PI * k / CircleSegments;
			const double A1 = UE_DOUBLE_TWO_PI * (k + 1) / CircleSegments;
			TwoSided(Polar(A0, 72.0), Polar(A0, 75.0), Polar(A1, 75.0), Polar(A1, 72.0), Cream());
		}

		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(GetTransientPackage(), M, Mat, false, 0.f, 1.f, 0.f);
		if (Mesh) { Mesh->AddToRoot(); }
		Cached = Mesh;
		return Mesh;
	}

	/**
	 * Columna de luz tenue: cono de 10 lados con las dos caras, de radio 50 abajo a 30 arriba y 100 de alto, con el alfa
	 * del vértice de 0,42 abajo a 0 arriba (se desvanece hacia arriba; el material la funde también con el suelo).
	 */
	inline UStaticMesh* BeamMesh()
	{
		static TWeakObjectPtr<UStaticMesh> Cached;
		if (Cached.IsValid()) { return Cached.Get(); }
		UMaterialInterface* Mat = TNAmbientFX::MaterialFor(true);
		if (!Mat) { return nullptr; }

		const FLinearColor Color = FMath::Lerp(Gold(), FLinearColor::White, 0.45f);
		TNProcMesh::FTNProcMeshBuffers M;
		constexpr int32 Sides = 10;
		constexpr int32 Bands = 5;
		for (int32 r = 0; r < Bands; ++r)
		{
			const double Z0 = BeamUnitHeight * r / Bands;
			const double Z1 = BeamUnitHeight * (r + 1) / Bands;
			const double Ra = FMath::Lerp(static_cast<double>(BeamUnitRadius), 30.0, static_cast<double>(r) / Bands);
			const double Rb = FMath::Lerp(static_cast<double>(BeamUnitRadius), 30.0, static_cast<double>(r + 1) / Bands);
			for (int32 s = 0; s < Sides; ++s)
			{
				const double A0 = UE_DOUBLE_TWO_PI * s / Sides;
				const double A1 = UE_DOUBLE_TWO_PI * (s + 1) / Sides;
				const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.0);
				const FVector D1(FMath::Cos(A1), FMath::Sin(A1), 0.0);
				const FVector P00 = D0 * Ra + FVector(0.0, 0.0, Z0);
				const FVector P10 = D1 * Ra + FVector(0.0, 0.0, Z0);
				const FVector P11 = D1 * Rb + FVector(0.0, 0.0, Z1);
				const FVector P01 = D0 * Rb + FVector(0.0, 0.0, Z1);
				const FVector Outward = (D0 + D1).GetSafeNormal();
				M.AddQuad(P00, P10, P11, P01, Outward, Color);
				M.AddQuad(P00, P10, P11, P01, -Outward, Color);
			}
		}
		for (int32 i = 0; i < M.Verts.Num(); ++i)
		{
			const double U = FMath::Clamp(M.Verts[i].Z / BeamUnitHeight, 0.0, 1.0);
			M.Colors[i].A = 0.42f * static_cast<float>(FMath::Pow(1.0 - U, 1.4));
		}
		// Alfa de los propios buffers (FixedAlpha = -2): la opacidad del material suave.
		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(GetTransientPackage(), M, Mat, false, 0.f, 1.f, -2.f);
		if (Mesh) { Mesh->AddToRoot(); }
		Cached = Mesh;
		return Mesh;
	}

	/**
	 * Chispitas doradas (brasas blandas que suben y se apagan): las de los decorados que se rebuscan. Los objetos del
	 * suelo usan las mismas con otro ritmo (nacen solas, más rectas hacia arriba).
	 */
	inline TNAmbientFX::FEmitterDesc SparkleDesc(int32 MaxParticles, float WakeDistance)
	{
		TNAmbientFX::FEmitterDesc Sparkle;
		Sparkle.Shape = TNAmbientFX::EShape::Ember;
		Sparkle.bSoft = true;
		Sparkle.Color = SparkleColor();
		Sparkle.Alpha = 0.95f;
		Sparkle.MaxParticles = MaxParticles;
		Sparkle.Rate = 0.f;
		Sparkle.SpawnRadius = 10.f;
		Sparkle.Speed = 60.f;
		Sparkle.SpeedJitter = 0.5f;
		Sparkle.Spread = 0.8f;
		Sparkle.Gravity = 0.f;
		Sparkle.Buoyancy = 20.f;
		Sparkle.Drag = 1.2f;
		Sparkle.LifeMin = 0.7f;
		Sparkle.LifeMax = 1.2f;
		Sparkle.SizeStart = 9.f;
		Sparkle.SizeEnd = 1.f;
		Sparkle.WakeDistance = WakeDistance;
		return Sparkle;
	}
}
