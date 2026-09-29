// Nombres de sala para las partidas en línea (contrato en Multiplayer/TN_RoomNames.h).
//
// ATENCIÓN: el ORDEN de la tabla no se cambia NUNCA. La sesión anuncia solo el índice (ROOMNAME_ID) y las salas se
// guardan y se ven entre versiones distintas del juego: quitar, mover o reordenar un nombre cambia el de todas las
// salas que ya lo usan. Los nombres nuevos van SIEMPRE al final de la tabla; uno que ya no guste se reescribe en su
// sitio (misma posición), no se borra.
//
// Los nombres están en el canal de localización del motor (Docs/Localizacion.md): el texto origen es el español de España, con
// una clave estable por índice («Room_000»...) en el espacio de nombres «TNRoomNames». Cada NSLOCTEXT lleva su clave escrita a
// mano (y no un macro que la componga) porque el recolector de textos del motor solo entiende las llamadas literales. Las
// traducciones (inglés incluido) viven en Content/Localization/Game/<cultura>/Game.po; las adaptaciones inglesas de siempre están
// en Tools/Localization/room_names_en.csv, listas para meterse como traducción al inglés.
//
// Reglas de la lista (las comprueba la prueba automática Tortunabo.Multiplayer.RoomNames): 28 caracteres como máximo en cada
// idioma, sin comillas ni emojis, mayúsculas de frase en español y de título en inglés, sin repetidos en ningún idioma. Las
// traducciones son adaptaciones con la misma gracia, no traducciones literales: un juego de palabras se cambia por otro que
// funcione en el idioma de destino.

#include "Multiplayer/TN_RoomNames.h"

