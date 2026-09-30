#include "VR/TN_VRRig.h"
#include "VR/TN_VRScreenWidget.h"
#include "VR/TN_VRSubsystem.h"
#include "VR/TN_VRMath.h"
#include "Core/TN_Log.h"
#include "Core/TN_ProjectMaterials.h"
#include "Player/MP_GamePlayerController.h"
#include "Player/TortugaCharacter.h"
#include "Settings/TN_GameSettingsSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/HitResult.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MotionControllerComponent.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"

// ─────────────────────────────────────────────────────────────────────────────
// Consola
// ─────────────────────────────────────────────────────────────────────────────

static TAutoConsoleVariable<float> CVarTNVRHudDistance(TEXT("TN.VR.HudDistance"), 140.f,
	TEXT("Distancia (cm) del HUD delante de los ojos en VR. Se acerca solo si hay una pared en medio."), ECVF_Default);
static TAutoConsoleVariable<float> CVarTNVRHudFov(TEXT("TN.VR.HudFov"), 50.f,
	TEXT("Ancho (grados) que ocupa el HUD en VR."), ECVF_Default);
static TAutoConsoleVariable<float> CVarTNVRMenuDistance(TEXT("TN.VR.MenuDistance"), 160.f,
	TEXT("Distancia (cm) a la que se ponen los menús en VR."), ECVF_Default);
static TAutoConsoleVariable<float> CVarTNVRMenuFov(TEXT("TN.VR.MenuFov"), 58.f,
	TEXT("Ancho (grados) que ocupan los menús en VR."), ECVF_Default);
static TAutoConsoleVariable<float> CVarTNVRSmoothTurnSpeed(TEXT("TN.VR.SmoothTurnSpeed"), 120.f,
	TEXT("Grados por segundo del giro suave en VR (ajuste «Giro en VR: suave»)."), ECVF_Default);

namespace TNVRRigDetail
{
	const TCHAR* const ControlsPath = TEXT("/Game/Blueprints/Gameplay/Controls/");

	/** Color sRGB 0xRRGGBB para M_CosmeticVertexColor (MakeStaticMesh decodifica una vez más: así llega lineal). */
	FLinearColor Pal(uint32 Hex)
	{
		return FLinearColor(TNProcRuntimeMesh::SRGBToLinear(((Hex >> 16) & 255) / 255.f), TNProcRuntimeMesh::SRGBToLinear(((Hex >> 8) & 255) / 255.f),
			TNProcRuntimeMesh::SRGBToLinear((Hex & 255) / 255.f), 1.f);
	}

	UMaterialInterface* VertexColorMaterial()
	{
		return TNMaterials::VertexColor();
	}

	UInputAction* LoadAction(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("%s%s.%s"), ControlsPath, Name, Name);
		return LoadObject<UInputAction>(nullptr, *Path);
	}

	/**
	 * Aleta de tortuga de la mano: pala aplanada a lo largo de +X (de la muñeca a la punta, 17 cm), con la manga de
	 * caparazón en la muñeca y tres uñas claras en el borde.
	 */
	UStaticMesh* BuildFlipperMesh(UObject* Outer)
	{
		TNProcMesh::FTNProcMeshBuffers B;
		const FLinearColor Skin = Pal(0x6FC25C);
		const FLinearColor Shell = Pal(0x3F7F37);
		const FLinearColor Nail = Pal(0xEDE3B6);
		constexpr int32 Seg = 12;
		const double Xs[] = { 0.0, 3.0, 7.0, 11.0, 14.5, 17.0 };
		const double HalfW[] = { 3.0, 4.0, 4.9, 4.7, 3.5, 1.4 };
		const double HalfT[] = { 2.4, 1.9, 1.5, 1.2, 0.9, 0.5 };
		const int32 NumRings = static_cast<int32>(UE_ARRAY_COUNT(Xs));
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r < NumRings; ++r)
		{
			TArray<FVector> Ring;
			for (int32 k = 0; k < Seg; ++k)
			{
				const double Ang = 2.0 * PI * k / Seg;
				Ring.Add(FVector(Xs[r], FMath::Cos(Ang) * HalfW[r], FMath::Sin(Ang) * HalfT[r]));
			}
			Rings.Add(Ring);
		}
		B.AddSweep(Rings, true, Skin);
		const FVector WristC(Xs[0], 0.0, 0.0);
		const FVector TipC(Xs[NumRings - 1] + 0.6, 0.0, 0.0);
		for (int32 k = 0; k < Seg; ++k)
		{
			B.AddTri(WristC, Rings[0][k], Rings[0][(k + 1) % Seg], FVector(-1.0, 0.0, 0.0), Skin);
			B.AddTri(TipC, Rings[NumRings - 1][k], Rings[NumRings - 1][(k + 1) % Seg], FVector(1.0, 0.0, 0.0), Skin);
		}
		// Manga de caparazón en la muñeca.
		TNProcMesh::TNProcAddCylinder(B, FVector(-5.0, 0.0, 0.0), FVector(0.5, 0.0, 0.0), 3.7, 3.3, Seg, Shell);
		// Uñas en el borde de delante.
		for (int32 n = -1; n <= 1; ++n)
		{
			const FVector Base(13.0, n * 2.6, 0.0);
			TNProcMesh::TNProcAddCylinder(B, Base, Base + FVector(4.2, n * 0.5, 0.0), 0.8, 0.2, 6, Nail);
		}
		return TNProcRuntimeMesh::MakeStaticMesh(Outer, B, VertexColorMaterial());
	}

	UCameraComponent* FindActiveCamera(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}
		TInlineComponentArray<UCameraComponent*> Cameras(Actor);
		for (UCameraComponent* Camera : Cameras)
		{
			if (Camera && Camera->IsActive())
			{
				return Camera;
			}
		}
		return nullptr;
	}

	/** Postura de las aletas en la vista simulada (sin gafas): abajo a los lados, apuntando al frente. */
	const FTransform SimLeftHand(FRotator(-8.0, 10.0, -20.0), FVector(34.0, -19.0, -22.0));
	const FTransform SimRightHand(FRotator(-8.0, -10.0, 20.0), FVector(34.0, 19.0, -22.0));
}

