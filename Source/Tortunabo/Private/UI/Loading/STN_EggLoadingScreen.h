#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

class UTexture2D;

/**
 * Arte de la pantalla de carga del huevo, dibujado en código con las funciones de distancia de TNHUDArt (sin texturas
 * externas). Cada imagen se genera una vez (hilo de juego; UTN_LoadingScreenSubsystem lo hace al arrancar) y queda en
 * la raíz del recolector: así también la puede pintar el hilo de carga de MoviePlayer durante un LoadMap bloqueante.
 */
namespace TNEggLoadingArt
{
	/**
	 * Superficie de la cáscara: crema con un moteado suave, motas turquesa, coral, doradas y lila y pintitas de cáscara,
	 * en un lienzo de 2,4:1 que se ajusta cubriendo la pantalla. El volumen (sombreado, sombra junto a la unión) va por
	 * vértice, a la medida de cada pantalla.
	 */
	UTexture2D* ShellSurface();
	/** Blanco liso: rellenos que se colorean con los vértices (tinta, filo de la cáscara, luz de la rendija, fogonazo). */
	UTexture2D* White();
	/** Brillo de la cáscara: media luna blanca que se apaga hacia la punta. */
	UTexture2D* Glint();
	/** Tortuga de perfil andando hacia la derecha: 4 colores de caparazón y 2 fotogramas de patas. */
	UTexture2D* Turtle(int32 ColorIndex, int32 Frame);
	/** Punto blando (polvo del cierre y brillito). */
	UTexture2D* Dot();
	/** Trozo de cáscara con contorno y filo claro (NumShardShapes formas). */
	UTexture2D* Shard(int32 Shape);
	/** Estrella del «¡PUM!». */
	UTexture2D* Burst();

	/** Genera todo de una vez (hilo de juego). */
	void Warm();

	constexpr int32 NumTurtles = 4;
	constexpr int32 NumShardShapes = 3;
}

/**
 * Tiempos del huevo (segundos de FPlatformTime). Las dos mitades se mueven con un grado de cierre K (0 = fuera de la
 * pantalla, 1 = cerradas); Close y Open lo animan desde donde esté, así que un cierre puede darse la vuelta a medias.
 */
struct FTNEggTimeline
{
	/** Origen de las animaciones de fondo (tortugas, puntos, consejos): la copia de MoviePlayer usa el mismo. */
	double Origin = 0.0;
	/** Movimiento en curso: empieza en MoveStart desde el cierre MoveFrom, hacia cerrado (bClosing) o hacia abierto. */
	double MoveStart = 0.0;
	float MoveFrom = 0.f;
	bool bClosing = true;
	/** Cuenta los movimientos: el subsistema hace sonar un «¡clac!» por cada cierre. */
	int32 MoveSerial = 0;
	/** Momento en que empezó a romperse (< 0 si no se está rompiendo). */
	double BreakTime = -1.0;

	/** Cierre completo (desde fuera de la pantalla), apertura completa y golpe del cierre hasta quedarse quieto. */
	static constexpr float CloseSeconds = 0.5f;
	static constexpr float OpenSeconds = 0.55f;
	static constexpr float SettleSeconds = 0.3f;
	/** Rotura: tiembla y se agrieta, «¡pum!» en PopAt y las mitades salen de la pantalla antes de BreakEnd. */
	static constexpr float PopAt = 1.05f;
	static constexpr float BreakEnd = 1.9f;

	float MoveDuration() const
	{
		return FMath::Max(0.05f, bClosing ? CloseSeconds * (1.f - MoveFrom) : OpenSeconds * MoveFrom);
	}

	/** Grado de cierre: al cerrarse acelera (las mitades se estampan); al abrirse, también (salen disparadas). */
	float Closedness(double Now) const
	{
		const float Progress = FMath::Clamp(static_cast<float>(Now - MoveStart) / MoveDuration(), 0.f, 1.f);
		return bClosing ? MoveFrom + (1.f - MoveFrom) * Progress * Progress : MoveFrom * (1.f - Progress * Progress);
	}

