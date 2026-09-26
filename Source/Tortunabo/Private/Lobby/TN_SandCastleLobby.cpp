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
		TEXT("1 = el lobby es el castillo de arena (ATN_SandCastleLobby); 0 = la maqueta de siempre (vuelve a cargar el lobby)."));

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

	// Medidas (cm, coordenadas del lobby).
	constexpr double WallH = 560.0;
	constexpr double WallHalfT = 90.0;
	constexpr double Yard = 2400.0;
	constexpr double GateY = 2150.0;
	constexpr double GateHalfW = 500.0;
	constexpr double GateH = 460.0;
	constexpr double RoomHalfX = 1600.0;
	constexpr double RoomEndY = 3150.0;
	constexpr double SeaHalfW = 450.0;
	constexpr double SeaGateH = 430.0;
	constexpr double FloorZ = 2.0;

	const FVector2D EggSpots[ATN_SandCastleLobby::NumEggs] = {
		FVector2D(-525.0, 2450.0), FVector2D(-175.0, 2450.0), FVector2D(175.0, 2450.0), FVector2D(525.0, 2450.0),
		FVector2D(-525.0, 2800.0), FVector2D(-175.0, 2800.0), FVector2D(175.0, 2800.0), FVector2D(525.0, 2800.0),
	};
	const uint32 EggAccents[ATN_SandCastleLobby::NumEggs] = { 0x2EC4B6, 0xFF6A52, 0xFFCB3D, 0x9B5DE5, 0x7FD8C8, 0xFF8FB1, 0x62B6F0, 0xFFB077 };

	/** Perfil del huevo (altura, radio); la costura entre la base y la tapa está en EggSeam. */
	constexpr double EggSeam = 110.0;
	const double EggZ[] = { 0.0, 25.0, 60.0, 110.0, 150.0, 190.0, 222.0, 238.0 };
	const double EggR[] = { 55.0, 84.0, 97.0, 96.0, 88.0, 68.0, 40.0, 12.0 };

	/**
	 * Superficie de revolución de eje vertical en Center: anillos a las alturas Zs con radios Rs, caras hacia fuera
	 * (bOutward) o hacia dentro. Accent pinta uno de cada AccentEvery trozos del segundo anillo (motas).
	 */
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

	/** Muralla de arena de A a C con almenas por fuera (Out), marcas de cubo y conchas incrustadas por dentro. */
	void AddWall(FBuffers& B, FBuffers& Decor, const FVector2D& A, const FVector2D& C, const FVector2D& Out, uint32 Seed, TArray<FBox>& OutBarriers)
	{
		const FLinearColor Sand = Col(0xF0D49A);
		const FLinearColor SandDark = Col(0xD9B474);
		const FVector2D D = C - A;
		const double Len = D.Size();
		if (Len < 10.0) { return; }
		const FVector2D Dir = D / Len;
		const FVector AxisX(Dir.X, Dir.Y, 0.0);
		const FVector Mid((A.X + C.X) * 0.5, (A.Y + C.Y) * 0.5, 0.0);
		B.AddBox(Mid + FVector(0.0, 0.0, WallH * 0.5), AxisX, FVector(Len * 0.5, WallHalfT, WallH * 0.5), Sand);
		for (const double Z : { 140.0, 290.0, 440.0 })
		{
			B.AddBox(Mid + FVector(0.0, 0.0, Z), AxisX, FVector(Len * 0.5, WallHalfT + 5.0, 7.0), SandDark);
		}
		const FVector OutV(Out.X, Out.Y, 0.0);
		for (double T = 120.0; T < Len - 60.0; T += 240.0)
		{
			const FVector P(A.X + Dir.X * T, A.Y + Dir.Y * T, 0.0);
			B.AddBox(P + OutV * (WallHalfT - 30.0) + FVector(0.0, 0.0, WallH + 45.0), AxisX, FVector(60.0, 30.0, 45.0), Sand);
		}
		// Conchas de vieira incrustadas en la cara de dentro.
		const FLinearColor ShellColors[4] = { Col(0xFFB4A2), Col(0xFFE0C2), Col(0xE6D0FF), Col(0xFFF6E8) };
		for (int32 s = 0; s < FMath::Max(2, static_cast<int32>(Len / 700.0)); ++s)
		{
			const double T = (0.12 + 0.76 * FMath::Frac(0.618034 * (s + 1) + Seed * 0.1)) * Len;
			const double Z = 120.0 + 300.0 * FMath::Frac(0.371 * (s + 3) + Seed * 0.07);
			const FVector Face = FVector(A.X + Dir.X * T, A.Y + Dir.Y * T, Z) - OutV * (WallHalfT + 2.0);
			const FVector Up(0.0, 0.0, 1.0);
			const FLinearColor ShellC = ShellColors[(s + Seed) % 4];
			for (int32 f = 0; f < 5; ++f)
			{
				const double Fa = FMath::DegreesToRadians(-60.0 + f * 30.0);
				const double Fb = FMath::DegreesToRadians(-60.0 + (f + 1) * 30.0);
				const FVector Ea = Face + (AxisX * FMath::Sin(Fa) + Up * FMath::Cos(Fa)) * 26.0;
				const FVector Eb = Face + (AxisX * FMath::Sin(Fb) + Up * FMath::Cos(Fb)) * 26.0;
				Decor.AddTri(Face - Up * 10.0, Ea, Eb, -OutV, (f % 2) ? ShellC : ShellC * 0.85f);
			}
		}
		// Barrera invisible sobre el borde de fuera del adarve (nadie se cae fuera del castillo).
		const FVector BarrierCenter = Mid + OutV * (WallHalfT - 20.0) + FVector(0.0, 0.0, WallH + 420.0);
		OutBarriers.Add(FBox::BuildAABB(BarrierCenter, FVector(FMath::Abs(Dir.X) * Len * 0.5 + FMath::Abs(Out.X) * 20.0,
			FMath::Abs(Dir.Y) * Len * 0.5 + FMath::Abs(Out.Y) * 20.0, 420.0)));
	}

	/** Torre de cubo (tronco de cono con marcas), almenas y bandera. */
	void AddTower(FBuffers& B, FBuffers& Decor, const FVector2D& C, double R, double H, const FLinearColor& FlagColor)
	{
		const FLinearColor Sand = Col(0xF0D49A);
		const FLinearColor SandDark = Col(0xD9B474);
		const FVector Base(C.X, C.Y, 0.0);
		const FVector Up(0.0, 0.0, 1.0);
		TNProcMesh::TNProcAddCylinder(B, Base, Base + Up * H, R, R * 0.86, 16, Sand);
		for (const double K : { 0.28, 0.55, 0.8 })
		{
			const double Rr = FMath::Lerp(R, R * 0.86, K) + 6.0;
			TNProcMesh::TNProcAddCylinder(B, Base + Up * (H * K - 9.0), Base + Up * (H * K + 9.0), Rr, Rr, 16, SandDark);
		}
		const double Rt = R * 0.86;
		for (int32 k = 0; k < 8; ++k)
		{
			const double Ang = TNProcMap::TwoPi * k / 8.0;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.0);
			B.AddBox(Base + Dir * (Rt - 26.0) + Up * (H + 40.0), Dir, FVector(26.0, Rt * 0.27, 40.0), Sand);
		}
		const FVector PoleTop = Base + Up * (H + 280.0);
		TNProcMesh::TNProcAddCylinder(Decor, Base + Up * H, PoleTop, 6.0, 4.0, 6, Col(0x7A4E2B));
		const FVector F1 = PoleTop - Up * 12.0 + FVector(0.0, 120.0, -20.0);
		const FVector F2 = PoleTop - Up * 80.0;
		Decor.AddTri(PoleTop - Up * 4.0, F1, F2, FVector(1.0, 0.0, 0.0), FlagColor);
		Decor.AddTri(PoleTop - Up * 4.0, F1, F2, FVector(-1.0, 0.0, 0.0), FlagColor * 0.85f);
	}

	/** Bloque de arena (escalón o plataforma) con la tapa algo más clara y marcas de cubo en los lados. */
	void AddBlock(FBuffers& B, const FVector2D& C, const FVector2D& Half, double Height, bool bRidges = true)
	{
		const FVector Base(C.X, C.Y, 0.0);
		B.AddBox(Base + FVector(0.0, 0.0, Height * 0.5), FVector(1.0, 0.0, 0.0), FVector(Half.X, Half.Y, Height * 0.5), Col(0xE8C889));
		B.AddBox(Base + FVector(0.0, 0.0, Height + 1.5), FVector(1.0, 0.0, 0.0), FVector(Half.X - 8.0, Half.Y - 8.0, 1.5), Col(0xF6E0AE));
		if (bRidges && Height > 120.0)
		{
			for (double Z = 70.0; Z < Height - 30.0; Z += 110.0)
			{
				B.AddBox(Base + FVector(0.0, 0.0, Z), FVector(1.0, 0.0, 0.0), FVector(Half.X + 4.0, Half.Y + 4.0, 6.0), Col(0xD2AC6A));
			}
		}
	}

	/** Pilar de cubo de playa boca abajo (de plástico de color, con su reborde). */
	void AddBucketPillar(FBuffers& B, const FVector2D& C, double Height, const FLinearColor& Color)
	{
		const FVector Base(C.X, C.Y, 0.0);
		const FVector Up(0.0, 0.0, 1.0);
		TNProcMesh::TNProcAddCylinder(B, Base, Base + Up * Height, 92.0, 72.0, 14, Color);
		TNProcMesh::TNProcAddCylinder(B, Base, Base + Up * 16.0, 101.0, 101.0, 14, Color * 0.85f);
		TNProcMesh::TNProcAddCylinder(B, Base + Up * (Height * 0.5 - 5.0), Base + Up * (Height * 0.5 + 5.0), 86.0, 86.0, 14, Color * 1.12f);
	}

	/** Estrella de mar plana (adorno del suelo). */
	void AddStarfish(FBuffers& Decor, const FVector& C, double R, double Spin, const FLinearColor& Color)
	{
		const FVector Up(0.0, 0.0, 1.0);
		for (int32 k = 0; k < 5; ++k)
		{
			const double A0 = Spin + TNProcMap::TwoPi * k / 5.0;
			const double Am = A0 + TNProcMap::TwoPi / 10.0;
			const double A1 = A0 + TNProcMap::TwoPi / 5.0;
			const FVector Tip = C + FVector(FMath::Cos(A0) * R, FMath::Sin(A0) * R, 1.5);
			const FVector Inner0 = C + FVector(FMath::Cos(Am) * R * 0.38, FMath::Sin(Am) * R * 0.38, 2.5);
			const FVector Inner1 = C + FVector(FMath::Cos(A1 - TNProcMap::TwoPi / 10.0) * R * 0.38, FMath::Sin(A1 - TNProcMap::TwoPi / 10.0) * R * 0.38, 2.5);
			Decor.AddTri(C + Up * 4.0, Inner1, Tip, Up, Color);
			Decor.AddTri(C + Up * 4.0, Tip, Inner0, Up, Color * 0.9f);
		}
	}

	/** Concha de vieira en el suelo (abanico de costillas). */
	void AddFloorShell(FBuffers& Decor, const FVector& C, double R, double Yaw, const FLinearColor& Color)
	{
		const FVector Up(0.0, 0.0, 1.0);
		const FVector Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
		const FVector Side(-Fwd.Y, Fwd.X, 0.0);
		for (int32 f = 0; f < 6; ++f)
		{
			const double Fa = FMath::DegreesToRadians(-60.0 + f * 20.0);
			const double Fb = FMath::DegreesToRadians(-60.0 + (f + 1) * 20.0);
			const FVector Ea = C + (Fwd * FMath::Cos(Fa) + Side * FMath::Sin(Fa)) * R + Up * 3.0;
			const FVector Eb = C + (Fwd * FMath::Cos(Fb) + Side * FMath::Sin(Fb)) * R + Up * 3.0;
			Decor.AddTri(C + Up * 5.0, Ea, Eb, Up, (f % 2) ? Color : Color * 0.85f);
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
			const double Top = Height - 18.0 * FMath::Sin(PI * (p + 0.5) / Planks);
			B.AddBox(FVector(X, 0.0, Top * 0.5), FVector(1.0, 0.0, 0.0), FVector(PlankW * 0.5 - 2.0, 15.0, Top * 0.5), (p % 2) ? WoodA : WoodB);
		}
		for (const double Z : { Height * 0.18, Height * 0.5, Height * 0.8 })
		{
			B.AddBox(FVector(Width * 0.5, 0.0, Z), FVector(1.0, 0.0, 0.0), FVector(Width * 0.5 - 6.0, 18.0, 9.0), Iron);
		}
		// Tirador dorado junto al centro de la puerta (el extremo libre de la hoja).
		TNProcMesh::TNProcAddCylinder(B, FVector(Width - 50.0, -18.0, Height * 0.42), FVector(Width - 50.0, -30.0, Height * 0.42), 22.0, 18.0, 10, Gold);
		TNProcMesh::TNProcAddCylinder(B, FVector(Width - 50.0, 18.0, Height * 0.42), FVector(Width - 50.0, 30.0, Height * 0.42), 22.0, 18.0, 10, Gold);
	}

	/** Base (media cáscara con borde en zigzag) de un huevo en Center: cara de fuera y de dentro. */
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
		// Dientes del borde (la tapa tiene los contrarios), un poco por dentro: cerrado, quedan bajo la tapa.
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
		// Nido de paja alrededor.
		for (int32 n = 0; n < 14; ++n)
		{
			const double A = TNProcMap::TwoPi * n / 14.0;
			const FVector P0 = Center + FVector(FMath::Cos(A) * 120.0, FMath::Sin(A) * 120.0, 6.0);
			const FVector P1 = Center + FVector(FMath::Cos(A + 0.7) * 112.0, FMath::Sin(A + 0.7) * 112.0, 14.0 + 6.0 * (n % 2));
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
		// Un poco por fuera de la base: cerrado, el zigzag de la tapa tapa el de la base sin parpadeos.
		const double Rim = Rs[0] * 1.025;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg;
			const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			B.AddTri(Top + FVector(0.0, 0.0, 3.0), FVector(FMath::Cos(A0) * Rs.Last(), FMath::Sin(A0) * Rs.Last(), Zs.Last()),
				FVector(FMath::Cos(A1) * Rs.Last(), FMath::Sin(A1) * Rs.Last(), Zs.Last()), FVector(0.0, 0.0, 1.0), Shell);
			// Diente hacia abajo en cada vértice del anillo (encaja entre dos dientes de la base).
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
}