// ─────────────────────────────────────────────────────────────────────────────
// Construcción
// ─────────────────────────────────────────────────────────────────────────────

ATN_VRRig::ATN_VRRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bReplicates = false;
	SetCanBeDamaged(false);

	RigRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RigRoot"));
	RootComponent = RigRoot;

	RigCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RigCamera"));
	RigCamera->SetupAttachment(RigRoot);
	RigCamera->bLockToHmd = true;
	RigCamera->FieldOfView = 90.f;

	auto MakeController = [this](const TCHAR* Name, const TCHAR* Source)
	{
		UMotionControllerComponent* Controller = CreateDefaultSubobject<UMotionControllerComponent>(Name);
		Controller->SetupAttachment(RigRoot);
		Controller->MotionSource = FName(Source);
		return Controller;
	};
	LeftGrip = MakeController(TEXT("LeftGrip"), TEXT("LeftGrip"));
	RightGrip = MakeController(TEXT("RightGrip"), TEXT("RightGrip"));
	RightAim = MakeController(TEXT("RightAim"), TEXT("RightAim"));

	LeftHand = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(LeftGrip);
	RightHand = CreateDefaultSubobject<USceneComponent>(TEXT("RightHand"));
	RightHand->SetupAttachment(RightGrip);

	auto MakeVisual = [this](const TCHAR* Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* Visual = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Visual->SetupAttachment(Parent);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetCastShadow(false);
		Visual->SetGenerateOverlapEvents(false);
		return Visual;
	};
	LeftFlipper = MakeVisual(TEXT("LeftFlipper"), LeftHand);
	LeftFlipper->SetRelativeLocation(FVector(-3.0, 0.0, 0.0));
	RightFlipper = MakeVisual(TEXT("RightFlipper"), RightHand);
	RightFlipper->SetRelativeLocation(FVector(-3.0, 0.0, 0.0));

	// La interfaz, el puntero y los mandos siguen vivos aunque el juego esté en pausa.
	LeftGrip->PrimaryComponentTick.bTickEvenWhenPaused = true;
	RightGrip->PrimaryComponentTick.bTickEvenWhenPaused = true;
	RightAim->PrimaryComponentTick.bTickEvenWhenPaused = true;

	LaserBeam = MakeVisual(TEXT("LaserBeam"), RigRoot);
	LaserBeam->SetUsingAbsoluteLocation(true);
	LaserBeam->SetUsingAbsoluteRotation(true);
	LaserBeam->SetUsingAbsoluteScale(true);
	LaserBeam->SetVisibility(false);
	LaserDot = MakeVisual(TEXT("LaserDot"), RigRoot);
	LaserDot->SetUsingAbsoluteLocation(true);
	LaserDot->SetUsingAbsoluteRotation(true);
	LaserDot->SetUsingAbsoluteScale(true);
	LaserDot->SetVisibility(false);

	Pointer = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("Pointer"));
	Pointer->SetupAttachment(RigRoot);
	Pointer->InteractionSource = EWidgetInteractionSource::Custom;
	Pointer->InteractionDistance = 2000.f;
	Pointer->bShowDebug = false;
	Pointer->bEnableHitTesting = true;
	// Usuario virtual propio: su foco y su captura no pisan los del jugador (teclado y mando).
	Pointer->VirtualUserIndex = 4;
	Pointer->PointerIndex = 0;
	Pointer->PrimaryComponentTick.bTickEvenWhenPaused = true;

	ScreenPanel = CreateDefaultSubobject<UWidgetComponent>(TEXT("ScreenPanel"));
	ScreenPanel->SetupAttachment(RigRoot);
	ScreenPanel->SetUsingAbsoluteLocation(true);
	ScreenPanel->SetUsingAbsoluteRotation(true);
	ScreenPanel->SetUsingAbsoluteScale(true);
	ScreenPanel->SetWidgetSpace(EWidgetSpace::World);
	ScreenPanel->SetDrawSize(FVector2D(UTN_VRScreenWidget::ScreenWidth, UTN_VRScreenWidget::ScreenHeight));
	ScreenPanel->SetPivot(FVector2D(0.5f, 0.5f));
	ScreenPanel->SetTwoSided(false);
	ScreenPanel->SetBlendMode(EWidgetBlendMode::Transparent);
	ScreenPanel->SetBackgroundColor(FLinearColor::Transparent);
	ScreenPanel->SetTickWhenOffscreen(true);
	ScreenPanel->SetWindowFocusable(true);
	ScreenPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScreenPanel->SetGenerateOverlapEvents(false);
	ScreenPanel->SetCastShadow(false);
	ScreenPanel->SetTranslucentSortPriority(100);
	ScreenPanel->PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void ATN_VRRig::BeginPlay()
{
	Super::BeginPlay();
	Mode = TNVR::GetMode();
	BuildHands();
	BuildLaser();
	EnsureScreen();
	OnModeChanged(Mode);

	// Lo que alguien puso en el viewport antes de que existiera el rig (el mundo acaba de empezar) pasa al panel.
	TArray<UUserWidget*> Found;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (Widget && Widget != Screen && Widget->GetWorld() == GetWorld() && Widget->IsInViewport())
		{
			Found.Add(Widget);
		}
	}
	for (UUserWidget* Widget : Found)
	{
		HostWidget(Widget, 0);
	}
	if (Found.Num() > 0)
	{
		UE_LOG(LogTortunabo, Log, TEXT("[VR] %d widgets del viewport pasan al panel VR."), Found.Num());
	}
}

