// Torreta del buggy construida en ejecución (#435): aro fijo sobre las barandillas, carro con horquilla que gira solo en
// guiñada, cañón con cuerpo, escudo y brazo lateral que gira y cabecea con el apuntado, y caña con boca ensanchada que
// toma el tinte de la munición. Estilo de juguete a juego con SM_TN_BuggyBody (Art/Source/Vehicles/Buggy/build_buggy.py).
//
// Ejes de la torreta (los de UTN_BuggyTurretComponent): origen en el pivote, X hacia el apuntado, Y a su derecha, Z arriba.
// El cañón va a la derecha de la artillera porque su cabeza ocupa el eje de disparo desde 5 hasta 55 cm por delante del
// pivote (Scripts/tools/data/turtle_geo.json a escala 2,5): la boca queda a MuzzleDistanceCm por delante y MuzzleSideCm a
// la derecha, y el proyectil sale de ahí (TNRallyTurret::MuzzleWorldLocation). Los tests en Tortunabo.Rally.Turret.Mesh.
#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapMeshKit.h"

namespace TNBuggyTurretMesh
{
	/** Aro fijo a la altura de la cintura de la artillera (sobre el respaldo, que acaba 27,4 cm bajo el pivote). */
	constexpr double RingZ = -22.0;
	constexpr double RingRadius = 38.0;
	constexpr double RingTube = 2.2;
	/** Barandillas laterales de SM_TN_BuggyBody respecto al pivote (ROLL_Y y GUNNER_RAIL_Z de build_buggy.py). */
	constexpr double RailY = 55.0;
	constexpr double RailZ = -24.38;

	/** Horquilla del carro (a la derecha, sobre el aro) y su eje de cabeceo, que pasa por el pivote. */
	constexpr double YokeY = 38.5;
	constexpr double HubInnerY = 33.0;
	constexpr double HubOuterY = 41.5;
	constexpr double HubRadius = 5.5;

	/** Cuerpo del cañón, escudo y caña (cm, ejes de la torreta). */
	constexpr double BodyMinX = 31.0;
	constexpr double BodyMaxX = 46.0;
	constexpr double ShieldX = 47.0;
	constexpr double ShieldMinY = 10.0;
	constexpr double BarrelRadius = 4.2;
	constexpr double BellStartX = 56.0;
	constexpr double MouthRadius = 7.5;

	/** Aro y tirantes a las barandillas: va fijo en el chasis, no gira. */
	void BuildRing(TNProcMesh::FTNProcMeshBuffers& Out);

	/** Carro sobre el aro, horquilla y cubo del eje de cabeceo: gira solo en guiñada. */
	void BuildMount(TNProcMesh::FTNProcMeshBuffers& Out);

	/** Brazo lateral, cuerpo, mira y escudo: giran y cabecean con el apuntado. Colores propios (sin tinte). */
	void BuildGun(TNProcMesh::FTNProcMeshBuffers& Out);

	/** Caña y boca ensanchada, centradas en (MuzzleDistanceCm, MuzzleSideCm, 0): llevan el tinte de la munición. */
	void BuildBarrel(TNProcMesh::FTNProcMeshBuffers& Out);
}