bool ATN_SandCastleLobby::IsEnabled()
{
	return TNCastleDetail::CVarLobbyCastle.GetValueOnGameThread() != 0;
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
}

void ATN_SandCastleLobby::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_SandCastleLobby, bGateOpen);
	DOREPLIFETIME(ATN_SandCastleLobby, EggMask);
}

void ATN_SandCastleLobby::BeginPlay()
{
	Super::BeginPlay();
	HideMaquette();
	BuildCastle();
	BuildGatesAndEggs();
	UE_LOG(LogTortunabo, Log, TEXT("[Castillo] Lobby de castillo de arena construido en %s."), *GetActorLocation().ToString());
}

void ATN_SandCastleLobby::HideMaquette()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	const FVector OldEggs = GetActorLocation() + FVector(0.0, -250.0, 0.0);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == this) { continue; }
		if (ATN_LobbyReadyZone* Zone = Cast<ATN_LobbyReadyZone>(Actor))
		{
			// Ahora se está listo metiéndose en un huevo de la sala de espera.
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
		const bool bFence = ClassName.Contains(TEXT("BP_Fence")) || ClassName.Contains(TEXT("BP_Tower"));
		// Las paredes (unos 10 m de alto); si el suelo también se llama «Extrude», se queda.
		FVector BoundsOrigin, BoundsExtent;
		Actor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
		const bool bWalls = Names.Contains(TEXT("Extrude")) && BoundsExtent.Z > 150.0;
		const bool bOldEgg = Actor->IsA<AStaticMeshActor>() && Names.Contains(TEXT("Capsule")) && FVector::Dist2D(Actor->GetActorLocation(), OldEggs) < 800.0;
		if (bFence || bWalls || bOldEgg)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
	}
}