void ATN_VRRig::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bPointerDown)
	{
		PointerRelease();
	}
	RemoveVRMapping();
	if (Screen)
	{
		Screen->ReleaseAll(false);
	}
	// Apagado a mitad de partida (no al cambiar de mapa): la tortuga vuelve a su cámara de siempre.
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		if (ATortugaCharacter* Turtle = ViewTurtle.Get())
		{
			Turtle->SetVRView(false, false);
		}
		if (bOwnsViewTarget)
		{
			if (APlayerController* PC = GetLocalPC())
			{
				PC->SetViewTarget(PC->GetPawn() ? static_cast<AActor*>(PC->GetPawn()) : static_cast<AActor*>(PC));
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ATN_VRRig::BuildHands()
{
	UStaticMesh* Flipper = TNVRRigDetail::BuildFlipperMesh(this);
	if (!Flipper)
	{
		return;
	}
	LeftFlipper->SetStaticMesh(Flipper);
	RightFlipper->SetStaticMesh(Flipper);
}

void ATN_VRRig::BuildLaser()
{
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Basic)
	{
		LaserMaterial = UMaterialInstanceDynamic::Create(Basic, this);
		LaserMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.78f, 0.25f));
	}
	if (Cylinder)
	{
		LaserBeam->SetStaticMesh(Cylinder);
		if (LaserMaterial) { LaserBeam->SetMaterial(0, LaserMaterial); }
	}
	if (Sphere)
	{
		LaserDot->SetStaticMesh(Sphere);
		if (LaserMaterial) { LaserDot->SetMaterial(0, LaserMaterial); }
	}
}

void ATN_VRRig::EnsureScreen()
{
	if (Screen || !GetWorld())
	{
		return;
	}
	Screen = CreateWidget<UTN_VRScreenWidget>(GetWorld(), UTN_VRScreenWidget::StaticClass());
	if (Screen)
	{
		ScreenPanel->SetWidget(Screen);
	}
}

