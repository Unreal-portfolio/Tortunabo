// Nombres de sala para las partidas en línea (contrato en Multiplayer/TN_RoomNames.h).
//
// ATENCIÓN: el ORDEN de la tabla no se cambia NUNCA. La sesión anuncia solo el índice (ROOMNAME_ID) y las salas se
// guardan y se ven entre versiones distintas del juego: quitar, mover o reordenar un nombre cambia el de todas las
// salas que ya lo usan. Los nombres nuevos van SIEMPRE al final de la tabla; uno que ya no guste se reescribe en su
// sitio (misma posición), no se borra.
//
// Cada pareja es (español de España, inglés). El inglés es una adaptación con la misma gracia, no una traducción:
// si el chiste español es un juego de palabras, el inglés busca otro que funcione en inglés. Reglas de la lista
// (las comprueba la prueba automática Tortunabo.Multiplayer.RoomNames): 28 caracteres como máximo en cada idioma,
// sin comillas ni emojis, mayúsculas de frase en español y de título en inglés, sin repetidos en ningún idioma.

#include "Multiplayer/TN_RoomNames.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace
{
	struct FTNRoomNamePair
	{
		const TCHAR* Es;
		const TCHAR* En;
	};

	static const FTNRoomNamePair GTNRoomNames[] =
	{
		// 0
		{ TEXT("Tortugas al horno"), TEXT("Slow Roasted Shells") },
		{ TEXT("Caparazón contento"), TEXT("Shell Yeah") },
		{ TEXT("Tarde pero con estilo"), TEXT("Fashionably Late Turtles") },
		{ TEXT("Lentos pero furiosos"), TEXT("Slow but Furious") },
		{ TEXT("Sin prisa pero con pausa"), TEXT("Slow Motion Commotion") },
		{ TEXT("La liebre se durmió"), TEXT("Hare Today Gone Tomorrow") },
		{ TEXT("Calma y caparazón"), TEXT("Keep Calm and Carapace On") },
		{ TEXT("Ganar por una cabeza"), TEXT("Win by a Neck") },
		{ TEXT("Cabeza a cabeza"), TEXT("Neck and Neck") },
		{ TEXT("Listos, preparados, siesta"), TEXT("Ready Steady Nap") },
		// 10
		{ TEXT("Ya casi llegamos"), TEXT("Are We There Yet") },
		{ TEXT("Farolillo rojo feliz"), TEXT("Wooden Spoon Winners") },
		{ TEXT("Medalla de chocolate"), TEXT("Participation Trophy") },
		{ TEXT("Los últimos serán primeros"), TEXT("Last Turtle Standing") },
		{ TEXT("Cien metros lentos"), TEXT("Hundred Meter Crawl") },
		{ TEXT("Foto finish a cámara lenta"), TEXT("Photo Finish Next Week") },
		{ TEXT("Huevo y cuchara"), TEXT("Egg and Spoon Race") },
		{ TEXT("Vuelta de honor"), TEXT("Lap of Luxury") },
		{ TEXT("Calentando desde ayer"), TEXT("Warming Up Since Monday") },
		{ TEXT("Corre y espera"), TEXT("Hurry Up and Wait") },
		// 20
		{ TEXT("Lenta como un rayo"), TEXT("Lightning in a Shell") },
		{ TEXT("A toda tortuga"), TEXT("Full Throttle Turtle") },
		{ TEXT("Sin prisa y sin ganas"), TEXT("No Hurry No Worry") },
		{ TEXT("Maratón de siestas"), TEXT("Snooze Cruise") },
		{ TEXT("Quien espera se tuesta"), TEXT("Good Tan Takes Time") },
		{ TEXT("Más vale lenta que nunca"), TEXT("Better Slow Than Never") },
		{ TEXT("Ni tortuga ni liebre"), TEXT("Neither Turtle nor Hare") },
		{ TEXT("Tortuga que se duerme"), TEXT("Snooze You Lose") },
		{ TEXT("Como el caballo del malo"), TEXT("Slow as Molasses") },
		{ TEXT("Juan Salvador Tortuga"), TEXT("Jonathan Livingston Turtle") },
		// 30
		{ TEXT("Carpe siesta"), TEXT("Seas the Day") },
		{ TEXT("La vida es siesta"), TEXT("Life Is a Beach") },
		{ TEXT("Rodando nos entendemos"), TEXT("Roll Model") },
		{ TEXT("Dando vueltas al asunto"), TEXT("Getting Round to It") },
		{ TEXT("Que ruede la bola"), TEXT("Let the Good Shells Roll") },
		{ TEXT("Hecha un ovillo"), TEXT("Ball Up Buttercup") },
		{ TEXT("Encogida y contenta"), TEXT("Snug as a Shell") },
		{ TEXT("Susto en el caparazón"), TEXT("Shell Shocked") },
		{ TEXT("Sal de tu caparazón"), TEXT("Shell We Dance") },
		{ TEXT("A ritmo de marea"), TEXT("Twist and Shell") },
		// 40
		{ TEXT("Tortugas tortolitas"), TEXT("Two Turtle Doves") },
		{ TEXT("Club del cuello alto"), TEXT("Turtleneck Club") },
		{ TEXT("Tortugas hasta el fondo"), TEXT("Turtles All the Way Down") },
		{ TEXT("Tortugas mini, sueños maxi"), TEXT("Tiny Shells Big Dreams") },
		{ TEXT("Patas arriba"), TEXT("Turn Turtle") },
		{ TEXT("Trile de caparazones"), TEXT("Shell Game Champions") },
		{ TEXT("Cucú tras, caparazón"), TEXT("Peekaboo Shell") },
		{ TEXT("Pilla pilla lento"), TEXT("Slow-Mo Tag") },
		{ TEXT("Un dos tres tortuguita"), TEXT("Red Light Green Turtle") },
		{ TEXT("Asomando la cabeza"), TEXT("Stick Your Neck Out") },
		// 50
		{ TEXT("Aleteando como locas"), TEXT("Flipping Out") },
		{ TEXT("Chapoteo a tope"), TEXT("Splish Splash Bash") },
		{ TEXT("Tirarse al mar"), TEXT("Take the Plunge") },
		{ TEXT("Al agua, patos"), TEXT("Water You Waiting For") },
		{ TEXT("Huevos rotos de playa"), TEXT("Sunny Side Sand Up") },
		{ TEXT("Nido dulce nido"), TEXT("Nest Sweet Nest") },
		{ TEXT("Huevo duro de pelar"), TEXT("Tough Egg to Crack") },
		{ TEXT("Los empollones del nido"), TEXT("Eggheads of the Nest") },
		{ TEXT("La yema de la fiesta"), TEXT("The Yolk Is on You") },
		{ TEXT("Atrápame si eclosiono"), TEXT("Hatch Me If You Can") },
		// 60
		{ TEXT("Feliz eclosión"), TEXT("Happy Hatch Day") },
		{ TEXT("Cuidado con el huevo"), TEXT("Eggstra Careful") },
		{ TEXT("Nos cascamos de risa"), TEXT("Crack Up Crew") },
		{ TEXT("Caza del huevo playera"), TEXT("Sandy Egg Hunt") },
		{ TEXT("Cava que te cava"), TEXT("Dig Deep for Eggs") },
		{ TEXT("Tortuguita a bordo"), TEXT("Hatchling on Board") },
		{ TEXT("Nido vacío, playa llena"), TEXT("Empty Nest Best") },
		{ TEXT("Luna llena, nido lleno"), TEXT("Full Moon, Full Nest") },
		{ TEXT("A la luz de la luna"), TEXT("Moonlight Hatchlings") },
		{ TEXT("El huevo o la tortuga"), TEXT("Which Came First") },
		// 70
		{ TEXT("Solo en el nido"), TEXT("Nest Alone") },
		{ TEXT("Tortilla de arena"), TEXT("Scrambled on the Sand") },
		{ TEXT("Grandes eclosiones"), TEXT("Great Eggspectations") },
		{ TEXT("Buscar un huevo en la arena"), TEXT("Needle in a Sandstack") },
		{ TEXT("Granito a granito"), TEXT("Grain and Bear It") },
		{ TEXT("Arena de otro costal"), TEXT("Different Kettle of Crabs") },
		{ TEXT("Mucho ruido, poca arena"), TEXT("Much Ado About Dunes") },
		{ TEXT("Arenas movedizas"), TEXT("Sinking Feeling") },
		{ TEXT("Arena en el caparazón"), TEXT("Sandy Situation") },
		{ TEXT("Todo lo que brilla es arena"), TEXT("All That Glitters Is Sand") },
		// 80
		{ TEXT("Hechos arena"), TEXT("Dead Beat Beach") },
		{ TEXT("Huellas en la arena"), TEXT("Flipper Footprints") },
		{ TEXT("Duna a la vista"), TEXT("Sand Ho") },
		{ TEXT("Perdidos en la duna"), TEXT("Lost in Sandslation") },
		{ TEXT("Ricitos de arena"), TEXT("Sandilocks") },
		{ TEXT("Mi arena, mis normas"), TEXT("Sandbox Rules") },
		{ TEXT("A ojo de buen cubero"), TEXT("Beach Ball Park") },
		{ TEXT("Mar y duna"), TEXT("Surf and Turtle") },
		{ TEXT("A contracorriente"), TEXT("Against the Grain") },
		{ TEXT("Lo que la marea se llevó"), TEXT("Gone with the Tide") },
		// 90
		{ TEXT("Marea baja, moral alta"), TEXT("Low Tide, High Spirits") },
		{ TEXT("Mar de risas"), TEXT("Sea You Later") },
		{ TEXT("A merced de la marea"), TEXT("Tide Me Over") },
		{ TEXT("Ola que va, ola que viene"), TEXT("In One Wave, Out the Other") },
		{ TEXT("Con el agua al cuello"), TEXT("Head Above Water") },
		{ TEXT("Viento en popa"), TEXT("Wind Beneath My Shell") },
		{ TEXT("Llueve sobre mojado"), TEXT("Rain on My Beach Parade") },
		{ TEXT("En la cresta de la ola"), TEXT("Hang Ten Slowly") },
		{ TEXT("Marea y prejuicio"), TEXT("Tide and Prejudice") },
		{ TEXT("Salir a flote"), TEXT("Staying Afloat") },
		// 100
		{ TEXT("Hay que mojarse"), TEXT("Get Your Flippers Wet") },
		{ TEXT("Calma chicha"), TEXT("Calm Before the Splash") },
		{ TEXT("Vaivén de mareas"), TEXT("Ebb and Flop") },
		{ TEXT("Tormenta de ideas"), TEXT("Brainstorm Surge") },
		{ TEXT("Chubasco de malas ideas"), TEXT("Pour Decisions") },
		{ TEXT("Nublado con riesgo de caca"), TEXT("Gull Showers Expected") },
		{ TEXT("Tormenta en un vaso de agua"), TEXT("Storm in a Bucket") },
		{ TEXT("Rayos y centellas"), TEXT("Bolt from the Blue") },
		{ TEXT("Playas borrascosas"), TEXT("Weathering Heights") },
		{ TEXT("Mal tiempo, buen caparazón"), TEXT("Come Rain or Shell") },
		// 110
		{ TEXT("Camino de cangrejos"), TEXT("Crabby Road") },
		{ TEXT("Pinza y pega"), TEXT("Pinch Me Quick") },
		{ TEXT("Ermitaño con vistas"), TEXT("Hermit with a View") },
		{ TEXT("Pinzas y a correr"), TEXT("Claws for Alarm") },
		{ TEXT("Rojo como un cangrejo"), TEXT("Red as a Lobster") },
		{ TEXT("Cada cangrejo a su roca"), TEXT("Every Crab for Itself") },
		{ TEXT("Piedra, papel o pinza"), TEXT("Rock Paper Claws") },
		{ TEXT("Cangrejo talla XXL"), TEXT("Extra Large and Crabby") },
		{ TEXT("Bailando de lado"), TEXT("Sideways Shuffle") },
		{ TEXT("Sujétame el alga"), TEXT("Hold My Seaweed") },
		// 120
		{ TEXT("Agarrar al cangrejo"), TEXT("Grab the Crab") },
		{ TEXT("Entre la marea y la roca"), TEXT("Between a Rock and a Crab") },
		{ TEXT("Meter la aleta"), TEXT("Flipper in Mouth") },
		{ TEXT("Coraza fuera, corazón dentro"), TEXT("Tough Shell Soft Heart") },
		{ TEXT("Adiós, mi bocata"), TEXT("Chip Off the Old Gull") },
		{ TEXT("Once gaviotas y un botín"), TEXT("Gulls Eleven") },
		{ TEXT("Ojo con lo que cae"), TEXT("Poop Deck Party") },
		{ TEXT("La gaviota contraataca"), TEXT("The Gull Strikes Back") },
		{ TEXT("Gaviotas de ida y vuelta"), TEXT("Gullible Travels") },
		{ TEXT("Cacas voladoras"), TEXT("Splat Attack") },
		// 130
		{ TEXT("Cabeza de chorlito"), TEXT("Bird Brain Beach") },
		{ TEXT("Ladrones de bocatas"), TEXT("Sandwich Snatchers") },
		{ TEXT("El rastro de migas"), TEXT("Hansel and Gullet") },
		{ TEXT("Caparazoncita Roja"), TEXT("Little Red Riding Shell") },
		{ TEXT("La bolsa o la vida"), TEXT("Pouch or Your Life") },
		{ TEXT("Pico y pala"), TEXT("Beak to the Future") },
		{ TEXT("Pelícano sin dieta"), TEXT("Pouch Potato") },
		{ TEXT("Pico, pico, pelícano"), TEXT("Duck Duck Pelican") },
		{ TEXT("Cierra el pico"), TEXT("Beak It Up") },
		{ TEXT("Pulpo fiction"), TEXT("Octopulp Fiction") },
		// 140
		{ TEXT("Choca esos ocho"), TEXT("High Eight") },
		{ TEXT("Armado hasta las ventosas"), TEXT("Armed to the Suckers") },
		{ TEXT("Espectáculo de tentáculos"), TEXT("Tentacle Spectacle") },
		{ TEXT("Pulpo en un garaje"), TEXT("Octopus Buying Gloves") },
		{ TEXT("Fiesta en la poza"), TEXT("Tide Pool Party") },
		{ TEXT("Échame una mano, o ocho"), TEXT("Lend Me a Hand or Eight") },
		{ TEXT("Algas al gusto"), TEXT("Kelp Yourself") },
		{ TEXT("Algo de algas"), TEXT("Kelp Wanted") },
		{ TEXT("Mercadillo de pulgas"), TEXT("Flea Market Beach") },
		{ TEXT("Pulga detrás de la oreja"), TEXT("Flea to Meet You") },
		// 150
		{ TEXT("Un día de malas pulgas"), TEXT("Bad Flea Day") },
		{ TEXT("De salto en salto"), TEXT("Hop to It") },
		{ TEXT("Lagartos tomando el sol"), TEXT("Lounge Lizards") },
		{ TEXT("Ni idea, iguana"), TEXT("Iguana Have Fun") },
		{ TEXT("Lagarto, fuera de mi toalla"), TEXT("Gecko Out of Here") },
		{ TEXT("Sangre fría, playa caliente"), TEXT("Cold Blooded Beach Bums") },
		{ TEXT("Lagarto, lagarto"), TEXT("Knock on Driftwood") },
		{ TEXT("Trampolín de gelatina"), TEXT("Jelly Jump Street") },
		{ TEXT("Medusa botarate"), TEXT("Jelly Good Fellow") },
		{ TEXT("A otra cosa, medusa"), TEXT("Bounce Back Jelly") },
		// 160
		{ TEXT("Mi castillo es tu castillo"), TEXT("Home Is Where the Moat Is") },
		{ TEXT("Castillo de arena mojada"), TEXT("Fort Knock Down") },
		{ TEXT("Se busca foso"), TEXT("Moat Wanted") },
		{ TEXT("Caballeros del cubo"), TEXT("Knight in Sandy Armor") },
		{ TEXT("La gran muralla de arena"), TEXT("Great Wall of Sand") },
		{ TEXT("Con la casa a cuestas"), TEXT("Home Is Where the Shell Is") },
		{ TEXT("Mira qué botín"), TEXT("Loot at That") },
		{ TEXT("Una X en la arena"), TEXT("X Marks the Shell") },
		{ TEXT("Cofre y cuenta nueva"), TEXT("Treasure Chest Bump") },
		{ TEXT("Ábrete, cofre"), TEXT("Open Says Me") },
		// 170
		{ TEXT("Cofres a tutiplén"), TEXT("Chests Galore") },
		{ TEXT("Sombrilla voladora"), TEXT("Throwing Shade") },
		{ TEXT("Crema, que te quemas"), TEXT("Sunscreen Dream Team") },
		{ TEXT("Chancletas al ataque"), TEXT("Flip Flop Frenzy") },
		{ TEXT("Tumbados a la bartola"), TEXT("Deck Chair Legends") },
		{ TEXT("Helado derretido"), TEXT("Ice Cream Meltdown") },
		{ TEXT("Salvados por el flotador"), TEXT("Saved by the Buoy") },
		{ TEXT("El señor de los flotadores"), TEXT("Lord of the Swim Rings") },
		{ TEXT("Cubo y pala, gran gala"), TEXT("Bucket and Spade Parade") },
		{ TEXT("Llueve a cubos"), TEXT("Bucketing Down") },
		// 180
		{ TEXT("La gota que colmó el cubo"), TEXT("The Last Straw Hat") },
		{ TEXT("Infierno de toallas"), TEXT("Towelling Inferno") },
		{ TEXT("Vaya toalla"), TEXT("Holy Terrycloth") },
		{ TEXT("Muy guay para el nido"), TEXT("Too Cool for Shell") },
		{ TEXT("Vigilantes de la playa"), TEXT("Bay Watch Out") },
		{ TEXT("Sardinas en toalla"), TEXT("Packed Like Sardines") },
		{ TEXT("El cuñado con sombrilla"), TEXT("Know It All Uncle") },
		{ TEXT("Marineros de agua dulce"), TEXT("Landlubbers Anonymous") },
		{ TEXT("Tostados al sol"), TEXT("Basking in Glory") },
		{ TEXT("Chiringuito sin prisa"), TEXT("No Hurry Beach Bar") },
		// 190
		{ TEXT("El chiringuito del náufrago"), TEXT("The Castaway Cafe") },
		{ TEXT("Bar la sombrilla rota"), TEXT("The Busted Brolly") },
		{ TEXT("Bar marea alta"), TEXT("High Tide Tavern") },
		{ TEXT("Bar pinza perdida"), TEXT("The Lost Claw Diner") },
		{ TEXT("Bar ostras felices"), TEXT("Happy Clam Bar") },
		{ TEXT("El último helado"), TEXT("The Last Scoop Shack") },
		{ TEXT("La paella lleva arena"), TEXT("Extra Crunchy Paella") },
		{ TEXT("Carga, apunta, tortuga"), TEXT("Ready Aim Turtle") },
		{ TEXT("Catapulta y a ver qué pasa"), TEXT("A Fling and a Prayer") },
		{ TEXT("Cuando las tortugas vuelen"), TEXT("When Turtles Fly") },
		// 200
		{ TEXT("Aterrizaje forzoso"), TEXT("Splash Landing") },
		{ TEXT("Bomba al agua"), TEXT("Cannonball Craze") },
		{ TEXT("Tortugas con alas"), TEXT("Winging It") },
		{ TEXT("Al filo del acantilado"), TEXT("On the Edge of Your Shell") },
		{ TEXT("Colgados del acantilado"), TEXT("Cliffhanger Turtles") },
		{ TEXT("Panzada final"), TEXT("Belly Flop Finale") },
		{ TEXT("Salto con tirabuzón"), TEXT("Dive Bomb Diva") },
		{ TEXT("Agua va"), TEXT("Look Out Below") },
		{ TEXT("Todos al agua"), TEXT("Last Stop Splash") },
		{ TEXT("El gran chapuzón"), TEXT("The Big Dipper") },
		// 210
		{ TEXT("El último, para el gusano"), TEXT("Last One In Is Worm Food") },
		{ TEXT("Mucho gusano"), TEXT("Worm Regards") },
		{ TEXT("El gusano tiene hambre"), TEXT("A Worm Welcome") },
		{ TEXT("Piratas del caparazón"), TEXT("Pirates of the Carapace") },
		{ TEXT("Yo, ho, ho y un cubo"), TEXT("Yo Ho Ho and a Bucket") },
		{ TEXT("A sus órdenes, capitana"), TEXT("Aye Aye Captain Shell") },
		{ TEXT("La bella siestera"), TEXT("Snoozing Beauty") },
		{ TEXT("El patito flotador"), TEXT("The Ugly Duckling Float") },
		{ TEXT("La liebre ha vuelto"), TEXT("Return of the Hare") },
		{ TEXT("La vieja tortuga y el mar"), TEXT("The Old Turtle and the Sea") },
		// 220
		{ TEXT("Con toallas y a lo loco"), TEXT("Some Like It Salty") },
		{ TEXT("Con el norte perdido"), TEXT("Compass Confusion") },
		{ TEXT("Cabeza dentro, mundo fuera"), TEXT("Head in the Sand") },
		{ TEXT("Cada uno a su bola"), TEXT("Roll Your Own Way") },
		{ TEXT("Aleta con aleta"), TEXT("Hand in Flipper") },
		{ TEXT("Cabeza o cola"), TEXT("Heads or Tails") },
		{ TEXT("Tortugas tunantes"), TEXT("Terrific Terrapins") },
		{ TEXT("No mover ni una aleta"), TEXT("Not Lifting a Flipper") },
		{ TEXT("Sin decir ni glub"), TEXT("Bubble Trouble") },
		{ TEXT("Fiesta de espuma"), TEXT("Sea Foam Party") },
		// 230
		{ TEXT("Mensaje en un caparazón"), TEXT("Message in a Shell") },
		{ TEXT("Punto por punto"), TEXT("Connect the Dots") },
		{ TEXT("Fiesta de lunares"), TEXT("Polka Dot Party") },
		{ TEXT("Puntos de vista"), TEXT("Spot On Shells") },
		{ TEXT("Tortuga en un aprieto"), TEXT("Turtle in a Pickle") },
		{ TEXT("Cuento de nunca llegar"), TEXT("The Neverending Crawl") },
		{ TEXT("Mi media naranja"), TEXT("My Better Half Shell") },
		{ TEXT("Velocidad: tortuga"), TEXT("Speed Limit Zero") },
		{ TEXT("Mañana eclosiono"), TEXT("Hatching Tomorrow") },
		{ TEXT("Tengo prisa, pero poca"), TEXT("Mildly in a Hurry") },
		// 240
		{ TEXT("A mi ritmo"), TEXT("Own Pace Race") },
		{ TEXT("Arena que quema"), TEXT("Hot Sand Hop") },
	};

	static constexpr int32 GTNRoomNameCount = static_cast<int32>(UE_ARRAY_COUNT(GTNRoomNames));
}