void ATN_SandCastleLobby::AddBarrier(const FVector& Center, const FVector& Extent, float Yaw)
{
	UBoxComponent* Box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	Box->SetupAttachment(CastleRoot);
	Box->SetBoxExtent(Extent);
	Box->SetRelativeLocationAndRotation(Center, FRotator(0.f, Yaw, 0.f));
	Box->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Box->SetHiddenInGame(true);
	Box->SetCanEverAffectNavigation(false);
	Box->RegisterComponent();
	Barriers.Add(Box);
}

void ATN_SandCastleLobby::AddSign(const FString& Text, const FVector& Location, float Yaw, float WorldSize, const FColor& Color)
{
	UTextRenderComponent* SignText = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
	SignText->SetupAttachment(CastleRoot);
	SignText->SetRelativeLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f));
	SignText->SetHorizontalAlignment(EHTA_Center);
	SignText->SetVerticalAlignment(EVRTA_TextCenter);
	SignText->SetWorldSize(WorldSize);
	SignText->SetTextRenderColor(Color);
	SignText->SetText(FText::FromString(Text));
	SignText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SignText->RegisterComponent();
	Signs.Add(SignText);
}

void ATN_SandCastleLobby::BuildCastle()
{
	using namespace TNCastleDetail;
	FBuffers B;
	FBuffers Decor;
	TArray<FBox> BarrierBoxes;
	const FLinearColor Coral = Col(0xFF6A52);
	const FLinearColor Teal = Col(0x2EC4B6);
	const FLinearColor Gold = Col(0xFFCB3D);
	const FLinearColor Purple = Col(0x9B5DE5);
	const FLinearColor Sky = Col(0x4CC9F0);

	// ── Suelo de arena (con manchas de arena mojada) de todo el patio y la sala de espera ──
	{
		const double Cell = 250.0;
		for (double X = -2500.0; X < 2500.0 - 1.0; X += Cell)
		{
			for (double Y = -2500.0; Y < RoomEndY + 150.0 - 1.0; Y += Cell)
			{
				const int32 Ix = static_cast<int32>((X + 2500.0) / Cell);
				const int32 Iy = static_cast<int32>((Y + 2500.0) / Cell);
				const double N = TNProcMesh::TNProcHashNoise(Ix, Iy, 71u);
				const FLinearColor C = N > 0.62 ? Col(0xE3C284) : (N > -0.2 ? Col(0xF3DDA6) : Col(0xEED29A));
				const FVector P0(X, Y, FloorZ), P1(X + Cell, Y, FloorZ), P2(X + Cell, Y + Cell, FloorZ), P3(X, Y + Cell, FloorZ);
				B.AddQuad(P0, P1, P2, P3, FVector::UpVector, C);
			}
		}
		// Canto del suelo por fuera (se ve desde las murallas).
		const double MaxY = RoomEndY + 150.0;
		B.AddQuad(FVector(-2500.0, -2500.0, FloorZ), FVector(2500.0, -2500.0, FloorZ), FVector(2500.0, -2500.0, -60.0), FVector(-2500.0, -2500.0, -60.0),
			FVector(0.0, -1.0, 0.0), Col(0xC9A56A));
		B.AddQuad(FVector(-2500.0, MaxY, FloorZ), FVector(2500.0, MaxY, FloorZ), FVector(2500.0, MaxY, -60.0), FVector(-2500.0, MaxY, -60.0),
			FVector(0.0, 1.0, 0.0), Col(0xC9A56A));
	}

	// ── Murallas del patio (la del norte con la puerta grande) y de la sala de espera (con la puerta del mar) ──
	AddWall(B, Decor, FVector2D(-Yard, -Yard), FVector2D(Yard, -Yard), FVector2D(0.0, -1.0), 1u, BarrierBoxes);
	AddWall(B, Decor, FVector2D(-Yard, -Yard), FVector2D(-Yard, GateY), FVector2D(-1.0, 0.0), 2u, BarrierBoxes);
	AddWall(B, Decor, FVector2D(Yard, -Yard), FVector2D(Yard, GateY), FVector2D(1.0, 0.0), 3u, BarrierBoxes);
	AddWall(B, Decor, FVector2D(-Yard, GateY), FVector2D(-GateHalfW, GateY), FVector2D(0.0, 1.0), 4u, BarrierBoxes);
	AddWall(B, Decor, FVector2D(GateHalfW, GateY), FVector2D(Yard, GateY), FVector2D(0.0, 1.0), 5u, BarrierBoxes);
	AddWall(B, Decor, FVector2D(-RoomHalfX, GateY), FVector2D(-RoomHalfX, RoomEndY), FVector2D(-1.0, 0.0), 6u, BarrierBoxes);
	AddWall(B, Decor, FVector2D(RoomHalfX, GateY), FVector2D(RoomHalfX, RoomEndY), FVector2D(1.0, 0.0), 7u, BarrierBoxes);
	AddWall(B, Decor, FVector2D(-RoomHalfX, RoomEndY), FVector2D(-SeaHalfW, RoomEndY), FVector2D(0.0, 1.0), 8u, BarrierBoxes);
	AddWall(B, Decor, FVector2D(SeaHalfW, RoomEndY), FVector2D(RoomHalfX, RoomEndY), FVector2D(0.0, 1.0), 9u, BarrierBoxes);
	// Dinteles de las dos puertas (arcos de arena por encima del hueco).
	B.AddBox(FVector(0.0, GateY, (GateH + WallH) * 0.5 + 20.0), FVector(1.0, 0.0, 0.0), FVector(GateHalfW + 10.0, WallHalfT + 10.0, (WallH - GateH) * 0.5 + 20.0), Col(0xE8C889));
	B.AddBox(FVector(0.0, RoomEndY, (SeaGateH + WallH) * 0.5), FVector(1.0, 0.0, 0.0), FVector(SeaHalfW + 10.0, WallHalfT, (WallH - SeaGateH) * 0.5), Col(0xE8C889));
	// Por encima del hueco de la puerta del mar tampoco se sale.
	BarrierBoxes.Add(FBox::BuildAABB(FVector(0.0, RoomEndY + 70.0, WallH + 420.0), FVector(SeaHalfW + 20.0, 20.0, 420.0)));

	// ── Torres de cubo: esquinas, puerta grande, sala de espera y puerta del mar ──
	AddTower(B, Decor, FVector2D(-Yard, -Yard), 260.0, 820.0, Coral);
	AddTower(B, Decor, FVector2D(Yard, -Yard), 260.0, 820.0, Teal);
	AddTower(B, Decor, FVector2D(-Yard, GateY), 250.0, 800.0, Gold);
	AddTower(B, Decor, FVector2D(Yard, GateY), 250.0, 800.0, Purple);
	AddTower(B, Decor, FVector2D(-GateHalfW - 150.0, GateY), 220.0, 900.0, Coral);
	AddTower(B, Decor, FVector2D(GateHalfW + 150.0, GateY), 220.0, 900.0, Teal);
	AddTower(B, Decor, FVector2D(-RoomHalfX, RoomEndY), 200.0, 700.0, Sky);
	AddTower(B, Decor, FVector2D(RoomHalfX, RoomEndY), 200.0, 700.0, Gold);
	AddTower(B, Decor, FVector2D(-SeaHalfW - 120.0, RoomEndY), 170.0, 680.0, Purple);
	AddTower(B, Decor, FVector2D(SeaHalfW + 120.0, RoomEndY), 170.0, 680.0, Coral);

	// ── Parkour 1: escalera de arena hasta el adarve sur (esquina suroeste) ──
	for (int32 s = 0; s < 6; ++s)
	{
		AddBlock(B, FVector2D(-2150.0 + s * 200.0, -2195.0), FVector2D(98.0, 105.0), 90.0 * (s + 1));
	}

	// ── Parkour 2: circuito de saltos y panzazo (sur): escalón, A, B (salto de 1,8 m) y C (3,3 m: con panzazo) ──
	// Distancias medidas de borde a borde y pensadas para la velocidad del lobby (andar a 2 m/s).
	AddBlock(B, FVector2D(-1260.0, -1650.0), FVector2D(110.0, 110.0), 110.0);
	AddBlock(B, FVector2D(-900.0, -1650.0), FVector2D(160.0, 160.0), 220.0);
	AddBlock(B, FVector2D(-400.0, -1650.0), FVector2D(160.0, 160.0), 220.0);
	AddBlock(B, FVector2D(250.0, -1650.0), FVector2D(160.0, 160.0), 220.0);
	// Rampa de concha: baja de la plataforma C hasta la arena (costillas de colores, con grosor).
	{
		const double X0 = 410.0, X1 = 1020.0, Z0 = 220.0, Z1 = FloorZ;
		const int32 Ribs = 9;
		for (int32 r = 0; r < Ribs; ++r)
		{
			const double T0 = static_cast<double>(r) / Ribs, T1 = static_cast<double>(r + 1) / Ribs;
			const double TopHalf = 150.0, BottomHalf = 320.0;
			const FVector A0(X0, -1650.0 + FMath::Lerp(-TopHalf, TopHalf, T0), Z0);
			const FVector A1(X0, -1650.0 + FMath::Lerp(-TopHalf, TopHalf, T1), Z0);
			const FVector B0(X1, -1650.0 + FMath::Lerp(-BottomHalf, BottomHalf, T0), Z1 + 6.0);
			const FVector B1(X1, -1650.0 + FMath::Lerp(-BottomHalf, BottomHalf, T1), Z1 + 6.0);
			const FVector Lift(0.0, 0.0, 8.0 * FMath::Sin(PI * (T0 + T1) * 0.5 * Ribs));
			const FLinearColor RibC = (r % 2) ? Col(0xFFB4A2) : Col(0xFF8A70);
			B.AddQuad(A0 + Lift, A1 + Lift, B1 + Lift, B0 + Lift, FVector(0.34, 0.0, 1.0), RibC);
			B.AddQuad(A0 - FVector(0.0, 0.0, 14.0), A1 - FVector(0.0, 0.0, 14.0), B1 - FVector(0.0, 0.0, 14.0), B0 - FVector(0.0, 0.0, 14.0),
				FVector(-0.34, 0.0, -1.0), RibC * 0.8f);
		}
		// Bisagra de la concha arriba.
		B.AddBox(FVector(X0 - 10.0, -1650.0, Z0 - 6.0), FVector(1.0, 0.0, 0.0), FVector(22.0, 150.0, 12.0), Col(0xFF8A70));
	}

	// ── Parkour 3: pilares de cubo de playa (este), pasarela de palos de polo y torreón hasta el adarve este ──
	const FLinearColor BucketColors[6] = { Coral, Teal, Gold, Sky, Purple, Col(0x3DDC62) };
	for (int32 p = 0; p < 6; ++p)
	{
		AddBucketPillar(B, FVector2D(2080.0, -2000.0 + p * 300.0), 90.0 + p * 80.0, BucketColors[p]);
	}
	{
		const double PillarTop = 90.0 + 5 * 80.0;
		const double Y0 = -500.0 + 70.0, Y1 = 450.0 - 150.0;
		const double Z0 = PillarTop, Z1 = 500.0;
		// Dos largueros y los palos de polo encima, de través.
		for (const double Side : { -24.0, 24.0 })
		{
			B.AddBeam(FVector(2080.0 + Side, Y0, Z0 - 6.0), FVector(2080.0 + Side, Y1, Z1 - 6.0), 4.0, Col(0xC9A56A));
		}
		int32 Stick = 0;
		for (double Y = Y0 + 8.0; Y < Y1 - 4.0; Y += 15.0, ++Stick)
		{
			const double Z = FMath::Lerp(Z0, Z1, (Y - Y0) / (Y1 - Y0));
			B.AddBox(FVector(2080.0, Y, Z), FVector(1.0, 0.0, 0.0), FVector(40.0, 6.0, 2.5), (Stick % 2) ? Col(0xEFD29A) : Col(0xE2BF7C));
		}
		for (const double Y : { -150.0, 120.0 })
		{
			const double Z = FMath::Lerp(Z0, Z1, (Y - Y0) / (Y1 - Y0));
			B.AddBox(FVector(2080.0, Y, Z * 0.5), FVector(1.0, 0.0, 0.0), FVector(6.0, 6.0, Z * 0.5), Col(0xE2BF7C));
		}
		AddBlock(B, FVector2D(2080.0, 450.0), FVector2D(150.0, 150.0), 500.0);
		AddFloorShell(Decor, FVector(2080.0, 450.0, 505.0), 60.0, 0.4, Col(0xFFE0C2));
	}

	// ── Adornos: conchas y estrellas de mar por la arena, cubo y pala junto a la puerta, el mar al fondo ──
	for (int32 i = 0; i < 40; ++i)
	{
		const double X = -2250.0 + 4500.0 * FMath::Frac(i * 0.754877);
		const double Y = -2250.0 + 4300.0 * FMath::Frac(i * 0.569840 + 0.13);
		if (i % 3 == 0)
		{
			AddStarfish(Decor, FVector(X, Y, FloorZ), 26.0 + 10.0 * FMath::Frac(i * 0.31), i * 0.7, (i % 2) ? Col(0xFF8A70) : Col(0xFFB077));
		}
		else
		{
			AddFloorShell(Decor, FVector(X, Y, FloorZ), 22.0 + 10.0 * FMath::Frac(i * 0.43), i * 1.3, (i % 2) ? Col(0xFFE0C2) : Col(0xFFB4A2));
		}
	}
	{
		const FVector Bucket(760.0, 1950.0, FloorZ);
		TNProcMesh::TNProcAddCylinder(Decor, Bucket, Bucket + FVector(0.0, 0.0, 70.0), 38.0, 48.0, 12, Teal);
		Decor.AddBeam(Bucket + FVector(-48.0, 0.0, 70.0), Bucket + FVector(0.0, 0.0, 115.0), 2.0, Col(0x3B3F4A));
		Decor.AddBeam(Bucket + FVector(48.0, 0.0, 70.0), Bucket + FVector(0.0, 0.0, 115.0), 2.0, Col(0x3B3F4A));
		const FVector Spade(860.0, 1900.0, FloorZ);
		Decor.AddBeam(Spade + FVector(0.0, 0.0, 30.0), Spade + FVector(-20.0, 30.0, 140.0), 3.5, Coral);
		Decor.AddBox(Spade + FVector(4.0, -6.0, 18.0), FVector(0.8, 0.6, 0.0), FVector(18.0, 3.0, 20.0), Gold);
	}
	// El mar detrás de la puerta del mar (con la orilla de espuma).
	Decor.AddQuad(FVector(-6000.0, RoomEndY + 250.0, -40.0), FVector(6000.0, RoomEndY + 250.0, -40.0), FVector(6000.0, 12000.0, -40.0),
		FVector(-6000.0, 12000.0, -40.0), FVector::UpVector, Col(0x1E9CC6));
	Decor.AddQuad(FVector(-6000.0, RoomEndY + 150.0, -20.0), FVector(6000.0, RoomEndY + 150.0, -20.0), FVector(6000.0, RoomEndY + 260.0, -38.0),
		FVector(-6000.0, RoomEndY + 260.0, -38.0), FVector::UpVector, Col(0xE0F8F5));

	// ── Componentes ──
	UMaterialInterface* Mat = VertexColorMaterial();
	const TArray<FProcMeshTangent> NoTangents;
	CastleMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	CastleMesh->SetupAttachment(CastleRoot);
	// Colisión cocinada en el acto: el suelo de arena tiene que estar listo antes de que aparezca nadie encima.
	CastleMesh->bUseAsyncCooking = false;
	CastleMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	CastleMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	CastleMesh->RegisterComponent();
	CastleMesh->CreateMeshSection_LinearColor(0, B.Verts, B.Tris, B.Normals, B.UVs, B.Colors, NoTangents, true);
	CastleMesh->SetMaterial(0, Mat);

	DecorMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	DecorMesh->SetupAttachment(CastleRoot);
	DecorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecorMesh->RegisterComponent();
	DecorMesh->CreateMeshSection_LinearColor(0, Decor.Verts, Decor.Tris, Decor.Normals, Decor.UVs, Decor.Colors, NoTangents, false);
	DecorMesh->SetMaterial(0, Mat);

	for (const FBox& Box : BarrierBoxes)
	{
		AddBarrier(Box.GetCenter(), Box.GetExtent(), 0.f);
	}
	// La puerta del mar siempre bloquea (detrás no hay suelo).
	AddBarrier(FVector(0.0, RoomEndY, SeaGateH * 0.5), FVector(SeaHalfW, 40.0, SeaGateH * 0.5), 0.f);

	// Carteles: la sala de espera sobre la puerta grande, la playa sobre la del mar y el nombre en la muralla sur.
	AddSign(TEXT("SALA DE ESPERA"), FVector(0.0, GateY - WallHalfT - 12.0, 515.0), -90.f, 70.f, FColor(255, 214, 90));
	AddSign(TEXT("¡A LA PLAYA!"), FVector(0.0, RoomEndY - WallHalfT - 8.0, 490.0), -90.f, 64.f, FColor(255, 246, 232));
	AddSign(TEXT("TORTUNAVY"), FVector(0.0, -Yard + WallHalfT + 10.0, 400.0), 90.f, 150.f, FColor(255, 150, 120));
}

