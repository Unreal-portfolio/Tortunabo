#include "World/Beach/TN_BeachCatapult.h"
#include "World/Beach/TN_BeachCameraShake.h"
#include "World/Beach/TN_BeachTrapSynthComponent.h"
#include "Lobby/Playground/TN_PlaygroundSynthComponent.h"
#include "Core/TN_Log.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "TN_BeachRideKit.h"
#include "TN_BeachTrapKit.h"

/**
 * Geometría de la catapulta. El brazo (la cuchara con el cubito) se construye en el espacio de su eje: origen sobre el
 * tapón, X hacia el cubito, la cara de arriba del mango en Z = +grosor/2. El cazo queda en el extremo -X. La piedra, el
 * tapón, el palo que sujeta y los banderines van en el espacio del marco (X hacia el mar, origen en la arena).
 */
namespace TNBeachCatapultDetail
{
	using FBuffers = TNBeachTrapKit::FBuffers;
	using FHulls = TNBeachTrapKit::FHulls;

	constexpr double HandleWidth = 90.0;
	constexpr double SwingSeconds = 0.16;
	constexpr double BounceEnd = 0.6;
	constexpr double HoldEnd = 1.1;
	constexpr double SettleSeconds = 0.3;
	constexpr double RatchetStep = 0.2;
	/** Colisión del brazo apagada durante el golpe (las bolas salen sin tocarlo). */
	constexpr double NoCollisionSeconds = 0.55;

	struct FCatapultArm
	{
		double Long = 750.0;
		double Short = 350.0;
		double Thick = 30.0;
		double BowlL = 300.0;
		double BowlHW = 110.0;
		double BowlDepth = 30.0;
		double BucketR = 95.0;
		double BucketH = 95.0;
		int32 Style = 0;
	};

	/** Cazo: plato elíptico (borde a ras de la cara de arriba del mango) con su cara de abajo. */
	void AddBowl(FBuffers& B, const FCatapultArm& A, const FLinearColor& Col)
	{
		constexpr int32 Seg = 24;
		static const double Rings[5] = { 1.0, 0.85, 0.62, 0.34, 0.0 };
		const double Cx = -A.Long + 0.5 * A.BowlL;
		const double Top = 0.5 * A.Thick;
		const FLinearColor Inside = TNPlaygroundKit::Mix(Col, TNPlaygroundKit::Rgb(0xFFFFFF, Col.A), 0.18);
		TArray<FVector> In;
		TArray<FVector> Out;
		In.SetNum(5 * Seg);
		Out.SetNum(5 * Seg);
		for (int32 r = 0; r < 5; ++r)
		{
			for (int32 k = 0; k < Seg; ++k)
			{
				const double Phi = TNPlaygroundKit::KitTwoPi * k / Seg;
				const double X = Cx + 0.5 * A.BowlL * Rings[r] * FMath::Cos(Phi);
				const double Y = A.BowlHW * Rings[r] * FMath::Sin(Phi);
				const double Z = Top - A.BowlDepth * (1.0 - Rings[r] * Rings[r]);
				In[r * Seg + k] = FVector(X, Y, Z);
				Out[r * Seg + k] = FVector(X * 1.0, Y * 1.03, Z - 10.0);
			}
		}
		for (int32 r = 0; r < 4; ++r)
		{
			for (int32 k = 0; k < Seg; ++k)
			{
				const int32 K1 = (k + 1) % Seg;
				B.AddQuad(In[r * Seg + k], In[r * Seg + K1], In[(r + 1) * Seg + K1], In[(r + 1) * Seg + k], FVector::UpVector, (r % 2) ? Inside : TNPlaygroundKit::Shade(Inside, 0.96));
				B.AddQuad(Out[r * Seg + k], Out[r * Seg + K1], Out[(r + 1) * Seg + K1], Out[(r + 1) * Seg + k], -FVector::UpVector, TNPlaygroundKit::Shade(Col, 0.85));
			}
		}
		for (int32 k = 0; k < Seg; ++k)
		{
			const int32 K1 = (k + 1) % Seg;
			const double Am = TNPlaygroundKit::KitTwoPi * (k + 0.5) / Seg;
			B.AddQuad(In[k], In[K1], Out[K1], Out[k], FVector(FMath::Cos(Am), FMath::Sin(Am), 0.0), Col);
		}
	}

