"""Coloca el lobby del castillo redondo en LVL_Lobby (se ejecuta dentro del editor, con LVL_Lobby abierto y fuera de
PIE): aparta la maqueta original (50 m más abajo, en la carpeta Referencia_Blockout_Original, recuperable) y pone el
castillo, la tienda, el cuartel, los probadores, las medusas, el patio de pruebas y las salidas. Los actores propios que
ya hubiera se quitan y se vuelven a poner. No guarda el nivel: revisarlo y guardarlo a mano.
Ver Docs/Lobby_Castillo.md."""
import math
import unreal

out = []
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if les.is_in_play_in_editor():
    raise RuntimeError('PIE en marcha')
if 'LVL_Lobby' not in world.get_path_name():
    raise RuntimeError('No está abierto LVL_Lobby: ' + world.get_path_name())

R = 2400.0


def clock_spot(hour, dist):
    a = hour / 12.0 * 2.0 * math.pi
    x, y = -dist * math.sin(a), dist * math.cos(a)
    yaw = math.degrees(math.atan2(-y, -x))
    return unreal.Vector(x, y, 2.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)


acts = eas.get_all_level_actors()
# 1) Lo que ya hubiera de una colocación anterior se quita (se vuelve a poner con los sitios de ahora).
ours = ('TN_SandCastleLobby', 'TN_ShopKeeper', 'TN_GeneralBriefing', 'TN_ChangingBooth', 'TN_JellyfishTrampoline', 'TN_WobblyBridge', 'TN_PlaygroundPiece')
removed = 0
for a in acts:
    if a.get_class().get_name() in ours:
        eas.destroy_actor(a)
        removed += 1
out.append('quitados de antes: %d' % removed)

# 2) Maqueta original: 50 m más abajo y en su carpeta (recuperable subiéndola 50 m).
moved = []
acts = eas.get_all_level_actors()
for a in acts:
    cls = a.get_class().get_name()
    label = a.get_actor_label()
    if a.get_folder_path() and str(a.get_folder_path()).startswith('Referencia_Blockout'):
        continue
    names = label + ' ' + cls
    if isinstance(a, unreal.StaticMeshActor):
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        sm = smc.get_editor_property('static_mesh') if smc else None
        if sm:
            names += ' ' + sm.get_name()
    blockout = (
        any(k in names for k in ('Extrude', 'SandWall', 'BP_Fence', 'BP_Tower', 'VestidorBotella', 'ShellDoor', 'ChangingTent', 'SM_Palo', 'Rectangle', 'Boolean'))
        or (isinstance(a, unreal.StaticMeshActor) and 'Capsule' in names)
        or (isinstance(a, unreal.SkeletalMeshActor) and 'TotugaDemo' in names)
    )
    if blockout:
        l = a.get_actor_location()
        a.set_actor_location(unreal.Vector(l.x, l.y, l.z - 5000.0), False, True)
        a.set_folder_path('Referencia_Blockout_Original')
        moved.append(label)
out.append('maqueta apartada: %d (%s)' % (len(moved), ', '.join(sorted(moved)[:12])))


def spawn(cls, loc, rot, label, folder='Lobby_Castillo'):
    a = eas.spawn_actor_from_class(cls, loc, rot)
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


# 3) Castillo en el centro del anillo.
spawn(unreal.TN_SandCastleLobby, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0), 'Castillo_Arena')
# 4) Tienda y cuartel pegados a la muralla, mirando al centro: la tienda entre la torre de la izquierda de la puerta
#    doble y la siguiente (de las 10:30 a las 11:15) y el cuartel entre la de la derecha y la siguiente (de la 0:45 a la
#    1:25). La distancia deja la estantería de la tienda y los vientos de atrás del cuartel junto a la muralla (radio 2400).
loc, rot = clock_spot(10.88, 2225.0)
spawn(unreal.TN_ShopKeeper, loc, rot, 'Tienda_LaConchaDorada')
loc, rot = clock_spot(1.11, 2160.0)
spawn(unreal.TN_GeneralBriefing, loc, rot, 'Cuartel_General')
# 5) Probadores de las 2 a las 3:30, mirando al centro, en tresbolillo junto a la muralla.
for i, (hour, dist) in enumerate(((2.07, 2150.0), (2.53, 2040.0), (3.0, 2150.0), (3.43, 2040.0))):
    loc, rot = clock_spot(hour, dist)
    spawn(unreal.TN_ChangingBooth, loc, rot, 'Probador_%d' % (i + 1))
