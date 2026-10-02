#include "Rally/UI/TN_RallyCopilotTablet.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Rally/TN_RallyGameState.h"
#include "Rally/TN_RallyTrack.h"
#include "UObject/ObjectKey.h"
#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyGunnerPawn.h"
#include "Vehicles/TN_BuggyMath.h"
#include "Vehicles/TN_BuggyTurretComponent.h"
#include "VR/TN_VRMode.h"

namespace TNRallyTabletState
{
	/** Por encima del HUD del Rally y por debajo de los menús. */
	constexpr int32 ViewportZOrder = 20;
	/** Si el buggy queda a más de esto del arco que se seguía (reaparición), se busca en toda la pista (cm). */
	constexpr double ArcLostDistanceCm = 4000.0;

	/** Tableta de cada jugador local. Débil: la mantiene viva el viewport, no este registro. */
	TMap<TObjectKey<APlayerController>, TWeakObjectPtr<UTN_RallyCopilotTablet>>& Registry()
	{
		static TMap<TObjectKey<APlayerController>, TWeakObjectPtr<UTN_RallyCopilotTablet>> Map;
		return Map;
	}

	void PruneRegistry()
	{
		for (auto It = Registry().CreateIterator(); It; ++It)
		{
			if (!It.Value().IsValid() || !It.Key().ResolveObjectPtr()) { It.RemoveCurrent(); }
		}
	}
}

// ── Acceso por jugador local ───────────────────────────────────────────────────────────────────────────────────

UTN_RallyCopilotTablet* UTN_RallyCopilotTablet::FindFor(const APlayerController* Player)
{
	if (!Player)
	{
		return nullptr;
	}
	const TWeakObjectPtr<UTN_RallyCopilotTablet>* Found = TNRallyTabletState::Registry().Find(TObjectKey<APlayerController>(Player));
	return Found ? Found->Get() : nullptr;
}

UTN_RallyCopilotTablet* UTN_RallyCopilotTablet::FindOrCreateFor(APlayerController* Player)
{
	if (!Player || !Player->IsLocalController())
	{
		return nullptr;
	}
	if (UTN_RallyCopilotTablet* Existing = FindFor(Player))
	{
		return Existing;
	}
	UTN_RallyCopilotTablet* Tablet = CreateWidget<UTN_RallyCopilotTablet>(Player, UTN_RallyCopilotTablet::StaticClass());
	if (!Tablet)
	{
		return nullptr;
	}
	// En VR va al panel del mundo, como el resto de la interfaz.
	TNVR::AddToScreen(Tablet, TNRallyTabletState::ViewportZOrder);
	TNRallyTabletState::PruneRegistry();
	TNRallyTabletState::Registry().Add(TObjectKey<APlayerController>(Player), Tablet);
	return Tablet;
}

bool UTN_RallyCopilotTablet::ToggleFor(APlayerController* Player)
{
	UTN_RallyCopilotTablet* Tablet = FindOrCreateFor(Player);
	if (!Tablet)
	{
		return false;
	}
	Tablet->ToggleTablet();
	return Tablet->IsTabletOpen();
}

bool UTN_RallyCopilotTablet::IsOpenFor(const APlayerController* Player)
{
	const UTN_RallyCopilotTablet* Tablet = FindFor(Player);
	return Tablet && Tablet->IsTabletOpen();
}

void UTN_RallyCopilotTablet::BindToggleKeys(UInputComponent* Input, APawn* Pawn)
{
	if (!Input || !Pawn)
	{
		return;
	}
	const TWeakObjectPtr<APawn> WeakPawn(Pawn);
	// Teclas fijas mientras no haya acción de Enhanced Input para la tableta (UTN_BuggyInputSet).
	for (const FKey& Key : { EKeys::Tab, EKeys::M, EKeys::Gamepad_Special_Left })
	{
		FInputKeyBinding Binding{ FInputChord(Key), IE_Pressed };
		Binding.bConsumeInput = true;
		Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([WeakPawn]()
		{
			if (const APawn* Owner = WeakPawn.Get())
			{
				ToggleFor(Cast<APlayerController>(Owner->GetController()));
			}
		});
		Input->KeyBindings.Add(MoveTemp(Binding));
	}
}

FTNRallyTabletAmmo UTN_RallyCopilotTablet::ReadAmmo(const UTN_BuggyTurretComponent* Turret)
{
	FTNRallyTabletAmmo Result;
	if (!Turret)
	{
		return Result;
	}
	Result.Special = Turret->GetSpecialAmmo();
	Result.SpecialCharges = Turret->GetSpecialCharges();
	Result.Heat01 = Turret->GetHeat01();
	Result.bOverheated = Turret->IsOverheated();
	// La que dispara el botón principal (la cambian la rueda y las crucetas: UTN_BuggyTurretComponent::CycleAmmo).
	Result.Selected = Turret->GetSelectedAmmo();
	return Result;
}

