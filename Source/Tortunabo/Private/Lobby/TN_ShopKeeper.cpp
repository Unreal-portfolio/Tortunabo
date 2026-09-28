#include "Lobby/TN_ShopKeeper.h"
#include "Audio/TN_MusicSynthComponent.h"
#include "Core/TN_CosmeticLook.h"
#include "Core/TN_Log.h"
#include "Player/MP_GamePlayerController.h"
#include "Animation/AnimationAsset.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"

namespace TNShopKeeperDetail
{
	/** Color sRGB 0xRRGGBB para mallas en ejecución con M_CosmeticVertexColor (ver ATN_CosmeticPreview). */
	FLinearColor Pal(uint32 Hex)
	{
		return FLinearColor(TNProcRuntimeMesh::SRGBToLinear(((Hex >> 16) & 255) / 255.f), TNProcRuntimeMesh::SRGBToLinear(((Hex >> 8) & 255) / 255.f),
			TNProcRuntimeMesh::SRGBToLinear((Hex & 255) / 255.f), 1.f);
	}

	/** Mostrador delante del tendero (en su +X). */
	constexpr double CounterX = 125.0;
	constexpr double CounterHalfDepth = 32.0;
	constexpr double CounterHalfWidth = 190.0;
	constexpr double CounterHeight = 108.0;
	/** Toldo: el borde de delante (con el volante de picos) y el de detrás, más alto. */
	constexpr double CanopyFrontX = CounterX + 75.0;
	constexpr double CanopyBackX = -115.0;
	constexpr double CanopyFrontZ = 305.0;
	constexpr double CanopyBackZ = 378.0;
	constexpr double CanopyHalfWidth = CounterHalfWidth + 45.0;
	/** Tablero del cartel: de pie encima del borde de delante del toldo, por detrás del volante. */
	constexpr double SignX = CanopyFrontX - 12.0;
	constexpr double SignZ = CanopyFrontZ + 34.0;
	constexpr double SignHalfW = 215.0;
	constexpr double SignHalfH = 27.0;
}

ATN_ShopKeeper::ATN_ShopKeeper()
{
	using namespace TNShopKeeperDetail;
	PrimaryActorTick.bCanEverTick = true;
	NetDormancy = DORM_Awake;
	bAlwaysRelevant = true;
	CooldownSeconds = 0.6f;
	PromptText = NSLOCTEXT("Tortunabo", "ShopPrompt", "Hablar con el tendero");
	KeeperName = NSLOCTEXT("Tortunabo", "ShopKeeperName", "Don Tortugo");
	ShopName = NSLOCTEXT("Tortunabo", "ShopName", "La Concha Dorada");
	KeeperLook.HelmetId = TEXT("Helmet_Straw");
	KeeperLook.ShellId = TEXT("Shell_Scutes");
	KeeperLook.SkinId = TEXT("Body_Sand");

	// Hitbox de interacción (invisible) delante del mostrador; el aviso flota sobre él.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Mesh && Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetRelativeLocation(FVector(CounterX + 70.0, 0.0, 60.0));
		Mesh->SetRelativeScale3D(FVector(2.4f, 2.4f, 1.2f));
		Mesh->SetHiddenInGame(true);
	}
	if (PromptWidgetComponent)
	{
		// La hitbox va estirada: el aviso no hereda su escala.
		PromptWidgetComponent->SetUsingAbsoluteScale(true);
		PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	}

	Keeper = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Keeper"));
	Keeper->SetupAttachment(SceneRoot);
	Keeper->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Keeper->SetRelativeScale3D(FVector(KeeperScale));
	Keeper->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Keeper->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TurtleMesh(TEXT("/Game/Meshses/Characters/Player/TotugaDemo_Rig.TotugaDemo_Rig"));
	if (TurtleMesh.Succeeded()) { Keeper->SetSkeletalMeshAsset(TurtleMesh.Object); }

	KeeperHat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeeperHat"));
	KeeperHat->SetupAttachment(Keeper);
	KeeperHat->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	KeeperBlock = CreateDefaultSubobject<UCapsuleComponent>(TEXT("KeeperBlock"));
	KeeperBlock->SetupAttachment(SceneRoot);
	KeeperBlock->InitCapsuleSize(55.f, 95.f);
	KeeperBlock->SetRelativeLocation(FVector(0.f, 0.f, 95.f));
	KeeperBlock->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	KeeperBlock->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	Stall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stall"));
	Stall->SetupAttachment(SceneRoot);
	Stall->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CounterBlock = CreateDefaultSubobject<UBoxComponent>(TEXT("CounterBlock"));
	CounterBlock->SetupAttachment(SceneRoot);
	CounterBlock->InitBoxExtent(FVector(CounterHalfDepth + 4.0, CounterHalfWidth + 6.0, CounterHeight * 0.5));
	CounterBlock->SetRelativeLocation(FVector(CounterX, 0.0, CounterHeight * 0.5));
	CounterBlock->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	CounterBlock->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Cartel del toldo: mira hacia los clientes (+X).
	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(SceneRoot);
	// Delante del tablero del cartel (su cara está en SignX + 4).
	Sign->SetRelativeLocation(FVector(SignX + 6.5, 0.0, SignZ));
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetWorldSize(40.f);
	Sign->SetTextRenderColor(FColor(255, 214, 90));
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Animations/Character/TortugaDemo/Anim/Old_Man_Idle.Old_Man_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Wave(TEXT("/Game/Animations/Character/TortugaDemo/Anim/Salute.Salute"));
	IdleAnim = Idle.Succeeded() ? Idle.Object : nullptr;
	WaveAnim = Wave.Succeeded() ? Wave.Object : nullptr;
}