void ATN_VRRig::OnModeChanged(ETNVRMode NewMode)
{
	Mode = NewMode;
	const bool bHeadset = NewMode == ETNVRMode::Headset;
	// Con gafas los mandos se siguen solos; simulado, las aletas se quedan quietas delante de la cámara.
	LeftGrip->SetActive(bHeadset);
	RightGrip->SetActive(bHeadset);
	RightAim->SetActive(bHeadset);
	if (!bHeadset)
	{
		LeftGrip->SetRelativeTransform(TNVRRigDetail::SimLeftHand);
		RightGrip->SetRelativeTransform(TNVRRigDetail::SimRightHand);
		RightAim->SetRelativeTransform(TNVRRigDetail::SimRightHand);
		RemoveVRMapping();
	}
	RigCamera->bLockToHmd = bHeadset;
	LaserBeam->SetVisibility(false);
	LaserDot->SetVisibility(false);
	bPanelPlaced = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Fotograma
// ─────────────────────────────────────────────────────────────────────────────

APlayerController* ATN_VRRig::GetLocalPC() const
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC && PC->IsLocalController() ? PC : nullptr;
}

void ATN_VRRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Mode = TNVR::GetMode();
	if (Mode == ETNVRMode::Off)
	{
		return;
	}
	EnsureScreen();
	APlayerController* PC = GetLocalPC();
	if (!PC)
	{
		ScreenPanel->SetVisibility(false);
		return;
	}

	// La tortuga propia en primera persona (y la que ya no es nuestra, de vuelta a la de siempre).
	ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PC->GetPawn());
	if (Turtle && !Turtle->IsLocallyControlled())
	{
		Turtle = nullptr;
	}
	ATortugaCharacter* Previous = ViewTurtle.Get();
	if (Previous && Previous != Turtle)
	{
		Previous->SetVRView(false, false);
	}
	if (Turtle)
	{
		Turtle->SetVRView(true, Mode == ETNVRMode::Headset);
	}
	ViewTurtle = Turtle;

	UpdateViewAttachment(PC, Turtle);
	UpdateHands(DeltaSeconds);

	// Hacia dónde apunta la aleta derecha: lanzar compañeros y objetos (con gafas; simulado, la cámara).
	if (Turtle)
	{
		if (Mode == ETNVRMode::Headset)
		{
			UMotionControllerComponent* AimSource = RightAim->IsTracked() ? RightAim.Get() : RightGrip.Get();
			Turtle->SetLocalVRAim(AimSource->GetComponentRotation(), AimSource->IsTracked());
		}
		else
		{
			Turtle->SetLocalVRAim(FRotator::ZeroRotator, false);
		}
	}

	// ¿Menú delante? El juego enseña el cursor con cualquier menú (tienda, probador, general, pausa, salas, campeón);
	// las ruedas de emotes y frases también lo enseñan, pero se manejan con el stick y siguen siendo HUD.
	const AMP_GamePlayerController* GamePC = Cast<AMP_GamePlayerController>(PC);
	const bool bWheelOpen = GamePC && GamePC->IsRadialWheelOpen();
	const bool bNowMenu = PC->ShouldShowMouseCursor() && !bWheelOpen && Screen && Screen->CountVisible() > 0;
	if (bNowMenu != bMenuMode)
	{
		bMenuMode = bNowMenu;
		bPanelPlaced = false;
		if (!bMenuMode && bPointerDown)
		{
			PointerRelease();
		}
	}

	UpdateInput(PC, Turtle, DeltaSeconds);
	UpdatePanel(PC, DeltaSeconds);
	UpdatePointer(PC);
}

