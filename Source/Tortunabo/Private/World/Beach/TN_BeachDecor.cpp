#include "World/Beach/TN_BeachDecor.h"
#include "TN_BeachPropMeshes.h"
#include "../ProcMap/TN_ProcMapRuntimeMesh.h"
#include "Core/TN_Log.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"
#include "UObject/Package.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNBeachDecorDetail
{
	/** Mallas en caché de una receta (elemento y variante, o pieza de un tramo) y cómo se coloca cada ejemplar. */
	struct FCachedProp
	{
		TWeakObjectPtr<UStaticMesh> Body;
		TWeakObjectPtr<UStaticMesh> Moving;
		TNBeachProp::FPropInfo Info;
		bool bCollision = false;
		bool bHasMoving = false;
		bool bBuilt = false;
	};

	/** M_CosmeticVertexColor (color de vértice; el alfa es el brillo) o, si falta, el material de color de vértice del motor. */
	UMaterialInterface* DecorMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid())
		{
			UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cosmetics/Materials/M_CosmeticVertexColor.M_CosmeticVertexColor"));
			if (!Mat) { Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial")); }
			Cached = Mat;
		}
		return Cached.Get();
	}

	/**
	 * Malla estática en ejecución (RF_Transient | RF_DuplicateTransient, en el paquete transitorio y fuera del recolector:
	 * la comparten todos los ejemplares) con la colisión simple de la receta en su BodySetup: cajas, esferas y cápsulas,
	 * simple como compleja y sin nada que cocinar.
	 */
	UStaticMesh* MakeMesh(const TNProcMesh::FTNProcMeshBuffers& Buffers, const TNBeachProp::FParts* Collision)
	{
		UMaterialInterface* Mat = DecorMaterial();
		if (!Mat || Buffers.IsEmpty()) { return nullptr; }
		// Alfa de los propios buffers (FixedAlpha = -2): el brillo de M_CosmeticVertexColor.
		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(GetTransientPackage(), Buffers, Mat, false, 0.f, 1.f, -2.f);
		if (!Mesh) { return nullptr; }
		Mesh->AddToRoot();
		UBodySetup* Setup = Mesh->GetBodySetup();
		if (!Setup) { return Mesh; }
		Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		Setup->bNeverNeedsCookedCollisionData = true;
		if (!Collision) { return Mesh; }
		for (const TNBeachProp::FColBox& Box : Collision->Boxes)
		{
			FKBoxElem Elem(static_cast<float>(Box.Half.X * 2.0), static_cast<float>(Box.Half.Y * 2.0), static_cast<float>(Box.Half.Z * 2.0));
			Elem.Center = Box.Center;
			Elem.Rotation = Box.Rot.Rotator();
			Setup->AggGeom.BoxElems.Add(Elem);
		}
		for (const TNBeachProp::FColSphere& Ball : Collision->Spheres)
		{
			FKSphereElem Elem(static_cast<float>(Ball.Radius));
			Elem.Center = Ball.Center;
			Setup->AggGeom.SphereElems.Add(Elem);
		}
		for (const TNBeachProp::FColCapsule& Pill : Collision->Capsules)
		{
			const FVector Axis = Pill.B - Pill.A;
			const double Len = Axis.Size();
			if (Len < 1.0)
			{
				FKSphereElem Elem(static_cast<float>(Pill.Radius));
				Elem.Center = Pill.A;
				Setup->AggGeom.SphereElems.Add(Elem);
				continue;
			}
			FKSphylElem Elem(static_cast<float>(Pill.Radius), static_cast<float>(Len));
			Elem.Center = (Pill.A + Pill.B) * 0.5;
			Elem.Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Axis / Len).Rotator();
			Setup->AggGeom.SphylElems.Add(Elem);
		}
		return Mesh;
	}

	/** Receta montada (una vez por clave, en la primera máquina que la pide; las mallas quedan para toda la partida). */
	FCachedProp PropFor(uint32 Key, TFunctionRef<void(TNBeachProp::FParts&)> Build)
	{
		static TMap<uint32, FCachedProp> Cache;
		FCachedProp& Entry = Cache.FindOrAdd(Key);
		const bool bLost = Entry.bBuilt && (!Entry.Body.IsValid() || (Entry.bHasMoving && !Entry.Moving.IsValid()));
		if (!Entry.bBuilt || bLost)
		{
			TNBeachProp::FParts Parts;
			Build(Parts);
			Entry.Info = Parts.Info;
			Entry.bCollision = Parts.HasCollision();
			Entry.bHasMoving = !Parts.Moving.IsEmpty();
			Entry.Body = MakeMesh(Parts.Body, &Parts);
			Entry.Moving = Entry.bHasMoving ? MakeMesh(Parts.Moving, nullptr) : nullptr;
			Entry.bBuilt = Entry.Body.IsValid();
		}
		return Entry;
	}

	uint32 SingleKey(ETNBeachElement Element, int32 Variant)
	{
		return (static_cast<uint32>(Element) << 16) | (static_cast<uint32>(Variant) & 0xFFu);
	}

	uint32 PieceKey(ETNBeachElement Element, int32 Piece)
	{
		return (static_cast<uint32>(Element) << 16) | 0x8000u | (static_cast<uint32>(Piece) & 0xFFu);
	}

	/** Semilla de la receta: la misma para todos los ejemplares de una variante (la malla es compartida). */
	uint32 RecipeSeed(ETNBeachElement Element, int32 Index)
	{
		return TNBeachProp::HashMix(static_cast<uint32>(Element) * 131u + 17u, static_cast<uint32>(Index) * 7919u + 3u);
	}

	/** Distancia a la que deja de dibujarse: lo pequeño desde 120 m; lo de más de 10 m de huella, nunca. */
	float CullDistanceFor(ETNBeachElement Element, float Size)
	{
		const double Radius = TNBeach::FootprintRadius(Element) * Size;
		return Radius >= 1000.0 ? 0.f : static_cast<float>(FMath::Clamp(Radius * 60.0, 12000.0, 60000.0));
	}

	/** Distancia a una cámara local hasta la que se mueve la parte animada (60-200 m según el tamaño). */
	float AnimRangeFor(ETNBeachElement Element, float Size)
	{
		return static_cast<float>(FMath::Clamp(TNBeach::FootprintRadius(Element) * Size * 12.0, 6000.0, 20000.0));
	}

	/** Colisión de una pieza: bloquea todo y deja pasar la cámara salvo en lo grande y macizo (no da tirones). */
	void SetupCollision(UPrimitiveComponent* Comp, bool bCollision, bool bBlocksCamera)
	{
		if (!Comp) { return; }
		if (!bCollision)
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			return;
		}
		Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Comp->SetCollisionResponseToChannel(ECC_Camera, bBlocksCamera ? ECR_Block : ECR_Ignore);
	}
}

