#include "Lobby/TN_SandCastleLobby.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_Log.h"
#include "Core/TN_MatchFlowTypes.h"
#include "Lobby/TN_HQGameMode.h"
#include "Lobby/TN_LobbyReadyZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"

namespace TNCastleDetail
{
	TAutoConsoleVariable<int32> CVarLobbyCastle(TEXT("TN.Lobby.Castle"), 1,
		TEXT("1 = el lobby es el castillo de arena (ATN_SandCastleLobby); 0 = escondido (vuelve a cargar el lobby)."));

	using FBuffers = TNProcMesh::FTNProcMeshBuffers;

	/** Color sRGB 0xRRGGBB para M_CosmeticVertexColor en malla procedural (lineal; alfa 0 = mate, sin metal). */
	FLinearColor Col(uint32 Hex)
	{
		return FLinearColor(TNProcRuntimeMesh::SRGBToLinear(((Hex >> 16) & 255) / 255.f), TNProcRuntimeMesh::SRGBToLinear(((Hex >> 8) & 255) / 255.f),
			TNProcRuntimeMesh::SRGBToLinear((Hex & 255) / 255.f), 0.f);
	}

	/** Para mallas estáticas en ejecución: MakeStaticMesh decodifica una vez más (ver ATN_ShopKeeper). */
	FLinearColor Pal(uint32 Hex)
	{
		FLinearColor C = Col(Hex);
		C.A = 1.f;
		return C;
	}

	// ── Medidas (cm, locales del castillo: centro del círculo en el origen, +Y es la puerta) ──
	constexpr double R = ATN_SandCastleLobby::Radius;
	constexpr double WallT = 190.0;
	constexpr double RO = R + WallT;
	constexpr double RM = R + WallT * 0.5;
	constexpr double WallH = 600.0;
	constexpr double FloorZ = 2.0;
	constexpr double GateHalfW = 430.0;
	constexpr double GateH = 540.0;
	/** Plano de la puerta (y) entre la cara de dentro y la de fuera. */
	constexpr double GateY = R + 70.0;
	constexpr double CutY = ATN_SandCastleLobby::CutY;
	constexpr double CutHalfT = 85.0;
	constexpr double CutH = 600.0;
	/** Torre del homenaje en el centro del muro interior. */
	constexpr double KeepR = 400.0;
	constexpr double KeepRoofZ = 900.0;
	constexpr double DoorHalfW = 130.0;
	constexpr double DoorH = 340.0;
	/** Escalera de caracol: por fuera de la torre, del suelo (lado oeste) a la azotea (lado este) pasando sobre la puerta. */
	constexpr double StairIn = KeepR;
	constexpr double StairOut = KeepR + 210.0;
	constexpr int32 StairSteps = 36;
	/** Pila de huevos: montículo de dos alturas en la plaza. */
	const FVector2D EggsCenter(0.0, 700.0);
	constexpr double Tier1R = 430.0;
	constexpr double Tier1H = 60.0;
	constexpr double Tier2R = 175.0;
	constexpr double Tier2H = 200.0;

	/** Ángulo (radianes, sentido de las agujas del reloj desde +Y) de una hora del reloj. */
	double ClockAngle(double Hour)
	{
		return Hour / 12.0 * TNProcMap::TwoPi;
	}

	/** Punto a la hora Clock y a Dist del centro (12 = +Y, 3 = -X). */
	FVector2D ClockPoint(double Hour, double Dist)
	{
		const double A = ClockAngle(Hour);
		return FVector2D(-Dist * FMath::Sin(A), Dist * FMath::Cos(A));
	}

	/** Huevos: posición local (x, y) y cota de su base. El de arriba es el último. */
	FVector EggSpot(int32 Index)
	{
		if (Index >= ATN_SandCastleLobby::NumEggs - 1)
		{
			return FVector(EggsCenter.X, EggsCenter.Y, FloorZ + Tier2H);
		}
		const double A = TNProcMap::TwoPi * Index / 3.0 + 0.35;
		return FVector(EggsCenter.X + FMath::Cos(A) * 265.0, EggsCenter.Y + FMath::Sin(A) * 265.0, FloorZ + Tier1H);
	}
	const uint32 EggAccents[ATN_SandCastleLobby::NumEggs] = { 0x2EC4B6, 0xFF6A52, 0xFFCB3D, 0x9B5DE5 };

	/** Perfil del huevo (altura, radio); la costura entre la base y la tapa está en EggSeam. */
	constexpr double EggSeam = 110.0;
	const double EggZ[] = { 0.0, 25.0, 60.0, 110.0, 150.0, 190.0, 222.0, 238.0 };
	const double EggR[] = { 55.0, 84.0, 97.0, 96.0, 88.0, 68.0, 40.0, 12.0 };

	/** Torres de la muralla: hora del reloj, radio y alto (irregulares a propósito) y color de la bandera. */
	struct FTowerDef
	{
		double Clock;
		double Radius;
		double Height;
		uint32 Flag;
	};
	const FTowerDef Towers[] = {
		{ 11.43, 290.0, 1350.0, 0xFF6A52 }, { 0.57, 290.0, 1350.0, 0x2EC4B6 },
		{ 1.62, 230.0, 980.0, 0xFFCB3D }, { 2.85, 200.0, 760.0, 0x9B5DE5 },
		{ 3.66, 300.0, 1180.0, 0x4CC9F0 }, { 4.62, 210.0, 820.0, 0xFF8FB1 },
		{ 6.0, 265.0, 1060.0, 0x3DDC62 }, { 7.32, 200.0, 760.0, 0xFFB077 },
		{ 8.34, 300.0, 1250.0, 0xFF6A52 }, { 9.2, 220.0, 880.0, 0x2EC4B6 },
		{ 10.35, 250.0, 960.0, 0xFFCB3D },
	};