void ATN_VRRig::UpdateViewAttachment(APlayerController* PC, ATortugaCharacter* Turtle)
{
	const bool bHeadset = Mode == ETNVRMode::Headset;
	AActor* ViewTarget = PC->GetViewTarget();

	// Sin peón y mirando al propio PlayerController (menú principal, antes de aparecer): la vista es la del rig. Con gafas,
	// también si se mira a algo sin cámara (el peón por defecto de un menú): sin cámara no hay a quién ponerle la cabeza.
	const bool bPawnless = !PC->GetPawn() && (ViewTarget == PC || ViewTarget == nullptr);
	const bool bNoCameraView = bHeadset && ViewTarget && ViewTarget != this && !ViewTarget->IsA<ATortugaCharacter>()
		&& !TNVRRigDetail::FindActiveCamera(ViewTarget);
	if (bPawnless || bNoCameraView)
	{
		if (PC->GetViewTarget() != this)
		{
			FVector Location = GetActorLocation();
			FRotator Rotation = GetActorRotation();
			GetViewPoint(PC, Location, Rotation);
			RigRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			AttachedBase = nullptr;
			AttachedSocket = NAME_None;
			SetActorLocationAndRotation(Location, FRotator(0.0, Rotation.Yaw, 0.0));
			PC->SetViewTarget(this);
		}
		bOwnsViewTarget = true;
		ViewTarget = this;
	}
	else if (ViewTarget != this)
	{
		bOwnsViewTarget = false;
	}
	RigCamera->SetActive(ViewTarget == this);

	USceneComponent* Base = nullptr;
	FName Socket = NAME_None;
	if (ViewTarget != this)
	{
		if (Turtle && ViewTarget == Turtle && Turtle->GetVRCamera())
		{
			// Con gafas, el origen del seguimiento (la cámara se mueve con la cabeza dentro de él); simulado, la cámara.
			Base = bHeadset ? Turtle->GetVROrigin() : Turtle->GetVRCamera();
		}
		else if (UCameraComponent* Camera = TNVRRigDetail::FindActiveCamera(ViewTarget))
		{
			// Otra cámara (espectador): con gafas su padre hace de origen del seguimiento (el motor le pone la pose de la
			// cabeza encima); simulado, ella misma.
			if (bHeadset)
			{
				Base = Camera->GetAttachParent();
				Socket = Camera->GetAttachSocketName();
			}
			else
			{
				Base = Camera;
			}
		}
	}
	if (Base != AttachedBase.Get() || Socket != AttachedSocket)
	{
		if (Base)
		{
			RigRoot->AttachToComponent(Base, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		}
		else
		{
			RigRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
		AttachedBase = Base;
		AttachedSocket = Socket;
		bPanelPlaced = false;
	}
	if (!Base && ViewTarget != this)
	{
		// Una vista sin cámara conocida: el rig donde está la cámara del juego.
		FVector Location;
		FRotator Rotation;
		if (GetViewPoint(PC, Location, Rotation))
		{
			SetActorLocationAndRotation(Location, bHeadset ? FRotator(0.0, Rotation.Yaw, 0.0) : Rotation);
		}
	}
}

void ATN_VRRig::UpdateHands(float DeltaSeconds)
{
	if (Mode == ETNVRMode::Headset)
	{
		// Una aleta sin seguimiento (mando apagado, fuera de la vista de las cámaras) no se enseña.
		LeftFlipper->SetVisibility(LeftGrip->IsTracked());
		RightFlipper->SetVisibility(RightGrip->IsTracked());
		return;
	}
	// Simulado: quietas delante, con un vaivén muy suave para que se note que están vivas.
	const float Time = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	const FVector Bob(0.0, 0.0, FMath::Sin(Time * 1.7f) * 0.6);
	FTransform Left = TNVRRigDetail::SimLeftHand;
	Left.AddToTranslation(Bob);
	FTransform Right = TNVRRigDetail::SimRightHand;
	Right.AddToTranslation(-Bob);
	LeftGrip->SetRelativeTransform(Left);
	RightGrip->SetRelativeTransform(Right);
	RightAim->SetRelativeTransform(Right);
	LeftFlipper->SetVisibility(true);
	RightFlipper->SetVisibility(true);
}

void ATN_VRRig::UpdateInput(APlayerController* PC, ATortugaCharacter* Turtle, float DeltaSeconds)
{
	if (Mode != ETNVRMode::Headset)
	{
		return;
	}
	EnsureVRMapping(PC);

	// Clic del stick derecho: recentrar.
	const bool bRecenter = PC->IsInputKeyDown(FTNVRKeys::RightStickClick);
	if (bRecenter && !bRecenterHeld)
	{
		if (UTN_VRSubsystem* VR = UTN_VRSubsystem::Get(this))
		{
			VR->Recenter();
		}
	}
	bRecenterHeld = bRecenter;

	AMP_GamePlayerController* GamePC = Cast<AMP_GamePlayerController>(PC);
	if (bMenuMode || (GamePC && GamePC->IsRadialWheelOpen()))
	{
		bSnapLatched = true;
		return;
	}
	const float TurnAxis = PC->GetInputAnalogKeyState(FTNVRKeys::RightStickX);
	AActor* ViewTarget = PC->GetViewTarget();
	if (Turtle && ViewTarget == Turtle)
	{
		const UTN_GameSettingsSubsystem* Settings = UTN_GameSettingsSubsystem::Get(this);
		const uint8 TurnMode = Settings ? Settings->GetSettings().VRTurn : 0;
		if (TurnMode == 2)
		{
			// Giro suave.
			if (FMath::Abs(TurnAxis) > 0.2f)
			{
				Turtle->AddVRYaw(TurnAxis * CVarTNVRSmoothTurnSpeed.GetValueOnGameThread() * DeltaSeconds);
			}
			return;
		}
		const int32 Step = TNVRMath::SnapTurnStep(TurnAxis, bSnapLatched);
		if (Step != 0)
		{
			Turtle->AddVRYaw(Step * (TurnMode == 1 ? 45.f : 30.f));
		}
		return;
	}
	// Mirando a otra tortuga (espectador): el stick derecho cambia de tortuga.
	if (GamePC && ViewTarget && ViewTarget != PC->GetPawn() && ViewTarget->IsA<ATortugaCharacter>())
	{
		const int32 Step = TNVRMath::SnapTurnStep(TurnAxis, bSnapLatched);
		if (Step > 0) { GamePC->SpectateNextPlayer(); }
		else if (Step < 0) { GamePC->SpectatePreviousPlayer(); }
	}
}

void ATN_VRRig::EnsureVRMapping(APlayerController* PC)
{
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input)
	{
		return;
	}
	if (!VRMapping)
	{
		using namespace TNVRRigDetail;
		VRMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_VR"), RF_Transient);
		auto Map = [this](UInputAction* Action, const FKey& Key, bool bSwizzle)
		{
			if (!Action || !Key.IsValid())
			{
				return;
			}
			FEnhancedActionKeyMapping& Mapping = VRMapping->MapKey(Action, Key);
			if (Action->ValueType != EInputActionValueType::Boolean)
			{
				Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(VRMapping));
			}
			if (bSwizzle)
			{
				// El eje Y del stick va a la Y de la acción de dos ejes.
				Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(VRMapping));
			}
		};
		UInputAction* Move = LoadAction(TEXT("IA_Move"));
		Map(Move, FTNVRKeys::LeftStickX, false);
		Map(Move, FTNVRKeys::LeftStickY, true);
		Map(LoadAction(TEXT("IA_Jump")), FTNVRKeys::A, false);
		Map(LoadAction(TEXT("IA_Shell")), FTNVRKeys::B, false);
		Map(LoadAction(TEXT("IA_Interact")), FTNVRKeys::RightTrigger, false);
		Map(LoadAction(TEXT("IA_DropItem")), FTNVRKeys::RightGrip, false);
		Map(LoadAction(TEXT("IA_Sprint")), FTNVRKeys::LeftGrip, false);
		Map(LoadAction(TEXT("IA_RotateInventory")), FTNVRKeys::X, false);
		Map(LoadAction(TEXT("IA_OpenEmoteWheel")), FTNVRKeys::Y, false);
		Map(LoadAction(TEXT("IA_OpenChatWheel")), FTNVRKeys::LeftTrigger, false);
		UInputAction* Radial = LoadAction(TEXT("IA_RadialNavigate"));
		Map(Radial, FTNVRKeys::RightStickX, false);
		Map(Radial, FTNVRKeys::RightStickY, true);
		UE_LOG(LogTortunabo, Log, TEXT("[VR] Mandos VR: %d asignaciones sobre las acciones del juego."), VRMapping->GetMappings().Num());
	}
	// La tortuga y los ajustes rehacen los mapeos al poseer o al cambiar teclas: se vuelve a poner si falta.
	if (!Input->HasMappingContext(VRMapping))
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = false;
		Input->AddMappingContext(VRMapping, 10, Options);
	}
	MappedPC = PC;
}