ATN_BeachDecor::ATN_BeachDecor()
{
	// Solo tiene tick la parte animada, y solo cerca de una cámara (lo enciende y apaga UpdateAnimActivity).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetRootComponent());
	BodyMesh->SetMobility(EComponentMobility::Movable);
	BodyMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	BodyMesh->SetGenerateOverlapEvents(false);

	AnimMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnimMesh"));
	AnimMesh->SetupAttachment(BodyMesh);
	AnimMesh->SetMobility(EComponentMobility::Movable);
	AnimMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimMesh->SetGenerateOverlapEvents(false);
	AnimMesh->SetCanEverAffectNavigation(false);
	AnimMesh->SetVisibility(false);
}

void ATN_BeachDecor::ApplySpec()
{
	StopAnimation();
	ClearTiles();
	const ETNBeachElement Element = Spec.Element;
	if (TNBeach::CategoryOf(Element) != ETNBeachCategory::Decor)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Playa] ATN_BeachDecor con %s, que no es decorado: no se construye."), *UEnum::GetValueAsString(Element));
		BodyMesh->SetStaticMesh(nullptr);
		AnimMesh->SetStaticMesh(nullptr);
		return;
	}
	// El tamaño va en la malla (el actor no se escala); fuera de 0,5-1,6 los escalones dejarían de poder subirse.
	const float Size = FMath::Clamp(Spec.SizeScale, 0.5f, 1.6f);
	if (TNBeachProp::IsTiled(Element))
	{
		BuildTiles(Element, Size);
	}
	else
	{
		BuildSingle(Element, Size);
	}
}