	/** Superficie de revolución de eje vertical en Center (anillos a las alturas Zs con radios Rs), hacia fuera o hacia dentro. */
	void AddRevolution(FBuffers& B, const FVector& Center, const TArray<double>& Zs, const TArray<double>& Rs, int32 Seg, const FLinearColor& Color,
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

	/** Alto de la muralla en el ángulo A (radianes): ondulado suave entre torres, para que no sea una tapia recta. */
	double WallHeightAt(double A)
	{
		return WallH + 45.0 * FMath::Sin(A * 3.0 + 0.7) + 25.0 * FMath::Sin(A * 7.0 + 2.1);
	}

	/** Cierto si el ángulo A (radianes) cae en el hueco de la puerta grande. */
	bool InGate(double A)
	{
		double X = FMath::Fmod(A, TNProcMap::TwoPi);
		if (X > PI) { X -= TNProcMap::TwoPi; }
		if (X < -PI) { X += TNProcMap::TwoPi; }
		return FMath::Abs(R * FMath::Sin(X)) < GateHalfW + 20.0 && FMath::Cos(X) > 0.0;
	}

	/** Torre de cubo de arena (tronco de cono con marcas), almenas, tejadillo de cono y bandera, desde la cota BaseZ. */
	void AddTower(FBuffers& B, FBuffers& Decor, const FVector2D& C, double TowerR, double H, const FLinearColor& FlagColor, int32 Variant, double BaseZ = 0.0)
	{
		const FLinearColor SandC = Col(0xF0D49A);
		const FLinearColor SandDark = Col(0xD9B474);
		const FVector Base(C.X, C.Y, BaseZ);
		const FVector Up(0.0, 0.0, 1.0);
		TNProcMesh::TNProcAddCylinder(B, Base, Base + Up * H, TowerR, TowerR * 0.86, 20, SandC);
		for (const double K : { 0.25, 0.52, 0.78 })
		{
			const double Rr = FMath::Lerp(TowerR, TowerR * 0.86, K) + 7.0;
			TNProcMesh::TNProcAddCylinder(B, Base + Up * (H * K - 9.0), Base + Up * (H * K + 9.0), Rr, Rr, 20, SandDark);
		}
		const double Rt = TowerR * 0.86;
		// Coronación: un anillo que vuela un poco y almenas encima.
		TNProcMesh::TNProcAddCylinder(B, Base + Up * (H - 4.0), Base + Up * (H + 30.0), Rt + 28.0, Rt + 28.0, 20, SandDark);
		const int32 Merlons = Variant % 2 ? 8 : 10;
		for (int32 k = 0; k < Merlons; ++k)
		{
			const double Ang = TNProcMap::TwoPi * k / Merlons;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.0);
			B.AddBox(Base + Dir * (Rt + 4.0) + Up * (H + 75.0), Dir, FVector(24.0, (Rt + 28.0) * 0.27, 45.0), SandC);
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
	void AddScallop(FBuffers& Decor, const FVector& C, const FVector& Normal, const FVector& UpDir, double Size, const FLinearColor& Color)
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
	void AddStarfish(FBuffers& Decor, const FVector& C, double Size, double Spin, const FLinearColor& Color)
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

	/** Hoja de puerta de madera: tablones verticales de 0 a Width en +X, con herrajes y un tirador de concha. */
	void BuildLeaf(FBuffers& B, double Width, double Height)
	{
		const FLinearColor WoodA = Pal(0xB07A4A);
		const FLinearColor WoodB = Pal(0x9A6538);
		const FLinearColor Iron = Pal(0x3B3F4A);
		const FLinearColor Gold = Pal(0xFFCB3D);
		const int32 Planks = FMath::Max(3, static_cast<int32>(Width / 70.0));
		const double PlankW = Width / Planks;
		for (int32 p = 0; p < Planks; ++p)
		{
			const double X = (p + 0.5) * PlankW;
			const double Top = Height - 22.0 * FMath::Sin(PI * (p + 0.5) / Planks);
			B.AddBox(FVector(X, 0.0, Top * 0.5), FVector(1.0, 0.0, 0.0), FVector(PlankW * 0.5 - 2.0, 15.0, Top * 0.5), (p % 2) ? WoodA : WoodB);
		}
		for (const double Z : { Height * 0.18, Height * 0.5, Height * 0.8 })
		{
			B.AddBox(FVector(Width * 0.5, 0.0, Z), FVector(1.0, 0.0, 0.0), FVector(Width * 0.5 - 6.0, 18.0, 9.0), Iron);
		}
		TNProcMesh::TNProcAddCylinder(B, FVector(Width - 55.0, -18.0, Height * 0.42), FVector(Width - 55.0, -30.0, Height * 0.42), 22.0, 18.0, 10, Gold);
		TNProcMesh::TNProcAddCylinder(B, FVector(Width - 55.0, 18.0, Height * 0.42), FVector(Width - 55.0, 30.0, Height * 0.42), 22.0, 18.0, 10, Gold);
	}

	/** Base (media cáscara con borde en zigzag) de un huevo en Center, con su nido de paja. */
	void BuildEggCup(FBuffers& Decor, const FVector& Center, const FLinearColor& Shell, const FLinearColor& Accent)
	{
		TArray<double> Zs, Rs, Ri;
		const int32 NumZ = UE_ARRAY_COUNT(EggZ);
		for (int32 i = 0; i < NumZ; ++i)
		{
			if (EggZ[i] > EggSeam + 0.1) { break; }
			Zs.Add(EggZ[i]);
			Rs.Add(EggR[i]);
			Ri.Add(EggR[i] - 5.0);
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
	void BuildEggLid(FBuffers& B, const FLinearColor& Shell, const FLinearColor& Accent)
	{
		TArray<double> Zs, Rs, Ri;
		const int32 NumZ = UE_ARRAY_COUNT(EggZ);
		for (int32 i = 0; i < NumZ; ++i)
		{
			if (EggZ[i] < EggSeam - 0.1) { continue; }
			Zs.Add(EggZ[i] - EggSeam);
			Rs.Add(EggR[i]);
			Ri.Add(FMath::Max(2.0, EggR[i] - 5.0));
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

	UMaterialInterface* VertexColorMaterial()
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cosmetics/Materials/M_CosmeticVertexColor.M_CosmeticVertexColor"));
		return Mat ? Mat : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	}

	float SmoothStep01(float X)
	{
		const float T = FMath::Clamp(X, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	void UploadSection(UProceduralMeshComponent* Comp, const FBuffers& B, bool bCollision, UMaterialInterface* Mat)
	{
		if (!Comp) { return; }
		Comp->ClearAllMeshSections();
		if (B.IsEmpty()) { return; }
		const TArray<FProcMeshTangent> NoTangents;
		Comp->CreateMeshSection_LinearColor(0, B.Verts, B.Tris, B.Normals, B.UVs, B.Colors, NoTangents, bCollision);
		if (Mat) { Comp->SetMaterial(0, Mat); }
	}
}

bool ATN_SandCastleLobby::IsEnabled()
{
	return TNCastleDetail::CVarLobbyCastle.GetValueOnGameThread() != 0;
}

FVector ATN_SandCastleLobby::LayoutSpot(double ClockHour, double Dist, float& OutYawToCenter)
{
	const FVector2D P = TNCastleDetail::ClockPoint(ClockHour, Dist);
	OutYawToCenter = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-P.Y, -P.X)));
	return FVector(P.X, P.Y, TNCastleDetail::FloorZ);
}

void ATN_SandCastleLobby::GetSpawnSpots(TArray<FTransform>& OutLocalSpots)
{
	OutLocalSpots.Reset();
	for (const double X : { -450.0, -150.0, 150.0, 450.0 })
	{
		const FVector Where(X, 1700.0 - FMath::Abs(X) * 0.25, TNCastleDetail::FloorZ + 95.0);
		const FVector ToEggs = FVector(TNCastleDetail::EggsCenter.X, TNCastleDetail::EggsCenter.Y, Where.Z) - Where;
		OutLocalSpots.Add(FTransform(ToEggs.Rotation(), Where));
	}
}

ATN_SandCastleLobby::ATN_SandCastleLobby()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(10.f);

	CastleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CastleRoot"));
	SetRootComponent(CastleRoot);

	// Las mallas se generan en código (editor y ejecución) y no se guardan con el nivel: RF_Transient. El segundo
	// parámetro de CreateDefaultSubobject no basta (solo evita copiar la plantilla del arquetipo): sin la marca, las
	// secciones del castillo se guardaban dentro de LVL_Lobby (35 MB).
	CastleMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CastleMesh"));
	CastleMesh->SetFlags(RF_Transient);
	CastleMesh->SetupAttachment(CastleRoot);
	CastleMesh->bUseAsyncCooking = false;
	CastleMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	DecorMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DecorMesh"));
	DecorMesh->SetFlags(RF_Transient);
	DecorMesh->SetupAttachment(CastleRoot);
	DecorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecorMesh->SetCastShadow(true);

	BarrierMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BarrierMesh"));
	BarrierMesh->SetFlags(RF_Transient);
	BarrierMesh->SetupAttachment(CastleRoot);
	BarrierMesh->bUseAsyncCooking = false;
	BarrierMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	BarrierMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BarrierMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	BarrierMesh->SetVisibility(false);
	BarrierMesh->SetHiddenInGame(true);

	GateLeafLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateLeafLeft"), true);
	GateLeafLeft->SetupAttachment(CastleRoot);
	GateLeafLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GateLeafRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateLeafRight"), true);
	GateLeafRight->SetupAttachment(CastleRoot);
	GateLeafRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GateBlock = CreateDefaultSubobject<UBoxComponent>(TEXT("GateBlock"));
	GateBlock->SetupAttachment(CastleRoot);
	GateBlock->InitBoxExtent(FVector(TNCastleDetail::GateHalfW, 40.0, TNCastleDetail::GateH * 0.5));
	GateBlock->SetRelativeLocation(FVector(0.0, TNCastleDetail::GateY, TNCastleDetail::GateH * 0.5));
	GateBlock->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	GateBlock->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GateBlock->SetHiddenInGame(true);

	for (int32 i = 0; i < NumEggs; ++i)
	{
		UStaticMeshComponent* Lid = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("EggLid%d"), i), true);
		Lid->SetupAttachment(CastleRoot);
		Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EggLids.Add(Lid);
	}

	GateSignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GateSignText"));
	GateSignText->SetupAttachment(CastleRoot);
	GateSignText->SetHorizontalAlignment(EHTA_Center);
	GateSignText->SetVerticalAlignment(EVRTA_TextCenter);
	GateSignText->SetTextRenderColor(FColor(255, 214, 90));
	GateSignText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATN_SandCastleLobby::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_SandCastleLobby, bGateOpen);
	DOREPLIFETIME(ATN_SandCastleLobby, EggMask);
}

