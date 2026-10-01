// Arte del buggy del Rally hecho en C++ (#297): carrocería de tortuga por piezas (caparazón de placas, cabeza con ojos
// que hacen de faros, aletas por guardabarros, cola de escape, alerón...), ruedas, cañón y antena, para los tres
// modelos de TNBuggyCosmetics. Mallas de caras planas (el estilo low poly del juego) construidas una vez por pieza y
// compartidas por todos los buggies; las pinta M_BuggyPaint (Scripts/build_buggy_paint.py) con los parámetros de la
// pintura. Cada pieza tiene un nombre estable (PieceName) para poder sustituirla por arte más adelante.
#pragma once

#include "CoreMinimal.h"
#include "Vehicles/TN_BuggyCosmetics.h"
#include "../World/ProcMap/TN_ProcMapMeshKit.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

namespace TNBuggyArt
{
	/** Piezas: las de carrocería van colgadas del chasis; Wheel, Cannon y Antenna, en sus propios componentes. */
	enum class EPiece : uint8
	{
		Chassis,  // bajos, suspensión y pilotos traseros
		Cockpit,  // bañera, asiento, timón, salpicadero, caja de cocos y sillín de la artillera
		Shell,    // caparazón con sus placas
		Head,     // cuello, cabeza, ojos-faro y boca
		Fenders,  // las cuatro aletas que hacen de guardabarros
		Tail,     // cola de escape
		Rear,     // alerón de vieira, rueda de repuesto o alerón de carreras
		Extras,   // parachoques, barra de luces, tubo de buceo, escapes laterales...
		Wheel,
		Cannon,
		Antenna,
		/** Poste del cañón sobre el sillín: solo se ve sin artillera (la conductora sola dispara con él). */
		TurretPost,
		Count
	};

	/** Piezas de carrocería (de Chassis a Extras, en este orden). */
	constexpr int32 NumBodyPieces = static_cast<int32>(EPiece::Wheel);

	/** Nombre estable de la pieza para el catálogo de arte (#319): «Rally.Buggy.<Pieza>.<Modelo>». */
	FName PieceName(ETNBuggyBodyStyle Style, EPiece Piece);

	/**
	 * Códigos de zona en el alfa del color de vértice, en octavos (los lee M_BuggyPaint): pintura (el RGB son las
	 * máscaras de carrocería, placas y piel, con su sombreado), pintura sin dibujo (llantas, franja del cañón), color del
	 * equipo, luz (faros y pilotos: brillan con LightGlow), metal y mate (el RGB es el color lineal).
	 */
	namespace Zone
	{
		constexpr float Matte = 0.f;
		constexpr float Metal = 0.25f;
		constexpr float Light = 0.5f;
		constexpr float Team = 0.75f;
		constexpr float PaintPlain = 0.875f;
		constexpr float Paint = 1.f;
	}

	/** Medidas del chasis SKM_Offroad (espacio del chasis, cm): ruedas y plazas. ATN_Buggy usa las mismas. */
	namespace Frame
	{
		const FVector FrontWheel(168.3, 124.1, 51.1);
		const FVector RearWheel(-135.2, 139.8, 50.8);
		constexpr double WheelRadius = 51.0;
		constexpr double WheelWidth = 35.0;
		const FVector DriverSeat(25.0, -35.0, 67.0);
		const FVector GunnerSeat(-80.0, 0.0, 127.4);
		/** Pivote de la torreta (asiento + 70) y largo del cañón: la carrocería deja libre su barrido. */
		constexpr double TurretPivotZ = 197.4;
		constexpr double CannonLength = 140.0;
		/** Cabeceo mínimo del cañón (grados, negativo hacia abajo). */
		constexpr double CannonMinPitchDeg = -10.0;
		constexpr double CannonRadius = 12.5;
	}

	/** Buffers de una pieza (pura: sin UObjects; la usan los tests). Las ruedas y el cañón, en su espacio local. */
	TNProcMesh::FTNProcMeshBuffers BuildPiece(ETNBuggyBodyStyle Style, EPiece Piece);

	/** Dónde va la antena en el chasis (su pie). */
	FVector AntennaMount(ETNBuggyBodyStyle Style);

	/** Sustituye los códigos de zona por los colores de la pintura (M_CosmeticVertexColor, si falta M_BuggyPaint). */
	void BakePaint(TNProcMesh::FTNProcMeshBuffers& Buffers, const FTNBuggyPaintInfo& Paint, const FLinearColor& TeamColor);

	/** M_BuggyPaint (null si el asset no existe). */
	UMaterialInterface* PaintMaterial();

	/**
	 * Malla de la pieza, construida una vez y compartida (fuera del recolector). Con BakePaint, la versión con los colores
	 * horneados para M_CosmeticVertexColor. Null si la pieza no tiene nada en ese modelo.
	 */
	UStaticMesh* GetPieceMesh(ETNBuggyBodyStyle Style, EPiece Piece, const FTNBuggyPaintInfo* BakedPaint = nullptr);

	/** Escribe la pintura y el color del equipo en una instancia de M_BuggyPaint. */
	void ApplyPaint(UMaterialInstanceDynamic* MID, const FTNBuggyPaintInfo& Paint, const FLinearColor& TeamColor);
}