void ATN_ShopKeeper::BeginPlay()
{
	Super::BeginPlay();
	Keeper->SetRelativeScale3D(FVector(KeeperScale));
	Sign->SetText(FText::FromString(ShopName.ToString().ToUpper()));
	if (IdleAnim) { Keeper->PlayAnimation(IdleAnim, true); }
	UTN_CosmeticLook::ApplyLook(this, Keeper, KeeperHat, KeeperLook, KeeperDefaults);
	BuildStall();
	HideBlockoutKeeper();
	if (GetNetMode() != NM_DedicatedServer)
	{
		Radio = UTN_MusicSynthComponent::AttachMusic3D(this, GetActorTransform().TransformPosition(FVector(0.0, 0.0, 180.0)),
			ETNMusicTrack::Shop, RadioVolume);
	}
}

FVector ATN_ShopKeeper::GetInteractionPoint() const
{
	using namespace TNShopKeeperDetail;
	return GetActorTransform().TransformPosition(FVector(CounterX + CounterHalfDepth + 60.0, 0.0, 0.0));
}

void ATN_ShopKeeper::SetRadiosDucked(UWorld* World, bool bDucked)
{
	if (!World) { return; }
	for (TActorIterator<ATN_ShopKeeper> It(World); It; ++It)
	{
		if (UTN_MusicSynthComponent* ShopRadio = It->Radio)
		{
			ShopRadio->SetMusicVolume(bDucked ? It->RadioVolume * 0.15f : It->RadioVolume);
		}
	}
}

