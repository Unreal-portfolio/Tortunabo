#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

/**
 * Materiales del proyecto que usan las mallas generadas en código.
 *
 * Ningún fichero carga materiales de /Engine/EngineDebugMaterials ni de /Engine/Editor*: tienen aspecto de depuración
 * y pueden no cocinarse. Si faltara el material del proyecto, se usa el material por defecto del motor (siempre se
 * cocina) y se avisa una vez en el log.
 */
namespace TNMaterials
{
	/** Ruta de M_CosmeticVertexColor: opaco con luz; color del vértice como base y su alfa como brillo. */
	inline constexpr const TCHAR* VertexColorPath =TEXT("/Game/Cosmetics/Materials/M_CosmeticVertexColor.M_CosmeticVertexColor");

	/** Material de color de vértice del proyecto (M_CosmeticVertexColor) o, si faltara, el material por defecto del motor. */
	UMaterialInterface* VertexColor();
}
