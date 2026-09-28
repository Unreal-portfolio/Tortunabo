#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "TN_GhostHatchWidget.generated.h"

class APlayerController;
class UCanvasPanel;
class UTN_EggSynthComponent;

/** Grieta de la cáscara oscura: polilínea desde la unión hacia dentro de una mitad (0-1 de la pantalla). */
struct FTNGhostHatchCrack
{
	TArray<FVector2f> Points;
	bool bTop = true;
	/** Golpe (1-3) con el que aparece. */
	int32 Knock = 1;
};

/** Trozo de cáscara que sale despedido al abrirse. */
struct FTNGhostHatchShard
{
	FVector2f Origin = FVector2f::ZeroVector;
	FVector2f Velocity = FVector2f::ZeroVector;
	float Spin = 0.f;
	float Size = 30.f;
	int32 Shape = 0;
};

/**
 * Transición de pantalla del que vuelve a la vida (TNGhost::ReviveIntoEgg, Docs/Fantasma_Espectador.md), solo en la
 * suya: mientras su fantasma se mete en el huevo la pantalla se pone negra; ¡pum! y una cáscara de huevo oscura tapa la
 * pantalla entera; con cada «pum» del huevo se resquebraja con líneas de luz por el medio; al eclosionar (y en cuanto ya
 * tiene su tortuga) las dos mitades salen despedidas entre trozos de cáscara y un fogonazo, y se ve a su tortuga saliendo
 * del huevo. Pintado en código (como la pantalla de carga del huevo, de la que reutiliza el blanco, los trozos de
 * cáscara y los sonidos sintetizados: crujidos, «¡pum!» y el soplido de las mitades).
 */
UCLASS()
class TORTUNABO_API UTN_GhostHatchWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Enseña la transición en la pantalla del jugador local PC. SecondsToDark: hasta que el fantasma entra en el huevo
	 * (todo negro y ¡pum!); SecondsToHatch: hasta que eclosiona (se abre en cuanto además ya tiene su tortuga).
	 */
	static void ShowFor(APlayerController* PC, float SecondsToDark, float SecondsToHatch);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void Begin(float SecondsToDark, float SecondsToHatch);
	void Finish();
	float Elapsed() const;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Root;

	UPROPERTY(Transient)
	TObjectPtr<UTN_EggSynthComponent> Synth;

	/** Reloj de la aplicación (FPlatformTime) al empezar, y los tiempos desde ahí. */
	double StartTime = 0.0;
	float DarkAt = 1.f;
	float HatchAt = 2.f;
	/** Cuándo se abrió (< 0: todavía no) y los golpes que ya han sonado. */
	float OpenAt = -1.f;
	int32 KnocksDone = 0;
	TArray<float> KnockTimes;
	bool bSlamDone = false;
	bool bFinished = false;

	/** Línea de unión (0-1 de la pantalla, en zigzag), grietas y trozos (con una semilla por transición). */
	TArray<FVector2f> Seam;
	TArray<FTNGhostHatchCrack> Cracks;
	TArray<FTNGhostHatchShard> Shards;
	TArray<FVector2f> Speckles;

	FSlateBrush WhiteBrush;
	FSlateBrush ShardBrushes[3];
	FSlateFontInfo PumFont;
};