void ATN_ShopKeeper::BuildStall()
{
	using namespace TNShopKeeperDetail;
	TNProcMesh::FTNProcMeshBuffers B;
	const FLinearColor Wood = Pal(0xB07A4A);
	const FLinearColor WoodDark = Pal(0x8A5A32);
	const FLinearColor WoodTop = Pal(0xD9A066);
	const FLinearColor Coral = Pal(0xFF6A52);
	const FLinearColor Cream = Pal(0xFFF2D4);
	const FLinearColor Navy = Pal(0x12305A);
	const FLinearColor Gold = Pal(0xFFCB3D);
	const FLinearColor Flags[4] = { Pal(0xFF6FA8), Pal(0xFFD23F), Pal(0x4CC9F0), Pal(0x3DDC62) };

	// Mostrador de tablones con tapa clara y franja coral delante.
	B.AddBox(FVector(CounterX, 0.0, CounterHeight * 0.5 - 4.0), FVector(1.0, 0.0, 0.0), FVector(CounterHalfDepth, CounterHalfWidth, CounterHeight * 0.5 - 4.0), Wood);
	B.AddBox(FVector(CounterX + 2.0, 0.0, CounterHeight - 3.0), FVector(1.0, 0.0, 0.0), FVector(CounterHalfDepth + 8.0, CounterHalfWidth + 10.0, 4.5), WoodTop);
	for (int32 i = -5; i <= 5; ++i)
	{
		const double Y = i * (CounterHalfWidth / 5.6);
		B.AddBox(FVector(CounterX + CounterHalfDepth + 0.8, Y, CounterHeight * 0.5 - 4.0), FVector(1.0, 0.0, 0.0), FVector(1.2, 2.4, CounterHeight * 0.5 - 8.0), WoodDark);
	}
	B.AddBox(FVector(CounterX + CounterHalfDepth + 1.8, 0.0, CounterHeight * 0.62), FVector(1.0, 0.0, 0.0), FVector(0.8, CounterHalfWidth - 8.0, 10.0), Coral);

	// Postes (delante, a la altura del borde del toldo; detrás, más altos).
	for (const double PX : { CanopyFrontX - 20.0, CanopyBackX + 20.0 })
	{
		for (const double PY : { -CanopyHalfWidth + 20.0, CanopyHalfWidth - 20.0 })
		{
			const double Top = PX > 0.0 ? CanopyFrontZ : CanopyBackZ;
			B.AddBeam(FVector(PX, PY, 0.0), FVector(PX, PY, Top), 6.5, WoodDark);
		}
	}

	// Toldo inclinado a rayas (las dos caras) y volante de picos colgando del borde de delante, un poco por fuera.
	const int32 Stripes = 12;
	for (int32 k = 0; k < Stripes; ++k)
	{
		const double Ya = FMath::Lerp(-CanopyHalfWidth, CanopyHalfWidth, static_cast<double>(k) / Stripes);
		const double Yb = FMath::Lerp(-CanopyHalfWidth, CanopyHalfWidth, static_cast<double>(k + 1) / Stripes);
		const FLinearColor C = (k % 2) ? Cream : Coral;
		const FVector A(CanopyFrontX, Ya, CanopyFrontZ), Bf(CanopyFrontX, Yb, CanopyFrontZ), Cb(CanopyBackX, Yb, CanopyBackZ), D(CanopyBackX, Ya, CanopyBackZ);
		B.AddQuad(A, Bf, Cb, D, FVector(0.3, 0.0, 1.0), C);
		B.AddQuad(A, Bf, Cb, D, FVector(-0.3, 0.0, -1.0), C * 0.8f);
		const FVector Va(CanopyFrontX + 2.0, Ya, CanopyFrontZ + 1.0), Vb(CanopyFrontX + 2.0, Yb, CanopyFrontZ + 1.0);
		const FVector Tip(CanopyFrontX + 2.0, (Ya + Yb) * 0.5, CanopyFrontZ - 26.0);
		B.AddTri(Va, Vb, Tip, FVector(1.0, 0.0, 0.0), C);
		B.AddTri(Va, Vb, Tip, FVector(-1.0, 0.0, 0.0), C * 0.8f);
	}

	// Cartel de pie sobre el borde de delante del toldo, con marco dorado y dos patas (el texto es el componente Sign).
	B.AddBox(FVector(SignX, 0.0, SignZ), FVector(1.0, 0.0, 0.0), FVector(4.0, SignHalfW, SignHalfH), Navy);
	B.AddBox(FVector(SignX - 1.5, 0.0, SignZ), FVector(1.0, 0.0, 0.0), FVector(4.0, SignHalfW + 7.0, SignHalfH + 7.0), Gold);
	for (const double LegY : { -SignHalfW * 0.6, SignHalfW * 0.6 })
	{
		B.AddBeam(FVector(SignX - 2.0, LegY, CanopyFrontZ - 4.0), FVector(SignX - 2.0, LegY, SignZ - SignHalfH), 4.0, WoodDark);
	}

	// Guirnalda de banderines de fiesta de poste a poste, delante del volante (no lo toca).
	const FVector G0(CanopyFrontX + 16.0, -CanopyHalfWidth + 20.0, CanopyFrontZ - 34.0);
	const FVector G1(CanopyFrontX + 16.0, CanopyHalfWidth - 20.0, CanopyFrontZ - 34.0);
	const int32 FlagCount = 14;
	for (int32 f = 0; f <= FlagCount; ++f)
	{
		const double T0 = static_cast<double>(f) / (FlagCount + 1);
		const double T1 = static_cast<double>(f + 1) / (FlagCount + 1);
		auto Sag = [&](double T) { return FMath::Lerp(G0, G1, T) - FVector(0.0, 0.0, 38.0 * 4.0 * T * (1.0 - T)); };
		B.AddBeam(Sag(T0), Sag(T1), 0.9, Cream * 0.8f);
		if (f == FlagCount) { continue; }
		const FVector Top0 = Sag(T0 + 0.18 / (FlagCount + 1)), Top1 = Sag(T1 - 0.18 / (FlagCount + 1));
		const FVector Tip = (Top0 + Top1) * 0.5 - FVector(0.0, 0.0, 26.0);
		B.AddTri(Top0, Top1, Tip, FVector(1.0, 0.0, 0.0), Flags[f % 4]);
		B.AddTri(Top0, Top1, Tip, FVector(-1.0, 0.0, 0.0), Flags[f % 4] * 0.8f);
	}

	// Cofre del tesoro con monedas a un lado del mostrador.
	const FVector Chest(CounterX - 10.0, CounterHalfWidth + 75.0, 0.0);
	B.AddBox(Chest + FVector(0.0, 0.0, 26.0), FVector(1.0, 0.0, 0.0), FVector(30.0, 40.0, 26.0), Wood);
	B.AddBox(Chest + FVector(-4.0, 0.0, 58.0), FVector(1.0, 0.0, 0.0), FVector(28.0, 41.0, 6.0), WoodDark);
	B.AddBox(Chest + FVector(30.5, 0.0, 26.0), FVector(1.0, 0.0, 0.0), FVector(1.0, 7.0, 9.0), Gold);
	for (int32 c = 0; c < 9; ++c)
	{
		const double A = c * 0.8;
		TNProcMesh::TNProcAddCylinder(B, Chest + FVector(8.0 + 12.0 * FMath::Cos(A), 14.0 * FMath::Sin(A), 52.0 + (c % 3) * 1.4),
			Chest + FVector(8.0 + 12.0 * FMath::Cos(A), 14.0 * FMath::Sin(A), 53.8 + (c % 3) * 1.4), 5.0, 5.0, 8, Gold);
	}

	UMaterialInterface* VertexColorMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cosmetics/Materials/M_CosmeticVertexColor.M_CosmeticVertexColor"));
	Stall->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, B, VertexColorMat));
}

