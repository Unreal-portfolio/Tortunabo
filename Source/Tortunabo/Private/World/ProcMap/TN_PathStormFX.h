#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "TN_ProcMapAmbientFX.h"
#include "TN_ProcMapMeshKit.h"

/**
 * Aspecto de la tormenta del Coop según el bioma (ATN_PathStorm): lo que arrastra (arena en el desierto, hojas en
 * la selva, brasas y ceniza en el volcán, espuma en la playa, lluvia en el agua, polvo y piedrecillas en la roca,
 * hojas y lluvia en el manglar, humo y papeles en la zona humana), el color del velo del frente y la niebla y el
 * tinte de dentro. La tormenta mezcla los biomas con pesos suavizados en el tiempo: al pasar de uno a otro el
 * efecto cambia en degradado, no de golpe. Solo visual y local.
 */
namespace TNStormFX
{
	using TNAmbientFX::EShape;

	/** Una clase de partícula arrastrada por la tormenta. */
	struct FDebris
	{
		EShape Shape = EShape::Puff;
		FLinearColor Color = FLinearColor::White;
		bool bSoft = false;
		float Alpha = 0.5f;
		/** Partículas por segundo con el bioma al 100 %. */
		float Rate = 30.f;
		float Size = 12.f;
		float Speed = 900.f;
		float Gravity = -150.f;
		float Buoyancy = 0.f;
		float Drag = 0.3f;
		float LifeMin = 1.5f;
		float LifeMax = 3.f;
		int32 Max = 90;
	};

	struct FLook
	{
		/** Velo del frente (color) y niebla de dentro (color, densidad y distancia a la que ya no se ve). */
		FLinearColor Veil = FLinearColor::Gray;
		FLinearColor Fog = FLinearColor::Gray;
		float FogDensity = 0.2f;
		/** Tinte y saturación de la imagen dentro. */
		FLinearColor Tint = FLinearColor::White;
		float Saturation = 0.7f;
		FDebris Debris[2];
	};

	inline FDebris Make(EShape Shape, const FLinearColor& Color, bool bSoft, float Alpha, float Rate, float Size, float Speed, float Gravity,
		float Buoyancy, float Drag, float LifeMin, float LifeMax, int32 Max)
	{
		FDebris D;
		D.Shape = Shape; D.Color = Color; D.bSoft = bSoft; D.Alpha = Alpha; D.Rate = Rate; D.Size = Size; D.Speed = Speed;
		D.Gravity = Gravity; D.Buoyancy = Buoyancy; D.Drag = Drag; D.LifeMin = LifeMin; D.LifeMax = LifeMax; D.Max = Max;
		return D;
	}

