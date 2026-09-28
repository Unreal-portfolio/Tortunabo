"""Crea el nivel de la carrera en la playa (modo carrera, Docs/Modo_Carrera.md): /Game/Maps/Run/LVL_BeachRace.

Se ejecuta DENTRO del editor de Unreal, con el C++ ya compilado (ATN_BeachRaceGenerator y ATN_BeachRaceGameMode):
    exec(open(r"<repo>/Scripts/build_beach_race.py", encoding="utf-8").read())

Crea el nivel (o lo abre, si ya existe) y deja:
  - Sol (luz direccional que mueve el cielo) casi cenital a la espalda de la salida, cielo (SkyAtmosphere), luz del cielo en tiempo
    real y niebla suave de altura, como LVL_ProcMap.
  - El generador de la playa (ATN_BeachRaceGenerator, etiqueta «PlayaCarrera») en el origen: el terreno fijo se
    construye solo, también en el editor; cada ronda la reparte el GameMode.
  - Cuatro PlayerStart en los sitios de salida del generador (el GameMode ya pone a cada tortuga en el suyo; quedan por
    si el nivel se juega con otro GameMode).
  - En World Settings, el GameMode /Script/Tortunabo.TN_BeachRaceGameMode.
Lo guarda y queda abierto: con Play se juega una carrera (sin lobby, el GameMode reparte solo).

Idempotente: lo que ya existe (por su etiqueta) se reutiliza sin tocarlo. Guarda antes lo que tengas abierto: el
script cambia de nivel.

Para revisar el reparto sin jugar: selecciona «PlayaCarrera» y en Details > Beach|Editor pulsa «Preview Round» (con
Editor Seed o una al azar); «Clear Preview» lo quita. Nada de eso se guarda con el nivel.

Vistas para las capturas (mueven la cámara del visor; con el nivel abierto):
    BEACH_SKIP_MAIN = True
    exec(open(r"<repo>/Scripts/build_beach_race.py", encoding="utf-8").read())
    vista("salida")      # desde la salida, a la altura de una tortuga: la playa hasta el mar
    vista("planta")      # la playa entera desde arriba (con Preview Round, el reparto y sus huellas)
    vista("castillo")    # el castillo con salas y sus alas desde el lado de la salida
    vista("acantilado")  # el borde de roca, el agua de meta y las banderas desde la playa
    vista("meta")        # desde el agua: el acantilado, el arco de la meta y la selva de los lados
    vista("selva")       # la orilla con las palmeras inclinadas sobre la arena
    vista("huevos")      # la fila de cuatro huevos de la salida en su nido de arena (de frente, algo de lado)
    vista("dunas")       # a ras de arena desde la salida: el relieve, los corredores y las crestas hasta la mitad
    vista("trinchera")   # las dos líneas de trincheras en zigzag, con sacos, tablones y puentes
    vista("poza")        # una poza que corta un corredor (agua nadable) y su orilla
    vista("cresta")      # una duna con cresta desde su cara empinada: la cornisa que se salta y un collado
    vista("huecos")      # desde la orilla hacia la selva: lianas, enredaderas y hojas enormes entre las copas
    vista("sprint")      # la línea del sprint de desempate a mitad de la playa
"""

import unreal

MAP_PATH = "/Game/Maps/Run/LVL_BeachRace"
GAME_MODE_CLASS = "/Script/Tortunabo.TN_BeachRaceGameMode"
GENERATOR_LABEL = "PlayaCarrera"

# Medidas de TN_BeachLayout.h (cm, espacio del generador: X hacia el mar, Y a lo ancho, el agua en Z = 0).
COURSE_LENGTH = 120000.0
HALF_WIDTH = 14000.0
CLIFF_TOP = 1550.0

asset_lib = unreal.EditorAssetLibrary


def require_cpp():
    if not hasattr(unreal, "TN_BeachRaceGenerator"):
        raise RuntimeError("[Playa] Falta ATN_BeachRaceGenerator: compila el C++ y vuelve a abrir el editor.")


def open_level():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if asset_lib.does_asset_exist(MAP_PATH):
        level_subsystem.load_level(MAP_PATH)
    else:
        level_subsystem.new_level(MAP_PATH)
    return level_subsystem


def find_generator():
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, unreal.TN_BeachRaceGenerator):
            return actor
    return None


