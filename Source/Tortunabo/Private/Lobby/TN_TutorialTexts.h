#pragma once

#include "CoreMinimal.h"
#include "TN_TutorialLayout.h"

/**
 * Textos del tutorial (español de España; se localizan con el resto del juego, espacio «TNTutorial»): el nombre de cada
 * estación (también en su cartel de madera), lo que pide cada tarea y un consejo. La tecla la pone el HUD aparte.
 */
namespace TNTutorialTexts
{
	using TNTutorial::EStation;

	inline FText Title(EStation Station)
	{
		switch (Station)
		{
			case EStation::Welcome:    return NSLOCTEXT("TNTutorial", "TitleWelcome", "¡Hola, tortuga!");
			case EStation::Sprint:     return NSLOCTEXT("TNTutorial", "TitleSprint", "Correr");
			case EStation::Jump:       return NSLOCTEXT("TNTutorial", "TitleJump", "Saltar");
			case EStation::BellyDive:  return NSLOCTEXT("TNTutorial", "TitleDive", "Panzazo");
			case EStation::Pickup:     return NSLOCTEXT("TNTutorial", "TitlePickup", "Coger objetos");
			case EStation::Search:     return NSLOCTEXT("TNTutorial", "TitleSearch", "Rebuscar");
			case EStation::Slots:      return NSLOCTEXT("TNTutorial", "TitleSlots", "La aleta y el caparazón");
			case EStation::Throw:      return NSLOCTEXT("TNTutorial", "TitleThrow", "Lanzar");
			case EStation::UseItem:    return NSLOCTEXT("TNTutorial", "TitleUse", "Usar objetos");
			case EStation::Shell:      return NSLOCTEXT("TNTutorial", "TitleShell", "El caparazón");
			case EStation::Trampoline: return NSLOCTEXT("TNTutorial", "TitleTrampoline", "Trampolín");
			case EStation::Catapult:   return NSLOCTEXT("TNTutorial", "TitleCatapult", "Catapulta");
			case EStation::Swim:       return NSLOCTEXT("TNTutorial", "TitleSwim", "Nadar");
			case EStation::Carry:      return NSLOCTEXT("TNTutorial", "TitleCarry", "Coger a una tortuga");
			case EStation::Escape:     return NSLOCTEXT("TNTutorial", "TitleEscape", "¡Que te cogen!");
			case EStation::Emotes:     return NSLOCTEXT("TNTutorial", "TitleEmotes", "Bailes y frases");
			case EStation::Voice:      return NSLOCTEXT("TNTutorial", "TitleVoice", "Tu voz");
			case EStation::PauseMenu:  return NSLOCTEXT("TNTutorial", "TitlePause", "El menú de pausa");
			case EStation::Waterfall:  return NSLOCTEXT("TNTutorial", "TitleWaterfall", "¡Al castillo!");
			default:                   return FText::GetEmpty();
		}
	}

	inline FText Task(EStation Station, int32 Index)
	{
		switch (Station)
		{
			case EStation::Welcome:
				return Index == 0 ? NSLOCTEXT("TNTutorial", "TaskMove", "Muévete")
					: NSLOCTEXT("TNTutorial", "TaskLook", "Mira alrededor");
			case EStation::Sprint:     return NSLOCTEXT("TNTutorial", "TaskSprint", "Mantén para correr");
			case EStation::Jump:       return NSLOCTEXT("TNTutorial", "TaskJump", "Salta al escalón");
			case EStation::BellyDive:  return NSLOCTEXT("TNTutorial", "TaskDive", "En el aire, pulsa otra vez: te tiras en plancha");
			case EStation::Pickup:     return NSLOCTEXT("TNTutorial", "TaskPickup", "Coge la barrita de energía");
			case EStation::Search:     return NSLOCTEXT("TNTutorial", "TaskSearch", "Mantén junto al montículo y coge lo que salga");
			case EStation::Slots:
				return Index == 0 ? NSLOCTEXT("TNTutorial", "TaskSwap", "Cambia lo de la aleta por lo del caparazón")
					: NSLOCTEXT("TNTutorial", "TaskDrop", "Suelta lo de la aleta (y vuelve a cogerlo)");
			case EStation::Throw:      return NSLOCTEXT("TNTutorial", "TaskThrow", "Con la bola en la aleta, apunta con la cámara y dale al cangrejo");
			case EStation::UseItem:    return NSLOCTEXT("TNTutorial", "TaskUse", "Usa la barrita y corre sin cansarte");
			case EStation::Shell:
				return Index == 0 ? NSLOCTEXT("TNTutorial", "TaskShellIn", "Métete en el caparazón y rueda cuesta abajo")
					: NSLOCTEXT("TNTutorial", "TaskShellOut", "Sal con la misma tecla");
			case EStation::Trampoline: return NSLOCTEXT("TNTutorial", "TaskTrampoline", "Salta encima de la medusa para subir a la roca");
			case EStation::Catapult:   return NSLOCTEXT("TNTutorial", "TaskCatapult", "Métete en la cuchara, de pie o hecha bola, y vuela al otro lado");
			case EStation::Swim:
				return Index == 0 ? NSLOCTEXT("TNTutorial", "TaskSwim", "Cruza la laguna nadando")
					: NSLOCTEXT("TNTutorial", "TaskSwimOut", "Salta para salir del agua");
			case EStation::Carry:
				return Index == 0 ? NSLOCTEXT("TNTutorial", "TaskGrab", "Coge a Rodolfo, que está en su caparazón")
					: NSLOCTEXT("TNTutorial", "TaskToss", "Lánzalo (o déjalo en el suelo con la tecla de soltar)");
			case EStation::Escape:
				return Index == 0 ? NSLOCTEXT("TNTutorial", "TaskGetGrabbed", "Métete en el caparazón junto a Berta: te cogerá")
					: NSLOCTEXT("TNTutorial", "TaskStruggle", "Muévete sin parar para soltarte");
			case EStation::Emotes:
				return Index == 0 ? NSLOCTEXT("TNTutorial", "TaskEmote", "Mantén, elige un baile y suelta")
					: NSLOCTEXT("TNTutorial", "TaskChat", "Mantén, elige una frase y suelta");
			case EStation::Voice:      return NSLOCTEXT("TNTutorial", "TaskVoice", "Habla: te oyen las tortugas a menos de 25 m");
			case EStation::PauseMenu:  return NSLOCTEXT("TNTutorial", "TaskPause", "Abre el menú de pausa y mira los ajustes");
			case EStation::Waterfall:  return NSLOCTEXT("TNTutorial", "TaskWaterfall", "Salta por la cascada: caerás en el castillo");
			default:                   return FText::GetEmpty();
		}
	}

