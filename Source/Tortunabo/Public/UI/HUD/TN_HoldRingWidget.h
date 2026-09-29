#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_HoldRingWidget.generated.h"

/**
 * @brief Aro de progreso de las interacciones de mantener la tecla (rebuscar un decorado), en el estilo Tortunavy:
 * una pista azul marino translúcida con el filo crema y el relleno dorado que va de arriba en el sentido de las agujas
 * del reloj. Va alrededor de la tecla del aviso de interacción (UTN_RunHUDWidget). Se pinta en código (NativePaint),
 * sin texturas ni materiales; su tamaño lo fija SetRingSize (tamaño mínimo deseado).
 */
UCLASS()
class TORTUNABO_API UTN_HoldRingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Progreso [0..1] (se recorta). */
	void SetProgress(float InProgress);

	/** Diámetro exterior del aro (px de diseño) y grosor del relleno. */
	void SetRingSize(float InDiameter, float InThickness);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float Progress = 0.f;
	float RingDiameter = 70.f;
	float RingThickness = 6.f;
};
