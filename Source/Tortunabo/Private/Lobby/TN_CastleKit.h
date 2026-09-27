#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectGlobals.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"

/**
 * Piezas de arena del castillo del lobby que también se usan fuera de él. La puerta doble (dos puertas con una sala en
 * medio) y el montículo de los huevos salen iguales en el lobby (ATN_SandCastleLobby) y en la salida del mapa
 * procedural. Geometría en buffers de TNProcMesh, colores de vértice para M_CosmeticVertexColor.
 */
namespace TNCastleKit
{
	using FBuffers = TNProcMesh::FTNProcMeshBuffers;

	/** Color sRGB 0xRRGGBB para M_CosmeticVertexColor en malla procedural (lineal; alfa 0 = mate, sin metal). */
	inline FLinearColor Col(uint32 Hex)
	{
		return FLinearColor(TNProcRuntimeMesh::SRGBToLinear(((Hex >> 16) & 255) / 255.f), TNProcRuntimeMesh::SRGBToLinear(((Hex >> 8) & 255) / 255.f),
			TNProcRuntimeMesh::SRGBToLinear((Hex & 255) / 255.f), 0.f);
	}

	/** Para mallas estáticas en ejecución: MakeStaticMesh decodifica una vez más (ver ATN_ShopKeeper). */
	inline FLinearColor Pal(uint32 Hex)
	{
		FLinearColor C = Col(Hex);
		C.A = 1.f;
		return C;
	}

	inline const FLinearColor& SandC() { static const FLinearColor C = Col(0xF0D49A); return C; }
	inline const FLinearColor& SandDark() { static const FLinearColor C = Col(0xD9B474); return C; }
	inline const FLinearColor& SandLight() { static const FLinearColor C = Col(0xF6E0AE); return C; }