	inline const FLook& LookFor(ETNProcBiome Biome)
	{
		static FLook Looks[TNProcMap::NumBiomes];
		static bool bInit = false;
		if (!bInit)
		{
			bInit = true;
			auto Set = [](FLook& L, const FLinearColor& Veil, const FLinearColor& Fog, float Density, const FLinearColor& Tint, float Sat, const FDebris& A, const FDebris& B)
			{
				L.Veil = Veil; L.Fog = Fog; L.FogDensity = Density; L.Tint = Tint; L.Saturation = Sat; L.Debris[0] = A; L.Debris[1] = B;
			};
			// Selva: remolino de hojas verdes y secas, bruma verdosa.
			Set(Looks[static_cast<int32>(ETNProcBiome::Jungle)], FLinearColor(0.2f, 0.3f, 0.18f), FLinearColor(0.2f, 0.28f, 0.18f), 0.22f, FLinearColor(0.85f, 1.f, 0.8f), 0.75f,
				Make(EShape::Leaf, FLinearColor(0.12f, 0.36f, 0.07f), false, 1.f, 45.f, 16.f, 900.f, -120.f, 0.f, 0.5f, 2.f, 4.f, 110),
				Make(EShape::Leaf, FLinearColor(0.5f, 0.36f, 0.08f), false, 1.f, 20.f, 14.f, 850.f, -100.f, 0.f, 0.5f, 2.f, 4.f, 60));
			// Playa: espuma y rociones del mar, bruma clara.
			Set(Looks[static_cast<int32>(ETNProcBiome::Beach)], FLinearColor(0.62f, 0.7f, 0.76f), FLinearColor(0.55f, 0.63f, 0.7f), 0.16f, FLinearColor(0.9f, 0.97f, 1.05f), 0.8f,
				Make(EShape::Drop, FLinearColor(0.85f, 0.92f, 1.f), true, 0.7f, 90.f, 7.f, 1500.f, -700.f, 0.f, 0.2f, 0.8f, 1.6f, 150),
				Make(EShape::Puff, FLinearColor(0.92f, 0.95f, 0.97f), true, 0.35f, 14.f, 160.f, 500.f, 0.f, 60.f, 0.6f, 2.f, 3.5f, 45));
			// Desierto: tormenta de arena, estelas rápidas y nubes de polvo ocre, muy cerrada.
			Set(Looks[static_cast<int32>(ETNProcBiome::Desert)], FLinearColor(0.72f, 0.52f, 0.28f), FLinearColor(0.62f, 0.44f, 0.22f), 0.36f, FLinearColor(1.1f, 0.92f, 0.7f), 0.55f,
				Make(EShape::Streak, FLinearColor(0.86f, 0.66f, 0.38f), true, 0.8f, 160.f, 40.f, 2200.f, -60.f, 0.f, 0.1f, 0.5f, 1.2f, 200),
				Make(EShape::Puff, FLinearColor(0.78f, 0.58f, 0.34f), true, 0.4f, 24.f, 260.f, 700.f, 0.f, 20.f, 0.5f, 2.f, 4.f, 60));
			// Volcán: fuego, brasas que suben y ceniza, humo rojizo.
			Set(Looks[static_cast<int32>(ETNProcBiome::Volcanic)], FLinearColor(0.35f, 0.12f, 0.06f), FLinearColor(0.22f, 0.08f, 0.05f), 0.3f, FLinearColor(1.15f, 0.8f, 0.65f), 0.7f,
				Make(EShape::Ember, FLinearColor(1.f, 0.5f, 0.12f), true, 0.95f, 80.f, 9.f, 700.f, 0.f, 420.f, 0.4f, 1.2f, 2.6f, 140),
				Make(EShape::Flake, FLinearColor(0.16f, 0.15f, 0.15f), false, 1.f, 40.f, 10.f, 700.f, -120.f, 0.f, 0.5f, 2.f, 4.f, 90));
			// Agua: cortina de lluvia y bruma azulada.
			Set(Looks[static_cast<int32>(ETNProcBiome::Water)], FLinearColor(0.4f, 0.48f, 0.58f), FLinearColor(0.36f, 0.43f, 0.52f), 0.2f, FLinearColor(0.85f, 0.95f, 1.1f), 0.7f,
				Make(EShape::Drop, FLinearColor(0.7f, 0.8f, 0.95f), true, 0.6f, 180.f, 6.f, 1800.f, -2400.f, 0.f, 0.05f, 0.6f, 1.2f, 220),
				Make(EShape::Puff, FLinearColor(0.6f, 0.68f, 0.78f), true, 0.3f, 12.f, 200.f, 450.f, 0.f, 20.f, 0.6f, 2.f, 3.5f, 40));
			// Roca: polvo gris y piedrecillas.
			Set(Looks[static_cast<int32>(ETNProcBiome::Rocky)], FLinearColor(0.46f, 0.44f, 0.41f), FLinearColor(0.42f, 0.4f, 0.37f), 0.28f, FLinearColor(1.f, 0.97f, 0.92f), 0.5f,
				Make(EShape::Ember, FLinearColor(0.32f, 0.3f, 0.28f), false, 1.f, 35.f, 9.f, 1100.f, -600.f, 0.f, 0.2f, 0.8f, 1.8f, 90),
				Make(EShape::Puff, FLinearColor(0.58f, 0.55f, 0.5f), true, 0.4f, 22.f, 240.f, 650.f, 0.f, 15.f, 0.5f, 2.f, 4.f, 60));
			// Manglar: hojas oscuras y lluvia, bruma verde.
			Set(Looks[static_cast<int32>(ETNProcBiome::Mangrove)], FLinearColor(0.26f, 0.32f, 0.26f), FLinearColor(0.24f, 0.3f, 0.25f), 0.25f, FLinearColor(0.88f, 1.f, 0.9f), 0.7f,
				Make(EShape::Leaf, FLinearColor(0.16f, 0.26f, 0.1f), false, 1.f, 30.f, 15.f, 850.f, -120.f, 0.f, 0.5f, 2.f, 4.f, 80),
				Make(EShape::Drop, FLinearColor(0.62f, 0.72f, 0.68f), true, 0.55f, 110.f, 6.f, 1600.f, -2200.f, 0.f, 0.05f, 0.6f, 1.2f, 150));
			// Zona humana (guerra): humo oscuro y papeles.
			Set(Looks[static_cast<int32>(ETNProcBiome::Human)], FLinearColor(0.24f, 0.22f, 0.2f), FLinearColor(0.2f, 0.18f, 0.16f), 0.3f, FLinearColor(1.f, 0.93f, 0.85f), 0.45f,
				Make(EShape::Puff, FLinearColor(0.14f, 0.13f, 0.12f), true, 0.55f, 26.f, 260.f, 550.f, 0.f, 90.f, 0.5f, 2.5f, 4.5f, 70),
				Make(EShape::Flake, FLinearColor(0.86f, 0.82f, 0.7f), false, 1.f, 22.f, 14.f, 900.f, -80.f, 0.f, 0.5f, 2.f, 4.f, 60));
		}
		return Looks[FMath::Clamp(static_cast<int32>(Biome), 0, TNProcMap::NumBiomes - 1)];
	}