void ATN_SandCastleLobby::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildAll(true);
}

void ATN_SandCastleLobby::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	// Al cargar el nivel (editor) o al duplicarlo para jugar, las mallas transitorias llegan vacías: se rehacen.
	if (!IsTemplate() && GetWorld()) { BuildAll(false); }
}

void ATN_SandCastleLobby::BeginPlay()
{
	Super::BeginPlay();
	BuildAll(false);
	if (!IsEnabled())
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetActorTickEnabled(false);
		return;
	}
	HideMaquette();
	UE_LOG(LogTortunabo, Log, TEXT("[Castillo] Lobby de castillo de arena en %s."), *GetActorLocation().ToString());
}

void ATN_SandCastleLobby::BuildAll(bool bForce)
{
	if (!bForce && bBuilt && CastleMesh && CastleMesh->GetNumSections() > 0) { return; }
	BuildCastle();
	BuildGateAndEggs();
	bBuilt = true;
}

void ATN_SandCastleLobby::HideMaquette()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	const FVector Center = GetActorLocation();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == this) { continue; }
		if (ATN_LobbyReadyZone* Zone = Cast<ATN_LobbyReadyZone>(Actor))
		{
			// Se está listo metiéndose en un huevo de la pila.
			Zone->SetActorEnableCollision(false);
			continue;
		}
		const FString ClassName = Actor->GetClass()->GetName();
		FString Names = Actor->GetName();
#if WITH_EDITOR
		Names += TEXT(" ") + Actor->GetActorLabel();
