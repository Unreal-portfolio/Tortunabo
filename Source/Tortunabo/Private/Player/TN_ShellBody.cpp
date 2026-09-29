#include "Player/TN_ShellBody.h"

#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PhysicsVolume.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Player/TN_ShellComponent.h"
#include "Player/TortugaCharacter.h"

namespace TNShellBodyDetail
{
	/** Parada: por debajo de esta velocidad (cm/s) y este giro (rad/s) durante RestSeconds. */
	constexpr float RestLinearSpeed = 60.f;
	constexpr float RestAngularSpeed = 1.5f;
	constexpr float RestSeconds = 0.35f;
	/** Si sigue rodando o resbalando tanto tiempo, sale igual (no se queda atrapada en el caparazón). */
	constexpr float MaxSecondsBeforeExit = 9.f;
	/** Cada cuánto mira si ha caído al agua (s). */
	constexpr float WaterCheckInterval = 0.1f;

	/**
	 * Giro máximo de la caja (grados/s, unas 2,5 vueltas por segundo). Una caja que rueda no necesita más; lo que pasaba de
	 * ahí era un rebote contra una arista o una corrección de red, y se veía como una bola que se vuelve loca.
	 */
	constexpr float MaxSpinDegrees = 900.f;

	/**
	 * Velocidad máxima (cm/s) con la que se separa de lo que ya solapaba al nacer o al recolocarla (la ponen dentro de algo):
	 * sin tope, la física la escupía a varios metros por segundo.
	 */
	constexpr float MaxInitialDepenetration = 300.f;

	TAutoConsoleVariable<int32> CVarShellPhysicsRep(TEXT("TN.Shell.PhysicsRep"), 1,
		TEXT("Réplica de la física de la bola del caparazón en los clientes (se aplica a las bolas nuevas): 1 = interpolación predictiva (de serie: corrige con velocidad hacia el estado del servidor extrapolado, sin tirones); 0 = la de siempre del motor (fijaba cada paso la velocidad y el 40 % del giro hacia un estado ya viejo: la bola temblaba y rebotaba en los clientes)."));
}

ATN_ShellBody::ATN_ShellBody()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// Después de la física: la tortuga se coloca donde la caja ha quedado en este fotograma.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->InitBoxExtent(BoxHalfExtent());
	Box->SetCollisionProfileName(TEXT("PhysicsActor"));
	// La cámara no choca con el caparazón (ni con el propio ni con el de los demás).
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Box->SetSimulatePhysics(true);
	Box->SetEnableGravity(true);
	Box->SetUseCCD(true);
	Box->SetNotifyRigidBodyCollision(false);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCanEverAffectNavigation(false);
	Box->CanCharacterStepUpOn = ECB_No;
	Box->BodyInstance.SetMassOverride(38.f, true);
	// La lineal no se toca: la tormenta calcula sus patadas con ella (ATN_BeachStorm, BallisticLaunch).
	Box->BodyInstance.LinearDamping = 0.25f;
	Box->BodyInstance.AngularDamping = 1.4f;
	// Estable sobre el terreno de la playa: sus teselas son mallas distintas y la caja tropezaba en cada costura y en cada
	// arista interior (saltitos y vueltas sin motivo). Caro, pero son como mucho ocho bolas.
	Box->BodyInstance.bSmoothEdgeCollisions = true;
	Box->BodyInstance.bOverrideMaxAngularVelocity = true;
	Box->BodyInstance.MaxAngularVelocity = TNShellBodyDetail::MaxSpinDegrees;

	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(10.f);
	// En los clientes la caja también simula (choca con lo de su máquina) y la réplica la lleva al estado del servidor. La de
	// siempre del motor fijaba en cada paso la velocidad y el 40 % del giro hacia un estado de hace ~50-100 ms: una caja que
	// rueda temblaba, rebotaba contra el suelo y daba tirones en la cámara de su dueño. La interpolación predictiva extrapola
	// el estado del servidor y corrige con velocidad (TN.Shell.PhysicsRep 0 vuelve a la de siempre para comparar).
	SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);
	// Relevante justo cuando lo es su tortuga (es su dueña): si no, un cliente podría ver a la tortuga sin su caja.
	bNetUseOwnerRelevancy = true;
}

void ATN_ShellBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ShellBody, Turtle);
}

void ATN_ShellBody::InitBody(ATortugaCharacter* InTurtle, bool bInExitOnRest)
{
	Turtle = InTurtle;
	bExitOnRest = bInExitOnRest;
	Age = 0.f;
	RestTime = 0.f;
	ForceNetUpdate();
}

