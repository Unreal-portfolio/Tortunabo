#include "Vehicles/TN_BuggyLookComponent.h"
#include "Vehicles/TN_BuggyCosmetics.h"
#include "Vehicles/TN_BuggyMath.h"
#include "TN_BuggyArt.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace TNBuggyLookDetail
{
	using TNBuggyArt::EPiece;

	/** Antena: inclinación (grados) por cm/s² de aceleración y por cm/s de velocidad, tope, muelle y amortiguación. */
	constexpr float LeanPerAccel = 0.0035f;
	constexpr float LeanPerSpeed = 0.0032f;
	constexpr float MaxLeanDeg = 15.f;
	constexpr float Spring = 70.f;
	constexpr float Damping = 6.5f;

	const TCHAR* PieceComponentName(int32 Index)
	{
		static const TCHAR* const Names[] = { TEXT("BuggyChassis"), TEXT("BuggyCockpit"), TEXT("BuggyShell"), TEXT("BuggyHead"), TEXT("BuggyFenders"),
			TEXT("BuggyTail"), TEXT("BuggyRear"), TEXT("BuggyExtras") };
		return Index >= 0 && Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("BuggyPiece");
	}
}

UTN_BuggyLookComponent::UTN_BuggyLookComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UTN_BuggyLookComponent::UseExternalWheels(const TArray<UStaticMeshComponent*>& InWheels)
{
	bExternalWheels = true;
	Wheels.Reset();
	for (UStaticMeshComponent* Wheel : InWheels) { Wheels.Add(Wheel); }
}

void UTN_BuggyLookComponent::UseExternalCannon(UStaticMeshComponent* InCannon)
{
	Cannon = InCannon;
	bExternalCannon = InCannon != nullptr;
}

void UTN_BuggyLookComponent::SetStudio(const FLightingChannels& Channels)
{
	bStudio = true;
	StudioChannels = Channels;
	TArray<UPrimitiveComponent*> Parts;
	GetPrimitives(Parts);
	if (Cannon) { Parts.AddUnique(Cannon); }
	for (UPrimitiveComponent* Part : Parts) { if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Part)) { SetupPart(Mesh); } }
}

void UTN_BuggyLookComponent::SetGunnerSeated(bool bSeated)
{
	bGunnerSeated = bSeated;
	if (TurretPost) { TurretPost->SetVisibility(!bGunnerSeated && TurretPost->GetStaticMesh() != nullptr); }
}

void UTN_BuggyLookComponent::SetupPart(UStaticMeshComponent* Part) const
{
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCastShadow(true);
	if (bStudio)
	{
		Part->SetVisibleInSceneCaptureOnly(true);
		Part->LightingChannels = StudioChannels;
	}
}

UStaticMeshComponent* UTN_BuggyLookComponent::MakePart(const TCHAR* Name, USceneComponent* Parent)
{
	AActor* Owner = GetOwner();
	UObject* Outer = Owner ? static_cast<UObject*>(Owner) : static_cast<UObject*>(this);
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Outer, MakeUniqueObjectName(Outer, UStaticMeshComponent::StaticClass(), Name), RF_Transient);
	Part->SetupAttachment(Parent);
	SetupPart(Part);
	if (IsRegistered()) { Part->RegisterComponent(); }
	return Part;
}

void UTN_BuggyLookComponent::EnsureParts()
{
	using namespace TNBuggyArt;
	if (BodyPieces.Num() == NumBodyPieces) { return; }
	BodyPieces.Reset();
	for (int32 i = 0; i < NumBodyPieces; ++i)
	{
		BodyPieces.Add(MakePart(TNBuggyLookDetail::PieceComponentName(i), this));
	}
	if (!bExternalWheels)
	{
		// Las cuatro ruedas en su sitio, las izquierdas giradas para que la llanta mire afuera.
		const FVector Spots[] = { Frame::FrontWheel * FVector(1.0, -1.0, 1.0), Frame::FrontWheel, Frame::RearWheel * FVector(1.0, -1.0, 1.0), Frame::RearWheel };
		for (int32 i = 0; i < 4; ++i)
		{
			UStaticMeshComponent* Wheel = MakePart(TEXT("BuggyWheel"), this);
			Wheel->SetRelativeLocationAndRotation(Spots[i], FRotator(0.0, Spots[i].Y < 0.0 ? 180.0 : 0.0, 0.0));
			Wheels.Add(Wheel);
		}
	}
	if (!Cannon)
	{
		// Escaparate: sin artillera, el cañón en su poste, un poco levantado.
		Cannon = MakePart(TEXT("BuggyCannon"), this);
		Cannon->SetRelativeLocationAndRotation(FVector(Frame::GunnerSeat.X, Frame::GunnerSeat.Y, Frame::TurretPivotZ), FRotator(6.0, 0.0, 0.0));
	}
	AntennaPivot = NewObject<USceneComponent>(GetOwner() ? static_cast<UObject*>(GetOwner()) : static_cast<UObject*>(this),
		MakeUniqueObjectName(GetOwner(), USceneComponent::StaticClass(), TEXT("BuggyAntennaPivot")), RF_Transient);
	AntennaPivot->SetupAttachment(this);
	if (IsRegistered()) { AntennaPivot->RegisterComponent(); }
	Antenna = MakePart(TEXT("BuggyAntenna"), AntennaPivot);
	TurretPost = MakePart(TEXT("BuggyTurretPost"), this);
}

