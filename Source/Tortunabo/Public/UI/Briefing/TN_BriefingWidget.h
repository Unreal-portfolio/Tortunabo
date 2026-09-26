#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_BriefingWidget.generated.h"

class ATN_GeneralBriefing;
class UScrollBox;
class UTextBlock;
class UTN_ShopButton;
class UVerticalBox;

/**
 * Sesión informativa del General Galápago (ATN_GeneralBriefing), con el estilo de la tienda: a la izquierda el general
 * hablando en su bocadillo (letra a letra) y a la derecha cuatro pestañas con su contenido:
 *  - «Cómo se juega»: objetivo, moverse, panzazo, nado, caparazón, llevar y lanzar, peligros y huevos.
 *  - «Modos de juego»: Cooperativo, Carrera, 2 vs 2 y Clásico, y los selectores de modo y dificultad.
 *  - «Reglas»: salida, reaparición, tiempo límite, coger a rivales y juego limpio.
 *  - «Controles»: cada acción con sus teclas reales, leídas de Enhanced Input (teclado y mando).
 * Q/E, Tab o las flechas cambian de pestaña; arriba/abajo desplazan; Escape o «¡Entendido!» cierran.
 */
UCLASS()
class TORTUNABO_API UTN_BriefingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetGeneral(ATN_GeneralBriefing* InGeneral);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Scroll;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Page;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTN_ShopButton>> Tabs;

	TWeakObjectPtr<ATN_GeneralBriefing> General;
	int32 Tab = 0;
	FString FullLine;
	float Reveal = 0.f;

	void BuildTree();
	void ShowTab(int32 Index);
	void Say(const FText& Line);
	void Close();

	void AddHeading(const FText& Text);
	void AddParagraph(const FText& Text);
	/** Fila de «Controles»: acción y sus teclas (las de teclado en arena, las de mando en azul). */
	void AddControlRow(const FText& ActionName, const TArray<FString>& ActionPaths);

	/** Teclas de teclado y ratón (y de mando aparte) asignadas ahora a una acción de Enhanced Input. */
	void KeysFor(const TArray<FString>& ActionPaths, TArray<FString>& OutKeyboard, TArray<FString>& OutGamepad) const;
};