void ATN_ShellBody::BeginPlay()
{
	Super::BeginPlay();

	// Material resbaladizo propio: el caparazón patina por las cuestas y rebota un poco. Al cambiar los valores en
	// ejecución hay que refrescar el material de Chaos.
	Slippery = NewObject<UPhysicalMaterial>(this, TEXT("ShellSlippery"));
	Slippery->Friction = 0.25f;
	// Rebota algo menos que antes (0,35): contra el decorado pequeño y los bordes de las teselas botaba sin parar.
	Slippery->Restitution = 0.2f;
	FPhysicsInterface::UpdateMaterial(Slippery->GetPhysicsMaterial(), Slippery);
	Box->SetPhysMaterialOverride(Slippery);
	// Si nace (o la recolocan) metida en algo, sale despacio en vez de salir disparada.
	Box->BodyInstance.SetMaxDepenetrationVelocity(TNShellBodyDetail::MaxInitialDepenetration);
	if (TNShellBodyDetail::CVarShellPhysicsRep.GetValueOnGameThread() == 0)
	{
		SetPhysicsReplicationMode(EPhysicsReplicationMode::Default);
	}

	// En los clientes la caja puede llegar antes o después que la referencia del componente de caparazón: el que
	// llegue segundo engancha a la tortuga.
	if (!HasAuthority() && Turtle)
	{
		if (UTN_ShellComponent* Shell = Turtle->GetShellComponent())
		{
			Shell->AdoptBody(this);
		}
	}
}

void ATN_ShellBody::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ATortugaCharacter* OwnerTurtle = Turtle;
	if (!IsValid(OwnerTurtle))
	{
		// La tortuga se ha ido (jugador que sale): el caparazón no se queda suelto por el mapa.
		if (HasAuthority() && Age > 0.5f)
		{
			bReleased = true;
			Destroy();
		}
		Age += DeltaSeconds;
		return;
	}
	Age += DeltaSeconds;

	UTN_ShellComponent* Shell = OwnerTurtle->GetShellComponent();
	if (!Shell)
	{
		return;
	}
	// Todas las máquinas: la tortuga sigue a la caja (solo si es su caja y la tiene enganchada en esta máquina).
	Shell->FollowBody(this);

	if (HasAuthority())
	{
		ServerChecks(DeltaSeconds);
	}
}

void ATN_ShellBody::ServerChecks(float DeltaSeconds)
{
	using namespace TNShellBodyDetail;
	ATortugaCharacter* OwnerTurtle = Turtle;
	UTN_ShellComponent* Shell = OwnerTurtle ? OwnerTurtle->GetShellComponent() : nullptr;
	if (bReleased || !Shell || Shell->GetBody() != this || !Box)
	{
		return;
	}

	// Al agua: sale del caparazón y nada.
	WaterCheckTimer -= DeltaSeconds;
	if (WaterCheckTimer <= 0.f)
	{
		WaterCheckTimer = WaterCheckInterval;
		if (IsInWater(*Box))
		{
			Shell->NotifyBodyInWater();
			return;
		}
	}

	if (!bExitOnRest)
	{
		return;
	}

	// Lanzada (o caída de altura, o escapada): rueda y rebota con la física y, cuando se para, sale.
	const float Linear = static_cast<float>(Box->GetPhysicsLinearVelocity().Size());
	const float Angular = static_cast<float>(Box->GetPhysicsAngularVelocityInRadians().Size());
	const bool bSlow = Linear < RestLinearSpeed && Angular < RestAngularSpeed;
	RestTime = bSlow ? RestTime + DeltaSeconds : 0.f;
	if ((RestTime >= RestSeconds && Age > 0.3f) || Age > MaxSecondsBeforeExit)
	{
		Shell->NotifyBodyAtRest();
	}
}

void ATN_ShellBody::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Destruida sin pasar por el componente (caída fuera del mundo, por ejemplo): la tortuga no puede quedarse sin
	// movimiento. En los clientes, si era la caja enganchada, se suelta igual.
	if (EndPlayReason == EEndPlayReason::Destroyed && !bReleased)
	{
		if (ATortugaCharacter* OwnerTurtle = Turtle)
		{
			if (UTN_ShellComponent* Shell = OwnerTurtle->GetShellComponent())
			{
				Shell->NotifyBodyEnded(this);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

FTransform ATN_ShellBody::MeshWorldTransform(const FTransform& BoxWorld, const FVector& MeshScale)
{
	// Malla tumbada sobre la tripa: su +Z (la cabeza) va a +X de la caja, su +Y (hacia donde mira, la tripa) abajo y su
	// izquierda (+X) a -Y. El centro del tronco (0; 0,25; 27 en unidades de malla) cae en el centro de la caja.
	static const FQuat LyingRotation(FMatrix(
		FPlane(0.0, -1.0, 0.0, 0.0),
		FPlane(0.0, 0.0, -1.0, 0.0),
		FPlane(1.0, 0.0, 0.0, 0.0),
		FPlane(0.0, 0.0, 0.0, 1.0)));
	const FVector Origin(-27.0 * MeshScale.Z, 0.0, 0.25 * MeshScale.Y);
	const FTransform MeshLocal(LyingRotation, Origin, MeshScale);
	return MeshLocal * FTransform(BoxWorld.GetRotation(), BoxWorld.GetLocation());
}

bool ATN_ShellBody::IsInWater(const UPrimitiveComponent& Component)
{
	const UWorld* World = Component.GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<APhysicsVolume> It(World); It; ++It)
	{
		const APhysicsVolume* Volume = *It;
		if (Volume && Volume->bWaterVolume && Volume->IsOverlapInVolume(Component))
		{
			return true;
		}
	}
	return false;
}