	/** Cubito de arena mojada atado en el extremo del mango, con su asa y una estrella. */
	void AddBucket(FBuffers& B, const FCatapultArm& A, uint32 Seed)
	{
		const FLinearColor Toy = TNPlaygroundKit::ToyColor(static_cast<int32>(Seed % 7u), 0.25f);
		const double Bx = A.Short - 0.9 * A.BucketR;
		const double Base = 0.5 * A.Thick;
		const double TopZ = Base + A.BucketH;
		TNPlaygroundKit::AddFrustum(B, FVector(Bx, 0.0, Base), FVector(Bx, 0.0, TopZ), 0.82 * A.BucketR, A.BucketR, 18, Toy, TNPlaygroundKit::Shade(Toy, 0.85), true, false);
		// Reborde, arena mojada con marcas de dedos y el asa caída.
		TNPlaygroundKit::AddFrustum(B, FVector(Bx, 0.0, TopZ - 10.0), FVector(Bx, 0.0, TopZ + 2.0), A.BucketR + 6.0, A.BucketR + 6.0, 18,
			TNPlaygroundKit::Shade(Toy, 1.1), TNPlaygroundKit::Shade(Toy, 1.1), true, true);
		TNPlaygroundKit::AddDisc(B, FVector(Bx, 0.0, TopZ - 6.0), FVector::UpVector, A.BucketR - 4.0, 18, TNBeachTrapKit::SandWet());
		for (int32 i = 0; i < 3; ++i)
		{
			const double Ang = TNPlaygroundKit::KitTwoPi * (i + 0.3) / 3.0;
			TNPlaygroundKit::AddEllipsoid(B, FVector(Bx + 35.0 * FMath::Cos(Ang), 35.0 * FMath::Sin(Ang), TopZ - 5.0), FVector::ForwardVector, FVector::RightVector,
				FVector::UpVector, FVector(12.0, 9.0, 3.0), 8, 3, TNBeachTrapKit::SandDeep());
		}
		TArray<FVector> Handle;
		for (int32 i = 0; i <= 8; ++i)
		{
			const double T = TNPlaygroundKit::KitPi * i / 8.0;
			Handle.Add(FVector(Bx, (A.BucketR + 8.0) * FMath::Cos(T), TopZ - 12.0 + 34.0 * FMath::Sin(T)));
		}
		TNPlaygroundKit::AddTube(B, Handle, { 5.0 }, 6, { TNPlaygroundKit::Shade(Toy, 0.8) }, FVector::ForwardVector, false);
		TNPlaygroundKit::AddStarfish(B, FVector(Bx - 0.91 * A.BucketR, 0.0, Base + 0.55 * A.BucketH), FVector(-1.0, 0.0, 0.1), FVector::UpVector, 26.0, 3.0,
			TNPlaygroundKit::Rgb(0xFFF1A8, 0.3f));
	}

	/** Cuchara: de plástico (0), de madera con vetas (1) o dos palos de polo atados con un vasito de yogur (2). */
	void BuildArm(FBuffers& B, const FCatapultArm& A, uint32 Seed)
	{
		const double Top = 0.5 * A.Thick;
		const double HandleX0 = -A.Long + 0.85 * A.BowlL;
		const double HandleX1 = A.Short;
		if (A.Style == 2)
		{
			const FLinearColor Wood = TNPlaygroundKit::Rgb(0xE2C08C, 0.05f);
			for (const double Side : { -1.0, 1.0 })
			{
				TNPlaygroundKit::AddStick(B, FVector(0.5 * (HandleX0 + HandleX1), Side * 23.0, 0.0), FVector::ForwardVector, FVector::RightVector, HandleX1 - HandleX0 + 40.0,
					46.0, A.Thick, TNPlaygroundKit::Shade(Wood, Side < 0.0 ? 1.0 : 0.94));
			}
			// Gomas elásticas que atan los palos.
			const FLinearColor Band = TNPlaygroundKit::Rgb(0xFF5FA2, 0.2f);
			for (const double Bx : { HandleX0 + 60.0, -40.0, 0.5 * HandleX1 })
			{
				TNPlaygroundKit::AddAxisBox(B, FVector(Bx, 0.0, Top + 2.0), FVector(7.0, 50.0, 2.5), Band);
				TNPlaygroundKit::AddAxisBox(B, FVector(Bx, 0.0, -Top - 2.0), FVector(7.0, 50.0, 2.5), Band);
				for (const double Side : { -1.0, 1.0 })
				{
					TNPlaygroundKit::AddAxisBox(B, FVector(Bx, Side * 49.0, 0.0), FVector(7.0, 2.5, Top + 3.0), Band);
				}
			}
			// Vasito de yogur por cazo (blanco con etiqueta de color).
			AddBowl(B, A, TNPlaygroundKit::Rgb(0xF7F3EA, 0.3f));
			const FLinearColor Label = TNPlaygroundKit::ToyColor(static_cast<int32>((Seed >> 4) % 7u), 0.2f);
			const double Cx = -A.Long + 0.5 * A.BowlL;
			TNPlaygroundKit::AddFrustum(B, FVector(Cx, 0.0, Top - A.BowlDepth - 12.0), FVector(Cx, 0.0, Top - 4.0), 0.72 * A.BowlHW, 0.98 * A.BowlHW, 20, Label,
				Label, false, false);
		}
		else
		{
			const FLinearColor Col = A.Style == 1 ? TNPlaygroundKit::Rgb(0xD9B07A, 0.05f) : TNPlaygroundKit::ToyColor(static_cast<int32>((Seed >> 2) % 7u), 0.35f);
			const TArray<FVector2D> Outline = TNPlaygroundKit::StadiumOutline(HandleX1 - HandleX0 + 30.0, HandleWidth, 5);
			TNPlaygroundKit::AddSlab(B, FVector(0.5 * (HandleX0 + HandleX1), 0.0, 0.0), FVector::ForwardVector, FVector::RightVector, FVector::UpVector, Outline, A.Thick,
				Col);
			// Cuello ensanchado hacia el cazo.
			const TArray<FVector2D> Neck = { FVector2D(HandleX0 - 40.0, -0.8 * A.BowlHW), FVector2D(HandleX0 + 120.0, -0.5 * HandleWidth),
				FVector2D(HandleX0 + 120.0, 0.5 * HandleWidth), FVector2D(HandleX0 - 40.0, 0.8 * A.BowlHW) };
			TNPlaygroundKit::AddSlab(B, FVector(0.0, 0.0, 0.0), FVector::ForwardVector, FVector::RightVector, FVector::UpVector, Neck, A.Thick * 0.9, Col);
			if (A.Style == 1)
			{
				// Vetas de la madera.
				for (int32 i = 0; i < 4; ++i)
				{
					const double Y = -30.0 + 20.0 * i;
					TNPlaygroundKit::AddAxisBox(B, FVector(0.5 * (HandleX0 + HandleX1) + 40.0 * TNBeachTrapKit::Hash01(i, 1, Seed), Y, Top + 0.6),
						FVector(0.3 * (HandleX1 - HandleX0), 2.0, 0.5), TNPlaygroundKit::Shade(Col, 0.8));
				}
			}
			AddBowl(B, A, Col);
		}
		AddBucket(B, A, Seed);
	}