void ATN_ShopKeeper::HideBlockoutKeeper()
{
	// El tendero de la maqueta (una tortuga suelta con la misma malla) queda debajo de este: se esconde en cada máquina.
	for (TActorIterator<ASkeletalMeshActor> It(GetWorld()); It; ++It)
	{
		ASkeletalMeshActor* Blockout = *It;
		const USkeletalMeshComponent* Comp = Blockout ? Blockout->GetSkeletalMeshComponent() : nullptr;
		const USkinnedAsset* Asset = Comp ? Comp->GetSkinnedAsset() : nullptr;
		if (Asset && Asset->GetName().Contains(TEXT("TotugaDemo")) && FVector::Dist2D(Blockout->GetActorLocation(), GetActorLocation()) < 150.0)
		{
			Blockout->SetActorHiddenInGame(true);
			Blockout->SetActorEnableCollision(false);
		}
	}
}

void ATN_ShopKeeper::OnInteracted_Implementation(APawn* Interactor)
{
	Super::OnInteracted_Implementation(Interactor);
	if (!HasAuthority() || !Interactor) { return; }
	if (AMP_GamePlayerController* PC = Cast<AMP_GamePlayerController>(Interactor->GetController()))
	{
		UE_LOG(LogTortunabo, Log, TEXT("[Tienda] %s abre la tienda."), *GetNameSafe(Interactor));
		PC->ClientOpenShop(this);
	}
}

void ATN_ShopKeeper::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) { return; }

	// Se gira hacia el jugador local si está cerca y le saluda de vez en cuando.
	const APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* LocalPawn = LocalPC ? LocalPC->GetPawn() : nullptr;
	float TargetYaw = 0.f;
	if (LocalPawn)
	{
		const FVector Local = GetActorTransform().InverseTransformPosition(LocalPawn->GetActorLocation());
		const float Dist = Local.Size2D();
		if (Dist < 900.f && Local.X > -50.f)
		{
			TargetYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)), -55.f, 55.f);
			if (Dist < 650.f && WaveCooldown <= 0.f && WaveAnim)
			{
				Keeper->PlayAnimation(WaveAnim, false);
				WaveTimeLeft = WaveAnim->GetPlayLength();
				WaveCooldown = 12.f;
			}
		}
	}
	LookYaw = FMath::FInterpTo(LookYaw, TargetYaw, DeltaSeconds, 3.f);
	Keeper->SetRelativeRotation(FRotator(0.f, -90.f + LookYaw, 0.f));
	WaveCooldown -= DeltaSeconds;
	if (WaveTimeLeft > 0.f)
	{
		WaveTimeLeft -= DeltaSeconds;
		if (WaveTimeLeft <= 0.f && IdleAnim) { Keeper->PlayAnimation(IdleAnim, true); }
	}
}