// ── Estado ─────────────────────────────────────────────────────────────────────────────────────────────────────

void UTN_RallyCopilotTablet::SetTabletOpen(bool bInOpen)
{
	bOpen = bInOpen;
}

void UTN_RallyCopilotTablet::ToggleTablet()
{
	SetTabletOpen(!bOpen);
}

bool UTN_RallyCopilotTablet::IsTabletOpen() const
{
	return bOpen && !bCompact;
}

void UTN_RallyCopilotTablet::SetCompactMode(bool bInCompact)
{
	bAutoRole = false;
	bCompact = bInCompact;
}

void UTN_RallyCopilotTablet::SetAutoRole(bool bInAutoRole)
{
	bAutoRole = bInAutoRole;
}

FText UTN_RallyCopilotTablet::GetNextNoteText() const
{
	return Ahead.Num() > 0 ? TNRallyPaceNotes::NoteText(Ahead[0].Note) : FText::GetEmpty();
}

void UTN_RallyCopilotTablet::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Solo pinta: no coge el ratón, el teclado ni el mando.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTN_RallyCopilotTablet::NativeDestruct()
{
	const APlayerController* Player = GetOwningPlayer();
	const TWeakObjectPtr<UTN_RallyCopilotTablet>* Found = Player
		? TNRallyTabletState::Registry().Find(TObjectKey<APlayerController>(Player)) : nullptr;
	if (Found && Found->Get() == this)
	{
		TNRallyTabletState::Registry().Remove(TObjectKey<APlayerController>(Player));
	}
	Super::NativeDestruct();
}

void UTN_RallyCopilotTablet::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	const UWorld* World = GetWorld();
	const ATN_RallyGameState* RallyState = World ? World->GetGameState<ATN_RallyGameState>() : nullptr;
	if (RallyState)
	{
		RefreshTrack(*RallyState);
	}
	RefreshView(RallyState);
	if (View == ETNRallyTabletView::Hidden || !RallyState)
	{
		Ahead.Reset();
		BoxesAhead.Reset();
		return;
	}
	RefreshMarks(*RallyState);
	if (const ATN_Buggy* Buggy = FindLocalBuggy(RallyState))
	{
		RefreshProgress(*Buggy);
		Ammo = ReadAmmo(Buggy->GetTurret());
	}
	Ahead = bHasArc ? TNRallyPaceNotes::NotesAhead(TrackNotes, MyArcCm, LookAheadCm) : TArray<TNRallyPaceNotes::FNoteAhead>();
	BoxesAhead = bHasArc ? TNRallyPaceNotes::ArcsAhead(AmmoRowNoteArcs, TrackNotes.LengthCm, TrackNotes.bClosed, MyArcCm, LookAheadCm)
		: TArray<double>();
}

// ── Refresco ───────────────────────────────────────────────────────────────────────────────────────────────────

const FTNRallyStanding* UTN_RallyCopilotTablet::FindLocalStanding(const ATN_RallyGameState* RallyState) const
{
	const APlayerController* Player = GetOwningPlayer();
	return (RallyState && Player) ? RallyState->FindStandingForPlayer(Player->PlayerState) : nullptr;
}

const ATN_Buggy* UTN_RallyCopilotTablet::FindLocalBuggy(const ATN_RallyGameState* RallyState) const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	if (const ATN_Buggy* Driven = Cast<ATN_Buggy>(Pawn))
	{
		return Driven;
	}
	if (const ATN_BuggyGunnerPawn* Gunner = Cast<ATN_BuggyGunnerPawn>(Pawn))
	{
		return Gunner->GetBuggy();
	}
	// Sin peón propio (reaparición, cambio de plaza): el buggy de su fila de puestos.
	const FTNRallyStanding* Mine = FindLocalStanding(RallyState);
	return Mine ? Cast<ATN_Buggy>(Mine->Vehicle) : nullptr;
}

