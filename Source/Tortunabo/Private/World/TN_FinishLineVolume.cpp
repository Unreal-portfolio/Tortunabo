#include "World/TN_FinishLineVolume.h"
#include "Game/TN_RunGameMode.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ATN_FinishLineVolume::ATN_FinishLineVolume()
{
	// Tick habilitado como red de seguridad: si el chunk spawneó alrededor
	// del jugador, OnBeginOverlap no dispara. El tick lo detecta cada 0.5s.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval  = 0.5f;

	bReplicates = false; // solo existe en el servidor

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	TriggerBox->ShapeColor = FColor::Green;
	TriggerBox->SetLineThickness(2.f);
#endif

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_FinishLineVolume::OnBoxBeginOverlap);
}

void ATN_FinishLineVolume::BeginPlay()
{
	Super::BeginPlay();

	// Deshabilitar tick en clientes — la detección de meta es solo servidor.
	// El Tick (0.5s) maneja el caso en que el chunk spawnea alrededor del jugador,
	// usando GetOverlappingActors. NO usar UpdateOverlaps() aquí: llamarlo en BeginPlay
	// dispara OnBoxBeginOverlap sincrónicamente si el jugador ya está dentro del volumen,
	// lo que causa un MarkPlayerFinished prematuro (el jugador pasa a espectador antes
	// de cruzar visualmente la meta, y al llegar a ella bHasFinishedRun=true la bloquea).
	if (!HasAuthority() || !TriggerBox)
	{
		SetActorTickEnabled(false);
	}
}

void ATN_FinishLineVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || !TriggerBox)
	{
		return;
	}

	ATN_RunGameMode* RunGM = ResolveRunGameMode();
	if (!RunGM)
	{
		return;
	}

	// Red de seguridad: consultar todos los pawns dentro del box cada 0.5s.
	// MarkPlayerFinished tiene sus propios guards (bHasFinishedRun, bIsAlive)
	// por lo que llamarlo varias veces es completamente seguro.
	TArray<AActor*> Overlapping;
	TriggerBox->GetOverlappingActors(Overlapping, APawn::StaticClass());

	for (AActor* Actor : Overlapping)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn) { continue; }

		APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
		if (!PC) { continue; }

		UE_LOG(LogTemp, Log, TEXT("[FinishLine] Tick: '%s' dentro de '%s' → MarkPlayerFinished."),
			*GetNameSafe(PC), *GetName());

		RunGM->MarkPlayerFinished(PC);
	}
}

void ATN_FinishLineVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FinishLine] '%s' — Pawn '%s' sin PlayerController. "
			"¿Pawn no poseído? Si ya terminó/murió su colisión debería estar desactivada."),
			*GetName(), *GetNameSafe(Pawn));
		return;
	}

	ATN_RunGameMode* RunGM = ResolveRunGameMode();
	if (!RunGM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FinishLine] '%s' — ResolveRunGameMode() null. "
			"¿BP_RunGameMode asignado en WorldSettings?"), *GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[FinishLine] '%s' — OnBeginOverlap: ¡META! → MarkPlayerFinished('%s')."),
		*GetName(), *GetNameSafe(PC));

	RunGM->MarkPlayerFinished(PC);
}

ATN_RunGameMode* ATN_FinishLineVolume::ResolveRunGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATN_RunGameMode>() : nullptr;
}