void ATN_VRRig::RemoveVRMapping()
{
	APlayerController* PC = MappedPC.Get();
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Input && VRMapping && Input->HasMappingContext(VRMapping))
	{
		Input->RemoveMappingContext(VRMapping);
	}
	MappedPC = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Panel de la interfaz
// ─────────────────────────────────────────────────────────────────────────────

bool ATN_VRRig::GetViewPoint(APlayerController* PC, FVector& OutLocation, FRotator& OutRotation) const
{
	const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return false;
	}
	OutLocation = Camera->GetCameraLocation();
	OutRotation = Camera->GetCameraRotation();
	return true;
}

float ATN_VRRig::FitDistance(const FVector& From, const FVector& Dir, float Desired) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Desired;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TNVRPanelFit), false, this);
	if (const APlayerController* PC = GetLocalPC())
	{
		Params.AddIgnoredActor(PC->GetPawn());
		Params.AddIgnoredActor(PC->GetViewTarget());
	}
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, From, From + Dir * Desired, ECC_Visibility, Params);
	return TNVRMath::PanelDistance(Desired, bBlocked, bBlocked ? static_cast<float>(Hit.Distance) : Desired);
}

void ATN_VRRig::PlacePanel(const FVector& ViewLocation, float Yaw, float Distance, float HorizontalFov)
{
	const FVector Dir = FRotator(0.0, Yaw, 0.0).Vector();
	// Un poco por debajo de los ojos: se lee sin levantar la vista.
	const FVector Location = ViewLocation + Dir * Distance - FVector(0.0, 0.0, Distance * 0.1);
	// +X del panel hacia los ojos (su cara de delante).
	const FRotator Facing = (ViewLocation - Location).Rotation();
	ScreenPanel->SetWorldLocationAndRotation(Location, Facing);
	ScreenPanel->SetWorldScale3D(FVector(TNVRMath::PanelScale(Distance, HorizontalFov, UTN_VRScreenWidget::ScreenWidth)));
}

