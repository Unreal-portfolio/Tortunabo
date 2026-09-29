#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "InputCoreTypes.h"

class ATN_VRRig;
class UTN_VRSubsystem;

/**
 * Entrada de los menús en VR, antes que nadie (preprocesador de Slate). Solo actúa con un menú delante (el panel VR en
 * modo menú); jugando no toca nada y los mandos van a Enhanced Input como siempre.
 *
 * - Gatillos: clic del puntero láser sobre el panel.
 * - A/X aceptar, B/Y atrás, agarre izquierdo/derecho = pestaña anterior/siguiente, menú = Start, sticks = cruceta (con
 *   repetición al mantener): se mandan como las teclas de mando que ya entienden todos los menús, al usuario que los tiene
 *   enfocados. Se comen las pulsaciones; los «soltar» pasan (así el juego no se queda con una tecla apretada).
 * - Simulado (sin gafas): el clic izquierdo y la rueda del ratón van al puntero, que sigue al ratón.
 */
class FTNVRInputProcessor : public IInputProcessor
{
public:
	explicit FTNVRInputProcessor(UTN_VRSubsystem* InOwner);

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("TNVRInput"); }

private:
	ATN_VRRig* GetRig() const;
	bool IsMenuUp() const;
	void SendKey(FSlateApplication& SlateApp, const FKey& Key, bool bDown, bool bRepeat) const;
	void TapKey(FSlateApplication& SlateApp, const FKey& Key) const;

	TWeakObjectPtr<UTN_VRSubsystem> Owner;

	/** Botón VR pulsado → tecla de mando mandada (para mandar su «soltar»). */
	TMap<FKey, FKey> Held;

	/** Sticks (0 izquierdo, 1 derecho), su dirección de menú y la espera hasta repetirla. */
	FVector2D Sticks[2] = { FVector2D::ZeroVector, FVector2D::ZeroVector };
	int32 StickDir[2] = { 0, 0 };
	float StickRepeat[2] = { 0.f, 0.f };

	/** Si el motor manda las direcciones del stick como botones, se usan esas y no las del eje (sin pasos dobles). */
	double LastDigitalStickTime = -100.0;

	uint32 UserIndex = 0;
	bool bMousePointerDown = false;
};
