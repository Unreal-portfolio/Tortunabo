#include "World/TN_TerrainModuleAsset.h"
#include "Core/TN_Log.h"

bool UTN_TerrainModuleAsset::SetHeightfield(int32 InResolution, float InHeightScale, int32 InHeightZero,
	const TArray<int32>& InHeights)
{
	if (InResolution < 2 || InHeights.Num() != InResolution * InResolution)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': SetHeightfield con %d valores para resolución %d."),
			*GetName(), InHeights.Num(), InResolution);
		return false;
	}
	if (InHeightScale <= 0.f || InHeightZero < 0 || InHeightZero > MAX_uint16)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': codificación inválida (escala %f, cero %d)."),
			*GetName(), InHeightScale, InHeightZero);
		return false;
	}

	TArray<uint16> Packed;
	Packed.Reserve(InHeights.Num());
	for (const int32 Value : InHeights)
	{
		if (Value < 0 || Value > MAX_uint16)
		{
			UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': altura %d fuera de [0, 65535]."), *GetName(), Value);
			return false;
		}
		Packed.Add(static_cast<uint16>(Value));
	}

	Resolution = InResolution;
	HeightScale = InHeightScale;
	HeightZero = InHeightZero;
	Heights = MoveTemp(Packed);
	MarkPackageDirty();
	return true;
}