	double ImpactTime() const { return MoveStart + MoveDuration(); }
	bool IsClosedAt(double Now) const { return bClosing && Now >= ImpactTime(); }
	bool IsSettledAt(double Now) const { return bClosing && Now >= ImpactTime() + SettleSeconds; }
	bool IsOpenedAt(double Now) const { return !bClosing && Now >= ImpactTime(); }
	float BreakElapsed(double Now) const { return BreakTime < 0.0 ? -1.f : static_cast<float>(Now - BreakTime); }
};

/**
 * Pantalla de carga de Tortunavy: el huevo ES la pantalla. Dos mitades de cáscara (crema con motas, unión en zigzag
 * irregular, sombreado suave, brillo y el grosor del filo) entran desde arriba y desde abajo, se estampan con un
 * «¡clac!» y tapan la pantalla entera. Encima de la cáscara van el nombre del juego (mitad de arriba), el estado con
 * puntos animados, un consejo y cuatro tortugas andando (mitad de abajo). Al romperse tiembla cada vez más, las grietas
 * salen de la unión, se abre una rendija de luz y, con el «¡PUM!», las mitades salen despedidas hacia arriba y hacia
 * abajo entre trozos de cáscara. También se puede abrir sin romperse (se retiran por donde vinieron).
 *
 * Todo se pinta en OnPaint a partir de FPlatformTime (también en el hilo de carga de MoviePlayer): las mitades son
 * mallas propias (MakeCustomVerts) con la superficie ajustada «cubriendo», así que se ve igual en 16:9, 16:10, 21:9 y
 * 4:3. La línea de unión, las grietas y los trozos salen de Seed: la copia de MoviePlayer, con la misma semilla y el
 * mismo origen de tiempos, pinta exactamente el mismo huevo.
 */
class STN_EggLoadingScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STN_EggLoadingScreen)
		: _StartClosed(false)
		, _Seed(0u)
		, _TimeOrigin(-1.0)
	{}
		/** Empieza ya cerrado del todo (sin la entrada de las dos mitades). */
		SLATE_ARGUMENT(bool, StartClosed)
		SLATE_ARGUMENT(FText, Status)
		/** Semilla de la línea de unión, las grietas y los trozos (0 = una al azar). */
		SLATE_ARGUMENT(uint32, Seed)
		/** Origen de las animaciones de fondo (< 0 = ahora). */
		SLATE_ARGUMENT(double, TimeOrigin)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D(64.0, 64.0); }

	/** Texto de estado (sin puntos suspensivos: con bInAnimateDots se animan solos). */
	void SetStatus(const FText& InStatus, bool bInAnimateDots = true);
	const FText& GetStatus() const { return Status; }

	/** Se cierra desde donde esté, con su animación. */
	void Close();
	/** Cerrado del todo ya, sin animación ni golpe (antes de un LoadMap, que no repinta nada). */
	void SnapClosed();
	/** Se abre sin romperse: las mitades se retiran por donde vinieron. */
	void Open();
	/** Tiembla, se agrieta y revienta. */
	void StartBreak();

	bool IsBreaking() const { return Timeline.BreakTime >= 0.0; }
	float GetBreakElapsed() const { return Timeline.BreakElapsed(FPlatformTime::Seconds()); }
	bool IsBreakFinished() const { return IsBreaking() && GetBreakElapsed() >= FTNEggTimeline::BreakEnd; }
	bool IsOpening() const { return !IsBreaking() && !Timeline.bClosing; }
	bool IsOpenFinished() const { return IsOpening() && Timeline.IsOpenedAt(FPlatformTime::Seconds()); }
	/** Las mitades se tocan. */
	bool IsClosed() const { return !IsBreaking() && Timeline.IsClosedAt(FPlatformTime::Seconds()); }
	/** Cerrado y ya quieto tras el golpe: nada del mundo se ve y se puede viajar o romper. */
	bool IsSettled() const { return !IsBreaking() && Timeline.IsSettledAt(FPlatformTime::Seconds()); }
	/** El cierre en curso viene de abierto (con golpe y «¡clac!»), no de un cierre en seco. */
	bool ClosesWithImpact() const { return Timeline.bClosing && Timeline.MoveFrom < 0.95f; }
	double GetImpactTime() const { return Timeline.ImpactTime(); }
	int32 GetMoveSerial() const { return Timeline.MoveSerial; }
	/** Segundos (desde que empieza a romperse) en que arranca cada grieta principal: los crujidos van con ellas. */
	const TArray<float>& GetCrackStartTimes() const { return CrackStartTimes; }
	uint32 GetSeed() const { return Seed; }
	double GetTimeOrigin() const { return Timeline.Origin; }