namespace TNRoomNames
{
	int32 Num()
	{
		return GTNRoomNameCount;
	}

	bool IsSpanish()
	{
		return FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName() == TEXT("es");
	}

	FText Get(int32 Id)
	{
		return FText::FromString(GetIn(Id, IsSpanish()));
	}

	FString GetIn(int32 Id, bool bSpanish)
	{
		if (Id < 0 || Id >= GTNRoomNameCount)
		{
			// Índice desconocido (una sala de otra versión con más nombres, o sin nombre): nunca queda vacío.
			return bSpanish ? FString(TEXT("Sala sin nombre")) : FString(TEXT("Nameless Nest"));
		}

		const FTNRoomNamePair& Pair = GTNRoomNames[Id];
		return FString(bSpanish ? Pair.Es : Pair.En);
	}

	int32 Random(int32 Avoid)
	{
		const int32 Count = GTNRoomNameCount;
		if (Count <= 1)
		{
			return 0;
		}

		// Avoid fuera de rango (INDEX_NONE incluido): no hay nada que evitar.
		if (Avoid < 0 || Avoid >= Count)
		{
			return FMath::RandRange(0, Count - 1);
		}

		// Se sortea entre los demás y se salta el evitado: uniforme y sin bucles de reintento.
		const int32 Pick = FMath::RandRange(0, Count - 2);
		return Pick >= Avoid ? Pick + 1 : Pick;
	}
}