#endif
		if (const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor))
		{
			const UStaticMeshComponent* Comp = MeshActor->GetStaticMeshComponent();
			if (Comp && Comp->GetStaticMesh()) { Names += TEXT(" ") + Comp->GetStaticMesh()->GetName(); }
		}
		FVector BoundsOrigin, BoundsExtent;
		Actor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
		// La muralla de la maqueta (anillo «SandWall» / «Extrude»), las vallas y torres de la zona de salida, los huevos
		// sueltos, la carpa vieja, el poste y el suelo de la zona de salida. El suelo grande solo deja de verse.
		const bool bRing = (Names.Contains(TEXT("Extrude")) || Names.Contains(TEXT("SandWall"))) && BoundsExtent.Z > 150.0;
		const bool bFence = ClassName.Contains(TEXT("BP_Fence")) || ClassName.Contains(TEXT("BP_Tower"));
		const bool bOldEgg = Actor->IsA<AStaticMeshActor>() && Names.Contains(TEXT("Capsule"));
		const bool bOldProps = ClassName.Contains(TEXT("ChangingTent")) || ClassName.Contains(TEXT("SM_Palo")) || Names.Contains(TEXT("Rectangle2"));
		if (bRing || bFence || bOldEgg || bOldProps)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			continue;
		}
		const bool bBigFloor = Actor->IsA<AStaticMeshActor>() && BoundsExtent.Z < 20.0 && BoundsExtent.X > 2000.0
			&& FVector::Dist2D(BoundsOrigin, Center) < 800.0;
		if (bBigFloor)
		{
			// Debajo del suelo del castillo y de la playa: no se ve (evita parpadeos), pero sigue sosteniendo.
			Actor->SetActorHiddenInGame(true);
		}
	}
}