def build_level():
    require_cpp()
    level_subsystem = open_level()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    by_label = {actor.get_actor_label(): actor for actor in actor_subsystem.get_all_level_actors()}

    def ensure_actor(label, actor_class, location=unreal.Vector(0, 0, 0), rotation=unreal.Rotator(0, 0, 0)):
        """(actor, creado): el que tenga esa etiqueta o uno nuevo."""
        if label in by_label:
            return by_label[label], False
        actor = actor_subsystem.spawn_actor_from_class(actor_class, location, rotation)
        actor.set_actor_label(label)
        by_label[label] = actor
        return actor, True

    # Sol casi cenital (72°) a la espalda de la salida (la luz va hacia el mar): la playa queda iluminada de frente para
    # quien corre hacia el mar y cada cosa proyecta su sombra casi debajo (se ve dónde va a caer la gaviota y dónde se
    # puede subir). Con el sol sobre el mar, todo se veía a contraluz; la selva y el cerro de detrás de la salida no dan
    # sombra (ver TileCastsShadow y ShadowZone), que lo dejaban todo a oscuras.
    sun, created = ensure_actor("Sol", unreal.DirectionalLight, unreal.Vector(0, 0, 30000))
    sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-72.0, yaw=15.0), False)
    if created:
        light = sun.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_editor_property("atmosphere_sun_light", True)
        light.set_editor_property("intensity", 10.0)
    ensure_actor("Cielo", unreal.SkyAtmosphere)
    sky, created = ensure_actor("LuzDelCielo", unreal.SkyLight, unreal.Vector(0, 0, 30000))
    if created:
        sky.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True)
    # Niebla suave: nada en los primeros 300 m; lejos, el mar y la selva se funden con el cielo.
    fog, created = ensure_actor("Niebla", unreal.ExponentialHeightFog, unreal.Vector(0, 0, -500))
    if created:
        fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        fog_comp.set_editor_property("fog_density", 0.006)
        fog_comp.set_editor_property("fog_height_falloff", 0.05)
        fog_comp.set_editor_property("start_distance", 30000.0)

    generator, _ = ensure_actor(GENERATOR_LABEL, unreal.TN_BeachRaceGenerator)

    # PlayerStart en los sitios de salida (GetStartTransform: 110 cm sobre el suelo, mirando al mar).
    for index in range(4):
        start_xf = generator.get_start_transform(index)
        ensure_actor(f"Salida_{index + 1}", unreal.PlayerStart, start_xf.translation, start_xf.rotation.rotator())

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    game_mode = unreal.load_class(None, GAME_MODE_CLASS)
    if game_mode:
        world.get_world_settings().set_editor_property("default_game_mode", game_mode)
    else:
        unreal.log_warning(f"[Playa] No existe {GAME_MODE_CLASS}: pon el GameMode en World Settings cuando compile.")

    level_subsystem.save_current_level()
    unreal.log(f"[Playa] {MAP_PATH} listo: generador «{GENERATOR_LABEL}», 4 salidas y GameMode de la carrera.")


# ── Vistas para las capturas ──────────────────────────────────────────────────

VIEWS = {
    # nombre: (punto de vista, punto al que mira), en el espacio del generador
    "salida": ((-900.0, 0.0, 5186.0 + 450.0), (COURSE_LENGTH, 0.0, 1500.0)),
    "planta": ((COURSE_LENGTH * 0.5, 0.0, 190000.0), (COURSE_LENGTH * 0.5 + 10.0, 0.0, 0.0)),
    "castillo": ((42000.0, -9000.0, 5500.0), (60000.0, 0.0, 2500.0)),
    "acantilado": ((COURSE_LENGTH - 6000.0, 9000.0, CLIFF_TOP + 1200.0), (COURSE_LENGTH + 3000.0, -2000.0, 200.0)),
    "meta": ((COURSE_LENGTH + 16000.0, 4000.0, 2500.0), (COURSE_LENGTH - 4000.0, 0.0, 1400.0)),
    "selva": ((60000.0, HALF_WIDTH - 6000.0, 3500.0), (75000.0, HALF_WIDTH + 12000.0, 9000.0)),
    # Relieve y salida nuevos (cotas medidas del terreno fijo de TN_BeachLayout.h, con margen por encima).
    "huevos": ((1400.0, 2000.0, 5087.0 + 380.0), (-800.0, 0.0, 5186.0 + 120.0)),
    "dunas": ((9000.0, 0.0, 4721.0 + 450.0), (60000.0, 0.0, 2661.0 + 200.0)),
    "trinchera": ((33500.0, -2500.0, 3472.0 + 1500.0), (36600.0, 1500.0, 3575.0)),
    "poza": ((36500.0, 3500.0, 3680.0 + 900.0), (40200.0, 7160.0, 3286.0)),
    "cresta": ((40200.0, -3000.0, 3367.0 + 350.0), (43800.0, -3470.0, 3737.0 + 150.0)),
    "huecos": ((50000.0, HALF_WIDTH - 1500.0, 3259.0 + 500.0), (53000.0, HALF_WIDTH + 6000.0, 4973.0 + 3500.0)),
    "sprint": ((58500.0, 0.0, 2738.0 + 500.0), (61500.0, 0.0, 2602.0 + 150.0)),
}


def vista(name):
    """Pone la cámara del visor en una de VIEWS (relativa al generador del nivel)."""
    if name not in VIEWS:
        unreal.log_warning(f"[Playa] Vista desconocida {name}: {', '.join(VIEWS)}")
        return
    generator = find_generator()
    xf = generator.get_actor_transform() if generator else unreal.Transform()
    eye_local, target_local = VIEWS[name]
    eye = xf.transform_location(unreal.Vector(*eye_local))
    target = xf.transform_location(unreal.Vector(*target_local))
    rotation = unreal.MathLibrary.find_look_at_rotation(eye, target)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye, rotation)
    unreal.log(f"[Playa] Vista «{name}».")


# Para usar solo vista(...) sin rehacer el nivel: BEACH_SKIP_MAIN = True antes del exec (se consume en cada ejecución).
if not globals().pop("BEACH_SKIP_MAIN", False):
    build_level()
