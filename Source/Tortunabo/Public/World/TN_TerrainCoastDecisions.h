#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "World/TN_GridPathDecisions.h"
#include "World/TN_TerrainModuleAsset.h"
#include "World/TN_TerrainModuleDecisions.h"
#include "World/TN_TerrainModuleWallDecisions.h"

/**
 * Costa exterior de los módulos como funciones PURAS.
 *
 * Un lado de módulo que da fuera del mapa (celda sin módulo) no necesita pared: el
 * terreno de ese lado se hunde bajo el agua siguiendo una línea de costa irregular, y la
 * silueta del mapa deja de ser una suma de cuadrados. Se hace al construir el tile, no al
 * generar la librería, porque qué lados dan fuera solo se sabe al colocar el módulo.
 *
 * Reglas que mantienen el contrato del borde:
 *   - la costa solo baja terreno (nunca sube) y solo donde el asset lo permite
 *     (CoastWeights: 0 en el camino, 1 en paredes y mesetas);
 *   - se apaga por completo a menos de KeepFromSharedSide de cualquier lado que NO da
 *     fuera, así que los bordes compartidos con otros módulos no cambian;
 *   - la línea de costa sale de la semilla del tile (replicada): todas las máquinas
 *     construyen la misma.
 */
namespace TNTerrainCoast
{
	struct FCoastSettings
	{
		/** Cota del fondo de mar al que baja la costa, en uu (el agua está a -400). */
		double SeaFloor = -700.0;
		/** Distancia al lado exterior hasta la que llega el mar, entre estos dos valores. */
		double MinReach = 3000.0;
		double MaxReach = 9000.0;
		/** Anchura de la orilla (de tierra a fondo de mar). */
		double Shore = 1400.0;
		/** A menos de esta distancia de un lado compartido no se toca nada. */
		double KeepFromSharedSide = 1800.0;
		/** Longitud de onda de las entradas y cabos de la costa. */
		double Wavelength = 5200.0;
	};

	/** Fases de la línea de costa de cada lado, sacadas de la semilla del tile. */
	struct FCoastShape
	{
		double PhaseA[TNGridLogic::NumSides] = {};
		double PhaseB[TNGridLogic::NumSides] = {};

		explicit FCoastShape(int32 Seed)
		{
			for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
			{
				FRandomStream Stream(Seed * 92821 + Side * 68917);
				PhaseA[Side] = Stream.FRandRange(0.0, 2.0 * PI);
				PhaseB[Side] = Stream.FRandRange(0.0, 2.0 * PI);
			}
		}
	};

	/** Alcance del mar hacia dentro en el punto T (uu, a lo largo del lado) de un lado. */
	inline double CoastReach(int32 Side, double T, const FCoastShape& Shape, const FCoastSettings& Settings)
	{
		const double Wave = 0.6 * FMath::Sin(2.0 * PI * T / Settings.Wavelength + Shape.PhaseA[Side])
			+ 0.4 * FMath::Sin(2.0 * PI * T / (Settings.Wavelength * 0.43) + Shape.PhaseB[Side]);
		return FMath::Lerp(Settings.MinReach, Settings.MaxReach, 0.5 + 0.5 * Wave);
	}

	/** Distancia de un punto local al lado Side y coordenada a lo largo de él. */
	inline void SideFrame(int32 Side, const FVector2D& Local, double Half, double& OutDistance, double& OutAlong)
	{
		const FVector2D Out = TNTerrainModuleWall::SideDirection(Side);
		OutDistance = Half - FVector2D::DotProduct(Local, Out);
		OutAlong = FVector2D::DotProduct(Local, FVector2D(-Out.Y, Out.X));
	}

	/** Peso 0..1 de la costa en un punto local (antes de CoastWeights). OuterMask en el
	 *  espacio local del módulo tal como está colocado. */
	inline double CoastWeight(const FVector2D& Local, double Size, uint8 OuterMask, const FCoastShape& Shape, const FCoastSettings& Settings)
	{
		if (OuterMask == 0) { return 0.0; }
		const double Half = Size * 0.5;
		double Weight = 0.0;
		double Keep = 1.0;
		for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
		{
			double Distance = 0.0;
			double Along = 0.0;
			SideFrame(Side, Local, Half, Distance, Along);
			if (OuterMask & TNTerrainModule::SideBit(Side))
			{
				const double Reach = CoastReach(Side, Along, Shape, Settings);
				Weight = FMath::Max(Weight, 1.0 - TNGridTerrain::SmoothStep(Reach - Settings.Shore, Reach, Distance));
			}
			else
			{
				Keep *= TNGridTerrain::SmoothStep(Settings.KeepFromSharedSide * 0.5, Settings.KeepFromSharedSide, Distance);
			}
		}
		return Weight * Keep;
	}

	/**
	 * Alturas del asset (mismo orden que Asset.Heights) con la costa aplicada. Los lados de
	 * OuterMask están en el espacio del módulo colocado: si está reflejado, la columna J del
	 * asset cae en la R-1-J del tile. Sin lados exteriores o sin CoastWeights, devuelve las
	 * alturas tal cual.
	 */
	inline TArray<uint16> ApplyCoast(const UTN_TerrainModuleAsset& Asset, double Size, bool bMirrored, uint8 OuterMask,
		int32 Seed, const FCoastSettings& Settings = FCoastSettings())
	{
		TArray<uint16> Heights = Asset.Heights;
		const int32 R = Asset.Resolution;
		if (OuterMask == 0 || !Asset.IsValidModule() || Asset.CoastWeights.Num() != Heights.Num()) { return Heights; }

		const double Step = Size / (R - 1);
		const double Half = Size * 0.5;
		const double SeaValue = Settings.SeaFloor / Asset.HeightScale + Asset.HeightZero;
		const FCoastShape Shape(Seed);
		for (int32 I = 0; I < R; ++I)
		{
			for (int32 J = 0; J < R; ++J)
			{
				const int32 Index = I * R + J;
				const double Allow = Asset.CoastWeights[Index] / 255.0;
				if (Allow <= 0.0) { continue; }
				const int32 PlacedJ = bMirrored ? R - 1 - J : J;
				const FVector2D Local(I * Step - Half, PlacedJ * Step - Half);
				const double Weight = CoastWeight(Local, Size, OuterMask, Shape, Settings) * Allow;
				if (Weight <= 0.0) { continue; }
				const double Current = Heights[Index];
				Heights[Index] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(FMath::Min(Current, FMath::Lerp(Current, SeaValue, Weight))), 0, MAX_uint16));
			}
		}
		return Heights;
	}

	/** Caja invisible a lo largo de todo un lado exterior: nadie sale del mapa vadeando. */
	inline TNTerrainModuleWall::FWallBlocker BuildOuterBlocker(int32 Side, double ModuleSize)
	{
		const FVector2D Out = TNTerrainModuleWall::SideDirection(Side);
		const FVector2D Center2D = Out * (ModuleSize * 0.5 - 100.0);
		TNTerrainModuleWall::FWallBlocker Blocker;
		Blocker.Center = FVector(Center2D.X, Center2D.Y, 1500.0);
		Blocker.Extent = FVector(100.0, ModuleSize * 0.5, 3000.0);
		Blocker.Rotation = FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X)), 0.f);
		return Blocker;
	}
}