void ATN_SandCastleLobby::BuildCastle()
{
	using namespace TNCastleDetail;
	FBuffers B;
	FBuffers Decor;
	FBuffers Barrier;
	const FLinearColor SandC = Col(0xF0D49A);
	const FLinearColor SandDark = Col(0xD9B474);
	const FLinearColor SandLight = Col(0xF6E0AE);
	const FVector Up(0.0, 0.0, 1.0);

	// ── Suelo de arena redondo (con manchas de arena mojada y de arena clara) y playa por fuera ──
	{
		constexpr int32 Rings = 14;
		constexpr int32 Spokes = 72;
		for (int32 i = 0; i < Rings; ++i)
		{
			const double R0 = (RO + 30.0) * i / Rings;
			const double R1 = (RO + 30.0) * (i + 1) / Rings;
			for (int32 k = 0; k < Spokes; ++k)
			{
				const double A0 = TNProcMap::TwoPi * k / Spokes, A1 = TNProcMap::TwoPi * (k + 1) / Spokes;
				const double N = TNProcMesh::TNProcHashNoise(i, k, 71u);
				const FLinearColor C = N > 0.6 ? Col(0xE3C284) : (N > -0.25 ? Col(0xF3DDA6) : Col(0xEED29A));
				const FVector P0(R0 * FMath::Cos(A0), R0 * FMath::Sin(A0), FloorZ), P1(R0 * FMath::Cos(A1), R0 * FMath::Sin(A1), FloorZ);
				const FVector P2(R1 * FMath::Cos(A1), R1 * FMath::Sin(A1), FloorZ), P3(R1 * FMath::Cos(A0), R1 * FMath::Sin(A0), FloorZ);
				B.AddQuad(P0, P1, P2, P3, Up, C);
			}
		}
		// Playa de fuera (tapa el suelo de la maqueta) hasta el horizonte y el mar al norte, detrás de la puerta.
		for (int32 k = 0; k < Spokes; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Spokes, A1 = TNProcMap::TwoPi * (k + 1) / Spokes;
			const double RIn = RO + 20.0, ROut = 3900.0;
			Decor.AddQuad(FVector(RIn * FMath::Cos(A0), RIn * FMath::Sin(A0), 4.0), FVector(RIn * FMath::Cos(A1), RIn * FMath::Sin(A1), 4.0),
				FVector(ROut * FMath::Cos(A1), ROut * FMath::Sin(A1), 4.0), FVector(ROut * FMath::Cos(A0), ROut * FMath::Sin(A0), 4.0), Up,
				(k % 5 == 0) ? Col(0xEBD39C) : Col(0xF2DCA8));
			Decor.AddQuad(FVector(ROut * FMath::Cos(A0), ROut * FMath::Sin(A0), 4.0), FVector(ROut * FMath::Cos(A1), ROut * FMath::Sin(A1), 4.0),
				FVector(4400.0 * FMath::Cos(A1), 4400.0 * FMath::Sin(A1), -30.0), FVector(4400.0 * FMath::Cos(A0), 4400.0 * FMath::Sin(A0), -30.0), Up, Col(0xE2F6F2));
		}
		Decor.AddQuad(FVector(-30000.0, -30000.0, -34.0), FVector(30000.0, -30000.0, -34.0), FVector(30000.0, 30000.0, -34.0), FVector(-30000.0, 30000.0, -34.0),
			Up, Col(0x1E9CC6));
		// Explanada de fuera de la puerta (se puede pisar; más allá, barrera).
		const double ApronY0 = RO - 20.0, ApronY1 = RO + 520.0;
		B.AddQuad(FVector(-760.0, ApronY0, FloorZ), FVector(760.0, ApronY0, FloorZ), FVector(760.0, ApronY1, FloorZ), FVector(-760.0, ApronY1, FloorZ), Up, Col(0xF2DCA8));
		for (const double X : { -760.0, 760.0 })
		{
			Barrier.AddBox(FVector(X, (ApronY0 + ApronY1) * 0.5, 400.0), FVector(1.0, 0.0, 0.0), FVector(20.0, (ApronY1 - ApronY0) * 0.5, 400.0), SandC);
		}
		Barrier.AddBox(FVector(0.0, ApronY1, 400.0), FVector(1.0, 0.0, 0.0), FVector(780.0, 20.0, 400.0), SandC);
	}

	// ── Muralla redonda: cara de dentro y de fuera, adarve arriba, almenas por fuera y el hueco de la puerta ──
	{
		constexpr int32 Seg = 180;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg, A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			// En el sentido del reloj desde +Y: x = -r·sen(a), y = r·cos(a).
			if (InGate((A0 + A1) * 0.5)) { continue; }
			const double H0 = WallHeightAt(A0), H1 = WallHeightAt(A1);
			auto P = [](double Rad, double A, double Z) { return FVector(-Rad * FMath::Sin(A), Rad * FMath::Cos(A), Z); };
			const FVector In(FMath::Sin((A0 + A1) * 0.5), -FMath::Cos((A0 + A1) * 0.5), 0.0);
			const float Tone = TNProcMesh::TNProcTone(k, 17u) * 0.1f + 0.95f;
			B.AddQuad(P(R, A0, 0.0), P(R, A1, 0.0), P(R, A1, H1), P(R, A0, H0), In, SandC * Tone);
			B.AddQuad(P(RO, A0, 0.0), P(RO, A1, 0.0), P(RO, A1, H1), P(RO, A0, H0), -In, SandDark * Tone);
			B.AddQuad(P(R, A0, H0), P(R, A1, H1), P(RO, A1, H1), P(RO, A0, H0), Up, SandLight * Tone);
			// Marcas del molde de cubo: tres franjas que sobresalen un poco por dentro.
			for (const double Z : { 150.0, 320.0, 470.0 })
			{
				B.AddQuad(P(R - 7.0, A0, Z - 8.0), P(R - 7.0, A1, Z - 8.0), P(R - 7.0, A1, Z + 8.0), P(R - 7.0, A0, Z + 8.0), In, SandDark);
				B.AddQuad(P(R, A0, Z + 8.0), P(R, A1, Z + 8.0), P(R - 7.0, A1, Z + 8.0), P(R - 7.0, A0, Z + 8.0), Up, SandDark);
			}
			// Almenas en el borde de fuera, una sí y otra no.
			if (k % 2 == 0)
			{
				const FVector M0 = P(RO - 30.0, A0, H0), M1 = P(RO - 30.0, A1, H1);
				const FVector Dir = (M1 - M0).GetSafeNormal2D();
				B.AddBox((M0 + M1) * 0.5 + Up * 45.0, Dir, FVector(FVector::Dist2D(M0, M1) * 0.5, 30.0, 45.0), SandC);
			}
			// Barrera invisible sobre el borde de fuera.
			Barrier.AddQuad(P(RO + 10.0, A0, H0), P(RO + 10.0, A1, H1), P(RO + 10.0, A1, H1 + 900.0), P(RO + 10.0, A0, H0 + 900.0), In, SandC);
		}
		// Arco sobre la puerta: dintel entre las dos torres, con su cartel de madera.
		const double LintelY = RM - 20.0;
		B.AddBox(FVector(0.0, LintelY, (GateH + WallH) * 0.5 + 40.0), FVector(1.0, 0.0, 0.0),
			FVector(GateHalfW + 110.0, WallT * 0.5 + 10.0, (WallH - GateH) * 0.5 + 40.0), SandC);
		for (int32 m = -4; m <= 4; ++m)
		{
			B.AddBox(FVector(m * 105.0, LintelY + WallT * 0.5 - 20.0, WallH + 125.0), FVector(1.0, 0.0, 0.0), FVector(34.0, 26.0, 45.0), SandC);
		}
		// Arco de medio punto dibujado en la cara de dentro (dovelas de arena oscura).
		for (int32 s = 0; s < 12; ++s)
		{
			const double T0 = PI * s / 12.0, T1 = PI * (s + 1) / 12.0;
			const FVector C0(-GateHalfW * FMath::Cos(T0), R - 12.0, GateH - 40.0 + 80.0 * FMath::Sin(T0));
			const FVector C1(-GateHalfW * FMath::Cos(T1), R - 12.0, GateH - 40.0 + 80.0 * FMath::Sin(T1));
			Decor.AddBox((C0 + C1) * 0.5, (C1 - C0).GetSafeNormal2D().IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : (C1 - C0).GetSafeNormal2D(),
				FVector(FVector::Dist(C0, C1) * 0.5, 8.0, 18.0), (s % 2) ? SandDark : Col(0xCFA766));
		}
		// Cartel de madera con marco dorado y cuerdas, colgado del dintel por dentro.
		const FVector SignC(0.0, R - 34.0, GateH + 150.0);
		Decor.AddBox(SignC, FVector(1.0, 0.0, 0.0), FVector(300.0, 8.0, 56.0), Col(0x8C5A33));
		Decor.AddBox(SignC + FVector(0.0, 3.0, 0.0), FVector(1.0, 0.0, 0.0), FVector(312.0, 8.0, 68.0), Col(0xFFCB3D));
		for (int32 p = 0; p < 5; ++p)
		{
			Decor.AddBox(SignC + FVector(-240.0 + p * 120.0, -7.0, 0.0), FVector(1.0, 0.0, 0.0), FVector(58.0, 2.0, 52.0), (p % 2) ? Col(0x9A6538) : Col(0xA8713F));
		}
		for (const double X : { -250.0, 250.0 })
		{
			Decor.AddBeam(SignC + FVector(X, 0.0, 68.0), FVector(X * 0.8, R - 20.0, WallH - 10.0), 3.0, Col(0xC9A56A));
		}
	}

	// ── Torres irregulares repartidas por la muralla ──
	for (int32 t = 0; t < static_cast<int32>(UE_ARRAY_COUNT(Towers)); ++t)
	{
		const FTowerDef& Def = Towers[t];
		AddTower(B, Decor, ClockPoint(Def.Clock, RM), Def.Radius, Def.Height, Col(Def.Flag), t);
		// Barrera por fuera de la azotea de cada torre.
		const FVector2D C = ClockPoint(Def.Clock, RM);
		TNProcMesh::TNProcAddCylinder(Barrier, FVector(C.X, C.Y, Def.Height), FVector(C.X, C.Y, Def.Height + 900.0), Def.Radius + 40.0, Def.Radius + 40.0, 12, SandC, false);
	}

	// ── Muro interior (de las 3:40 a las 8:20) con la torre del homenaje en medio ──
	{
		const double Half = FMath::Sqrt(R * R - CutY * CutY) + 60.0;
		for (const double Sign : { -1.0, 1.0 })
		{
			const double X0 = Sign * (KeepR - 20.0), X1 = Sign * Half;
			const FVector Mid((X0 + X1) * 0.5, CutY, CutH * 0.5);
			B.AddBox(Mid, FVector(1.0, 0.0, 0.0), FVector(FMath::Abs(X1 - X0) * 0.5, CutHalfT, CutH * 0.5), SandC);
			for (const double Z : { 150.0, 320.0, 470.0 })
			{
				B.AddBox(FVector(Mid.X, CutY, Z), FVector(1.0, 0.0, 0.0), FVector(FMath::Abs(X1 - X0) * 0.5, CutHalfT + 7.0, 8.0), SandDark);
			}
			// Almenas a los dos lados del adarve del muro.
			for (double X = FMath::Min(X0, X1) + 70.0; X < FMath::Max(X0, X1) - 50.0; X += 180.0)
			{
				for (const double Face : { -1.0, 1.0 })
				{
					B.AddBox(FVector(X, CutY + Face * (CutHalfT - 22.0), CutH + 45.0), FVector(1.0, 0.0, 0.0), FVector(45.0, 22.0, 45.0), SandC);
				}
			}
			// Conchas incrustadas en la cara que da a la plaza.
			const FLinearColor ShellColors[4] = { Col(0xFFB4A2), Col(0xFFE0C2), Col(0xE6D0FF), Col(0xFFF6E8) };
			for (int32 s = 0; s < 6; ++s)
			{
				const double X = FMath::Lerp(X0, X1, 0.12 + 0.15 * s);
				AddScallop(Decor, FVector(X, CutY + CutHalfT, 230.0 + 120.0 * ((s + (Sign > 0.0 ? 1 : 0)) % 3)), FVector(0.0, 1.0, 0.0), Up, 30.0,
					ShellColors[s % 4]);
			}
		}
	}
	{
		// Torre del homenaje: cilindro con un paso de norte a sur (sin tapar la puerta), franjas y azotea-balcón.
		const FVector KeepC(0.0, CutY, 0.0);
		constexpr int32 KSeg = 40;
		auto KP = [&KeepC](double Rad, double A, double Z) { return KeepC + FVector(-Rad * FMath::Sin(A), Rad * FMath::Cos(A), Z); };
		for (int32 k = 0; k < KSeg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / KSeg, A1 = TNProcMap::TwoPi * (k + 1) / KSeg;
			const double Am = (A0 + A1) * 0.5;
			const bool bDoorSide = FMath::Abs(KeepR * FMath::Sin(Am)) < DoorHalfW + 5.0;
			const double Z0 = bDoorSide ? DoorH : 0.0;
			const FVector Out(-FMath::Sin(Am), FMath::Cos(Am), 0.0);
			B.AddQuad(KP(KeepR, A0, Z0), KP(KeepR, A1, Z0), KP(KeepR, A1, KeepRoofZ), KP(KeepR, A0, KeepRoofZ), Out, SandC);
			for (const double Z : { 200.0, 470.0, 740.0 })
			{
				if (Z < Z0 + 20.0) { continue; }
				B.AddQuad(KP(KeepR + 8.0, A0, Z - 9.0), KP(KeepR + 8.0, A1, Z - 9.0), KP(KeepR + 8.0, A1, Z + 9.0), KP(KeepR + 8.0, A0, Z + 9.0), Out, SandDark);
			}
			// Azotea (el balcón): anillo de arena clara y almenas en el borde, con hueco donde llega la escalera (lado este).
			B.AddTri(KeepC + Up * KeepRoofZ, KP(KeepR + 30.0, A0, KeepRoofZ), KP(KeepR + 30.0, A1, KeepRoofZ), Up, SandLight);
			B.AddQuad(KP(KeepR, A0, KeepRoofZ - 30.0), KP(KeepR, A1, KeepRoofZ - 30.0), KP(KeepR + 30.0, A1, KeepRoofZ), KP(KeepR + 30.0, A0, KeepRoofZ), Out, SandDark);
			const double Deg = FMath::RadiansToDegrees(Am);
			const bool bStairLanding = Deg > 48.0 && Deg < 92.0;
			if (k % 2 == 0 && !bStairLanding)
			{
				B.AddBox(KP(KeepR + 8.0, Am, KeepRoofZ + 50.0), Out, FVector(20.0, 36.0, 50.0), SandC);
			}
			if (!bStairLanding)
			{
				Barrier.AddQuad(KP(KeepR + 32.0, A0, KeepRoofZ), KP(KeepR + 32.0, A1, KeepRoofZ), KP(KeepR + 32.0, A1, KeepRoofZ + 260.0),
					KP(KeepR + 32.0, A0, KeepRoofZ + 260.0), -Out, SandC);
			}
		}
		// Paso por dentro: paredes, techo y umbral (de la cara norte a la sur).
		for (const double X : { -DoorHalfW, DoorHalfW })
		{
			B.AddBox(FVector(X + (X > 0.0 ? 8.0 : -8.0), CutY, DoorH * 0.5), FVector(1.0, 0.0, 0.0), FVector(8.0, KeepR, DoorH * 0.5), SandDark);
		}
		B.AddBox(FVector(0.0, CutY, DoorH + 10.0), FVector(1.0, 0.0, 0.0), FVector(DoorHalfW + 16.0, KeepR, 10.0), SandDark);
		// Arcos de las dos bocas del paso y cartel de madera de «PRUEBAS» encima de la de la plaza.
		for (const double Face : { 1.0, -1.0 })
		{
			for (int32 s = 0; s < 10; ++s)
			{
				const double T0 = PI * s / 10.0, T1 = PI * (s + 1) / 10.0;
				const FVector C0(-(DoorHalfW + 16.0) * FMath::Cos(T0), CutY + Face * (KeepR + 4.0), DoorH - 30.0 + 70.0 * FMath::Sin(T0));
				const FVector C1(-(DoorHalfW + 16.0) * FMath::Cos(T1), CutY + Face * (KeepR + 4.0), DoorH - 30.0 + 70.0 * FMath::Sin(T1));
				const FVector D = (C1 - C0).GetSafeNormal();
				const FVector AxisX = FVector(D.X, D.Y, 0.0).IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : FVector(D.X, D.Y, 0.0).GetSafeNormal();
				Decor.AddBox((C0 + C1) * 0.5, AxisX, FVector(FVector::Dist(C0, C1) * 0.5, 10.0, 16.0), (s % 2) ? SandDark : Col(0xCFA766));
			}
		}
		// Torrecilla sobre la azotea (al sur), con tejado de cono y bandera: la silueta alta del castillo.
		AddTower(B, Decor, FVector2D(0.0, CutY - 170.0), 150.0, 430.0, Col(0xFF6A52), 0, KeepRoofZ);
		// Escalera de caracol por fuera: del suelo (oeste, a las 8 de la torre) a la azotea (este), pasando sobre la puerta.
		for (int32 s = 0; s < StairSteps; ++s)
		{
			const double T0 = static_cast<double>(s) / StairSteps, T1 = static_cast<double>(s + 1) / StairSteps;
			// Ángulos de la torre en sentido del reloj desde +Y: de 290° (oeste-noroeste, ya fuera del muro) pasando por 0°
			// (norte, por encima del arco de la puerta) a 70° (este), donde se llega a la azotea.
			const double A0 = FMath::DegreesToRadians(290.0 + 140.0 * T0), A1 = FMath::DegreesToRadians(290.0 + 140.0 * T1);
			const double Z = KeepRoofZ * T1;
			const FVector Outer0 = KP(StairOut, A0, Z), Outer1 = KP(StairOut, A1, Z);
			const FVector Inner0 = KP(StairIn, A0, Z), Inner1 = KP(StairIn, A1, Z);
			const FLinearColor StepC = (s % 2) ? SandLight : Col(0xEFD29A);
			B.AddQuad(Inner0, Outer0, Outer1, Inner1, Up, StepC);
			const FVector Down(0.0, 0.0, -28.0);
			const FVector Out(-FMath::Sin((A0 + A1) * 0.5), FMath::Cos((A0 + A1) * 0.5), 0.0);
			B.AddQuad(Outer0 + Down, Outer1 + Down, Outer1, Outer0, Out, SandDark);
			B.AddQuad(Inner0 + Down, Inner1 + Down, Outer1 + Down, Outer0 + Down, -Up, SandDark);
			B.AddQuad(Inner0 + Down, Outer0 + Down, Outer0, Inner0, (Outer0 - Outer1).GetSafeNormal(), SandDark);
			// Barandilla invisible por fuera (no se cae uno al subir) y bolardos de arena de adorno.
			Barrier.AddQuad(KP(StairOut + 12.0, A0, Z), KP(StairOut + 12.0, A1, Z), KP(StairOut + 12.0, A1, Z + 180.0), KP(StairOut + 12.0, A0, Z + 180.0), -Out, SandC);
			if (s % 3 == 0)
			{
				TNProcMesh::TNProcAddCylinder(B, KP(StairOut - 15.0, A0, Z), KP(StairOut - 15.0, A0, Z + 55.0), 14.0, 11.0, 8, SandDark);
			}
		}
	}

	// ── Montículo de la pila de huevos (dos alturas) con un escalón de concha para subir al de arriba ──
	{
		const FVector C(EggsCenter.X, EggsCenter.Y, 0.0);
		TNProcMesh::TNProcAddCylinder(B, C, C + Up * (FloorZ + Tier1H), Tier1R, Tier1R - 20.0, 32, SandC);
		TNProcMesh::TNProcAddCylinder(B, C + Up * (FloorZ + Tier1H - 12.0), C + Up * (FloorZ + Tier1H), Tier1R - 16.0, Tier1R - 20.0, 32, SandDark);
		TNProcMesh::TNProcAddCylinder(B, C + Up * (FloorZ + Tier1H), C + Up * (FloorZ + Tier2H), Tier2R + 25.0, Tier2R, 24, SandC);
		// Escalón: tocón de arena con reborde y una vieira grande encima, abierta hacia la puerta.
		const FVector StepP = C + FVector(0.0, Tier2R + 110.0, 0.0);
		const FVector StepOut(0.0, 1.0, 0.0);
		TNProcMesh::TNProcAddCylinder(B, StepP + Up * (FloorZ + Tier1H), StepP + Up * (FloorZ + 132.0), 78.0, 70.0, 16, SandC);
		TNProcMesh::TNProcAddCylinder(B, StepP + Up * (FloorZ + 118.0), StepP + Up * (FloorZ + 132.0), 74.0, 72.0, 16, SandDark);
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

	// ── Adornos sueltos por la plaza y el patio: conchas y estrellas ──
	for (int32 i = 0; i < 60; ++i)
	{
		const double A = TNProcMap::TwoPi * FMath::Frac(i * 0.618034 + 0.11);
		const double Dist = (R - 200.0) * FMath::Sqrt(FMath::Frac(i * 0.754877 + 0.29));
		const FVector P(-Dist * FMath::Sin(A), Dist * FMath::Cos(A), FloorZ);
		if (FVector2D::Distance(FVector2D(P.X, P.Y), EggsCenter) < Tier1R + 120.0) { continue; }
		if (FMath::Abs(P.Y - CutY) < 220.0) { continue; }
		if (i % 3 == 0) { AddStarfish(Decor, P, 22.0 + 10.0 * FMath::Frac(i * 0.31), i * 0.7, (i % 2) ? Col(0xFF8A70) : Col(0xFFB077)); }
		else { AddScallop(Decor, P + Up * 2.0, Up, FVector(FMath::Cos(i * 1.3), FMath::Sin(i * 1.3), 0.0), 18.0 + 8.0 * FMath::Frac(i * 0.43), (i % 2) ? Col(0xFFE0C2) : Col(0xFFB4A2)); }
	}

	UMaterialInterface* Mat = VertexColorMaterial();
	UploadSection(CastleMesh, B, true, Mat);
	UploadSection(DecorMesh, Decor, false, Mat);
	UploadSection(BarrierMesh, Barrier, true, nullptr);

	// Rótulo del cartel de la puerta: siempre dentro de la tabla (se encoge si el nombre es largo).
	if (GateSignText)
	{
		GateSignText->SetRelativeLocationAndRotation(FVector(0.0, R - 45.0, GateH + 150.0), FRotator(0.f, -90.f, 0.f));
		GateSignText->SetWorldSize(90.f);
		GateSignText->SetText(GateName);
		const double Width = GateSignText->GetTextLocalSize().Y;
		if (Width > 540.0) { GateSignText->SetWorldSize(static_cast<float>(90.0 * 540.0 / Width)); }
	}
}

