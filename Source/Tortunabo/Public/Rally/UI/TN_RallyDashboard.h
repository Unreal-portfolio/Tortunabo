// Interfaz diegética de la conductora del Rally (#299): la conductora no tiene HUD de pantalla salvo los avisos (semáforo,
// contramano, reaparición y resultados); la velocidad, el turbo y la vida van en el salpicadero del buggy, y el puesto y la
// vuelta en un cartel del arco antivuelco. Son dos widgets en el mundo (UWidgetComponent) que solo existen en la máquina de
// las ocupantes (no se replican): los pone ATN_RallyPlayerController en el buggy local. Medidas pensadas para leerse a
// 1080p con la cámara de persecución (TNRallyDashboard::ProjectedGlyphPx, tests Tortunabo.Rally.Dashboard.*).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SceneComponent.h"
#include "TN_RallyDashboard.generated.h"

class APlayerController;
class ATN_Buggy;
class UProgressBar;
class UTextBlock;
class UWidgetComponent;
struct FTNRallyStanding;

/** Qué panel del buggy pinta un UTN_RallyDashboardWidget. */
UENUM()
enum class ETNRallyDashboardPanel : uint8
{
	/** Salpicadero: velocidad, turbo y vida del buggy. */
	Dash,
	/** Cartel del arco antivuelco: puesto y vuelta (o puerta, o el tiempo de meta). */
	RollBar
};

namespace TNRallyDashboard
{
	/** Medidas de un panel en el buggy, relativas al asiento de la conductora (ATN_Buggy::DriverSeatLocal). */
	struct FPanelLayout
	{
		FVector OffsetFromDriverSeat = FVector::ZeroVector;
		/** Yaw 180: el panel mira hacia atrás (hacia la cámara de persecución y la conductora); el pitch lo inclina hacia arriba. */
		FRotator Rotation = FRotator(0.0, 180.0, 0.0);
		FIntPoint DrawSizePx = FIntPoint(480, 240);
		/** Centímetros del mundo por píxel del widget. */
		float CmPerPx = 0.25f;
		/** Tamaño de la fuente del dato principal (velocidad o puesto), en píxeles del widget. */
		int32 MainFontPx = 120;
	};

	TORTUNABO_API FPanelLayout DashLayout();
	TORTUNABO_API FPanelLayout RollBarLayout();

	/** Alto de las mayúsculas y las cifras respecto al tamaño de la fuente (Roboto: ~0,71). */
	inline constexpr float CapHeightRatio = 0.71f;

	/** Alto en el mundo (cm) de las cifras del dato principal de un panel. */
	TORTUNABO_API float MainGlyphCm(const FPanelLayout& Layout);

	/**
	 * Alto en pantalla (px) de un objeto de GlyphCm a DistanceCm de la cámara, con el FOV horizontal dado y una pantalla de
	 * ScreenSize (relación de aspecto incluida).
	 */
	TORTUNABO_API float ProjectedGlyphPx(float GlyphCm, float DistanceCm, float HorizontalFovDeg, const FIntPoint& ScreenSize);

	/** Línea de vuelta del cartel: «¡Meta! 1:52.3», «Vuelta 2/3 · Puerta 4/9» o «Puerta 4/9». */
	TORTUNABO_API FText LapLine(const FTNRallyStanding& Mine, int32 Laps, int32 NumGates, bool bCircuit);

	/** «2.º / 6». */
	TORTUNABO_API FText PlaceLine(int32 Place, int32 Total);

	/** Tiempo de carrera «m:ss.d». */
	TORTUNABO_API FText RaceTime(float Seconds);
}

/** Un panel del buggy (C++ sin asset UMG): lee el buggy y su fila de puestos (estado replicado). */
UCLASS()
class TORTUNABO_API UTN_RallyDashboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(ATN_Buggy* InBuggy, ETNRallyDashboardPanel InPanel);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDash();
	void BuildRollBar();
	void RefreshDash(const ATN_Buggy& Buggy);
	void RefreshRollBar(const ATN_Buggy& Buggy);

	TWeakObjectPtr<ATN_Buggy> Buggy;
	ETNRallyDashboardPanel Panel = ETNRallyDashboardPanel::Dash;
	float RefreshAccumulator = 1.f;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> MainText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> BoostBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
};

/** Los dos paneles en el buggy local. Solo en la máquina de las ocupantes: nunca en el servidor dedicado ni en los demás. */
UCLASS(Transient)
class TORTUNABO_API UTN_RallyDashboardComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTN_RallyDashboardComponent();

	/**
	 * Pone los paneles en Buggy (si no los tiene ya) para el jugador local Player. Nullptr si Player no es local, la
	 * máquina no pinta o falta algo.
	 */
	static UTN_RallyDashboardComponent* AttachTo(ATN_Buggy* Buggy, APlayerController* Player);

	/** Los paneles de este buggy en esta máquina (nullptr si no tiene). */
	static UTN_RallyDashboardComponent* FindOn(const ATN_Buggy* Buggy);

protected:
	virtual void OnUnregister() override;

private:
	UWidgetComponent* MakePanel(ATN_Buggy& Buggy, APlayerController& Player, ETNRallyDashboardPanel Kind,
		const TNRallyDashboard::FPanelLayout& Layout);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> Panels;
};
