// ATN_RallyPlayerController: avisos entre el servidor y las ocupantes de un buggy (confirmación de impactos, #332).

#include "Rally/TN_RallyPlayerController.h"
#include "Core/TN_LocText.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Rally/TN_RallyGameState.h"
#include "Rally/TN_RallyHUDWidget.h"
#include "Rally/TN_RallyLogic.h"
#include "Rally/UI/TN_RallyCopilotTablet.h"
#include "Sound/SoundBase.h"
#include "Vehicles/TN_BuggyMath.h"

namespace TNRallyCrewDetail
{
	/** Tono del sonido de acierto: más agudo si cuenta, más grave si lo para el escudo. */
	constexpr float HitPitch = 1.35f;
	constexpr float BlockedPitch = 0.8f;
	/** Color de las líneas del registro sin equipo conocido y de las que recibe el buggy propio. */
	const FLinearColor UnknownColor(0.9f, 0.9f, 0.9f, 1.f);
	const FLinearColor IncomingColor(1.f, 0.55f, 0.45f, 1.f);

	/** Nombre del otro equipo (su conductora o, si no tiene, su artillera) y su color; vacío si no se encuentra. */
	void DescribeOther(const UWorld* World, const APawn* Other, FText& OutName, FLinearColor& OutColor)
	{
		OutName = FText::GetEmpty();
		OutColor = UnknownColor;
		const ATN_RallyGameState* RallyState = World ? World->GetGameState<ATN_RallyGameState>() : nullptr;
		const FTNRallyStanding* Standing = RallyState && Other ? RallyState->FindStandingForVehicle(Other) : nullptr;
		if (!Standing)
		{
			return;
		}
		// Con piloto IA, el nombre de la artillera humana.
		const bool bHumanDriver = Standing->Driver && !Standing->Driver->IsABot();
		const APlayerState* Named = (bHumanDriver || !Standing->Gunner) ? Standing->Driver.Get() : Standing->Gunner.Get();
		if (Named)
		{
			OutName = TNLocText::PlayerName(Named->GetPlayerName());
		}
		OutColor = TNBuggy::TeamColor(Standing->TeamIndex);
	}
}

void ATN_RallyPlayerController::ClientRallyHitReport_Implementation(const FTNRallyHitReport& Report)
{
	using namespace TNRallyCrewDetail;
	FText OtherName;
	FLinearColor OtherColor;
	DescribeOther(GetWorld(), Report.OtherVehicle, OtherName, OtherColor);
	const FText Line = TNRallyHitLog::LineFor(Report, OtherName);
	UE_LOG(LogTNRally, Log, TEXT("[RallyHit] %s: %s"), *GetNameSafe(this), *Line.ToString());
	if (UTN_RallyCopilotTablet* Tablet = UTN_RallyCopilotTablet::FindFor(this))
	{
		Tablet->AddHitLine(Line, Report.bOutgoing ? OtherColor : IncomingColor);
	}
	if (!TNRallyHitLog::ShowsMarker(Report))
	{
		return;
	}
	if (RallyHUD)
	{
		RallyHUD->ShowHitMarker(Report.bBlocked);
	}
	if (HitConfirmSound)
	{
		UGameplayStatics::PlaySound2D(this, HitConfirmSound, HitConfirmVolume, Report.bBlocked ? BlockedPitch : HitPitch);
	}
}