void ATN_BeachDecor::BuildSingle(ETNBeachElement Element, float Size)
{
	using namespace TNBeachDecorDetail;
	const uint32 Seed = static_cast<uint32>(Spec.Seed);
	const int32 NumVar = FMath::Max(1, TNBeachProp::NumVariants(Element));
	const int32 Variant = static_cast<int32>(TNBeachProp::HashMix(Seed, 0x5EEDu) % static_cast<uint32>(NumVar));
	const uint32 MeshSeed = RecipeSeed(Element, Variant);
	const FCachedProp Prop = PropFor(SingleKey(Element, Variant), [Element, Variant, MeshSeed](TNBeachProp::FParts& Parts)
	{
		TNBeachProp::BuildDecor(Parts, Element, Variant, MeshSeed);
	});
	const TNBeachProp::FPropInfo& Info = Prop.Info;

	// Colocación de este ejemplar, igual en todas las máquinas (sale de Spec.Seed): giro, inclinación y hundimiento.
	const double Yaw = Info.bFreeYaw ? TNBeachProp::RndIn(Seed, 1, 0.0, 360.0) : TNBeachProp::RndIn(Seed, 1, -Info.YawJitter, Info.YawJitter);
	const double TiltDeg = Info.TiltMax * TNBeachProp::Rnd(Seed, 2);
	const double TiltDir = TNBeachProp::RndIn(Seed, 3, 0.0, UE_DOUBLE_TWO_PI);
	const double Sink = FMath::Lerp(static_cast<double>(Info.SinkMin), static_cast<double>(Info.SinkMax), TNBeachProp::Rnd(Seed, 4)) * Size;
	const FQuat Tilt(FVector(FMath::Cos(TiltDir), FMath::Sin(TiltDir), 0.0), FMath::DegreesToRadians(TiltDeg));
	BodyMesh->SetRelativeTransform(FTransform(Tilt * TNBeachProp::YawQ(Yaw), FVector(0.0, 0.0, -Sink), FVector(static_cast<double>(Size))));

	UStaticMesh* BodyAsset = Prop.Body.Get();
	const float Cull = CullDistanceFor(Element, Size);
	BodyMesh->SetStaticMesh(BodyAsset);
	BodyMesh->SetCastShadow(Info.bCastShadow);
	BodyMesh->SetCullDistance(Cull);
	SetupCollision(BodyMesh.Get(), Prop.bCollision && BodyAsset != nullptr, Info.bBlocksCamera);

	UStaticMesh* MovingAsset = Prop.Moving.Get();
	AnimMesh->SetStaticMesh(MovingAsset);
	AnimMesh->SetVisibility(MovingAsset != nullptr);
	AnimMesh->SetRelativeTransform(FTransform(FQuat::Identity, Info.AnimPivot, FVector::OneVector));
	AnimMesh->SetCastShadow(Info.bCastShadow);
	AnimMesh->SetCullDistance(Cull);
	if (MovingAsset && Info.Anim != TNBeachProp::EAnim::None)
	{
		StartAnimation(static_cast<uint8>(Info.Anim), Info.AnimAxis, Info.AnimAmp, Info.AnimRate, AnimRangeFor(Element, Size));
	}
}