void UTN_BuggyLookComponent::ApplyLook(const FTN_BuggyLook& InLook, int32 InTeamIndex, bool bForce)
{
	using namespace TNBuggyArt;
	if (GetNetMode() == NM_DedicatedServer) { return; }
	const FTN_BuggyLook Clean = TNBuggyCosmetics::Sanitize(InLook);
	const FString Key = FString::Printf(TEXT("%s|%d"), *TNBuggyCosmetics::LookKey(Clean), InTeamIndex);
	if (!bForce && Key == AppliedKey) { return; }
	EnsureParts();

	const FTNBuggyModelInfo& Model = TNBuggyCosmetics::ResolveModel(Clean.ModelId);
	const FTNBuggyPaintInfo& Paint = TNBuggyCosmetics::ResolvePaint(Clean.PaintId);
	const FLinearColor TeamColor = TNBuggy::TeamColor(InTeamIndex);
	UMaterialInterface* PaintMat = PaintMaterial();
	// Sin M_BuggyPaint: mallas con la pintura horneada en el color de vértice (sin dibujo ni color de equipo).
	const FTNBuggyPaintInfo* Bake = PaintMat ? nullptr : &Paint;
	if (PaintMat)
	{
		if (!PaintMID || PaintMID->Parent != PaintMat) { PaintMID = UMaterialInstanceDynamic::Create(PaintMat, this); }
		ApplyPaint(PaintMID, Paint, TeamColor);
	}
	const auto SetPiece = [this, &Model, Bake](UStaticMeshComponent* Part, EPiece Piece)
	{
		if (!Part) { return; }
		UStaticMesh* Mesh = GetPieceMesh(Model.Style, Piece, Bake);
		Part->SetStaticMesh(Mesh);
		Part->SetVisibility(Mesh != nullptr);
		if (Mesh) { Part->SetMaterial(0, PaintMID && !Bake ? static_cast<UMaterialInterface*>(PaintMID) : nullptr); }
	};
	for (int32 i = 0; i < BodyPieces.Num(); ++i) { SetPiece(BodyPieces[i], static_cast<EPiece>(i)); }
	for (UStaticMeshComponent* Wheel : Wheels) { SetPiece(Wheel, EPiece::Wheel); }
	if (Cannon)
	{
		SetPiece(Cannon, EPiece::Cannon);
		// El cilindro provisional iba escalado y tumbado: la malla del cañón ya está hecha a lo largo de +X desde el pivote.
		if (bExternalCannon) { Cannon->SetRelativeTransform(FTransform::Identity); }
	}
	SetPiece(Antenna, EPiece::Antenna);
	SetPiece(TurretPost, EPiece::TurretPost);
	SetGunnerSeated(bGunnerSeated);
	if (AntennaPivot) { AntennaPivot->SetRelativeLocation(AntennaMount(Model.Style)); }

	Look = Clean;
	TeamIndex = InTeamIndex;
	AppliedKey = Key;
}

void UTN_BuggyLookComponent::GetPrimitives(TArray<UPrimitiveComponent*>& Out) const
{
	for (UStaticMeshComponent* Part : BodyPieces) { if (Part) { Out.Add(Part); } }
	if (!bExternalWheels)
	{
		for (UStaticMeshComponent* Wheel : Wheels) { if (Wheel) { Out.Add(Wheel); } }
	}
	if (Cannon && !bExternalCannon) { Out.Add(Cannon); }
	if (Antenna) { Out.Add(Antenna); }
	if (TurretPost) { Out.Add(TurretPost); }
}

void UTN_BuggyLookComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!AntennaPivot || !Antenna || GetNetMode() == NM_DedicatedServer) { return; }
	if (!bStudio && !Antenna->WasRecentlyRendered(0.5f))
	{
		bHasLastLocation = false;
		return;
	}
	UpdateAntenna(FMath::Min(DeltaTime, 0.05f));
}

void UTN_BuggyLookComponent::UpdateAntenna(float DeltaTime)
{
	using namespace TNBuggyLookDetail;
	AntennaTime += DeltaTime;
	const FVector Location = GetComponentLocation();
	if (!bHasLastLocation || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		LastLocation = Location;
		LastVelocity = FVector::ZeroVector;
		bHasLastLocation = true;
		return;
	}
	FVector Velocity = (Location - LastLocation) / DeltaTime;
	// Una reaparición o un enderezado teletransportan el chasis: no es un acelerón.
	if (Velocity.SizeSquared() > FMath::Square(8000.f)) { Velocity = LastVelocity; }
	const FVector Accel = (Velocity - LastVelocity) / DeltaTime;
	LastLocation = Location;
	LastVelocity = Velocity;

	const FTransform& Xf = GetComponentTransform();
	const FVector LocalAccel = Xf.InverseTransformVectorNoScale(Accel);
	const FVector LocalVelocity = Xf.InverseTransformVectorNoScale(Velocity);
	// Cabeceo positivo: la varilla se va hacia atrás (al acelerar y con el viento); alabeo: hacia el lado contrario al giro.
	const float Idle = 1.6f * FMath::Sin(AntennaTime * 2.3f) + 0.8f * FMath::Sin(AntennaTime * 5.1f);
	const FVector2D Target(
		FMath::Clamp(static_cast<float>(LocalAccel.X) * LeanPerAccel + static_cast<float>(LocalVelocity.X) * LeanPerSpeed + Idle, -MaxLeanDeg, MaxLeanDeg),
		FMath::Clamp(static_cast<float>(LocalAccel.Y) * LeanPerAccel + Idle * 0.5f, -MaxLeanDeg, MaxLeanDeg));
	LeanSpeed += ((Target - Lean) * Spring - LeanSpeed * Damping) * DeltaTime;
	Lean += LeanSpeed * DeltaTime;
	Lean.X = FMath::Clamp(Lean.X, -MaxLeanDeg, MaxLeanDeg);
	Lean.Y = FMath::Clamp(Lean.Y, -MaxLeanDeg, MaxLeanDeg);
	AntennaPivot->SetRelativeRotation(FRotator(Lean.X, 0.0, Lean.Y));
}