void ATN_SandCastleLobby::BuildGateAndEggs()
{
	using namespace TNCastleDetail;
	UMaterialInterface* Mat = VertexColorMaterial();

	// Puerta grande: la hoja izquierda (bisagra en -X) cerrada apunta a +X; la derecha, a -X.
	FBuffers Leaf;
	BuildLeaf(Leaf, GateHalfW, GateH);
	UStaticMesh* LeafMesh = TNProcRuntimeMesh::MakeStaticMesh(this, Leaf, Mat);
	if (GateLeafLeft)
	{
		GateLeafLeft->SetStaticMesh(LeafMesh);
		GateLeafLeft->SetRelativeLocationAndRotation(FVector(-GateHalfW, GateY, FloorZ), FRotator::ZeroRotator);
	}
	if (GateLeafRight)
	{
		GateLeafRight->SetStaticMesh(LeafMesh);
		GateLeafRight->SetRelativeLocationAndRotation(FVector(GateHalfW, GateY, FloorZ), FRotator(0.f, 180.f, 0.f));
	}
	bGateBlocking = true;
	if (GateBlock) { GateBlock->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }

	// Huevos: base en los adornos (sin colisión: se entra andando) y tapa que baja al ocuparlo.
	FBuffers Cups;
	for (int32 i = 0; i < NumEggs; ++i)
	{
		const FVector Spot = EggSpot(i);
		BuildEggCup(Cups, Spot, Col(0xFFF3DC), Col(EggAccents[i]));
		if (EggLids.IsValidIndex(i) && EggLids[i])
		{
			FBuffers Lid;
			BuildEggLid(Lid, Pal(0xFFF3DC), Pal(EggAccents[i]));
			EggLids[i]->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, Lid, Mat));
			EggLids[i]->SetRelativeLocation(Spot + FVector(0.0, 0.0, EggSeam + 160.0));
		}
	}
	// Las bases van con los adornos del castillo: se añaden a su sección aparte.
	if (DecorMesh && !Cups.IsEmpty())
	{
		const TArray<FProcMeshTangent> NoTangents;
		DecorMesh->CreateMeshSection_LinearColor(1, Cups.Verts, Cups.Tris, Cups.Normals, Cups.UVs, Cups.Colors, NoTangents, false);
		if (Mat) { DecorMesh->SetMaterial(1, Mat); }
	}
}

