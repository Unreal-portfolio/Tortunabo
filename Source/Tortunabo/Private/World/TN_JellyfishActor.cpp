#include "World/TN_JellyfishActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Player/TortugaCharacter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ATN_JellyfishActor::ATN_JellyfishActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false); // La posición se sincroniza una sola vez

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(Root);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Trigger de rebote: caja fina sobre la parte superior de la esfera.
	// Por defecto centrado en Z=0 del actor; en el BP se mueve para quedar encima del HeadMesh.
	BounceZone = CreateDefaultSubobject<UBoxComponent>(TEXT("BounceZone"));
	BounceZone->SetupAttachment(Root);
	BounceZone->SetBoxExtent(FVector(80.f, 80.f, 15.f));
	BounceZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BounceZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	BounceZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ATN_JellyfishActor::BeginPlay()
{
	Super::BeginPlay();

	// Guardar escala inicial del HeadMesh para la animación squish
	HeadMeshDefaultScale = HeadMesh->GetRelativeScale3D();

	if (HasAuthority())
	{
		BounceZone->OnComponentBeginOverlap.AddDynamic(
			this, &ATN_JellyfishActor::OnBounceZoneBeginOverlap);

		// Capturar posición inicial un tick después (patrón chunk: ChildActorComponent
		// puede no haber terminado de posicionar en este mismo tick).
		GetWorldTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ATN_JellyfishActor::DeferredCaptureInitialLocation));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — animación squish (local en todas las máquinas)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_JellyfishActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── Cooldowns de rebote (solo servidor) ──────────────────────────────────
	if (HasAuthority() && PlayerCooldowns.Num() > 0)
	{
		TArray<TWeakObjectPtr<ATortugaCharacter>> ToRemove;
		for (auto& Pair : PlayerCooldowns)
		{
			Pair.Value -= DeltaTime;
			if (Pair.Value <= 0.f)
			{
				ToRemove.Add(Pair.Key);
			}
		}
		for (const auto& Key : ToRemove)
		{
			PlayerCooldowns.Remove(Key);
		}
	}

	// ── Animación squish (local) ──────────────────────────────────────────────
	if (bSquishingIn)
	{
		SquishAlpha += DeltaTime / FMath::Max(SquishInDuration, 0.001f);
		if (SquishAlpha >= 1.f)
		{
			SquishAlpha = 1.f;
			bSquishingIn = false;
			bSquishingOut = true;
		}
		ApplySquishScale();
	}
	else if (bSquishingOut)
	{
		SquishAlpha -= DeltaTime / FMath::Max(SquishOutDuration, 0.001f);
		if (SquishAlpha <= 0.f)
		{
			SquishAlpha = 0.f;
			bSquishingOut = false;
			// Restaurar escala exacta para no acumular error de float
			HeadMesh->SetRelativeScale3D(HeadMeshDefaultScale);
		}
		ApplySquishScale();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplySquishScale — aplana la esfera en Z, expande en XY
// ─────────────────────────────────────────────────────────────────────────────

void ATN_JellyfishActor::ApplySquishScale() const
{
	// SquishAlpha 0→1: Z se reduce a SquishZScale; XY crecen para conservar volumen
	const float ZScale = FMath::Lerp(1.f, SquishZScale, SquishAlpha);

	// Expansión XY: sqrt(1/ZScale) conserva volumen de forma esférica aproximada
	const float XYScale = FMath::Lerp(1.f, FMath::Sqrt(1.f / FMath::Max(SquishZScale, 0.01f)), SquishAlpha);

	HeadMesh->SetRelativeScale3D(HeadMeshDefaultScale * FVector(XYScale, XYScale, ZScale));
}

// ─────────────────────────────────────────────────────────────────────────────
// Replicación de posición (mismo patrón que SeagullActor)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_JellyfishActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_JellyfishActor, InitialLocation);
}

void ATN_JellyfishActor::DeferredCaptureInitialLocation()
{
	InitialLocation = GetActorLocation();
	bPositionSynced = true; // Servidor: posición correcta; clientes reciben vía OnRep_InitialLocation
}

void ATN_JellyfishActor::OnRep_InitialLocation()
{
	SetActorLocation(InitialLocation);
	bPositionSynced = true;
}


// ─────────────────────────────────────────────────────────────────────────────
// Overlap — detección del rebote (solo servidor)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_JellyfishActor::OnBounceZoneBeginOverlap(
	UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(OtherActor);
	if (!TurtleChar)
	{
		return;
	}

	// Solo si el jugador llega desde arriba (Z negativa = cayendo/andando)
	if (TurtleChar->GetVelocity().Z > 50.f)
	{
		return;
	}

	// Cooldown por jugador para no disparar en cada tick mientras está encima
	const TWeakObjectPtr<ATortugaCharacter> WeakChar(TurtleChar);
	if (PlayerCooldowns.Contains(WeakChar))
	{
		return;
	}
	PlayerCooldowns.Add(WeakChar, BounceCooldown);

	ApplyBounce(TurtleChar);
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyBounce — servidor aplica el impulso
// ─────────────────────────────────────────────────────────────────────────────

void ATN_JellyfishActor::ApplyBounce(ATortugaCharacter* TurtleChar)
{
	if (!TurtleChar)
	{
		return;
	}

	// bXYOverride=false mantiene velocidad horizontal del jugador.
	// bZOverride=true SUSTITUYE la Z → siempre MaxBounceVelocity, sin acumulación.
	TurtleChar->LaunchCharacter(FVector(0.f, 0.f, MaxBounceVelocity), false, true);

	// Posición de los pies del jugador para sonido y VFX
	const FVector EffectLocation = TurtleChar->GetActorLocation();

	MulticastPlayBounceEffects(EffectLocation);
}

// ─────────────────────────────────────────────────────────────────────────────
// MulticastPlayBounceEffects — sonido + VFX + squish (todos los clientes)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_JellyfishActor::MulticastPlayBounceEffects_Implementation(FVector EffectLocation)
{
	// ── Sonido ──────────────────────────────────────────────────────────────────
	if (BounceSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BounceSound, EffectLocation);
	}

	// ── VFX (Niagara) ────────────────────────────────────────────────────────────
	if (BounceVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), BounceVFX, EffectLocation,
			FRotator::ZeroRotator,
			FVector::OneVector,
			true,   // auto destroy
			true);  // auto activate
	}

	// ── Squish: reiniciar desde el principio si ya estaba animando ───────────────
	SquishAlpha = 0.f;
	bSquishingIn = true;
	bSquishingOut = false;
}