# 6) Medusas trampolín (tamaño por propiedad, escala 1): la pequeña junto al adarve derecho del muro interior (se sube
#    botando), dos más en la plaza de las 8:20 a las 10 y una en la esquina del fondo del patio de pruebas, al pie de la
#    muralla (se sube al adarve de la muralla).
jelly_cls = getattr(unreal, 'TN_JellyfishTrampoline', None)
if jelly_cls:
    C = unreal.TNJellyfishColor
    jellies = [
        ((900.0, -470.0), 0.8, C.SKY),
        (clock_spot(9.17, 1900.0)[0], 1.1, C.PINK),
        (clock_spot(9.6, 1650.0)[0], 1.45, C.LILAC),
        ((880.0, -1900.0), 1.0, C.SKY),
    ]
    for i, (where, size, color) in enumerate(jellies):
        x, y = (where.x, where.y) if isinstance(where, unreal.Vector) else where
        yaw = math.degrees(math.atan2(-y, -x))
        j = spawn(jelly_cls, unreal.Vector(x, y, 2.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), 'Medusa_%d' % (i + 1))
        j.set_editor_property('size', size)
        j.set_editor_property('color_preset', color)
    out.append('medusas: %d' % len(jellies))

# 6b) Patio de pruebas detrás del muro (sur): circuito desde la salida del paso de la torre (0, -1200).
T = unreal.TNPlaygroundPieceType
course = [
    (T.POPSICLE_STEPS, (350.0, -1350.0), 0.0, {'step_count': 5, 'step_height': 30.0}),
    (T.COOKIE_PLATFORM, (820.0, -1380.0), 0.0, {'platform_height': 150.0, 'cookie_radius': 110.0}),
    (T.BUCKET_POST, (1120.0, -1560.0), 0.0, {'post_height': 190.0}),
    (T.BUCKET_POST, (1300.0, -1800.0), 0.0, {'post_height': 230.0}),
    (T.SPADE_SPINNER, (-420.0, -1620.0), 0.0, {'arm_length': 260.0}),
    (T.CASTLE_TUNNEL, (-980.0, -1700.0), 90.0, {'tunnel_length': 420.0}),
    (T.SHELL_SLIDE, (-1500.0, -1300.0), 0.0, {'slide_height': 250.0}),
    (T.BUCKET_POST, (-1250.0, -1900.0), 0.0, {'post_height': 140.0}),
]
for i, (kind, (x, y), yaw, props) in enumerate(course):
    p = spawn(unreal.TN_PlaygroundPiece, unreal.Vector(x, y, 2.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), 'Prueba_%02d' % (i + 1))
    p.set_editor_property('piece_type', kind)
    for k, v in props.items():
        p.set_editor_property(k, v)
    p.rebuild_piece()
b = spawn(unreal.TN_WobblyBridge, unreal.Vector(350.0, -1950.0, 2.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0), 'Puente_Bamboleante')
b.set_editor_property('span_length', 700.0)
b.set_editor_property('deck_height', 230.0)
out.append('patio de pruebas: %d piezas y el puente' % len(course))

# 7) Salidas: el PlayerStart que hubiera pasa al primer sitio y se añaden hasta cuatro, entre la puerta y los huevos.
spots = []
for x in (-450.0, -150.0, 150.0, 450.0):
    y = 1700.0 - abs(x) * 0.25
    yaw = math.degrees(math.atan2(700.0 - y, 0.0 - x))
    spots.append((unreal.Vector(x, y, 97.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)))
starts = [a for a in eas.get_all_level_actors() if isinstance(a, unreal.PlayerStart) and str(a.get_editor_property('player_start_tag')) != 'Tutorial']
for i, (loc, rot) in enumerate(spots):
    if i < len(starts):
        starts[i].set_actor_location_and_rotation(loc, rot, False, True)
        starts[i].set_folder_path('Lobby_Castillo')
    else:
        spawn(unreal.PlayerStart, loc, rot, 'Salida_%d' % (i + 1))
out.append('salidas: %d (reutilizadas %d)' % (len(spots), min(len(starts), len(spots))))

for line in out:
    unreal.log('[Lobby] ' + line)