#include "Settings/TN_LanguageSettings.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto del bloque.
namespace TNRoomNamesDetail
{
	/** Los nombres en su idioma de origen (español de España): FText localizables, «TNRoomNames» / «Room_000»...«Room_241». */
	const TArray<FText>& Names()
	{
		static const TArray<FText> Table =
		{
			// 0
			NSLOCTEXT("TNRoomNames", "Room_000", "Tortugas al horno"),
			NSLOCTEXT("TNRoomNames", "Room_001", "Caparazón contento"),
			NSLOCTEXT("TNRoomNames", "Room_002", "Tarde pero con estilo"),
			NSLOCTEXT("TNRoomNames", "Room_003", "Lentos pero furiosos"),
			NSLOCTEXT("TNRoomNames", "Room_004", "Sin prisa pero con pausa"),
			NSLOCTEXT("TNRoomNames", "Room_005", "La liebre se durmió"),
			NSLOCTEXT("TNRoomNames", "Room_006", "Calma y caparazón"),
			NSLOCTEXT("TNRoomNames", "Room_007", "Ganar por una cabeza"),
			NSLOCTEXT("TNRoomNames", "Room_008", "Cabeza a cabeza"),
			NSLOCTEXT("TNRoomNames", "Room_009", "Listos, preparados, siesta"),
			// 10
			NSLOCTEXT("TNRoomNames", "Room_010", "Ya casi llegamos"),
			NSLOCTEXT("TNRoomNames", "Room_011", "Farolillo rojo feliz"),
			NSLOCTEXT("TNRoomNames", "Room_012", "Medalla de chocolate"),
			NSLOCTEXT("TNRoomNames", "Room_013", "Los últimos serán primeros"),
			NSLOCTEXT("TNRoomNames", "Room_014", "Cien metros lentos"),
			NSLOCTEXT("TNRoomNames", "Room_015", "Foto finish a cámara lenta"),
			NSLOCTEXT("TNRoomNames", "Room_016", "Huevo y cuchara"),
			NSLOCTEXT("TNRoomNames", "Room_017", "Vuelta de honor"),
			NSLOCTEXT("TNRoomNames", "Room_018", "Calentando desde ayer"),
			NSLOCTEXT("TNRoomNames", "Room_019", "Corre y espera"),
			// 20
			NSLOCTEXT("TNRoomNames", "Room_020", "Lenta como un rayo"),
			NSLOCTEXT("TNRoomNames", "Room_021", "A toda tortuga"),
			NSLOCTEXT("TNRoomNames", "Room_022", "Sin prisa y sin ganas"),
			NSLOCTEXT("TNRoomNames", "Room_023", "Maratón de siestas"),
			NSLOCTEXT("TNRoomNames", "Room_024", "Quien espera se tuesta"),
			NSLOCTEXT("TNRoomNames", "Room_025", "Más vale lenta que nunca"),
			NSLOCTEXT("TNRoomNames", "Room_026", "Ni tortuga ni liebre"),
			NSLOCTEXT("TNRoomNames", "Room_027", "Tortuga que se duerme"),
			NSLOCTEXT("TNRoomNames", "Room_028", "Como el caballo del malo"),
			NSLOCTEXT("TNRoomNames", "Room_029", "Juan Salvador Tortuga"),
			// 30
			NSLOCTEXT("TNRoomNames", "Room_030", "Carpe siesta"),
			NSLOCTEXT("TNRoomNames", "Room_031", "La vida es siesta"),
			NSLOCTEXT("TNRoomNames", "Room_032", "Rodando nos entendemos"),
			NSLOCTEXT("TNRoomNames", "Room_033", "Dando vueltas al asunto"),
			NSLOCTEXT("TNRoomNames", "Room_034", "Que ruede la bola"),
			NSLOCTEXT("TNRoomNames", "Room_035", "Hecha un ovillo"),
			NSLOCTEXT("TNRoomNames", "Room_036", "Encogida y contenta"),
			NSLOCTEXT("TNRoomNames", "Room_037", "Susto en el caparazón"),
			NSLOCTEXT("TNRoomNames", "Room_038", "Sal de tu caparazón"),
			NSLOCTEXT("TNRoomNames", "Room_039", "A ritmo de marea"),
			// 40
			NSLOCTEXT("TNRoomNames", "Room_040", "Tortugas tortolitas"),
			NSLOCTEXT("TNRoomNames", "Room_041", "Club del cuello alto"),
			NSLOCTEXT("TNRoomNames", "Room_042", "Tortugas hasta el fondo"),
			NSLOCTEXT("TNRoomNames", "Room_043", "Tortugas mini, sueños maxi"),
			NSLOCTEXT("TNRoomNames", "Room_044", "Patas arriba"),
			NSLOCTEXT("TNRoomNames", "Room_045", "Trile de caparazones"),
			NSLOCTEXT("TNRoomNames", "Room_046", "Cucú tras, caparazón"),
			NSLOCTEXT("TNRoomNames", "Room_047", "Pilla pilla lento"),
			NSLOCTEXT("TNRoomNames", "Room_048", "Un dos tres tortuguita"),
			NSLOCTEXT("TNRoomNames", "Room_049", "Asomando la cabeza"),
			// 50
			NSLOCTEXT("TNRoomNames", "Room_050", "Aleteando como locas"),
			NSLOCTEXT("TNRoomNames", "Room_051", "Chapoteo a tope"),
			NSLOCTEXT("TNRoomNames", "Room_052", "Tirarse al mar"),
			NSLOCTEXT("TNRoomNames", "Room_053", "Al agua, patos"),
			NSLOCTEXT("TNRoomNames", "Room_054", "Huevos rotos de playa"),
			NSLOCTEXT("TNRoomNames", "Room_055", "Nido dulce nido"),
			NSLOCTEXT("TNRoomNames", "Room_056", "Huevo duro de pelar"),
			NSLOCTEXT("TNRoomNames", "Room_057", "Los empollones del nido"),
			NSLOCTEXT("TNRoomNames", "Room_058", "La yema de la fiesta"),
			NSLOCTEXT("TNRoomNames", "Room_059", "Atrápame si eclosiono"),
			// 60
			NSLOCTEXT("TNRoomNames", "Room_060", "Feliz eclosión"),
			NSLOCTEXT("TNRoomNames", "Room_061", "Cuidado con el huevo"),
			NSLOCTEXT("TNRoomNames", "Room_062", "Nos cascamos de risa"),
			NSLOCTEXT("TNRoomNames", "Room_063", "Caza del huevo playera"),
			NSLOCTEXT("TNRoomNames", "Room_064", "Cava que te cava"),
			NSLOCTEXT("TNRoomNames", "Room_065", "Tortuguita a bordo"),
			NSLOCTEXT("TNRoomNames", "Room_066", "Nido vacío, playa llena"),
			NSLOCTEXT("TNRoomNames", "Room_067", "Luna llena, nido lleno"),
			NSLOCTEXT("TNRoomNames", "Room_068", "A la luz de la luna"),
			NSLOCTEXT("TNRoomNames", "Room_069", "El huevo o la tortuga"),
			// 70
			NSLOCTEXT("TNRoomNames", "Room_070", "Solo en el nido"),
			NSLOCTEXT("TNRoomNames", "Room_071", "Tortilla de arena"),
			NSLOCTEXT("TNRoomNames", "Room_072", "Grandes eclosiones"),
			NSLOCTEXT("TNRoomNames", "Room_073", "Buscar un huevo en la arena"),
			NSLOCTEXT("TNRoomNames", "Room_074", "Granito a granito"),
			NSLOCTEXT("TNRoomNames", "Room_075", "Arena de otro costal"),
			NSLOCTEXT("TNRoomNames", "Room_076", "Mucho ruido, poca arena"),
			NSLOCTEXT("TNRoomNames", "Room_077", "Arenas movedizas"),
			NSLOCTEXT("TNRoomNames", "Room_078", "Arena en el caparazón"),
			NSLOCTEXT("TNRoomNames", "Room_079", "Todo lo que brilla es arena"),
			// 80
			NSLOCTEXT("TNRoomNames", "Room_080", "Hechos arena"),
			NSLOCTEXT("TNRoomNames", "Room_081", "Huellas en la arena"),
			NSLOCTEXT("TNRoomNames", "Room_082", "Duna a la vista"),
			NSLOCTEXT("TNRoomNames", "Room_083", "Perdidos en la duna"),
			NSLOCTEXT("TNRoomNames", "Room_084", "Ricitos de arena"),
			NSLOCTEXT("TNRoomNames", "Room_085", "Mi arena, mis normas"),
			NSLOCTEXT("TNRoomNames", "Room_086", "A ojo de buen cubero"),
			NSLOCTEXT("TNRoomNames", "Room_087", "Mar y duna"),
			NSLOCTEXT("TNRoomNames", "Room_088", "A contracorriente"),
			NSLOCTEXT("TNRoomNames", "Room_089", "Lo que la marea se llevó"),
			// 90
			NSLOCTEXT("TNRoomNames", "Room_090", "Marea baja, moral alta"),
			NSLOCTEXT("TNRoomNames", "Room_091", "Mar de risas"),
			NSLOCTEXT("TNRoomNames", "Room_092", "A merced de la marea"),
			NSLOCTEXT("TNRoomNames", "Room_093", "Ola que va, ola que viene"),
			NSLOCTEXT("TNRoomNames", "Room_094", "Con el agua al cuello"),
			NSLOCTEXT("TNRoomNames", "Room_095", "Viento en popa"),
			NSLOCTEXT("TNRoomNames", "Room_096", "Llueve sobre mojado"),
			NSLOCTEXT("TNRoomNames", "Room_097", "En la cresta de la ola"),
			NSLOCTEXT("TNRoomNames", "Room_098", "Marea y prejuicio"),
			NSLOCTEXT("TNRoomNames", "Room_099", "Salir a flote"),
			// 100
			NSLOCTEXT("TNRoomNames", "Room_100", "Hay que mojarse"),
			NSLOCTEXT("TNRoomNames", "Room_101", "Calma chicha"),
			NSLOCTEXT("TNRoomNames", "Room_102", "Vaivén de mareas"),
			NSLOCTEXT("TNRoomNames", "Room_103", "Tormenta de ideas"),
			NSLOCTEXT("TNRoomNames", "Room_104", "Chubasco de malas ideas"),
			NSLOCTEXT("TNRoomNames", "Room_105", "Nublado con riesgo de caca"),
			NSLOCTEXT("TNRoomNames", "Room_106", "Tormenta en un vaso de agua"),
			NSLOCTEXT("TNRoomNames", "Room_107", "Rayos y centellas"),
			NSLOCTEXT("TNRoomNames", "Room_108", "Playas borrascosas"),
			NSLOCTEXT("TNRoomNames", "Room_109", "Mal tiempo, buen caparazón"),
			// 110
			NSLOCTEXT("TNRoomNames", "Room_110", "Camino de cangrejos"),
			NSLOCTEXT("TNRoomNames", "Room_111", "Pinza y pega"),
			NSLOCTEXT("TNRoomNames", "Room_112", "Ermitaño con vistas"),
			NSLOCTEXT("TNRoomNames", "Room_113", "Pinzas y a correr"),
			NSLOCTEXT("TNRoomNames", "Room_114", "Rojo como un cangrejo"),
			NSLOCTEXT("TNRoomNames", "Room_115", "Cada cangrejo a su roca"),
			NSLOCTEXT("TNRoomNames", "Room_116", "Piedra, papel o pinza"),
			NSLOCTEXT("TNRoomNames", "Room_117", "Cangrejo talla XXL"),
			NSLOCTEXT("TNRoomNames", "Room_118", "Bailando de lado"),
			NSLOCTEXT("TNRoomNames", "Room_119", "Sujétame el alga"),
			// 120
			NSLOCTEXT("TNRoomNames", "Room_120", "Agarrar al cangrejo"),
			NSLOCTEXT("TNRoomNames", "Room_121", "Entre la marea y la roca"),
			NSLOCTEXT("TNRoomNames", "Room_122", "Meter la aleta"),
			NSLOCTEXT("TNRoomNames", "Room_123", "Coraza fuera, corazón dentro"),
			NSLOCTEXT("TNRoomNames", "Room_124", "Adiós, mi bocata"),
			NSLOCTEXT("TNRoomNames", "Room_125", "Once gaviotas y un botín"),
			NSLOCTEXT("TNRoomNames", "Room_126", "Ojo con lo que cae"),
			NSLOCTEXT("TNRoomNames", "Room_127", "La gaviota contraataca"),
			NSLOCTEXT("TNRoomNames", "Room_128", "Gaviotas de ida y vuelta"),
			NSLOCTEXT("TNRoomNames", "Room_129", "Cacas voladoras"),
			// 130
			NSLOCTEXT("TNRoomNames", "Room_130", "Cabeza de chorlito"),
			NSLOCTEXT("TNRoomNames", "Room_131", "Ladrones de bocatas"),
			NSLOCTEXT("TNRoomNames", "Room_132", "El rastro de migas"),
			NSLOCTEXT("TNRoomNames", "Room_133", "Caparazoncita Roja"),
			NSLOCTEXT("TNRoomNames", "Room_134", "La bolsa o la vida"),
			NSLOCTEXT("TNRoomNames", "Room_135", "Pico y pala"),
			NSLOCTEXT("TNRoomNames", "Room_136", "Pelícano sin dieta"),
			NSLOCTEXT("TNRoomNames", "Room_137", "Pico, pico, pelícano"),
			NSLOCTEXT("TNRoomNames", "Room_138", "Cierra el pico"),
			NSLOCTEXT("TNRoomNames", "Room_139", "Pulpo fiction"),
			// 140
			NSLOCTEXT("TNRoomNames", "Room_140", "Choca esos ocho"),
			NSLOCTEXT("TNRoomNames", "Room_141", "Armado hasta las ventosas"),
			NSLOCTEXT("TNRoomNames", "Room_142", "Espectáculo de tentáculos"),
			NSLOCTEXT("TNRoomNames", "Room_143", "Pulpo en un garaje"),
			NSLOCTEXT("TNRoomNames", "Room_144", "Fiesta en la poza"),
			NSLOCTEXT("TNRoomNames", "Room_145", "Échame una mano, o ocho"),
			NSLOCTEXT("TNRoomNames", "Room_146", "Algas al gusto"),
			NSLOCTEXT("TNRoomNames", "Room_147", "Algo de algas"),
			NSLOCTEXT("TNRoomNames", "Room_148", "Mercadillo de pulgas"),
			NSLOCTEXT("TNRoomNames", "Room_149", "Pulga detrás de la oreja"),
			// 150
			NSLOCTEXT("TNRoomNames", "Room_150", "Un día de malas pulgas"),
			NSLOCTEXT("TNRoomNames", "Room_151", "De salto en salto"),
			NSLOCTEXT("TNRoomNames", "Room_152", "Lagartos tomando el sol"),
			NSLOCTEXT("TNRoomNames", "Room_153", "Ni idea, iguana"),
			NSLOCTEXT("TNRoomNames", "Room_154", "Lagarto, fuera de mi toalla"),
			NSLOCTEXT("TNRoomNames", "Room_155", "Sangre fría, playa caliente"),
			NSLOCTEXT("TNRoomNames", "Room_156", "Lagarto, lagarto"),
			NSLOCTEXT("TNRoomNames", "Room_157", "Trampolín de gelatina"),
			NSLOCTEXT("TNRoomNames", "Room_158", "Medusa botarate"),
			NSLOCTEXT("TNRoomNames", "Room_159", "A otra cosa, medusa"),
			// 160
			NSLOCTEXT("TNRoomNames", "Room_160", "Mi castillo es tu castillo"),
			NSLOCTEXT("TNRoomNames", "Room_161", "Castillo de arena mojada"),
			NSLOCTEXT("TNRoomNames", "Room_162", "Se busca foso"),
			NSLOCTEXT("TNRoomNames", "Room_163", "Caballeros del cubo"),
			NSLOCTEXT("TNRoomNames", "Room_164", "La gran muralla de arena"),
			NSLOCTEXT("TNRoomNames", "Room_165", "Con la casa a cuestas"),
			NSLOCTEXT("TNRoomNames", "Room_166", "Mira qué botín"),
			NSLOCTEXT("TNRoomNames", "Room_167", "Una X en la arena"),
			NSLOCTEXT("TNRoomNames", "Room_168", "Cofre y cuenta nueva"),
			NSLOCTEXT("TNRoomNames", "Room_169", "Ábrete, cofre"),
			// 170
			NSLOCTEXT("TNRoomNames", "Room_170", "Cofres a tutiplén"),
			NSLOCTEXT("TNRoomNames", "Room_171", "Sombrilla voladora"),
			NSLOCTEXT("TNRoomNames", "Room_172", "Crema, que te quemas"),
			NSLOCTEXT("TNRoomNames", "Room_173", "Chancletas al ataque"),
			NSLOCTEXT("TNRoomNames", "Room_174", "Tumbados a la bartola"),
			NSLOCTEXT("TNRoomNames", "Room_175", "Helado derretido"),
			NSLOCTEXT("TNRoomNames", "Room_176", "Salvados por el flotador"),
			NSLOCTEXT("TNRoomNames", "Room_177", "El señor de los flotadores"),
			NSLOCTEXT("TNRoomNames", "Room_178", "Cubo y pala, gran gala"),
			NSLOCTEXT("TNRoomNames", "Room_179", "Llueve a cubos"),
			// 180
			NSLOCTEXT("TNRoomNames", "Room_180", "La gota que colmó el cubo"),
			NSLOCTEXT("TNRoomNames", "Room_181", "Infierno de toallas"),
			NSLOCTEXT("TNRoomNames", "Room_182", "Vaya toalla"),
			NSLOCTEXT("TNRoomNames", "Room_183", "Muy guay para el nido"),
			NSLOCTEXT("TNRoomNames", "Room_184", "Vigilantes de la playa"),
			NSLOCTEXT("TNRoomNames", "Room_185", "Sardinas en toalla"),
			NSLOCTEXT("TNRoomNames", "Room_186", "El cuñado con sombrilla"),
			NSLOCTEXT("TNRoomNames", "Room_187", "Marineros de agua dulce"),
			NSLOCTEXT("TNRoomNames", "Room_188", "Tostados al sol"),
			NSLOCTEXT("TNRoomNames", "Room_189", "Chiringuito sin prisa"),
			// 190
			NSLOCTEXT("TNRoomNames", "Room_190", "El chiringuito del náufrago"),
			NSLOCTEXT("TNRoomNames", "Room_191", "Bar la sombrilla rota"),
			NSLOCTEXT("TNRoomNames", "Room_192", "Bar marea alta"),
			NSLOCTEXT("TNRoomNames", "Room_193", "Bar pinza perdida"),
			NSLOCTEXT("TNRoomNames", "Room_194", "Bar ostras felices"),
			NSLOCTEXT("TNRoomNames", "Room_195", "El último helado"),
			NSLOCTEXT("TNRoomNames", "Room_196", "La paella lleva arena"),
			NSLOCTEXT("TNRoomNames", "Room_197", "Carga, apunta, tortuga"),
			NSLOCTEXT("TNRoomNames", "Room_198", "Catapulta y a ver qué pasa"),
			NSLOCTEXT("TNRoomNames", "Room_199", "Cuando las tortugas vuelen"),
			// 200
			NSLOCTEXT("TNRoomNames", "Room_200", "Aterrizaje forzoso"),
			NSLOCTEXT("TNRoomNames", "Room_201", "Bomba al agua"),
			NSLOCTEXT("TNRoomNames", "Room_202", "Tortugas con alas"),
			NSLOCTEXT("TNRoomNames", "Room_203", "Al filo del acantilado"),
			NSLOCTEXT("TNRoomNames", "Room_204", "Colgados del acantilado"),
			NSLOCTEXT("TNRoomNames", "Room_205", "Panzada final"),
			NSLOCTEXT("TNRoomNames", "Room_206", "Salto con tirabuzón"),
			NSLOCTEXT("TNRoomNames", "Room_207", "Agua va"),
			NSLOCTEXT("TNRoomNames", "Room_208", "Todos al agua"),
			NSLOCTEXT("TNRoomNames", "Room_209", "El gran chapuzón"),
			// 210
			NSLOCTEXT("TNRoomNames", "Room_210", "El último, para el gusano"),
			NSLOCTEXT("TNRoomNames", "Room_211", "Mucho gusano"),
			NSLOCTEXT("TNRoomNames", "Room_212", "El gusano tiene hambre"),
			NSLOCTEXT("TNRoomNames", "Room_213", "Piratas del caparazón"),
			NSLOCTEXT("TNRoomNames", "Room_214", "Yo, ho, ho y un cubo"),
			NSLOCTEXT("TNRoomNames", "Room_215", "A sus órdenes, capitana"),
			NSLOCTEXT("TNRoomNames", "Room_216", "La bella siestera"),
			NSLOCTEXT("TNRoomNames", "Room_217", "El patito flotador"),
			NSLOCTEXT("TNRoomNames", "Room_218", "La liebre ha vuelto"),
			NSLOCTEXT("TNRoomNames", "Room_219", "La vieja tortuga y el mar"),
			// 220
			NSLOCTEXT("TNRoomNames", "Room_220", "Con toallas y a lo loco"),
			NSLOCTEXT("TNRoomNames", "Room_221", "Con el norte perdido"),
			NSLOCTEXT("TNRoomNames", "Room_222", "Cabeza dentro, mundo fuera"),
			NSLOCTEXT("TNRoomNames", "Room_223", "Cada uno a su bola"),
			NSLOCTEXT("TNRoomNames", "Room_224", "Aleta con aleta"),
			NSLOCTEXT("TNRoomNames", "Room_225", "Cabeza o cola"),
			NSLOCTEXT("TNRoomNames", "Room_226", "Tortugas tunantes"),
			NSLOCTEXT("TNRoomNames", "Room_227", "No mover ni una aleta"),
			NSLOCTEXT("TNRoomNames", "Room_228", "Sin decir ni glub"),
			NSLOCTEXT("TNRoomNames", "Room_229", "Fiesta de espuma"),
			// 230
			NSLOCTEXT("TNRoomNames", "Room_230", "Mensaje en un caparazón"),
			NSLOCTEXT("TNRoomNames", "Room_231", "Punto por punto"),
			NSLOCTEXT("TNRoomNames", "Room_232", "Fiesta de lunares"),
			NSLOCTEXT("TNRoomNames", "Room_233", "Puntos de vista"),
			NSLOCTEXT("TNRoomNames", "Room_234", "Tortuga en un aprieto"),
			NSLOCTEXT("TNRoomNames", "Room_235", "Cuento de nunca llegar"),
			NSLOCTEXT("TNRoomNames", "Room_236", "Mi media naranja"),
			NSLOCTEXT("TNRoomNames", "Room_237", "Velocidad: tortuga"),
			NSLOCTEXT("TNRoomNames", "Room_238", "Mañana eclosiono"),
			NSLOCTEXT("TNRoomNames", "Room_239", "Tengo prisa, pero poca"),
			// 240
			NSLOCTEXT("TNRoomNames", "Room_240", "A mi ritmo"),
			NSLOCTEXT("TNRoomNames", "Room_241", "Arena que quema"),
		};
		return Table;
	}

