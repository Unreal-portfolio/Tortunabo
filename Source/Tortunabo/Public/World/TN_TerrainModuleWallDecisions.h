#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "World/TN_GridJunkDecisions.h"
#include "World/TN_GridPathDecisions.h"
#include "World/TN_GridTerrainDecisions.h"

/**
 * Muros de basura que tapan las bocas de un módulo que no se usan: funciones PURAS.
 *
 * Una boca es la abertura canónica del borde (suelo a cota 0 en |t| < 18 m, talud hasta
 * la cresta de 10 m). El muro es un montón de piezas de basura (cubos y cilindros) que
 * llena la boca por dentro del borde, más una caja de colisión invisible que garantiza
 * que no hay paso aunque el montón deje huecos. Todo en espacio LOCAL del módulo,
 * determinista por semilla: servidor y clientes construyen el mismo montón.
 */
namespace TNTerrainModuleWall
{
	/** Semiancho de la boca canónica, en uu. Debe casar con OPEN_HALF_M del generador. */
	constexpr double MouthHalfWidth = 1200.0;
	/** Cota de la cresta canónica, en uu. */
	constexpr double CrestHeight = 1000.0;
	/** Desde dónde hasta dónde, hacia dentro del borde, se apila la basura. */
	constexpr double HeapNear = 300.0;
	constexpr double HeapFar = 1400.0;
	constexpr int32 PiecesPerWall = 110;

	struct FWallPiece
	{
		TNGridJunk::EJunkShape Shape = TNGridJunk::EJunkShape::Cube;
		FTransform Transform;
		FLinearColor Color = FLinearColor::White;
	};

	/** Caja de colisión invisible: centro y semiextensión, en espacio local. */
	struct FWallBlocker
	{
		FVector Center = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
	};

	/** Dirección hacia fuera del lado, en espacio local (Norte = +X, Este = +Y). */
	inline FVector2D SideDirection(int32 Side)
	{
		const FIntPoint Step = TNGridLogic::StepForSide(Side);
		// StepForSide devuelve (columna, fila) = (Y, X) del grid.
		return FVector2D(Step.Y, Step.X);
	}

	inline FWallBlocker BuildWallBlocker(int32 Side, double ModuleSize)
	{
		const FVector2D Out = SideDirection(Side);
		const FVector2D Center2D = Out * (ModuleSize * 0.5 - (HeapNear + HeapFar) * 0.5);
		FWallBlocker Blocker;
		Blocker.Center = FVector(Center2D.X, Center2D.Y, CrestHeight * 0.6);
		// Semiextensión en el marco del lado: X = profundidad, Y = ancho de la boca con margen.
		Blocker.Extent = FVector((HeapFar - HeapNear) * 0.5, MouthHalfWidth + 200.0, CrestHeight * 0.6);
		Blocker.Rotation = FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X)), 0.f);
		return Blocker;
	}

	/**
	 * Piezas del montón de un lado. La altura máxima cae hacia los extremos de la boca,
	 * donde el talud canónico ya sube: el montón se lee como basura acumulada contra
	 * las paredes de la boca.
	 */
	inline TArray<FWallPiece> BuildWallPieces(int32 Side, double ModuleSize, int32 Seed)
	{
		TArray<FWallPiece> Pieces;
		Pieces.Reserve(PiecesPerWall);
		FRandomStream Stream(Seed * 7919 + Side * 104729);

		const FVector2D Out = SideDirection(Side);
		const FVector2D Lateral(-Out.Y, Out.X);
		const FVector2D EdgeMid = Out * (ModuleSize * 0.5);

		for (int32 Index = 0; Index < PiecesPerWall; ++Index)
		{
			const double T = Stream.FRandRange(-1.f, 1.f);
			const double Depth = Stream.FRandRange(static_cast<float>(HeapNear), static_cast<float>(HeapFar));
			const double MaxHeight = CrestHeight * (1.05 - 0.75 * T * T);
			const double Z = Stream.FRandRange(0.f, static_cast<float>(MaxHeight));
			const FVector2D P = EdgeMid - Out * Depth + Lateral * (T * MouthHalfWidth);

			FWallPiece Piece;
			Piece.Shape = (Stream.FRand() < 0.25f) ? TNGridJunk::EJunkShape::Cylinder : TNGridJunk::EJunkShape::Cube;
			const FVector Scale(Stream.FRandRange(1.2f, 3.2f), Stream.FRandRange(1.2f, 3.2f), Stream.FRandRange(0.8f, 2.6f));
			const FRotator Rotation(Stream.FRandRange(-25.f, 25.f), Stream.FRandRange(0.f, 360.f), Stream.FRandRange(-25.f, 25.f));
			Piece.Transform = FTransform(Rotation, FVector(P.X, P.Y, Z), Scale);
			Piece.Color = TNGridTerrain::JunkPaletteColor(Stream.RandRange(0, TNGridTerrain::JunkPaletteSize - 1));
			Pieces.Add(Piece);
		}
		return Pieces;
	}
}