void ATN_SandCastleLobby::ServerUpdate(float DeltaSeconds)
{
	using namespace TNCastleDetail;
	UWorld* World = GetWorld();
	if (!World) { return; }
	ATN_HQGameMode* HQ = World->GetAuthGameMode<ATN_HQGameMode>();
	const FTransform Xf = GetActorTransform();

	int32 Mask = 0;
	bool bAnyoneNearGate = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		const APawn* PawnInLobby = PC ? PC->GetPawn() : nullptr;
		bool bInEgg = false;
		if (PawnInLobby)
		{
			const FVector Local = Xf.InverseTransformPosition(PawnInLobby->GetActorLocation());
			for (int32 i = 0; i < NumEggs; ++i)
			{
				const FVector Spot = EggSpot(i);
				// Dentro del huevo: cerca de su eje y a la altura de su base (el de arriba está en el piso alto del montículo).
				if (FVector2D::Distance(FVector2D(Local.X, Local.Y), FVector2D(Spot.X, Spot.Y)) < 95.0 && Local.Z > Spot.Z && Local.Z < Spot.Z + 320.0)
				{
					Mask |= 1 << i;
					bInEgg = true;
					break;
				}
			}
			bAnyoneNearGate |= FVector2D::Distance(FVector2D(Local.X, Local.Y), FVector2D(0.0, GateY)) < 900.0;
		}
		if (PC)
		{
			const bool* Sent = ReadySent.Find(PC);
			if (!Sent || *Sent != bInEgg)
			{
				ReadySent.Add(PC, bInEgg);
				if (HQ) { HQ->SetPlayerReadyState(PC, bInEgg); }
			}
		}
	}
	EggMask = Mask;

	const ATN_CoopGameState* GS = World->GetGameState<ATN_CoopGameState>();
	const bool bCountdown = GS && (GS->MatchFlowState == ETNMatchFlowState::Countdown || GS->MatchFlowState == ETNMatchFlowState::Cinematic);
	GateHoldTimer = (bAnyoneNearGate || bCountdown) ? 2.5f : GateHoldTimer - DeltaSeconds;
	bGateOpen = GateHoldTimer > 0.f;
}