	/** Nombre de reserva (índice desconocido: una sala de otra versión con más nombres, o sin nombre): nunca queda vacío. */
	const FText& Fallback()
	{
		static const FText Text = NSLOCTEXT("TNRooms", "RoomNameFallback", "Sala sin nombre");
		return Text;
	}
}

namespace TNRoomNames
{
	int32 Num()
	{
		return TNRoomNamesDetail::Names().Num();
	}

	bool IsSpanish()
	{
		// El idioma que manda es el del ajuste del juego (UTN_GameSettingsSubsystem), no la cultura de Windows ni la del editor.
		return TNLanguage::IsActiveLanguage(TEXT("es"));
	}

	FText Get(int32 Id)
	{
		const TArray<FText>& Names = TNRoomNamesDetail::Names();
		return Names.IsValidIndex(Id) ? Names[Id] : TNRoomNamesDetail::Fallback();
	}

	FString GetSource(int32 Id)
	{
		return Get(Id).BuildSourceString();
	}

	FString GetKey(int32 Id)
	{
		return FString::Printf(TEXT("Room_%03d"), Id);
	}

	FString GetIn(int32 Id, bool bSpanish)
	{
		return bSpanish ? GetSource(Id) : Get(Id).ToString();
	}

	int32 Random(int32 Avoid)
	{
		const int32 Count = Num();
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