void ATN_VRRig::UpdatePanel(APlayerController* PC, float DeltaSeconds)
{
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!Screen || !GetViewPoint(PC, ViewLocation, ViewRotation))
	{
		return;
	}
	ScreenPanel->SetVisibility(Screen->CountVisible() > 0);

	if (bMenuMode)
	{
		// Menú: quieto delante, donde miraba la cabeza al abrirse (se vuelve a poner si la vista se va lejos).
		if (!bPanelPlaced || FVector::Dist(ViewLocation, MenuPlacedFrom) > 150.0)
		{
			const float Desired = CVarTNVRMenuDistance.GetValueOnGameThread();
			const float Distance = FitDistance(ViewLocation, FRotator(0.0, ViewRotation.Yaw, 0.0).Vector(), Desired);
			PlacePanel(ViewLocation, static_cast<float>(ViewRotation.Yaw), Distance, CVarTNVRMenuFov.GetValueOnGameThread());
			MenuPlacedFrom = ViewLocation;
			HudYaw = static_cast<float>(ViewRotation.Yaw);
			PanelDistanceSmoothed = Distance;
			bPanelPlaced = true;
		}
		return;
	}

	// HUD: delante de los ojos. Con gafas sigue a la cabeza con retraso (se lee mirando de reojo); simulado, fijo en la
	// pantalla como el HUD de siempre.
	if (!bPanelPlaced)
	{
		HudYaw = static_cast<float>(ViewRotation.Yaw);
		PanelDistanceSmoothed = CVarTNVRHudDistance.GetValueOnGameThread();
		bHudFollowing = false;
		bPanelPlaced = true;
	}
	if (Mode == ETNVRMode::Headset)
	{
		HudYaw = TNVRMath::LazyFollowYaw(HudYaw, static_cast<float>(ViewRotation.Yaw), DeltaSeconds, bHudFollowing);
	}
	else
	{
		HudYaw = static_cast<float>(ViewRotation.Yaw);
	}
	const FRotator HudRotation(Mode == ETNVRMode::Headset ? 0.0 : ViewRotation.Pitch, HudYaw, 0.0);
	const float Wanted = FitDistance(ViewLocation, HudRotation.Vector(), CVarTNVRHudDistance.GetValueOnGameThread());
	// Se acerca de golpe (una pared) y se aleja poco a poco.
	PanelDistanceSmoothed = Wanted < PanelDistanceSmoothed ? Wanted : FMath::FInterpTo(PanelDistanceSmoothed, Wanted, DeltaSeconds, 4.f);
	if (Mode == ETNVRMode::Headset)
	{
		PlacePanel(ViewLocation, HudYaw, PanelDistanceSmoothed, CVarTNVRHudFov.GetValueOnGameThread());
	}
	else
	{
		// Simulado: el panel pegado a la vista (también con la inclinación de la cámara).
		const FVector Location = ViewLocation + HudRotation.Vector() * PanelDistanceSmoothed;
		ScreenPanel->SetWorldLocationAndRotation(Location, (ViewLocation - Location).Rotation());
		ScreenPanel->SetWorldScale3D(FVector(TNVRMath::PanelScale(PanelDistanceSmoothed, CVarTNVRHudFov.GetValueOnGameThread(), UTN_VRScreenWidget::ScreenWidth)));
	}
}