	inline FText Tip(EStation Station)
	{
		switch (Station)
		{
			case EStation::Welcome:
				return NSLOCTEXT("TNTutorial", "TipWelcome", "La tortuga va hacia donde mira la cámara. Sigue el camino: cada cartel te enseña una cosa.");
			case EStation::Sprint:
				return NSLOCTEXT("TNTutorial", "TipSprint", "El salvavidas de abajo es tu energía: se gasta al correr y, si se vacía, jadeas hasta que se recarga.");
			case EStation::Jump:
				return NSLOCTEXT("TNTutorial", "TipJump", "Saltas algo más de un metro. Corriendo llegas más lejos.");
			case EStation::BellyDive:
				return NSLOCTEXT("TNTutorial", "TipDive", "Vuelas más lejos y resbalas por el suelo: sirve para cruzar huecos y esquivar la caca de gaviota. Muévete o salta para levantarte.");
			case EStation::Pickup:
				return NSLOCTEXT("TNTutorial", "TipPickup", "Lo que coges va a la aleta; si ya llevas algo, al caparazón. Todo pesa: con mucho encima te cansas antes.");
			case EStation::Search:
				return NSLOCTEXT("TNTutorial", "TipSearch", "Muchas rocas, estatuas y cajas del camino se pueden rebuscar igual: a veces sale algo.");
			case EStation::Slots:
				return NSLOCTEXT("TNTutorial", "TipSlots", "Abajo, en el centro, ves lo que llevas: en la aleta y en el caparazón.");
			case EStation::Throw:
				return NSLOCTEXT("TNTutorial", "TipThrow", "Lo que lanzas marea a los enemigos. Si fallas, la bola se queda en el suelo: cógela y prueba otra vez.");
			case EStation::UseItem:
				return NSLOCTEXT("TNTutorial", "TipUse", "Al lanzar la bola, lo del caparazón sube a la aleta. Sin nada que coger delante, la misma tecla usa lo que llevas.");
			case EStation::Shell:
				return NSLOCTEXT("TNTutorial", "TipShell", "Con algo en la aleta no puedes: guárdalo antes. Si caes desde más de 5 m, te haces bola tú sola.");
			case EStation::Trampoline:
				return NSLOCTEXT("TNTutorial", "TipTrampoline", "Cuanto más alto caes, más alto rebotas. Una bola de caparazón también rebota.");
			case EStation::Catapult:
				return NSLOCTEXT("TNTutorial", "TipCatapult", "Si alguien salta sobre el cubito del otro extremo, dispara al momento. ¿Mejor no? El tronco del lado también cruza.");
			case EStation::Swim:
				return NSLOCTEXT("TNTutorial", "TipSwim", "En el agua no te puedes meter en el caparazón, y las caídas en el agua no te hacen nada.");
			case EStation::Carry:
				return NSLOCTEXT("TNTutorial", "TipCarry", "Solo se coge a quien va en bola o está panza arriba. Con alguien en alto, salta y haz el panzazo para lanzarlo más lejos.");
			case EStation::Escape:
				return NSLOCTEXT("TNTutorial", "TipEscape", "Mientras forcejeas, quien te lleva te lanza con menos fuerza.");
			case EStation::Emotes:
				return NSLOCTEXT("TNTutorial", "TipEmotes", "Si una compañera cae panza arriba, baila a su lado y la levantas.");
			case EStation::Voice:
				return NSLOCTEXT("TNTutorial", "TipVoice", "Con «Pulsar para hablar» (menú de pausa > Ajustes > Voz) solo se te oye con esta tecla. Ahí puedes bajar o silenciar a cada compañera.");
			case EStation::PauseMenu:
				return NSLOCTEXT("TNTutorial", "TipPause", "Ajustes, Controles (cambia cualquier tecla) y accesibilidad: filtro para daltónicos, tamaño de la interfaz, «Quién habla»... El juego no se para: es en red.");
			case EStation::Waterfall:
				return NSLOCTEXT("TNTutorial", "TipWaterfall", "En el castillo, el General Galápago te lo cuenta todo. Para jugar, ponte lista en un huevo o en la sala de la puerta.");
			default:
				return FText::GetEmpty();
		}
	}

	/** Felicitación al aprender una estación (una de cuatro). */
	inline FText Cheer(int32 Seed)
	{
		switch (((Seed % 4) + 4) % 4)
		{
			case 0:  return NSLOCTEXT("TNTutorial", "Cheer0", "¡Bien!");
			case 1:  return NSLOCTEXT("TNTutorial", "Cheer1", "¡Eso es!");
			case 2:  return NSLOCTEXT("TNTutorial", "Cheer2", "¡Genial!");
			default: return NSLOCTEXT("TNTutorial", "Cheer3", "¡Muy bien!");
		}
	}
}