	/** Cascos del brazo (espacio del eje): suelo y bordes del cazo, mango y cubito. */
	void BuildArmHulls(FHulls& Hulls, const FCatapultArm& A)
	{
		const double Top = 0.5 * A.Thick;
		const double Floor = Top - A.BowlDepth;
		const double X0 = -A.Long;
		const double X1 = -A.Long + A.BowlL;
		Hulls.Add(TNPlaygroundKit::HullAxisBox(FVector(0.5 * (X0 + X1), 0.0, Floor - 7.0), FVector(0.5 * (X1 - X0) - 12.0, A.BowlHW - 12.0, 7.0)));
		Hulls.Add(TNPlaygroundKit::HullAxisBox(FVector(X0 + 6.0, 0.0, 0.5 * (Floor - 14.0 + Top)), FVector(12.0, A.BowlHW, 0.5 * (Top - Floor + 14.0))));
		for (const double Side : { -1.0, 1.0 })
		{
			Hulls.Add(TNPlaygroundKit::HullAxisBox(FVector(0.5 * (X0 + X1), Side * (A.BowlHW - 9.0), 0.5 * (Floor - 14.0 + Top)),
				FVector(0.5 * (X1 - X0), 9.0, 0.5 * (Top - Floor + 14.0))));
		}
		Hulls.Add(TNPlaygroundKit::HullAxisBox(FVector(0.5 * (X1 - 20.0 + A.Short), 0.0, 0.0), FVector(0.5 * (A.Short - X1 + 20.0), 0.5 * HandleWidth, Top)));
		Hulls.Add(TNPlaygroundKit::HullCylinder(FVector(A.Short - 0.9 * A.BucketR, 0.0, Top), A.BucketH, 0.82 * A.BucketR, A.BucketR, 12));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachCatapult
// ─────────────────────────────────────────────────────────────────────────────

ATN_BeachCatapult::ATN_BeachCatapult()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(3.f);

	Frame = CreateDefaultSubobject<USceneComponent>(TEXT("Frame"));
	Frame->SetupAttachment(GetRootComponent());

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(Frame);
	TNBeachTrapKit::ConfigureVisual(BaseMesh);

	BaseCollision = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BaseCollision"));
	BaseCollision->SetupAttachment(Frame);
	TNBeachTrapKit::ConfigureSolid(BaseCollision, false);

	ArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ArmPivot"));
	ArmPivot->SetupAttachment(Frame);

	ArmMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmMesh"));
	ArmMesh->SetupAttachment(ArmPivot);
	TNBeachTrapKit::ConfigureVisual(ArmMesh);

	ArmCollision = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ArmCollision"));
	ArmCollision->SetupAttachment(ArmPivot);
	TNBeachTrapKit::ConfigureSolid(ArmCollision, false);

	PropPivot = CreateDefaultSubobject<USceneComponent>(TEXT("PropPivot"));
	PropPivot->SetupAttachment(Frame);
	PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
	PropMesh->SetupAttachment(PropPivot);
	TNBeachTrapKit::ConfigureVisual(PropMesh);

	FlagGreen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlagGreen"));
	FlagGreen->SetupAttachment(Frame);
	TNBeachTrapKit::ConfigureVisual(FlagGreen);
	FlagRed = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlagRed"));
	FlagRed->SetupAttachment(Frame);
	TNBeachTrapKit::ConfigureVisual(FlagRed);
	FlagRed->SetVisibility(false);
}

void ATN_BeachCatapult::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachCatapult, ArmedAt);
	DOREPLIFETIME(ATN_BeachCatapult, FiredAt);
}