void ATN_SandCastleLobby::BuildGatesAndEggs()
{
	using namespace TNCastleDetail;
	UMaterialInterface* Mat = VertexColorMaterial();

	auto MakeLeaf = [this, Mat](double Width, double Height, const FVector& Hinge, float Yaw) -> UStaticMeshComponent*
	{
		FBuffers Leaf;
		BuildLeaf(Leaf, Width, Height);
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Comp->SetupAttachment(CastleRoot);
		Comp->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, Leaf, Mat));
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetRelativeLocationAndRotation(Hinge, FRotator(0.f, Yaw, 0.f));
		Comp->RegisterComponent();
		return Comp;
	};
	// Puerta grande: la hoja izquierda (bisagra en -X) cerrada apunta a +X; la derecha, a -X.
	GateLeaves.Add(MakeLeaf(GateHalfW, GateH, FVector(-GateHalfW, GateY, FloorZ), 0.f));
	GateLeaves.Add(MakeLeaf(GateHalfW, GateH, FVector(GateHalfW, GateY, FloorZ), 180.f));
	SeaLeaves.Add(MakeLeaf(SeaHalfW, SeaGateH, FVector(-SeaHalfW, RoomEndY, FloorZ), 0.f));
	SeaLeaves.Add(MakeLeaf(SeaHalfW, SeaGateH, FVector(SeaHalfW, RoomEndY, FloorZ), 180.f));

	GateBlock = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	GateBlock->SetupAttachment(CastleRoot);
	GateBlock->SetBoxExtent(FVector(GateHalfW, 40.0, GateH * 0.5));
	GateBlock->SetRelativeLocation(FVector(0.0, GateY, GateH * 0.5));
	GateBlock->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	GateBlock->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GateBlock->SetHiddenInGame(true);
	GateBlock->RegisterComponent();
	bGateBlocking = true;

	// Huevos de la sala de espera: base en los adornos (sin colisión: se entra andando) y tapa que baja al ocuparlo.
	FBuffers Cups;
	for (int32 i = 0; i < NumEggs; ++i)
	{
		const FLinearColor Shell = Col(0xFFF3DC);
		const FLinearColor Accent = Col(EggAccents[i]);
		BuildEggCup(Cups, FVector(EggSpots[i].X, EggSpots[i].Y, FloorZ), Shell, Accent);
		FBuffers Lid;
		BuildEggLid(Lid, Pal(0xFFF3DC), Pal(EggAccents[i]));
		UStaticMeshComponent* LidComp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		LidComp->SetupAttachment(CastleRoot);
		LidComp->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, Lid, Mat));
		LidComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LidComp->SetRelativeLocation(FVector(EggSpots[i].X, EggSpots[i].Y, FloorZ + EggSeam + 160.0));
		LidComp->RegisterComponent();
		EggLids.Add(LidComp);
	}
	UProceduralMeshComponent* CupMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	CupMesh->SetupAttachment(CastleRoot);
	CupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CupMesh->RegisterComponent();
	const TArray<FProcMeshTangent> NoTangents;
	CupMesh->CreateMeshSection_LinearColor(0, Cups.Verts, Cups.Tris, Cups.Normals, Cups.UVs, Cups.Colors, NoTangents, false);
	CupMesh->SetMaterial(0, Mat);
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
				if (FVector2D::Distance(FVector2D(Local.X, Local.Y), EggSpots[i]) < 95.0 && Local.Z < 320.0)
				{
					Mask |= 1 << i;
					bInEgg = true;
					break;
				}
			}
			bAnyoneNearGate |= FVector2D::Distance(FVector2D(Local.X, Local.Y), FVector2D(0.0, GateY)) < 1000.0;
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

	// Puerta grande: se abre hacia la sala de espera en ~1,2 s; bloquea solo cerrada del todo.
	GateOpenness = FMath::FInterpConstantTo(GateOpenness, bGateOpen ? 1.f : 0.f, DeltaSeconds, 0.8f);
	const float GateAngle = 100.f * SmoothStep01(GateOpenness);
	if (GateLeaves.Num() == 2)
	{
		GateLeaves[0]->SetRelativeRotation(FRotator(0.f, GateAngle, 0.f));
		GateLeaves[1]->SetRelativeRotation(FRotator(0.f, 180.f - GateAngle, 0.f));
	}
	const bool bShouldBlock = !bGateOpen && GateOpenness < 0.05f;
	if (GateBlock && bShouldBlock != bGateBlocking)
	{
		bGateBlocking = bShouldBlock;
		GateBlock->SetCollisionEnabled(bShouldBlock ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	// Puerta del mar: se abre con la cuenta atrás (se ve el mar; sigue sin dejar salir).
	const ATN_CoopGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATN_CoopGameState>() : nullptr;
	const bool bCountdown = GS && (GS->MatchFlowState == ETNMatchFlowState::Countdown || GS->MatchFlowState == ETNMatchFlowState::Cinematic);
	SeaOpenness = FMath::FInterpConstantTo(SeaOpenness, bCountdown ? 1.f : 0.f, DeltaSeconds, 0.6f);
	const float SeaAngle = 95.f * SmoothStep01(SeaOpenness);
	if (SeaLeaves.Num() == 2)
	{
		SeaLeaves[0]->SetRelativeRotation(FRotator(0.f, SeaAngle, 0.f));
		SeaLeaves[1]->SetRelativeRotation(FRotator(0.f, 180.f - SeaAngle, 0.f));
	}

	// Huevos: la tapa flota encima dando vueltecitas y baja a cerrar el huevo cuando alguien se mete (y se mece).
	for (int32 i = 0; i < EggLids.Num() && i < NumEggs; ++i)
	{
		const bool bOccupied = (EggMask & (1 << i)) != 0;
		EggClose[i] = FMath::FInterpConstantTo(EggClose[i], bOccupied ? 1.f : 0.f, DeltaSeconds, 2.2f);
		const float Close = SmoothStep01(EggClose[i]);
		const float Bob = 10.f * FMath::Sin(Clock * 1.6f + i * 0.9f);
		const float Height = FMath::Lerp(160.f + Bob, 0.f, Close);
		const float Tilt = FMath::Lerp(16.f, 0.f, Close);
		const float Rock = Close * 3.5f * FMath::Sin(Clock * 3.1f + i);
		EggLids[i]->SetRelativeLocationAndRotation(
			FVector(EggSpots[i].X, EggSpots[i].Y, FloorZ + EggSeam + Height),
			FRotator(Tilt * FMath::Sin(Clock * 0.7f + i) + Rock, Clock * 12.f * (1.f - Close) + i * 40.f, Tilt * FMath::Cos(Clock * 0.7f + i)));
	}
}
