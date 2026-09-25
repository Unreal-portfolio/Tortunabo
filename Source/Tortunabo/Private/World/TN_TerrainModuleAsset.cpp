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

bool UTN_TerrainModuleAsset::SetBiomeMask(const TArray<int32>& InMask)
{
	if (InMask.Num() == 0)
	{
		BiomeMask.Reset();
		MarkPackageDirty();
		return true;
	}
	if (InMask.Num() != Resolution * Resolution)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': SetBiomeMask con %d valores para resolución %d."),
			*GetName(), InMask.Num(), Resolution);
		return false;
	}

	TArray<uint16> Packed;
	Packed.Reserve(InMask.Num());
	for (const int32 Value : InMask)
	{
		if (Value < 0 || Value > MAX_uint16)
		{
			UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': valor de máscara %d fuera de [0, 65535]."), *GetName(), Value);
			return false;
		}
		Packed.Add(static_cast<uint16>(Value));
	}
	BiomeMask = MoveTemp(Packed);
	MarkPackageDirty();
	return true;
}

bool UTN_TerrainModuleAsset::SetCoastWeights(const TArray<int32>& InWeights)
{
	if (InWeights.Num() != 0 && InWeights.Num() != Resolution * Resolution)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': SetCoastWeights con %d valores para resolución %d."),
			*GetName(), InWeights.Num(), Resolution);
		return false;
	}

	TArray<uint8> Packed;
	Packed.Reserve(InWeights.Num());
	for (const int32 Value : InWeights)
	{
		if (Value < 0 || Value > MAX_uint8)
		{
			UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': peso de costa %d fuera de [0, 255]."), *GetName(), Value);
			return false;
		}
		Packed.Add(static_cast<uint8>(Value));
	}
	CoastWeights = MoveTemp(Packed);
	MarkPackageDirty();
	return true;
}

bool UTN_TerrainModuleAsset::SetTunnelLayers(const TArray<int32>& InRoof, const TArray<int32>& InCeiling)
{
	if (InRoof.Num() == 0 && InCeiling.Num() == 0)
	{
		RoofHeights.Reset();
		CeilingHeights.Reset();
		MarkPackageDirty();
		return true;
	}
	const int32 Count = Resolution * Resolution;
	if (Resolution < 2 || InRoof.Num() != Count || InCeiling.Num() != Count)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': SetTunnelLayers con %d/%d valores para resolución %d."),
			*GetName(), InRoof.Num(), InCeiling.Num(), Resolution);
		return false;
	}

	TArray<uint16> Roof;
	TArray<uint16> Ceiling;
	Roof.Reserve(Count);
	Ceiling.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 R = InRoof[Index];
		const int32 C = InCeiling[Index];
		if (R < 0 || R > MAX_uint16 || C < 0 || C > MAX_uint16 || (R == 0) != (C == 0))
		{
			UE_LOG(LogTortunabo, Error, TEXT("[TerrainModule] '%s': capa de túnel inválida en %d (techo %d, bóveda %d)."),
				*GetName(), Index, R, C);
			return false;
		}
		Roof.Add(static_cast<uint16>(R));
		Ceiling.Add(static_cast<uint16>(C));
	}
	RoofHeights = MoveTemp(Roof);
	CeilingHeights = MoveTemp(Ceiling);
	MarkPackageDirty();
	return true;
}