void ATN_BeachDecor::BuildTiles(ETNBeachElement Element, float Size)
{
	using namespace TNBeachDecorDetail;
	BodyMesh->SetStaticMesh(nullptr);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimMesh->SetStaticMesh(nullptr);
	AnimMesh->SetVisibility(false);
	const uint32 Seed = static_cast<uint32>(Spec.Seed);
	const double Length = FMath::Clamp(Spec.Extent > 1.f ? static_cast<double>(Spec.Extent) : 6000.0, 1500.0, TNBeach::CourseLength);
	// A lo largo y a lo ancho, a su tamaño; el alto no: la pasarela sigue a ~1,1 m y la cuerda por encima de la tortuga.
	const FVector Scale3(Size, Size, 1.0);
	TMap<int32, TArray<FTransform>> ByPiece;
	if (Element == ETNBeachElement::Boardwalk)
	{
		using namespace TNBeachProp::BoardwalkKit;
		const double Mod = ModuleLen * Size;
		const int32 Total = FMath::Max(1, FMath::RoundToInt32((Length - 2.0 * Mod) / Mod)) + 2;
		const double X0 = -0.5 * (Total - 1) * Mod;
		for (int32 m = 0; m < Total; ++m)
		{
			int32 Kind = RampKind;
			double Yaw = 0.0;
			if (m == 0)
			{
				// Bajada del extremo -X (la pieza baja hacia su +X: se gira).
				Yaw = 180.0;
			}
			else if (m < Total - 1)
			{
				// La mitad, tramos enteros; el resto, sin una tabla, con una rota, con una suelta o con tablas movidas.
				const double Roll = TNBeachProp::Rnd(Seed, 100 + m);
				Kind = Roll < 0.5 ? 0 : FMath::Clamp(1 + static_cast<int32>((Roll - 0.5) * 2.0 * (NumKinds - 1)), 1, NumKinds - 1);
				Yaw = TNBeachProp::Rnd(Seed, 300 + m) < 0.5 ? 180.0 : 0.0;
			}
			ByPiece.FindOrAdd(Kind).Add(FTransform(TNBeachProp::YawQ(Yaw), FVector(X0 + m * Mod, 0.0, 0.0), Scale3));
		}
	}
	else
	{
		using namespace TNBeachProp::PostPathKit;
		const double Gap = Spacing * Size;
		const int32 Count = FMath::Max(2, FMath::RoundToInt32(Length / Gap) + 1);
		const double X0 = -0.5 * (Count - 1) * Gap;
		const int32 RopeKind = static_cast<int32>(TNBeachProp::HashMix(Seed, 77u) % static_cast<uint32>(NumRopeKinds));
		for (const double SideSign : { -1.0, 1.0 })
		{
			TArray<FVector> Attach;
			for (int32 i = 0; i < Count; ++i)
			{
				// Palos algo torcidos y descolocados: la mayoría rectos, alguno roto y alguno con vueltas de cuerda.
				const int32 Salt = (SideSign > 0.0 ? 1000 : 2000) + i;
				const double Roll = TNBeachProp::Rnd(Seed, Salt);
				const int32 PostKind = Roll < 0.7 ? 0 : (Roll < 0.85 ? 1 : 2);
				const FVector Base(X0 + i * Gap + TNBeachProp::RndIn(Seed, Salt + 5000, -40.0, 40.0) * Size, SideSign * HalfWidth * Size, 0.0);
				const double LeanDir = TNBeachProp::RndIn(Seed, Salt + 9000, 0.0, UE_DOUBLE_TWO_PI);
				const FQuat Lean(FVector(FMath::Cos(LeanDir), FMath::Sin(LeanDir), 0.0), FMath::DegreesToRadians(TNBeachProp::RndIn(Seed, Salt + 7000, 0.0, 9.0)));
				const FTransform Xf(Lean * TNBeachProp::YawQ(TNBeachProp::RndIn(Seed, Salt + 11000, 0.0, 360.0)), Base, Scale3);
				ByPiece.FindOrAdd(PostKind).Add(Xf);
				Attach.Add(Xf.TransformPosition(FVector(0.0, 0.0, AttachZ(PostKind))));
			}
			// Un tramo de cuerda de cada palo al siguiente, estirado en X hasta él.
			for (int32 i = 0; i + 1 < Count; ++i)
			{
				const FVector D = Attach[i + 1] - Attach[i];
				const double Dist = D.Size();
				if (Dist < 1.0) { continue; }
				ByPiece.FindOrAdd(RopeBase + RopeKind).Add(FTransform(FQuat::FindBetweenNormals(FVector(1.0, 0.0, 0.0), D / Dist), Attach[i], FVector(Dist / Spacing, 1.0, 1.0)));
			}
		}
	}

	USceneComponent* Root = GetRootComponent();
	for (TPair<int32, TArray<FTransform>>& Entry : ByPiece)
	{
		const int32 Piece = Entry.Key;
		const FCachedProp Prop = PropFor(PieceKey(Element, Piece), [Element, Piece](TNBeachProp::FParts& Parts)
		{
			TNBeachProp::BuildTiledPiece(Parts, Element, Piece, RecipeSeed(Element, Piece));
		});
		UStaticMesh* Mesh = Prop.Body.Get();
		if (!Mesh) { continue; }
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Ism->SetupAttachment(Root);
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetStaticMesh(Mesh);
		Ism->SetCastShadow(Prop.Info.bCastShadow);
		Ism->SetGenerateOverlapEvents(false);
		SetupCollision(Ism, Prop.bCollision, false);
		Ism->RegisterComponent();
		Ism->AddInstances(Entry.Value, false, false);
		Tiles.Add(Ism);
	}
}