void UTN_RallyCopilotTablet::RefreshView(const ATN_RallyGameState* RallyState)
{
	const bool bUsable = RallyState && TrackNotes.IsValid() && RallyState->Phase != ETNRallyPhase::Results;
	if (!bAutoRole)
	{
		View = !bUsable ? ETNRallyTabletView::Hidden
			: (bCompact ? ETNRallyTabletView::Compact : (bOpen ? ETNRallyTabletView::Full : ETNRallyTabletView::Hidden));
		return;
	}
	const APawn* Pawn = GetOwningPlayerPawn();
	const bool bGunner = Cast<ATN_BuggyGunnerPawn>(Pawn) != nullptr;
	const ATN_Buggy* Driven = Cast<ATN_Buggy>(Pawn);
	const FTNRallyStanding* Mine = FindLocalStanding(RallyState);
	// Conductora sola: su fila no tiene artillera (o, sin fila todavía, el buggy no lleva peón de artillera).
	const bool bDriverAlone = Driven && (Mine ? Mine->Gunner == nullptr : Driven->GetGunnerPawn() == nullptr);
	bCompact = !bGunner;
	if (!bGunner)
	{
		// Al dejar la plaza de artillera se guarda: al volver a ella empieza cerrada.
		bOpen = false;
	}
	if (!bUsable)
	{
		View = ETNRallyTabletView::Hidden;
		return;
	}
	View = bGunner ? (bOpen ? ETNRallyTabletView::Full : ETNRallyTabletView::Hidden)
		: (bDriverAlone ? ETNRallyTabletView::Compact : ETNRallyTabletView::Hidden);
}

void UTN_RallyCopilotTablet::RefreshTrack(const ATN_RallyGameState& RallyState)
{
	const ATN_RallyTrack* Track = RallyState.GetTrack();
	if (!Track || !Track->IsBuilt())
	{
		if (TrackNotes.IsValid() || NotesTrack.IsValid())
		{
			TrackNotes = TNRallyPaceNotes::FTrackNotes();
			AmmoRowNoteArcs.Reset();
			NotesTrack.Reset();
			bHasArc = false;
		}
		return;
	}
	const float Length = Track->GetTrackLengthCm();
	if (NotesTrack.Get() == Track && FMath::IsNearlyEqual(Length, NotesTrackLengthCm, 1.f) && TrackNotes.IsValid())
	{
		return;
	}
	// Una vez por pista (o al reconstruirla): unos pocos miles de muestras de la spline.
	TrackNotes = TNRallyPaceNotes::BuildForTrack(*Track);
	NotesTrack = Track;
	NotesTrackLengthCm = Length;
	bHasArc = false;
	RefreshMapBounds();
	// Las filas de cajas, en el eje de las notas (una polilínea de la spline: su longitud difiere unas milésimas).
	AmmoRowNoteArcs.Reset();
	const double Scale = Length > 0.f ? TrackNotes.LengthCm / Length : 1.0;
	for (const double Arc : Track->GetAmmoRowArcs())
	{
		AmmoRowNoteArcs.Add(Arc * Scale);
	}
}

void UTN_RallyCopilotTablet::RefreshMapBounds()
{
	MapBounds = FBox2D(ForceInit);
	for (const FVector& Point : TrackNotes.Points)
	{
		MapBounds += FVector2D(Point.X, Point.Y);
	}
}

void UTN_RallyCopilotTablet::RefreshMarks(const ATN_RallyGameState& RallyState)
{
	Marks.Reset();
	const FTNRallyStanding* Mine = FindLocalStanding(&RallyState);
	for (const FTNRallyStanding& Entry : RallyState.Standings)
	{
		if (!Entry.Vehicle)
		{
			continue;
		}
		FTNRallyTabletMark Mark;
		Mark.Location = Entry.Vehicle->GetActorLocation();
		Mark.Color = TNBuggy::TeamColor(Entry.TeamIndex);
		Mark.Place = Entry.Place;
		Mark.bMine = Mine && Mine->TeamIndex == Entry.TeamIndex;
		Mark.bOut = Entry.bRetired || Entry.bFinished;
		if (Mark.bMine)
		{
			MyColor = Mark.Color;
		}
		Marks.Add(Mark);
	}
}

void UTN_RallyCopilotTablet::RefreshProgress(const ATN_Buggy& Buggy)
{
	const ATN_RallyTrack* Track = NotesTrack.Get();
	if (!Track || NotesTrackLengthCm <= 0.f)
	{
		bHasArc = false;
		return;
	}
	const FVector Location = Buggy.GetActorLocation();
	double Arc = bHasArc ? Track->FindArcNear(Location, TrackArcCm) : Track->FindArcGlobal(Location);
	if (FVector::Dist2D(Track->GetLocationAtArc(Arc), Location) > TNRallyTabletState::ArcLostDistanceCm)
	{
		// Reaparición o atajo: la ventana de FindArcNear ya no lo encuentra.
		Arc = Track->FindArcGlobal(Location);
	}
	TrackArcCm = Arc;
	bHasArc = true;
	// El eje de las notas es una polilínea de la spline: su longitud difiere unas milésimas.
	MyArcCm = TrackArcCm * (TrackNotes.LengthCm / NotesTrackLengthCm);
}