protected:
	/** Se anima en cada fotograma. */
	virtual bool ComputeVolatility() const override { return true; }

private:
	/** Postura de una mitad: desplazamiento y giro alrededor de Pivot (normalizado en la pantalla). */
	struct FHalfPose
	{
		FVector2f Offset = FVector2f::ZeroVector;
		float Angle = 0.f;
		FVector2f Pivot = FVector2f(0.5f, 0.5f);
	};

	struct FPoses
	{
		FHalfPose Top;
		FHalfPose Bottom;
		/** Temblor común (sin la rendija): la luz de dentro y el polvo del cierre. */
		FHalfPose Shake;
		/** Rendija entre las mitades justo antes del «¡pum!». */
		float Gap = 0.f;
	};

	/** Grieta: polilínea desde la unión hacia dentro de su mitad (coordenadas de la pantalla con el huevo cerrado). */
	struct FCrackPath
	{
		TArray<FVector2f> Points;
		TArray<float> Along;
		float Start = 0.f;
		float Duration = 0.3f;
		bool bTop = true;
	};

	struct FShardSpec
	{
		FVector2f Origin = FVector2f::ZeroVector;
		FVector2f Velocity = FVector2f::ZeroVector;
		float Spin = 0.f;
		float Angle = 0.f;
		float Size = 0.f;
		int32 Shape = 0;
	};

	struct FPuffSpec
	{
		FVector2f Origin = FVector2f::ZeroVector;
		float Drift = 0.f;
		float Size = 0.f;
		float Delay = 0.f;
	};

	/** Todo lo que depende del tamaño de la pantalla y de la semilla (se recalcula solo si cambia el tamaño). */
	struct FEggLayout
	{
		float Sw = 0.f;
		float Sh = 0.f;
		TArray<FVector2f> Seam;
		TArray<FVector2f> RimTop;
		TArray<FVector2f> RimBottom;
		float SeamMinY = 0.f;
		float SeamMaxY = 0.f;
		TArray<FCrackPath> Cracks;
		TArray<FShardSpec> Shards;
		TArray<FPuffSpec> Puffs;
		/** Ajuste «cubrir» de la superficie: esquina y tamaño del lienzo en coordenadas locales. */
		FVector2f SurfaceOrigin = FVector2f::ZeroVector;
		FVector2f SurfaceSize = FVector2f(1.f, 1.f);
	};

	void RefreshLayout(float Sw, float Sh) const;
	float SeamYAt(float X) const;
	void ComputePoses(double Now, float Sw, float Sh, FPoses& OutPoses) const;
	FString GetStatusString(double Now) const;
	FString GetTipString(double Now) const;

	FTNEggTimeline Timeline;
	uint32 Seed = 1u;
	FText Status;
	bool bAnimateDots = true;
	TArray<float> CrackStartTimes;

	FSlateBrush SurfaceBrush;
	FSlateBrush WhiteBrush;
	FSlateBrush GlintBrush;
	FSlateBrush DotBrush;
	FSlateBrush BurstBrush;
	FSlateBrush ShardBrushes[TNEggLoadingArt::NumShardShapes];
	FSlateBrush TurtleBrushes[TNEggLoadingArt::NumTurtles][2];

	FSlateFontInfo TitleFont;
	FSlateFontInfo TitleShadowFont;
	FSlateFontInfo StatusFont;
	FSlateFontInfo TipFont;
	FSlateFontInfo PumFont;

	mutable FEggLayout Layout;
};
