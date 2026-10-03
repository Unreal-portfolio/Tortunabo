#include "Core/TN_ProjectMaterials.h"

#include "Core/TN_Log.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

UMaterialInterface* TNMaterials::VertexColor()
{
	// Ya cargado, LoadObject solo lo busca en memoria.
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, VertexColorPath, nullptr, LOAD_NoWarn))
	{
		return Mat;
	}
	static bool bWarned = false;
	if (!bWarned)
	{
		bWarned = true;
		UE_LOG(LogTortunabo, Error, TEXT("Falta %s: las mallas generadas en código usan el material por defecto del motor."), VertexColorPath);
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}