void ATN_BeachDecor::ClearTiles()
{
	for (UInstancedStaticMeshComponent* Ism : Tiles)
	{
		if (Ism) { Ism->DestroyComponent(); }
	}
	Tiles.Reset();
}

void ATN_BeachDecor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAnimation();
	Super::EndPlay(EndPlayReason);
}

void ATN_BeachDecor::StartAnimation(uint8 Kind, const FVector& Axis, float Amp, float Rate, float Range)
{
	const UWorld* World = GetWorld();
	// Ni en el servidor dedicado ni en la ronda de prueba del editor.
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer) { return; }
	const FVector SafeAxis = Axis.GetSafeNormal();
	AnimKind = Kind;
	AnimAxis = SafeAxis.IsNearlyZero() ? FVector(0.0, 1.0, 0.0) : SafeAxis;
	AnimAmp = Amp;
	AnimRate = FMath::Max(0.01f, Rate);
	AnimRange = Range;
	const uint32 Seed = static_cast<uint32>(Spec.Seed);
	AnimPhase = static_cast<float>(TNBeachProp::Rnd(Seed, 21));
	AnimTime = 0.f;
	ClamCycle = -1;
	bClamHeld = false;
	ApplyAnimPose();
	// Cada ejemplar mira a su ritmo (no todos en el mismo fotograma).
	const float FirstCheck = 0.1f + 0.5f * static_cast<float>(TNBeachProp::Rnd(Seed, 22));
	GetWorldTimerManager().SetTimer(AnimCheckTimer, this, &ATN_BeachDecor::UpdateAnimActivity, 0.5f, true, FirstCheck);
}

void ATN_BeachDecor::StopAnimation()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AnimCheckTimer);
	}
	SetActorTickEnabled(false);
	AnimKind = 0;
}

void ATN_BeachDecor::UpdateAnimActivity()
{
	bool bNear = false;
	const UWorld* World = GetWorld();
	if (World && HasActorBegunPlay() && AnimKind != 0 && AnimMesh && AnimMesh->GetStaticMesh())
	{
		const FVector Here = AnimMesh->GetComponentLocation();
		const double RangeSq = FMath::Square(static_cast<double>(AnimRange));
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController() && PC->PlayerCameraManager
				&& FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), Here) < RangeSq)
			{
				bNear = true;
				break;
			}
		}
		// Si no se ha dibujado hace poco (detrás de la cámara o tapado), tampoco hace falta moverla.
		bNear = bNear && AnimMesh->WasRecentlyRendered(1.f);
	}
	if (bNear != IsActorTickEnabled())
	{
		SetActorTickEnabled(bNear);
	}
}

void ATN_BeachDecor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AnimTime += DeltaSeconds;
	ApplyAnimPose();
}