	inline UMaterialInterface* VertexColorMaterial()
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cosmetics/Materials/M_CosmeticVertexColor.M_CosmeticVertexColor"));
		return Mat ? Mat : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	}

	inline float SmoothStep01(float X)
	{
		const float T = FMath::Clamp(X, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** Sube los buffers como sección 0 del componente (lo vacía antes), con o sin colisión. */
	inline void UploadSection(UProceduralMeshComponent* Comp, const FBuffers& B, bool bCollision, UMaterialInterface* Mat)
	{
		if (!Comp) { return; }
		Comp->ClearAllMeshSections();
		if (B.IsEmpty()) { return; }
		const TArray<FProcMeshTangent> NoTangents;
		Comp->CreateMeshSection_LinearColor(0, B.Verts, B.Tris, B.Normals, B.UVs, B.Colors, NoTangents, bCollision);
		if (Mat) { Comp->SetMaterial(0, Mat); }
	}

	/** Caja cerrada entre dos esquinas (ejes del mundo). */
	inline void AddAxisBox(FBuffers& B, const FVector& Min, const FVector& Max, const FLinearColor& Color)
	{
		B.AddBox((Min + Max) * 0.5, FVector(1.0, 0.0, 0.0), (Max - Min) * 0.5, Color);
	}

	/** Superficie de revolución de eje vertical en Center (anillos a las alturas Zs con radios Rs), hacia fuera o hacia dentro. */
	inline void AddRevolution(FBuffers& B, const FVector& Center, const TArray<double>& Zs, const TArray<double>& Rs, int32 Seg, const FLinearColor& Color,
		bool bOutward, const FLinearColor& Accent = FLinearColor::Transparent, int32 AccentEvery = 0)
	{
		for (int32 r = 0; r + 1 < Zs.Num(); ++r)
		{
			for (int32 k = 0; k < Seg; ++k)
			{
				const double A0 = TNProcMap::TwoPi * k / Seg;
				const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
				const FVector P0 = Center + FVector(FMath::Cos(A0) * Rs[r], FMath::Sin(A0) * Rs[r], Zs[r]);
				const FVector P1 = Center + FVector(FMath::Cos(A1) * Rs[r], FMath::Sin(A1) * Rs[r], Zs[r]);
				const FVector P2 = Center + FVector(FMath::Cos(A1) * Rs[r + 1], FMath::Sin(A1) * Rs[r + 1], Zs[r + 1]);
				const FVector P3 = Center + FVector(FMath::Cos(A0) * Rs[r + 1], FMath::Sin(A0) * Rs[r + 1], Zs[r + 1]);
				const FVector Mid = (P0 + P2) * 0.5;
				const FVector Radial(Mid.X - Center.X, Mid.Y - Center.Y, 0.0);
				const bool bAccent = AccentEvery > 0 && (k + r) % AccentEvery == 0 && r >= 1;
				B.AddQuad(P0, P1, P2, P3, bOutward ? Radial : -Radial, bAccent ? Accent : Color);
			}
		}
	}

	/** Torre de cubo de arena (tronco de cono con marcas), almenas, tejadillo de cono y bandera, desde la cota BaseZ. */
	inline void AddTower(FBuffers& B, FBuffers& Decor, const FVector2D& C, double TowerR, double H, const FLinearColor& FlagColor, int32 Variant, double BaseZ = 0.0)
	{
		const FVector Base(C.X, C.Y, BaseZ);
		const FVector Up(0.0, 0.0, 1.0);
		TNProcMesh::TNProcAddCylinder(B, Base, Base + Up * H, TowerR, TowerR * 0.86, 20, SandC());
		for (const double K : { 0.25, 0.52, 0.78 })
		{
			const double Rr = FMath::Lerp(TowerR, TowerR * 0.86, K) + 7.0;
			TNProcMesh::TNProcAddCylinder(B, Base + Up * (H * K - 9.0), Base + Up * (H * K + 9.0), Rr, Rr, 20, SandDark());
		}
		const double Rt = TowerR * 0.86;
		// Coronación: un anillo que vuela un poco y almenas encima.
		TNProcMesh::TNProcAddCylinder(B, Base + Up * (H - 4.0), Base + Up * (H + 30.0), Rt + 28.0, Rt + 28.0, 20, SandDark());
		const int32 Merlons = Variant % 2 ? 8 : 10;
		for (int32 k = 0; k < Merlons; ++k)
		{
			const double Ang = TNProcMap::TwoPi * k / Merlons;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.0);
			B.AddBox(Base + Dir * (Rt + 4.0) + Up * (H + 75.0), Dir, FVector(24.0, (Rt + 28.0) * 0.27, 45.0), SandC());
		}
		// Unas torres acaban en cono de arena alisado (como el molde de cubo puntiagudo) y otras en azotea con mástil.
		if (Variant % 3 == 0)
		{
			TNProcMesh::TNProcAddCylinder(B, Base + Up * (H + 30.0), Base + Up * (H + 30.0 + Rt * 1.3), Rt * 0.8, 6.0, 20, Col(0xE8C889));
		}
		const double PoleBase = Variant % 3 == 0 ? H + 30.0 + Rt * 1.3 - 10.0 : H + 30.0;
		const FVector PoleTop = Base + Up * (PoleBase + 260.0);
		TNProcMesh::TNProcAddCylinder(Decor, Base + Up * PoleBase, PoleTop, 6.0, 4.0, 6, Col(0x7A4E2B));
		const FVector F1 = PoleTop - Up * 12.0 + FVector(120.0 * FMath::Cos(Variant * 1.7), 120.0 * FMath::Sin(Variant * 1.7), -24.0);
		const FVector F2 = PoleTop - Up * 84.0;
		Decor.AddTri(PoleTop - Up * 4.0, F1, F2, FVector(-FMath::Sin(Variant * 1.7), FMath::Cos(Variant * 1.7), 0.0), FlagColor);
		Decor.AddTri(PoleTop - Up * 4.0, F1, F2, FVector(FMath::Sin(Variant * 1.7), -FMath::Cos(Variant * 1.7), 0.0), FlagColor * 0.85f);
	}

	/** Concha de vieira (abanico de costillas) con la cara hacia Normal, incrustada en una pared o en el suelo. */
	inline void AddScallop(FBuffers& Decor, const FVector& C, const FVector& Normal, const FVector& UpDir, double Size, const FLinearColor& Color)
	{
		const FVector Side = FVector::CrossProduct(Normal, UpDir).GetSafeNormal();
		for (int32 f = 0; f < 5; ++f)
		{
			const double Fa = FMath::DegreesToRadians(-60.0 + f * 30.0);
			const double Fb = FMath::DegreesToRadians(-60.0 + (f + 1) * 30.0);
			const FVector Ea = C + (Side * FMath::Sin(Fa) + UpDir * FMath::Cos(Fa)) * Size;
			const FVector Eb = C + (Side * FMath::Sin(Fb) + UpDir * FMath::Cos(Fb)) * Size;
			Decor.AddTri(C - UpDir * Size * 0.4 + Normal * 2.0, Ea + Normal * 2.0, Eb + Normal * 2.0, Normal, (f % 2) ? Color : Color * 0.85f);
		}
	}

	/** Estrella de mar plana (adorno del suelo). */
	inline void AddStarfish(FBuffers& Decor, const FVector& C, double Size, double Spin, const FLinearColor& Color)
	{
		const FVector Up(0.0, 0.0, 1.0);
		for (int32 k = 0; k < 5; ++k)
		{
			const double A0 = Spin + TNProcMap::TwoPi * k / 5.0;
			const double Am = A0 + TNProcMap::TwoPi / 10.0;
			const double A1 = A0 + TNProcMap::TwoPi / 5.0;
			const FVector Tip = C + FVector(FMath::Cos(A0) * Size, FMath::Sin(A0) * Size, 1.5);
			const FVector Inner0 = C + FVector(FMath::Cos(Am) * Size * 0.38, FMath::Sin(Am) * Size * 0.38, 2.5);
			const FVector Inner1 = C + FVector(FMath::Cos(A1 - TNProcMap::TwoPi / 10.0) * Size * 0.38, FMath::Sin(A1 - TNProcMap::TwoPi / 10.0) * Size * 0.38, 2.5);
			Decor.AddTri(C + Up * 4.0, Inner1, Tip, Up, Color);
			Decor.AddTri(C + Up * 4.0, Tip, Inner0, Up, Color * 0.9f);
		}
	}

	/** Antorcha de pared: soporte de madera, cazoleta de hierro y llama; la luz la pone el actor. Out apunta fuera del muro. */
	inline void AddWallTorch(FBuffers& Decor, const FVector& WallPoint, const FVector& Out)
	{
		const FVector Up(0.0, 0.0, 1.0);
		const FVector Tip = WallPoint + Out * 38.0 + Up * 26.0;
		Decor.AddBeam(WallPoint - Up * 18.0, Tip, 5.0, Col(0x7A4E2B));
		TNProcMesh::TNProcAddCylinder(Decor, Tip - Up * 4.0, Tip + Up * 12.0, 9.0, 13.0, 8, Col(0x3B3F4A));
		TNProcMesh::TNProcAddCylinder(Decor, Tip + Up * 10.0, Tip + Up * 40.0, 11.0, 2.0, 7, Col(0xFF8A2A));
		TNProcMesh::TNProcAddCylinder(Decor, Tip + Up * 12.0, Tip + Up * 30.0, 6.0, 1.0, 6, Col(0xFFE27A));
	}

	/**
	 * Hoja de puerta de madera de 0 a Width en +X (bisagra en el origen), de Height de alto y sin rendijas: tablones sobre
	 * un tablero de fondo, tres herrajes y aldabas doradas por las dos caras.
	 */
	inline void BuildLeaf(FBuffers& B, double Width, double Height)
	{
		const FLinearColor WoodA = Pal(0xB07A4A);
		const FLinearColor WoodB = Pal(0x9A6538);
		const FLinearColor Back = Pal(0x6E4526);
		const FLinearColor Iron = Pal(0x3B3F4A);
		const FLinearColor Gold = Pal(0xFFCB3D);
		B.AddBox(FVector(Width * 0.5, 0.0, Height * 0.5), FVector(1.0, 0.0, 0.0), FVector(Width * 0.5, 11.0, Height * 0.5), Back);
		const int32 Planks = FMath::Max(3, static_cast<int32>(Width / 70.0));
		const double PlankW = Width / Planks;
		for (int32 p = 0; p < Planks; ++p)
		{
			const double X = (p + 0.5) * PlankW;
			B.AddBox(FVector(X, 0.0, Height * 0.5), FVector(1.0, 0.0, 0.0), FVector(PlankW * 0.5 - 2.0, 15.0, Height * 0.5), (p % 2) ? WoodA : WoodB);
		}
		for (const double Z : { Height * 0.18, Height * 0.5, Height * 0.8 })
		{
			B.AddBox(FVector(Width * 0.5, 0.0, Z), FVector(1.0, 0.0, 0.0), FVector(Width * 0.5 - 6.0, 18.0, 9.0), Iron);
		}
		TNProcMesh::TNProcAddCylinder(B, FVector(Width - 55.0, -18.0, Height * 0.42), FVector(Width - 55.0, -30.0, Height * 0.42), 22.0, 18.0, 10, Gold);
		TNProcMesh::TNProcAddCylinder(B, FVector(Width - 55.0, 18.0, Height * 0.42), FVector(Width - 55.0, 30.0, Height * 0.42), 22.0, 18.0, 10, Gold);
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Huevos
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Perfil del huevo (altura, radio); la costura entre la base y la tapa está en EggSeam. */
	constexpr double EggSeam = 110.0;
	inline const double* EggZ() { static const double Z[] = { 0.0, 25.0, 60.0, 110.0, 150.0, 190.0, 222.0, 238.0 }; return Z; }
	inline const double* EggR() { static const double R[] = { 55.0, 84.0, 97.0, 96.0, 88.0, 68.0, 40.0, 12.0 }; return R; }
	constexpr int32 EggProfileNum = 8;
	/** Color de cada huevo de la pila (turquesa, coral, amarillo y lila). */
	inline uint32 EggAccent(int32 Index)
	{
		static const uint32 Accents[4] = { 0x2EC4B6, 0xFF6A52, 0xFFCB3D, 0x9B5DE5 };
		return Accents[((Index % 4) + 4) % 4];
	}

	/** Base (media cáscara con borde en zigzag) de un huevo en Center, con su nido de paja. */
	inline void BuildEggCup(FBuffers& Decor, const FVector& Center, const FLinearColor& Shell, const FLinearColor& Accent)
	{
		TArray<double> Zs, Rs, Ri;
		for (int32 i = 0; i < EggProfileNum; ++i)
		{
			if (EggZ()[i] > EggSeam + 0.1) { break; }
			Zs.Add(EggZ()[i]);
			Rs.Add(EggR()[i]);
			Ri.Add(EggR()[i] - 5.0);
		}
		constexpr int32 Seg = 16;
		AddRevolution(Decor, Center, Zs, Rs, Seg, Shell, true, Accent, 3);
		AddRevolution(Decor, Center, Zs, Ri, Seg, Shell * 0.88f, false);
		const double Rim = Rs.Last() * 0.955;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg;
			const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			const double Am = (A0 + A1) * 0.5;
			const FVector P0 = Center + FVector(FMath::Cos(A0) * Rim, FMath::Sin(A0) * Rim, EggSeam);
			const FVector P1 = Center + FVector(FMath::Cos(A1) * Rim, FMath::Sin(A1) * Rim, EggSeam);
			const FVector Tip = Center + FVector(FMath::Cos(Am) * Rim, FMath::Sin(Am) * Rim, EggSeam + 18.0);
			const FVector Radial(FMath::Cos(Am), FMath::Sin(Am), 0.0);
			Decor.AddTri(P0, P1, Tip, Radial, Shell);
			Decor.AddTri(P0, P1, Tip, -Radial, Shell * 0.88f);
		}
		for (int32 n = 0; n < 14; ++n)
		{
			const double A = TNProcMap::TwoPi * n / 14.0;
			const FVector P0 = Center + FVector(FMath::Cos(A) * 118.0, FMath::Sin(A) * 118.0, 6.0);
			const FVector P1 = Center + FVector(FMath::Cos(A + 0.7) * 110.0, FMath::Sin(A + 0.7) * 110.0, 14.0 + 6.0 * (n % 2));
			Decor.AddBeam(P0, P1, 4.0, (n % 2) ? Col(0xD9B45A) : Col(0xC49A45));
		}
	}

	/** Tapa del huevo con su origen en el centro de la costura (dientes hacia abajo entre los de la base). */
	inline void BuildEggLid(FBuffers& B, const FLinearColor& Shell, const FLinearColor& Accent)
	{
		TArray<double> Zs, Rs, Ri;
		for (int32 i = 0; i < EggProfileNum; ++i)
		{
			if (EggZ()[i] < EggSeam - 0.1) { continue; }
			Zs.Add(EggZ()[i] - EggSeam);
			Rs.Add(EggR()[i]);
			Ri.Add(FMath::Max(2.0, EggR()[i] - 5.0));
		}
		constexpr int32 Seg = 16;
		AddRevolution(B, FVector::ZeroVector, Zs, Rs, Seg, Shell, true, Accent, 4);
		AddRevolution(B, FVector::ZeroVector, Zs, Ri, Seg, Shell * 0.88f, false);
		const FVector Top(0.0, 0.0, Zs.Last());
		const double Half = TNProcMap::TwoPi / Seg * 0.5;
		const double Rim = Rs[0] * 1.025;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg;
			const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			B.AddTri(Top + FVector(0.0, 0.0, 3.0), FVector(FMath::Cos(A0) * Rs.Last(), FMath::Sin(A0) * Rs.Last(), Zs.Last()),
				FVector(FMath::Cos(A1) * Rs.Last(), FMath::Sin(A1) * Rs.Last(), Zs.Last()), FVector(0.0, 0.0, 1.0), Shell);
			const FVector Prev(FMath::Cos(A0 - Half) * Rim, FMath::Sin(A0 - Half) * Rim, 2.0);
			const FVector Next(FMath::Cos(A0 + Half) * Rim, FMath::Sin(A0 + Half) * Rim, 2.0);
			const FVector Tip(FMath::Cos(A0) * Rim, FMath::Sin(A0) * Rim, -18.0);
			const FVector Radial(FMath::Cos(A0), FMath::Sin(A0), 0.0);
			B.AddTri(Prev, Next, Tip, Radial, Shell);
			B.AddTri(Prev, Next, Tip, -Radial, Shell * 0.88f);
		}
	}

	/** Montículo de dos alturas para la pila de cuatro huevos (medidas en cm). */
	namespace EggMound
	{
		constexpr int32 NumEggs = 4;
		constexpr double Tier1R = 430.0;
		constexpr double Tier1H = 60.0;
		constexpr double Tier2R = 175.0;
		constexpr double Tier2H = 200.0;
		/** Radio al que van los tres huevos del piso bajo. */
		constexpr double RingR = 265.0;
	}

	/**
	 * Base del huevo Index de un montículo con centro Center (en el suelo, a la cota FloorZ): tres en el piso bajo y el
	 * último arriba. StepDir apunta al escalón de subida (el lado por donde se llega).
	 */
	inline FVector EggMoundSpot(int32 Index, const FVector& Center, double FloorZ)
	{
		if (Index >= EggMound::NumEggs - 1)
		{
			return FVector(Center.X, Center.Y, FloorZ + EggMound::Tier2H);
		}
		const double A = TNProcMap::TwoPi * Index / 3.0 + 0.35;
		return FVector(Center.X + FMath::Cos(A) * EggMound::RingR, Center.Y + FMath::Sin(A) * EggMound::RingR, FloorZ + EggMound::Tier1H);
	}

	/**
	 * Montículo de arena de dos alturas con un escalón (tocón con una vieira encima) hacia StepDir y estrellas y conchas
	 * alrededor. B lleva la colisión; Decor, los adornos.
	 */
	inline void BuildEggMound(FBuffers& B, FBuffers& Decor, const FVector& Center, double FloorZ, const FVector& StepDir)
	{
		using namespace EggMound;
		const FVector Up(0.0, 0.0, 1.0);
		const FVector C(Center.X, Center.Y, 0.0);
		TNProcMesh::TNProcAddCylinder(B, C, C + Up * (FloorZ + Tier1H), Tier1R, Tier1R - 20.0, 32, SandC());
		TNProcMesh::TNProcAddCylinder(B, C + Up * (FloorZ + Tier1H - 12.0), C + Up * (FloorZ + Tier1H), Tier1R - 16.0, Tier1R - 20.0, 32, SandDark());
		TNProcMesh::TNProcAddCylinder(B, C + Up * (FloorZ + Tier1H), C + Up * (FloorZ + Tier2H), Tier2R + 25.0, Tier2R, 24, SandC());
		// Escalón: tocón de arena con reborde y una vieira grande encima, abierta hacia fuera.
		const FVector StepOut = FVector(StepDir.X, StepDir.Y, 0.0).GetSafeNormal();
		const FVector StepP = C + StepOut * (Tier2R + 110.0);
		TNProcMesh::TNProcAddCylinder(B, StepP + Up * (FloorZ + Tier1H), StepP + Up * (FloorZ + 132.0), 78.0, 70.0, 16, SandC());
		TNProcMesh::TNProcAddCylinder(B, StepP + Up * (FloorZ + 118.0), StepP + Up * (FloorZ + 132.0), 74.0, 72.0, 16, SandDark());
		AddScallop(Decor, StepP + Up * (FloorZ + 132.5) - StepOut * 20.0, Up, StepOut, 66.0, Col(0xFFB4A2));
		// Estrellas y conchas alrededor del montículo.
		for (int32 i = 0; i < 8; ++i)
		{
			const double A = TNProcMap::TwoPi * i / 8.0 + 0.2;
			const FVector P = C + FVector(FMath::Cos(A) * (Tier1R + 60.0), FMath::Sin(A) * (Tier1R + 60.0), FloorZ);
			if (i % 2 == 0) { AddStarfish(Decor, P, 26.0, A, (i % 4) ? Col(0xFF8A70) : Col(0xFFB077)); }
			else { AddScallop(Decor, P + Up * 3.0, Up, FVector(FMath::Cos(A), FMath::Sin(A), 0.0), 24.0, Col(0xFFE0C2)); }
		}
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Puerta doble
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/**
	 * Puerta doble (cm, locales): origen en el centro del umbral de la puerta 1 y +Y hacia la puerta 2. Entre las dos,
	 * una sala de 9,2 × 7 m abierta al cielo con dos torres grandes a los lados de la puerta 1 y dos torreones a los de la
	 * puerta 2. Cada puerta tiene dos hojas con la bisagra en X = ±HalfW; los pilares tapan los cantos (sin rendijas).
	 */
	namespace Gatehouse
	{
		/** Medio hueco de cada puerta (lo que mide cada hoja) y alto del hueco. */
		constexpr double HalfW = 430.0;
		constexpr double GateH = 540.0;
		/** De la puerta 1 (Y = 0) a la puerta 2 (Y = Depth). */
		constexpr double Depth = 700.0;
		/** Pilares a los lados de cada hueco y medio grosor de las fachadas. */
		constexpr double JambW = 110.0;
		constexpr double FacadeHalfT = 60.0;
		/** Alto de las fachadas (dintel) y de las paredes de la sala. */
		constexpr double FacadeH = 760.0;
		constexpr double WallH = 640.0;
		/** Paredes de la sala: cara de dentro y grosor. */
		constexpr double WallIn = HalfW + 30.0;
		constexpr double WallT = 150.0;
		/** Torres grandes (puerta 1) y torreones (puerta 2). */
		constexpr double BigTowerX = 740.0;
		constexpr double BigTowerY = -40.0;
		constexpr double BigTowerR = 290.0;
		constexpr double BigTowerH = 1350.0;
		constexpr double TurretX = 600.0;
		constexpr double TurretY = Depth + 40.0;
		constexpr double TurretR = 150.0;
		constexpr double TurretH = 980.0;
		/** Medio ancho del suelo (hasta la cara de fuera de las paredes). */
		constexpr double FloorHalfW = WallIn + WallT;
		/** Sala: la zona donde se está dentro (listo en el lobby). */
		constexpr double RoomHalfW = HalfW - 30.0;
		constexpr double RoomY0 = 70.0;
		constexpr double RoomY1 = Depth - 70.0;
		constexpr double RoomTopZ = 450.0;
		/** Cota del cartel de cada fachada (centro). */
		constexpr double SignZ = GateH + 112.0;

		/** Dentro de la sala (posición local de la puerta doble, con Z sobre su suelo). */
		inline bool IsInRoom(const FVector& Local)
		{
			return FMath::Abs(Local.X) < RoomHalfW && Local.Y > RoomY0 && Local.Y < RoomY1 && Local.Z > -60.0 && Local.Z < RoomTopZ;
		}

		/** Cuatro sitios de salida dentro de la sala (locales, a ras de suelo), mirando a la puerta 2 (+Y). */
		inline FVector SpawnSpot(int32 Index)
		{
			const double X = (Index % 2 == 0) ? -190.0 : 190.0;
			const double Y = (Index / 2 == 0) ? Depth * 0.62 : Depth * 0.34;
			return FVector(X, Y, 0.0);
		}
	}

	/**
	 * Fachada de una puerta (pilares, dintel con almenas y arco de dovelas por la cara Face = ±1 en Y) en Y = GateY. Los
	 * pilares bajan Sink por debajo del origen (terreno irregular).
	 */
	inline void AddGatehouseFacade(FBuffers& B, FBuffers& Decor, const FVector& O, double GateY, double Face, double FloorZ, double Sink = 0.0)
	{
		using namespace Gatehouse;
		const double Y0 = GateY - FacadeHalfT, Y1 = GateY + FacadeHalfT;
		for (const double Sx : { -1.0, 1.0 })
		{
			const double XA = Sx * HalfW, XB = Sx * (HalfW + JambW);
			AddAxisBox(B, O + FVector(FMath::Min(XA, XB), Y0, -Sink), O + FVector(FMath::Max(XA, XB), Y1, FloorZ + GateH), SandC());
		}
		AddAxisBox(B, O + FVector(-(HalfW + JambW), Y0, FloorZ + GateH), O + FVector(HalfW + JambW, Y1, FloorZ + FacadeH), SandC());
		// Marcas de cubo en la fachada, por las dos caras.
		for (const double Z : { FloorZ + GateH + 60.0, FloorZ + FacadeH - 50.0 })
		{
			AddAxisBox(B, O + FVector(-(HalfW + JambW) - 6.0, Y0 - 7.0, Z - 8.0), O + FVector(HalfW + JambW + 6.0, Y1 + 7.0, Z + 8.0), SandDark());
		}
		// Almenas encima, a lo largo del dintel.
		for (int32 m = -4; m <= 4; ++m)
		{
			B.AddBox(O + FVector(m * 115.0, GateY, FloorZ + FacadeH + 45.0), FVector(1.0, 0.0, 0.0), FVector(36.0, FacadeHalfT - 8.0, 45.0), SandC());
		}
		// Arco de medio punto de dovelas por la cara que se ve de frente.
		const double FaceY = GateY + Face * (FacadeHalfT + 4.0);
		for (int32 s = 0; s < 12; ++s)
		{
			const double T0 = PI * s / 12.0, T1 = PI * (s + 1) / 12.0;
			const FVector C0 = O + FVector(-HalfW * FMath::Cos(T0), FaceY, FloorZ + GateH - 40.0 + 80.0 * FMath::Sin(T0));
			const FVector C1 = O + FVector(-HalfW * FMath::Cos(T1), FaceY, FloorZ + GateH - 40.0 + 80.0 * FMath::Sin(T1));
			const FVector D = FVector(C1.X - C0.X, 0.0, 0.0).GetSafeNormal();
			Decor.AddBox((C0 + C1) * 0.5, D.IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : D, FVector(FVector::Dist(C0, C1) * 0.5, 8.0, 18.0),
				(s % 2) ? SandDark() : Col(0xCFA766));
		}
		// Cartel de madera con marco dorado sobre el arco (el rótulo lo pone el actor).
		const FVector SignC = O + FVector(0.0, GateY + Face * (FacadeHalfT + 10.0), FloorZ + SignZ);
		Decor.AddBox(SignC, FVector(1.0, 0.0, 0.0), FVector(300.0, 8.0, 50.0), Col(0x8C5A33));
		Decor.AddBox(SignC - FVector(0.0, Face * 3.0, 0.0), FVector(1.0, 0.0, 0.0), FVector(312.0, 8.0, 62.0), Col(0xFFCB3D));
		for (int32 p = 0; p < 5; ++p)
		{
			Decor.AddBox(SignC + FVector(-240.0 + p * 120.0, Face * 7.0, 0.0), FVector(1.0, 0.0, 0.0), FVector(58.0, 2.0, 46.0), (p % 2) ? Col(0x9A6538) : Col(0xA8713F));
		}
	}

	/** Centro del rótulo del cartel de la fachada de la puerta en GateY, por la cara Face (locales de la puerta doble). */
	inline FVector GatehouseSignText(double GateY, double Face, double FloorZ)
	{
		return FVector(0.0, GateY + Face * (Gatehouse::FacadeHalfT + 20.0), FloorZ + Gatehouse::SignZ);
	}

	/**
	 * Puerta doble completa en O (centro del umbral de la puerta 1, a ras de suelo; +Y hacia la puerta 2): suelo, dos
	 * fachadas, paredes de la sala con almenas, marcas, conchas y antorchas, torres y torreones con bandera y barreras
	 * invisibles por encima para que nadie salga de la sala saltando. B lleva la colisión; Decor, los adornos; Barrier,
	 * las barreras. Las hojas van aparte (BuildLeaf, bisagras en (±HalfW, 0) y (±HalfW, Depth)). Con Sink > 0, paredes,
	 * pilares y torres bajan esa cota por debajo del origen y el suelo lleva un zócalo (para terreno irregular).
	 */
	inline void BuildGatehouse(FBuffers& B, FBuffers& Decor, FBuffers& Barrier, const FVector& O, double FloorZ, int32 FlagSeed, double Sink = 0.0)
	{
		using namespace Gatehouse;
		const FVector Up(0.0, 0.0, 1.0);
		// Suelo de la sala y de los umbrales, algo más claro, con cuatro estrellas donde se aparece.
		B.AddQuad(O + FVector(-FloorHalfW, -FacadeHalfT, FloorZ), O + FVector(FloorHalfW, -FacadeHalfT, FloorZ),
			O + FVector(FloorHalfW, Depth + FacadeHalfT, FloorZ), O + FVector(-FloorHalfW, Depth + FacadeHalfT, FloorZ), Up, Col(0xF3DDA6));
		if (Sink > 0.0)
		{
			// Zócalo bajo el suelo: tapa el hueco con el terreno si queda por debajo.
			AddAxisBox(B, O + FVector(-FloorHalfW, -FacadeHalfT, -Sink), O + FVector(FloorHalfW, Depth + FacadeHalfT, FloorZ - 1.0), SandDark());
		}
		for (int32 i = 0; i < 4; ++i)
		{
			AddStarfish(Decor, O + SpawnSpot(i) + FVector(0.0, 0.0, FloorZ), 34.0, 0.3 + i * 1.1, (i % 2) ? Col(0xFF8A70) : Col(0xFFB077));
		}
		// Las dos fachadas: la puerta 1 se ve desde -Y y la 2 desde +Y.
		AddGatehouseFacade(B, Decor, O, 0.0, -1.0, FloorZ, Sink);
		AddGatehouseFacade(B, Decor, O, Depth, 1.0, FloorZ, Sink);
		// Paredes de la sala, con almenas por fuera, marcas de cubo por dentro, conchas y una antorcha en cada una.
		const FLinearColor ShellColors[3] = { Col(0xFFB4A2), Col(0xFFE0C2), Col(0xE6D0FF) };
		for (const double Sx : { -1.0, 1.0 })
		{
			const double XIn = Sx * WallIn, XOut = Sx * (WallIn + WallT);
			AddAxisBox(B, O + FVector(FMath::Min(XIn, XOut), FacadeHalfT, -Sink), O + FVector(FMath::Max(XIn, XOut), Depth - FacadeHalfT, FloorZ + WallH), SandC());
			for (const double Z : { 150.0, 320.0, 470.0 })
			{
				const double XBand = XIn - Sx * 7.0;
				AddAxisBox(B, O + FVector(FMath::Min(XIn, XBand), FacadeHalfT, FloorZ + Z - 8.0), O + FVector(FMath::Max(XIn, XBand), Depth - FacadeHalfT, FloorZ + Z + 8.0), SandDark());
			}
			for (double Y = FacadeHalfT + 70.0; Y < Depth - FacadeHalfT - 40.0; Y += 150.0)
			{
				B.AddBox(O + FVector(Sx * (WallIn + WallT - 26.0), Y, FloorZ + WallH + 42.0), FVector(0.0, 1.0, 0.0), FVector(40.0, 24.0, 42.0), SandC());
			}
			for (int32 s = 0; s < 3; ++s)
			{
				AddScallop(Decor, O + FVector(XIn - Sx * 1.0, Depth * (0.22 + 0.28 * s), FloorZ + 240.0 + 70.0 * (s % 2)), FVector(-Sx, 0.0, 0.0), Up, 26.0,
					ShellColors[s]);
			}
			AddWallTorch(Decor, O + FVector(XIn, Depth * 0.5, FloorZ + 330.0), FVector(-Sx, 0.0, 0.0));
			// Barrera: pared invisible encima de la de la sala.
			AddAxisBox(Barrier, O + FVector(FMath::Min(XIn, XOut), -FacadeHalfT, FloorZ + WallH), O + FVector(FMath::Max(XIn, XOut), Depth + FacadeHalfT, FloorZ + WallH + 1200.0), SandC());
		}
		for (const double GY : { 0.0, Depth })
		{
			AddAxisBox(Barrier, O + FVector(-(HalfW + JambW), GY - FacadeHalfT, FloorZ + FacadeH), O + FVector(HalfW + JambW, GY + FacadeHalfT, FloorZ + FacadeH + 1200.0), SandC());
		}
		// Torres grandes junto a la puerta 1 y torreones junto a la puerta 2.
		static const uint32 Flags[4] = { 0xFF6A52, 0x2EC4B6, 0xFFCB3D, 0x9B5DE5 };
		int32 Variant = FlagSeed;
		for (const double Sx : { -1.0, 1.0 })
		{
			const FVector2D Big(O.X + Sx * BigTowerX, O.Y + BigTowerY);
			AddTower(B, Decor, Big, BigTowerR, BigTowerH + Sink, Col(Flags[(Variant & 3)]), Variant, O.Z - Sink);
			TNProcMesh::TNProcAddCylinder(Barrier, FVector(Big.X, Big.Y, O.Z + BigTowerH), FVector(Big.X, Big.Y, O.Z + BigTowerH + 900.0), BigTowerR + 40.0, BigTowerR + 40.0, 12, SandC(), false);
			++Variant;
			const FVector2D Small(O.X + Sx * TurretX, O.Y + TurretY);
			AddTower(B, Decor, Small, TurretR, TurretH + Sink, Col(Flags[(Variant & 3)]), Variant * 3, O.Z - Sink);
			TNProcMesh::TNProcAddCylinder(Barrier, FVector(Small.X, Small.Y, O.Z + TurretH), FVector(Small.X, Small.Y, O.Z + TurretH + 900.0), TurretR + 40.0, TurretR + 40.0, 12, SandC(), false);
			++Variant;
		}
	}
}