void ATN_BeachCatapult::ApplySpec()
{
	using namespace TNBeachCatapultDetail;
	const double Fit = TNBeachTrapKit::FitRadius(Spec.Element, Spec.SizeScale);
	const uint32 Seed = TNBeachTrapKit::SeedOf(Spec.Seed, 83u);

	// Brazo: el cazo y el cubito tienen medidas de tortuga (no bajan de lo que cabe una); el resto escala con la huella.
	FCatapultArm A;
	A.Style = static_cast<int32>(Seed % 3u);
	const double Length = FMath::Clamp(1.22 * Fit, 820.0, 1150.0);
	A.Long = 0.68 * Length;
	A.Short = 0.32 * Length;
	A.Thick = 30.0;
	A.BowlL = FMath::Clamp(0.27 * Length, 250.0, 320.0);
	A.BowlHW = FMath::Clamp(0.1 * Length, 100.0, 120.0);
	A.BowlDepth = 30.0;
	// Cubito bajo y ancho: desde el mango se llega de un salto a su arena (~1,1 m por encima).
	A.BucketR = FMath::Clamp(0.08 * Length, 75.0, 92.0);
	A.BucketH = 70.0;
	LongArm = A.Long;
	ShortArm = A.Short;
	ArmThick = A.Thick;
	BowlLength = A.BowlL;
	BowlHalfWidth = A.BowlHW;
	BowlDepth = A.BowlDepth;
	BucketRadius = A.BucketR;
	BucketHeight = A.BucketH;

	// Fulcro: piedra y tapón de garrafa; el brazo se apoya encima.
	const double StoneH = FMath::Clamp(0.16 * Fit, 110.0, 145.0);
	const double CapH = 90.0;
	const double CapR = 78.0;
	PivotZ = StoneH + CapH + 0.5 * A.Thick;
	const double PivotX = FMath::Clamp(0.2 * Fit, 110.0, 190.0);

	// Reposo: el fondo del cazo toca la arena. Disparada: la cara de abajo del extremo del mango da en la arena.
	const double BowlCx = -(A.Long - 0.5 * A.BowlL);
	const double BowlBottom = 0.5 * A.Thick - A.BowlDepth - 10.0;
	double Theta = 0.2;
	for (int32 i = 0; i < 4; ++i)
	{
		Theta = FMath::Asin(FMath::Clamp((PivotZ + BowlBottom * FMath::Cos(Theta) - 2.0) / -BowlCx, 0.0, 0.9));
	}
	RestDeg = FMath::RadiansToDegrees(Theta);
	FiredDeg = -FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp((PivotZ - 0.5 * A.Thick - 4.0) / A.Short, 0.0, 0.95)));

	// Hacia su X si ya mira al mar (el reparto le deja libre el arco de salto por ahí); si no, hacia el mar.
	FrameYawDeg = TNBeachRideKit::LaunchYawInActor(this);
	Frame->SetRelativeRotation(FRotator(0.0, FrameYawDeg, 0.0));
	ArmPivot->SetRelativeLocationAndRotation(FVector(PivotX, 0.0, PivotZ), FRotator(RestDeg, 0.0, 0.0));

	TNBeachTrapKit::FBuffers Arm;
	BuildArm(Arm, A, Seed);
	TNBeachTrapKit::SetMesh(ArmMesh, this, Arm);
	TNBeachTrapKit::FHulls ArmHulls;
	BuildArmHulls(ArmHulls, A);
	ArmCollision->SetCollisionConvexMeshes(ArmHulls);

	// Piedra, tapón con estrías y una cuna para el brazo, arena removida.
	TNBeachTrapKit::FBuffers Base;
	TNBeachTrapKit::FHulls BaseHulls;
	const double StoneBaseR = CapR + 60.0;
	const double StoneTopR = CapR + 22.0;
	const TArray<FVector2D> StoneProfile = { FVector2D(0.0, StoneH), FVector2D(StoneTopR, StoneH), FVector2D(StoneTopR + 22.0, StoneH - 30.0),
		FVector2D(StoneBaseR, 30.0), FVector2D(StoneBaseR + 12.0, -25.0) };
	const TArray<FLinearColor> StoneCols = { TNBeachTrapKit::RockTone(0), TNBeachTrapKit::RockTone(2), TNBeachTrapKit::RockTone(1), TNBeachTrapKit::RockTone(0) };
	TNBeachTrapKit::AddLatheProfile(Base, FVector(PivotX, 0.0, 0.0), StoneProfile, StoneCols, 11, 0.07, Seed);
	BaseHulls.Add(TNPlaygroundKit::HullCylinder(FVector(PivotX, 0.0, -25.0), StoneH + 25.0, StoneBaseR, StoneTopR, 12));
	static const uint32 CapHex[4] = { 0xE63946, 0x1D7FD1, 0x2BB673, 0xF4F1EA };
	const FLinearColor CapCol = TNPlaygroundKit::Rgb(CapHex[(Seed >> 5) % 4u], 0.35f);
	TNPlaygroundKit::AddFrustum(Base, FVector(PivotX, 0.0, StoneH - 4.0), FVector(PivotX, 0.0, StoneH + CapH), CapR, CapR - 3.0, 24, CapCol,
		TNPlaygroundKit::Shade(CapCol, 1.08), false, true);
	for (int32 i = 0; i < 24; ++i)
	{
		const double Ang = 360.0 * i / 24.0;
		TNPlaygroundKit::AddXfBox(Base, FTransform(FRotator(0.0, Ang, 0.0), FVector(PivotX, 0.0, StoneH)), FVector(CapR, 0.0, 0.5 * CapH), FVector(4.0, 5.0, 0.5 * CapH - 6.0),
			TNPlaygroundKit::Shade(CapCol, 0.9));
	}
	for (const double Side : { -1.0, 1.0 })
	{
		TNPlaygroundKit::AddAxisBox(Base, FVector(PivotX, Side * (0.5 * HandleWidth + 14.0), StoneH + CapH + 16.0), FVector(22.0, 10.0, 16.0), TNPlaygroundKit::Shade(CapCol, 0.8));
	}
	BaseHulls.Add(TNPlaygroundKit::HullCylinder(FVector(PivotX, 0.0, StoneH - 4.0), CapH + 4.0, CapR, CapR - 3.0, 14));
	// Arena removida donde golpean el cazo y el extremo del mango.
	TNPlaygroundKit::AddDisc(Base, FVector(PivotX + BowlCx * FMath::Cos(Theta), 0.0, 1.5), FVector::UpVector, A.BowlHW + 40.0, 16, TNBeachTrapKit::SandWet());
	TNPlaygroundKit::AddDisc(Base, FVector(PivotX + A.Short * FMath::Cos(FMath::DegreesToRadians(FiredDeg)), 0.0, 1.5), FVector::UpVector, A.BucketR + 50.0, 16,
		TNBeachTrapKit::SandMark());

	// Banderines en un palillo clavado en la piedra (verde: lista; rojo: armada o recargando).
	const FVector PoleBase(PivotX - 20.0, -(StoneTopR - 6.0), StoneH - 10.0);
	const FVector PoleTop = PoleBase + FVector(0.0, 0.0, 190.0);
	TNPlaygroundKit::AddRod(Base, PoleBase, PoleTop, 6.0, 6, TNPlaygroundKit::Rgb(0xE8D2A6), FVector::ForwardVector);
	TNBeachTrapKit::SetMesh(BaseMesh, this, Base);
	BaseCollision->SetCollisionConvexMeshes(BaseHulls);
	TNBeachTrapKit::FBuffers Green;
	TNPlaygroundKit::AddPennant(Green, PoleTop, FVector(-1.0, 0.0, 0.0), 95.0, 60.0, TNPlaygroundKit::Rgb(0x3DDC97, 0.2f));
	TNBeachTrapKit::SetMesh(FlagGreen, this, Green);
	TNBeachTrapKit::FBuffers Red;
	TNPlaygroundKit::AddPennant(Red, PoleTop, FVector(-1.0, 0.0, 0.0), 95.0, 60.0, TNPlaygroundKit::Rgb(0xFF4B3E, 0.2f));
	TNBeachTrapKit::SetMesh(FlagRed, this, Red);

	// Palo de polo de pie bajo el extremo del mango, con su montoncito de arena.
	const double PropArmX = A.Short - 2.0 * A.BucketR - 10.0;
	const double PropX = PivotX + PropArmX * FMath::Cos(Theta);
	const double PropLen = PivotZ + PropArmX * FMath::Sin(Theta) - 0.5 * A.Thick * FMath::Cos(Theta);
	PropPivot->SetRelativeLocationAndRotation(FVector(PropX, 0.0, 0.0), FRotator::ZeroRotator);
	TNBeachTrapKit::FBuffers Prop;
	TNPlaygroundKit::AddStick(Prop, FVector(0.0, 0.0, 0.5 * PropLen), FVector::UpVector, FVector::RightVector, PropLen + 20.0, 48.0, 12.0,
		TNPlaygroundKit::Rgb(0xE9CC98, 0.05f));
	TNBeachTrapKit::SetMesh(PropMesh, this, Prop);

	UE_LOG(LogTortunabo, Verbose, TEXT("[Playa] Catapulta %s: brazo de %.0f cm, fulcro a %.0f, %.0f° en reposo y %.0f° disparada, estilo %d."), *GetName(),
		A.Long + A.Short, PivotZ, RestDeg, FiredDeg, A.Style);
}