void ATN_BeachDecor::ApplyAnimPose()
{
	if (!AnimMesh) { return; }
	switch (static_cast<TNBeachProp::EAnim>(AnimKind))
	{
	case TNBeachProp::EAnim::Breathe:
	{
		// Respira despacio (se ensancha al bajar) y a ratos tiembla deprisa.
		const float T = AnimTime + AnimPhase * 10.f;
		const float Breath = FMath::Sin(UE_TWO_PI * AnimRate * T);
		const float Burst = FMath::Max(0.f, FMath::Sin(UE_TWO_PI * 0.13f * T + 1.7f) - 0.6f) * 2.5f;
		const float Shiver = Burst * 0.012f * (FMath::Sin(T * 57.f) + 0.6f * FMath::Sin(T * 83.f + 1.3f));
		AnimMesh->SetRelativeScale3D(FVector(1.f + AnimAmp * Breath + Shiver, 1.f + AnimAmp * Breath - Shiver, 1.f - 1.4f * AnimAmp * Breath + Shiver * 0.5f));
		break;
	}
	case TNBeachProp::EAnim::Clam:
	{
		// Cada ciclo: cerrada un rato, se abre en 1 s, se queda abierta (respirando), se cierra de golpe y rebota un poco.
		// Si al empezar el ciclo hay alguien encima, ese ciclo no se abre.
		const float Period = FMath::Max(5.f, AnimRate);
		const float Local = AnimTime + AnimPhase * Period;
		const int32 Cycle = FMath::FloorToInt32(Local / Period);
		const float InCycle = Local - static_cast<float>(Cycle) * Period;
		if (Cycle != ClamCycle)
		{
			ClamCycle = Cycle;
			bClamHeld = IsSomeoneOnTop();
		}
		const uint32 Seed = static_cast<uint32>(Spec.Seed);
		const float Hold = 1.2f + 1.6f * static_cast<float>(TNBeachProp::Rnd(Seed, 500 + (Cycle & 0xFFFF)));
		const float OpenAt = Period - (1.f + Hold + 0.35f + 0.4f);
		float Open = 0.f;
		if (!bClamHeld && InCycle > OpenAt)
		{
			const float U = InCycle - OpenAt;
			if (U < 1.f) { Open = FMath::InterpEaseOut(0.f, 1.f, U, 2.f); }
			else if (U < 1.f + Hold) { Open = 1.f - 0.05f * FMath::Sin((U - 1.f) * 6.f); }
			else if (U < 1.f + Hold + 0.35f) { Open = 1.f - FMath::InterpEaseIn(0.f, 1.f, (U - 1.f - Hold) / 0.35f, 2.f); }
			else { Open = 0.12f * FMath::Sin(FMath::Clamp((U - 1.35f - Hold) / 0.4f, 0.f, 1.f) * UE_PI); }
		}
		const float Amount = 0.7f + 0.3f * static_cast<float>(TNBeachProp::Rnd(Seed, 900 + (Cycle & 0xFFFF)));
		AnimMesh->SetRelativeRotation(FQuat(AnimAxis, FMath::DegreesToRadians(static_cast<double>(AnimAmp * Amount * Open))));
		break;
	}
	case TNBeachProp::EAnim::Flutter:
	{
		// Ondea: giro alrededor de su eje con tres senos y un abombado a lo ancho.
		const float W = UE_TWO_PI * AnimRate * (AnimTime + AnimPhase * 10.f);
		const float Angle = AnimAmp * (0.65f * FMath::Sin(W) + 0.25f * FMath::Sin(2.3f * W + 1.1f) + 0.1f * FMath::Sin(5.1f * W + 0.4f));
		AnimMesh->SetRelativeRotation(FQuat(AnimAxis, FMath::DegreesToRadians(static_cast<double>(Angle))));
		AnimMesh->SetRelativeScale3D(FVector(1.0, 1.0 + 0.12 * FMath::Sin(1.3 * W + 0.7), 1.0));
		break;
	}
	case TNBeachProp::EAnim::Sway:
	{
		// Se mece despacio con el viento.
		const float W = UE_TWO_PI * AnimRate * (AnimTime + AnimPhase * 10.f);
		const float Angle = AnimAmp * (0.75f * FMath::Sin(W) + 0.25f * FMath::Sin(2.7f * W + 0.8f));
		AnimMesh->SetRelativeRotation(FQuat(AnimAxis, FMath::DegreesToRadians(static_cast<double>(Angle))));
		break;
	}
	default:
		break;
	}
}

bool ATN_BeachDecor::IsSomeoneOnTop() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS) { return false; }
	const FVector Here = GetActorLocation();
	const double Reach = GetFootprintRadius() * 1.3 + 60.0;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const APawn* Turtle = PS ? PS->GetPawn() : nullptr;
		if (!Turtle) { continue; }
		const FVector Rel = Turtle->GetActorLocation() - Here;
		if (FVector(Rel.X, Rel.Y, 0.0).SizeSquared() < Reach * Reach && Rel.Z > -50.0 && Rel.Z < 500.0) { return true; }
	}
	return false;
}