	/** Mezcla de los aspectos con los pesos W (suman 1): velo, niebla, densidad, tinte y saturación. */
	inline void Blend(const float W[TNProcMap::NumBiomes], FLinearColor& OutVeil, FLinearColor& OutFog, float& OutDensity, FLinearColor& OutTint, float& OutSat)
	{
		OutVeil = OutFog = OutTint = FLinearColor(0.f, 0.f, 0.f, 0.f);
		OutDensity = OutSat = 0.f;
		float Sum = 0.f;
		for (int32 b = 0; b < TNProcMap::NumBiomes; ++b)
		{
			const FLook& L = LookFor(TNProcMap::BiomeFromIndex(b));
			OutVeil += L.Veil * W[b]; OutFog += L.Fog * W[b]; OutTint += L.Tint * W[b];
			OutDensity += L.FogDensity * W[b]; OutSat += L.Saturation * W[b];
			Sum += W[b];
		}
		if (Sum > 1e-4f) { OutVeil /= Sum; OutFog /= Sum; OutTint /= Sum; OutDensity /= Sum; OutSat /= Sum; }
		OutVeil.A = OutFog.A = OutTint.A = 1.f;
	}

	/**
	 * Velo del frente: Layers láminas onduladas de Width x Height (cm) una tras otra (hacia -X, dentro de la
	 * tormenta), transparentes arriba, en los extremos y a ras de suelo, con el color Color. El alfa del vértice es
	 * la opacidad (material de efectos suaves).
	 */
	inline void BuildVeil(TNProcMesh::FTNProcMeshBuffers& M, const FLinearColor& Color, double Width, double Height, int32 Layers, uint32 Seed)
	{
		const int32 NY = 28, NZ = 9;
		for (int32 l = 0; l < Layers; ++l)
		{
			const double X0 = -250.0 - 700.0 * l;
			const float Opacity = 0.95f - 0.2f * l;
			const int32 Base = M.Verts.Num();
			for (int32 k = 0; k <= NZ; ++k)
			{
				const double V = static_cast<double>(k) / NZ;
				for (int32 j = 0; j <= NY; ++j)
				{
					const double U = static_cast<double>(j) / NY;
					const double Y = (U - 0.5) * Width;
					// Ondas grandes a lo ancho y el remate curvado hacia dentro arriba.
					const double X = X0 + 260.0 * FMath::Sin(U * 9.0 + l * 1.7 + Seed * 0.1) + 180.0 * FMath::Sin(U * 23.0 + l) - 900.0 * V * V;
					const double Z = -300.0 + V * Height;
					M.Verts.Add(FVector(X, Y, Z));
					M.Normals.Add(FVector(1.0, 0.0, 0.0));
					M.UVs.Add(FVector2D(U, V));
					const float Top = 1.f - static_cast<float>(TNProcMap::SmoothStep(0.55, 1.0, V));
					const float Sides = static_cast<float>(TNProcMap::SmoothStep(0.0, 0.18, U) * TNProcMap::SmoothStep(1.0, 0.82, U));
					const float Foot = 0.55f + 0.45f * static_cast<float>(TNProcMap::SmoothStep(0.0, 0.12, V));
					FLinearColor C = Color * (0.45f + 0.5f * static_cast<float>(V));
					C.A = Opacity * Top * Sides * Foot;
					M.Colors.Add(C);
				}
			}
			for (int32 k = 0; k < NZ; ++k)
			{
				for (int32 j = 0; j < NY; ++j)
				{
					const int32 A = Base + k * (NY + 1) + j;
					const int32 B = A + 1, C = A + NY + 1, D = C + 1;
					M.Tris.Add(A); M.Tris.Add(C); M.Tris.Add(B);
					M.Tris.Add(B); M.Tris.Add(C); M.Tris.Add(D);
				}
			}
		}
	}
}