void ATN_SandCastleLobby::Tick(float DeltaSeconds)
{
	using namespace TNCastleDetail;
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;

	if (HasAuthority())
	{
		ServerTimer -= DeltaSeconds;
		if (ServerTimer <= 0.f)
		{
			ServerTimer = 0.2f;
			ServerUpdate(0.2f);
		}
	}

	// Puerta grande: se abre hacia fuera en ~1,2 s; bloquea solo cerrada del todo.
	GateOpenness = FMath::FInterpConstantTo(GateOpenness, bGateOpen ? 1.f : 0.f, DeltaSeconds, 0.8f);
	const float GateAngle = 100.f * SmoothStep01(GateOpenness);
	if (GateLeafLeft) { GateLeafLeft->SetRelativeRotation(FRotator(0.f, GateAngle, 0.f)); }
	if (GateLeafRight) { GateLeafRight->SetRelativeRotation(FRotator(0.f, 180.f - GateAngle, 0.f)); }
	const bool bShouldBlock = !bGateOpen && GateOpenness < 0.05f;
	if (GateBlock && bShouldBlock != bGateBlocking)
	{
		bGateBlocking = bShouldBlock;
		GateBlock->SetCollisionEnabled(bShouldBlock ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	// Huevos: la tapa flota encima dando vueltecitas y baja a cerrar el huevo cuando alguien se mete (y se mece).
	for (int32 i = 0; i < EggLids.Num() && i < NumEggs; ++i)
	{
		if (!EggLids[i]) { continue; }
		const bool bOccupied = (EggMask & (1 << i)) != 0;
		EggClose[i] = FMath::FInterpConstantTo(EggClose[i], bOccupied ? 1.f : 0.f, DeltaSeconds, 2.2f);
		const float Close = SmoothStep01(EggClose[i]);
		const float Bob = 10.f * FMath::Sin(Clock * 1.6f + i * 0.9f);
		const float Height = FMath::Lerp(160.f + Bob, 0.f, Close);
		const float Tilt = FMath::Lerp(16.f, 0.f, Close);
		const float Rock = Close * 3.5f * FMath::Sin(Clock * 3.1f + i);
		EggLids[i]->SetRelativeLocationAndRotation(EggSpot(i) + FVector(0.0, 0.0, EggSeam + Height),
			FRotator(Tilt * FMath::Sin(Clock * 0.7f + i) + Rock, Clock * 12.f * (1.f - Close) + i * 40.f, Tilt * FMath::Cos(Clock * 0.7f + i)));
	}
}