void ATN_BeachCatapult::BeginPlay()
{
	Super::BeginPlay();
	const FVector Mid = ArmPivot->GetComponentLocation();
	Voice = UTN_BeachTrapSynthComponent::AttachTo(this, Mid, 800.f, 3500.f);
	Toy = UTN_PlaygroundSynthComponent::AttachTo(this, Mid, 800.f, 3500.f);
	Dust.Init(this, ETNTrapBurstShape::Blob, TNBeachTrapKit::SandTop(), 32);
	Dust.SetMotion(-500.f, 2.f, 40.f, 12.f, 0.4f, 0.9f);
	Chips.Init(this, ETNTrapBurstShape::Chip, TNPlaygroundKit::Rgb(0xE9CC98), 16);
	Chips.SetMotion(-980.f, 0.6f, 20.f, 8.f, 0.5f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Consultas
// ─────────────────────────────────────────────────────────────────────────────

bool ATN_BeachCatapult::IsLoaded(double ServerTime) const
{
	return FiredAt < 0.f || ServerTime - static_cast<double>(FiredAt) >= ReloadSeconds;
}

double ATN_BeachCatapult::ArmPitchAt(double Now) const
{
	using namespace TNBeachCatapultDetail;
	if (FiredAt < 0.f)
	{
		return RestDeg;
	}
	const double T = Now - static_cast<double>(FiredAt);
	if (T < 0.0 || T >= ReloadSeconds)
	{
		return RestDeg;
	}
	if (T < SwingSeconds)
	{
		const double U = T / SwingSeconds;
		return FMath::Lerp(RestDeg, FiredDeg, U * U);
	}
	if (T < BounceEnd)
	{
		// Rebote contra la arena: dos botes que se apagan.
		const double B = T - SwingSeconds;
		return FiredDeg + 9.0 * FMath::Exp(-B / 0.14) * FMath::Abs(FMath::Sin(TNPlaygroundKit::KitPi * B / 0.17));
	}
	if (T < HoldEnd)
	{
		return FiredDeg;
	}
	const double ReturnEnd = ReloadSeconds - SettleSeconds;
	if (T < ReturnEnd)
	{
		// Vuelve despacio, a golpes de carraca.
		const double U = (T - HoldEnd) / FMath::Max(0.1, ReturnEnd - HoldEnd);
		const double Steps = 14.0;
		const double Stepped = (FMath::FloorToDouble(U * Steps) + TNPlaygroundKit::Smooth01(0.0, 0.35, FMath::Frac(U * Steps))) / Steps;
		return FMath::Lerp(FiredDeg, RestDeg, Stepped);
	}
	const double S = T - ReturnEnd;
	return RestDeg - 2.5 * FMath::Exp(-S / 0.1) * FMath::Sin(TNPlaygroundKit::KitPi * S / 0.12);
}

int32 ATN_BeachCatapult::WhereOnArm(const ACharacter* Character) const
{
	if (!Character || Character->GetMovementBase() != ArmCollision.Get())
	{
		return 0;
	}
	const FVector Feet = TNBeachRideKit::FeetIn(ArmPivot->GetComponentTransform(), Character);
	if (Feet.X < -LongArm + BowlLength + 10.0 && FMath::Abs(Feet.Y) < BowlHalfWidth + 25.0)
	{
		return 1;
	}
	if (Feet.X > ShortArm - 2.0 * BucketRadius && Feet.Z > 0.5 * ArmThick + BucketHeight - 40.0)
	{
		return 3;
	}
	return 2;
}

FVector ATN_BeachCatapult::LaunchVelocity(float Fraction) const
{
	// Hacia el mar (X del marco) con desvío y elevación al azar (servidor).
	const float Yaw = FMath::FRandRange(-DeviationDeg, DeviationDeg);
	const float Pitch = LaunchPitch + FMath::FRandRange(-3.f, 3.f);
	const float Speed = LaunchSpeed * Fraction * FMath::FRandRange(0.95f, 1.05f);
	const FVector Local = FRotator(Pitch, Yaw, 0.f).Vector();
	return Frame->GetComponentTransform().TransformVectorNoScale(Local) * Speed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Servidor
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachCatapult::ServerTick(double Now)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector PivotAt = ArmPivot->GetComponentLocation();
	const double Reach = LongArm + 600.0;
	int32 InBowl = 0;
	bool bKick = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Walker = *It;
		if (!IsValid(Walker))
		{
			continue;
		}
		const bool bNear = FVector::DistSquared2D(Walker->GetActorLocation(), PivotAt) < Reach * Reach;
		if (!bNear)
		{
			Riders.Remove(Walker);
			continue;
		}
		FRider& Rider = Riders.FindOrAdd(Walker);
		const int32 Where = TNBeachRideKit::IsFreeRider(Walker) ? WhereOnArm(Walker) : 0;
		// Cae de un salto sobre el cubito: dispara.
		if (Where == 3 && !Rider.bOnBucket && Rider.LastVz < -80.f)
		{
			bKick = true;
		}
		Rider.bOnBucket = Where == 3;
		const UCharacterMovementComponent* Move = Walker->GetCharacterMovement();
		Rider.LastVz = Move ? static_cast<float>(Move->Velocity.Z) : 0.f;
		if (Where == 1)
		{
			++InBowl;
		}
	}
	for (auto It = Riders.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (!IsLoaded(Now) || !TNBeachRideKit::IsRaceLive(this))
	{
		if (ArmedAt >= 0.f)
		{
			ArmedAt = -1.f;
			ForceNetUpdate();
		}
		return;
	}
	if (bKick)
	{
		Fire(Now);
		return;
	}
	if (InBowl > 0)
	{
		EmptySince = -1.0;
		if (ArmedAt < 0.f)
		{
			ArmedAt = static_cast<float>(Now);
			ForceNetUpdate();
		}
		else if (Now >= static_cast<double>(ArmedAt) + WarnSeconds)
		{
			Fire(Now);
		}
	}
	else if (ArmedAt >= 0.f)
	{
		// Se ha bajado del cazo: se desarma si sigue vacío un momento.
		if (EmptySince < 0.0)
		{
			EmptySince = Now;
		}
		else if (Now - EmptySince > 0.35)
		{
			ArmedAt = -1.f;
			EmptySince = -1.0;
			ForceNetUpdate();
		}
	}
}

void ATN_BeachCatapult::Fire(double Now)
{
	FiredAt = static_cast<float>(Now);
	ArmedAt = -1.f;
	EmptySince = -1.0;
	// Sin colisión en el golpe: las bolas nacen dentro del cazo.
	ArmCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bArmCollisionOn = false;
	int32 Launched = 0;
	for (auto It = Riders.CreateIterator(); It; ++It)
	{
		ACharacter* Walker = It.Key().Get();
		if (!Walker || !TNBeachRideKit::IsFreeRider(Walker))
		{
			continue;
		}
		const int32 Where = WhereOnArm(Walker);
		if (Where != 1 && Where != 2)
		{
			continue;
		}
		if (TNBeachRideKit::LaunchAsBall(Cast<ATortugaCharacter>(Walker), LaunchVelocity(Where == 1 ? 1.f : HandleLaunchFraction)))
		{
			++Launched;
		}
	}
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] Catapulta %s dispara: %d lanzadas."), *GetName(), Launched);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick y efectos
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachCatapult::OnRep_Shot()
{
	// Todo sale de las horas en TickVisuals; aquí solo se asegura que el tick corre.
	SetActorTickEnabled(true);
}

void ATN_BeachCatapult::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = Clock.Advance(GetWorld(), DeltaSeconds);
	if (HasAuthority())
	{
		ServerTick(TNBeachTrapKit::ServerNow(GetWorld()));
	}
	TickVisuals(Now, DeltaSeconds);
	Dust.Tick(DeltaSeconds);
	Chips.Tick(DeltaSeconds);
	Pop.Tick(DeltaSeconds, GetWorld());
}

void ATN_BeachCatapult::TickVisuals(double Now, float DeltaSeconds)
{
	using namespace TNBeachCatapultDetail;
	const double Fired = static_cast<double>(FiredAt);
	const double T = FiredAt >= 0.f ? Now - Fired : 1e6;
	const bool bArmed = ArmedAt >= 0.f && Now >= static_cast<double>(ArmedAt);
	const double Warn = bArmed ? FMath::Clamp((Now - static_cast<double>(ArmedAt)) / FMath::Max(0.05, static_cast<double>(WarnSeconds)), 0.0, 1.0) : 0.0;

	// Brazo: con el aviso tiembla en su sitio.
	double Pitch = ArmPitchAt(Now);
	if (bArmed && (FiredAt < 0.f || T >= ReloadSeconds))
	{
		Pitch += (0.4 + 1.2 * Warn) * FMath::Sin(Now * 63.0);
	}
	ArmPivot->SetRelativeRotation(FRotator(Pitch, 0.0, 0.0));

	// Colisión del brazo apagada durante el golpe.
	const bool bWantCollision = !(FiredAt >= 0.f && T >= 0.0 && T < NoCollisionSeconds);
	if (bWantCollision != bArmCollisionOn)
	{
		bArmCollisionOn = bWantCollision;
		ArmCollision->SetCollisionEnabled(bWantCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	// Palo que sujeta: tiembla con el aviso, sale volando al disparar y se pone de pie al final de la recarga.
	double PropPitch = 0.0;
	double PropRoll = bArmed ? (2.0 + 4.0 * Warn) * FMath::Sin(Now * 47.0) : 0.0;
	if (FiredAt >= 0.f && T >= 0.0 && T < ReloadSeconds)
	{
		const double Down = T < 0.35 ? TNPlaygroundKit::Smooth01(0.0, 0.35, T) : 1.0;
		const double Up = TNPlaygroundKit::Smooth01(ReloadSeconds - 0.55, ReloadSeconds - 0.1, T);
		PropPitch = -84.0 * Down * (1.0 - Up) + (T > 0.35 && T < 0.6 ? 5.0 * FMath::Sin(TNPlaygroundKit::KitPi * (T - 0.35) / 0.25) : 0.0);
		PropRoll = 0.0;
	}
	PropPivot->SetRelativeRotation(FRotator(PropPitch, 0.0, PropRoll));

	if (GetNetMode() == NM_DedicatedServer)
	{
		LastVisualNow = Now;
		return;
	}

	// Banderín: verde lista; rojo parpadeando armada; rojo fijo recargando.
	const bool bLoaded = IsLoaded(Now);
	const bool bShowRed = !bLoaded || (bArmed && FMath::Frac(Now * 4.0) < 0.5);
	FlagGreen->SetVisibility(!bShowRed);
	FlagRed->SetVisibility(bShowRed);

	const double Before = LastVisualNow;
	auto Crossed = [Before, Now](double At) { return At >= 0.0 && Before < At && Now >= At && Now - At < 1.0; };
	const FTransform ArmXf = ArmPivot->GetComponentTransform();
	const FVector BowlAt = ArmXf.TransformPosition(FVector(-LongArm + 0.5 * BowlLength, 0.0, 40.0));
	const FVector BucketAt = ArmXf.TransformPosition(FVector(ShortArm - 0.9 * BucketRadius, 0.0, 0.0));

	// Aviso: crujidos cada vez más seguidos y agudos.
	if (bArmed && bLoaded)
	{
		if (Crossed(static_cast<double>(ArmedAt)))
		{
			Pop.Show(this, NSLOCTEXT("TNBeach", "CatapultWarn", "¡AGÁRRATE!"), FColor(255, 214, 90), BowlAt + FVector(0.0, 0.0, 220.0), 110.f);
		}
		CreakTimer -= DeltaSeconds;
		if (CreakTimer <= 0.f && Voice)
		{
			Voice->TriggerSoundAt(ETNBeachTrapSound::Creak, PropPivot->GetComponentLocation() + FVector(0.0, 0.0, 150.0), 1.f + 0.5f * static_cast<float>(Warn),
				0.6f + 0.5f * static_cast<float>(Warn));
			CreakTimer = static_cast<float>(FMath::Lerp(0.32, 0.1, Warn));
		}
	}
	else
	{
		CreakTimer = 0.f;
	}

	// Disparo: el palo se parte, el cubito cae y la cuchara da la vuelta.
	if (Crossed(Fired))
	{
		const FVector PropTop = PropPivot->GetComponentLocation() + FVector(0.0, 0.0, 120.0);
		Chips.Burst(PropTop, 10, FVector::UpVector, 420.f, 1.2f, 30.f);
		if (Voice)
		{
			Voice->TriggerSoundAt(ETNBeachTrapSound::Crack, PropTop, FMath::FRandRange(1.1f, 1.25f), 1.f);
		}
		if (Toy)
		{
			Toy->TriggerSound(ETNPlaygroundSound::Whoosh, 0.8f, 1.f);
		}
		Pop.Show(this, NSLOCTEXT("TNBeach", "CatapultFire", "¡ZAS!"), FColor(255, 170, 70), BowlAt + FVector(0.0, 0.0, 300.0), 150.f);
	}
	if (Crossed(Fired + SwingSeconds))
	{
		Dust.Burst(BucketAt, 16, FVector::UpVector, 380.f, 1.3f, static_cast<float>(BucketRadius));
		if (Voice)
		{
			Voice->TriggerSoundAt(ETNBeachTrapSound::Thud, BucketAt, FMath::FRandRange(0.8f, 0.9f), 1.2f);
			Voice->TriggerSoundAt(ETNBeachTrapSound::Twang, BowlAt, FMath::FRandRange(0.7f, 0.8f), 0.9f);
		}
		if (Toy)
		{
			Toy->TriggerSound(ETNPlaygroundSound::Bonk, 0.7f, 1.f);
		}
		UTN_BeachCameraShake::Kick(this, BucketAt, 0.4f, 600.f, 2600.f);
	}
	// Recarga: carraca a cada paso.
	if (FiredAt >= 0.f && T > HoldEnd && T < ReloadSeconds - SettleSeconds)
	{
		if (Now - LastRatchetAt >= RatchetStep || LastRatchetAt > Now)
		{
			LastRatchetAt = Now;
			if (Voice)
			{
				Voice->TriggerSoundAt(ETNBeachTrapSound::Plink, ArmPivot->GetComponentLocation(), 1.9f, 0.3f);
			}
		}
	}
	if (Crossed(Fired + ReloadSeconds) && Voice)
	{
		Voice->TriggerSoundAt(ETNBeachTrapSound::Clack, BowlAt, 1.2f, 0.7f);
		Dust.Burst(BowlAt - FVector(0.0, 0.0, 40.0), 6, FVector::UpVector, 200.f, 1.2f, 60.f);
	}
	LastVisualNow = Now;
}