void ATN_VRRig::RecenterPanel()
{
	bPanelPlaced = false;
	bHudFollowing = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Puntero
// ─────────────────────────────────────────────────────────────────────────────

bool ATN_VRRig::GetPointerRay(APlayerController* PC, FVector& OutOrigin, FVector& OutDir) const
{
	if (Mode == ETNVRMode::Headset)
	{
		UMotionControllerComponent* Source = RightAim->IsTracked() ? RightAim.Get() : RightGrip.Get();
		if (!Source->IsTracked())
		{
			return false;
		}
		OutOrigin = Source->GetComponentLocation();
		OutDir = Source->GetForwardVector();
		return true;
	}
	// Simulado: el ratón (el juego enseña el cursor con el menú).
	return PC && PC->DeprojectMousePositionToWorld(OutOrigin, OutDir);
}

void ATN_VRRig::UpdatePointer(APlayerController* PC)
{
	const bool bHeadset = Mode == ETNVRMode::Headset;
	bool bHit = false;
	FVector HitPoint = FVector::ZeroVector;
	FVector Origin = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	bool bHasRay = false;
	if (bMenuMode && ScreenPanel->IsVisible())
	{
		bHasRay = GetPointerRay(PC, Origin, Dir);
		FVector2D UV;
		bHit = bHasRay && TNVRMath::RayPanelHit(Origin, Dir, ScreenPanel->GetComponentTransform(),
			FVector2D(UTN_VRScreenWidget::ScreenWidth, UTN_VRScreenWidget::ScreenHeight), HitPoint, UV);
	}
	if (bHit)
	{
		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.Location = HitPoint;
		Hit.ImpactPoint = HitPoint;
		Hit.Normal = ScreenPanel->GetForwardVector();
		Hit.ImpactNormal = Hit.Normal;
		Hit.TraceStart = Origin;
		Hit.TraceEnd = Origin + Dir * Pointer->InteractionDistance;
		Hit.Distance = static_cast<float>(FVector::Dist(Origin, HitPoint));
		Hit.Component = ScreenPanel.Get();
		Hit.HitObjectHandle = FActorInstanceHandle(this);
		Pointer->SetCustomHitResult(Hit);
	}
	else
	{
		Pointer->SetCustomHitResult(FHitResult());
	}

	// El láser, solo con gafas (simulado apunta el cursor del ratón).
	const bool bShowLaser = bHeadset && bMenuMode && bHasRay;
	LaserBeam->SetVisibility(bShowLaser);
	LaserDot->SetVisibility(bShowLaser && bHit);
	if (bShowLaser)
	{
		const FVector End = bHit ? HitPoint : Origin + Dir * 300.0;
		const double Length = FMath::Max(1.0, FVector::Dist(Origin, End));
		LaserBeam->SetWorldLocationAndRotation((Origin + End) * 0.5, FRotationMatrix::MakeFromZ(End - Origin).Rotator());
		// El cilindro básico mide 100 × 100: 0,5 cm de grueso y el largo del rayo.
		LaserBeam->SetWorldScale3D(FVector(0.005, 0.005, Length / 100.0));
		if (bHit)
		{
			LaserDot->SetWorldLocation(HitPoint);
			LaserDot->SetWorldScale3D(FVector(bPointerDown ? 0.03 : 0.022));
		}
	}
}

void ATN_VRRig::PointerPress()
{
	if (!bMenuMode || bPointerDown)
	{
		return;
	}
	bPointerDown = true;
	Pointer->PressPointerKey(EKeys::LeftMouseButton);
}

void ATN_VRRig::PointerRelease()
{
	if (!bPointerDown)
	{
		return;
	}
	bPointerDown = false;
	Pointer->ReleasePointerKey(EKeys::LeftMouseButton);
}

void ATN_VRRig::PointerScroll(float Delta)
{
	if (bMenuMode)
	{
		Pointer->ScrollWheel(Delta);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Pantalla
// ─────────────────────────────────────────────────────────────────────────────

bool ATN_VRRig::HostWidget(UUserWidget* Widget, int32 ZOrder)
{
	EnsureScreen();
	return Screen && Screen->Host(Widget, ZOrder);
}

bool ATN_VRRig::IsHosting(const UUserWidget* Widget) const
{
	return Screen && Screen->IsHosting(Widget);
}

bool ATN_VRRig::HostSlate(const TSharedRef<SWidget>& Widget, int32 ZOrder)
{
	EnsureScreen();
	return Screen && Screen->HostSlate(Widget, ZOrder);
}

bool ATN_VRRig::UnhostSlate(const TSharedRef<SWidget>& Widget)
{
	return Screen && Screen->UnhostSlate(Widget);
}

void ATN_VRRig::ReleaseScreenToViewport()
{
	if (Screen)
	{
		Screen->ReleaseAll(true);
	}
}

USceneComponent* ATN_VRRig::GetHand(bool bRight) const
{
	return bRight ? RightHand.Get() : LeftHand.Get();
}
