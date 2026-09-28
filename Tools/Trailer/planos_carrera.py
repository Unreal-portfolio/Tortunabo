"""Planos de la carrera para el tráiler (PIE en LVL_BeachRace, recorrido de 800 m). Cada función prepara la escena y
arranca captura.shot(); se lanzan de una en una (rodar.sh) y se espera a que acabe cada una."""
import math

import unreal

import captura
from rodaje import actors, cmd, follow_actor, nearest, orbit, pawn, run_forward, tp, world, xyz


def _puppets(w):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Pawn) if a.actor_has_tag("TrailerPuppet")]


def _ground_at(x, y, w):
    gen = actors("TN_BeachRaceGenerator", w)
    return gen[0].get_ground_height_at(unreal.Vector(x, y, 0.0)) if gen else 3000.0


def beach_fly_start():
    w = world()
    fort = sorted(actors("TN_BeachFortress", w), key=lambda a: xyz(a)[0])[0]
    fx = xyz(fort)
    return captura.shot("beach_fly_start", 165, fov=75, tags=["playa", "vuelo", "salida"],
                        desc="Vuelo desde detrás de la salida hacia la playa llena y la primera fortaleza",
                        keys=[{"f": 0, "eye": [-3500, 0, 8200], "look": [15000, 0, 3200]},
                              {"f": 80, "eye": [3500, -1500, 5600], "look": [fx[0], fx[1], fx[2] + 800]},
                              {"f": 164, "eye": [fx[0] - 2500, fx[1] + 3800, fx[2] + 900], "look": [fx[0] + 9000, 0, 2800]}])


def beach_fly_mid():
    return captura.shot("beach_fly_mid", 150, fov=80, tags=["playa", "vuelo", "densidad"],
                        desc="Vuelo bajo a media carrera entre objetos gigantes",
                        keys=[{"f": 0, "eye": [15000, -9500, 3900], "look": [23000, -5000, 3100]},
                              {"f": 75, "eye": [24000, -2500, 3450], "look": [32000, 1500, 2800]},
                              {"f": 149, "eye": [33000, 4500, 3100], "look": [42000, 3000, 2400]}])


def beach_top():
    return captura.shot("beach_top", 120, fov=70, tags=["playa", "cenital", "densidad"],
                        desc="Cenital alto de la playa llena de cosas",
                        keys=[{"f": 0, "eye": [18000, -1500, 15500], "look": [20500, -1500, 2500]},
                              {"f": 119, "eye": [42000, 1500, 13500], "look": [44500, 1500, 1800]}])


def fortress_orbit():
    w = world()
    fort = max(actors("TN_BeachFortress", w), key=lambda a: a.get_actor_bounds(False)[1].x)
    c = xyz(fort)
    return captura.shot("fortress_orbit", 150, fov=65, tags=["fortaleza", "castillo"],
                        desc="Órbita alrededor de la fortaleza de arena más grande",
                        keys=orbit([c[0], c[1], c[2]], 7200.0, 2600.0, 200.0, 330.0, 150, fov=65, look_z=900.0))


def fortress_climb():
    """La tortuga aparece en la cima de una fortaleza, corre hacia el lanzador potenciado y sale disparada."""
    w = world()
    p = pawn(w)
    return captura.shot("fortress_climb", 200, fov=72, tags=["fortaleza", "tortuga", "catapulta", "vuelo"],
                        desc="Cima de la fortaleza: premio y lanzador potenciado",
                        events={-15: "TN.Beach.Fortress.Top 0"},
                        each=lambda ww, i: p.add_movement_input(unreal.Vector(1, 0, 0), 1.0, True) if 10 <= i <= 90 else None,
                        follow=follow_actor(lambda ww: pawn(ww), (-1100, -700, 450), (300, 0, 60), smooth=0.12, fov=72))


def catapult_launch():
    w = world()
    p = pawn(w)
    cat = sorted(actors("TN_BeachCatapult", w), key=lambda a: xyz(a)[0])[0]
    bowl = cat.get_editor_property("bowl_collision")
    origin, extent, _r = unreal.SystemLibrary.get_component_bounds(bowl)
    b = unreal.Vector(origin.x, origin.y, origin.z + extent.z + 60.0)
    return captura.shot("catapult_launch", 165, fov=78, tags=["catapulta", "tortuga", "vuelo"],
                        desc="Tortuga lanzada por una catapulta y seguida en el aire",
                        events={-12: (lambda ww: tp(p, [b.x, b.y, b.z], cat.get_actor_rotation().yaw))},
                        follow=follow_actor(lambda ww: pawn(ww), (-900, -1600, 500), (0, 0, 80), smooth=0.14, fov=78))


def trampoline_bounce():
    w = world()
    p = pawn(w)
    tr = sorted(actors("TN_BeachTrampoline", w), key=lambda a: xyz(a)[0])[1]
    t = xyz(tr)
    return captura.shot("trampoline_chain", 135, fov=70, tags=["trampolín", "tortuga", "vuelo"],
                        desc="Rebote en un trampolín",
                        events={-10: (lambda ww: tp(p, [t[0] - 150, t[1], t[2] + 1300], 0.0))},
                        keys=[{"f": 0, "eye": [t[0] - 1200, t[1] - 2600, t[2] + 700], "look": [t[0], t[1], t[2] + 700]},
                              {"f": 134, "eye": [t[0] + 1400, t[1] - 3000, t[2] + 900], "look": [t[0] + 1800, t[1], t[2] + 500]}])


def crab_charge():
    w = world()
    p = pawn(w)
    crab = sorted(actors("TN_BeachGiantCrab", w), key=lambda a: xyz(a)[0])[2]
    c = xyz(crab)
    yaw = math.radians(crab.get_actor_rotation().yaw)
    fx, fy = math.cos(yaw), math.sin(yaw)
    sx, sy = c[0] + fx * 950, c[1] + fy * 950
    start = [sx, sy, _ground_at(sx, sy, w) + 120]
    face = math.degrees(math.atan2(-fy, -fx))
    to_crab = unreal.Vector(-fx, -fy, 0.0)
    return captura.shot("crab_charge", 165, fov=72, tags=["cangrejo", "ragdoll", "enemigo"],
                        desc="Cangrejo gigante que oye a la tortuga, embiste y la manda por los aires",
                        events={-12: (lambda ww: tp(p, start, face))},
                        each=lambda ww, i: p.add_movement_input(to_crab, 0.45, True) if 0 <= i <= 60 else None,
                        keys=[{"f": 0, "eye": [(c[0] + sx) / 2 - fy * 1900, (c[1] + sy) / 2 + fx * 1900, c[2] + 450],
                               "look": [(c[0] + sx) / 2, (c[1] + sy) / 2, c[2] + 150]},
                              {"f": 164, "eye": [(c[0] + sx) / 2 - fy * 2400 + fx * 600, (c[1] + sy) / 2 + fx * 2400 + fy * 600, c[2] + 700],
                               "look": [sx, sy, c[2] + 250]}])


def gull_grab():
    w = world()
    p = pawn(w)
    gz = sorted(actors("TN_BeachGullZone", w), key=lambda a: xyz(a)[0])[0]
    g = xyz(gz)
    spot = [g[0], g[1], _ground_at(g[0], g[1], w) + 120]
    return captura.shot("gull_grab", 180, fov=80, tags=["gaviota", "enemigo", "tortuga"],
                        desc="La sombra crece, la gaviota baja en picado, coge a la tortuga y se la lleva",
                        events={-14: (lambda ww: tp(p, spot, 0.0)), 5: "TN.Beach.Gull.Attack 2"},
                        follow=follow_actor(lambda ww: pawn(ww), (-1500, -1500, 700), (0, 0, 500), smooth=0.1, fov=80))


def quad_pass():
    w = world()
    p = pawn(w)
    lane = sorted(actors("TN_BeachQuadLane", w), key=lambda a: xyz(a)[0])[0]
    l = xyz(lane)
    spot = [l[0] + 900, l[1] + 1500, _ground_at(l[0] + 900, l[1] + 1500, w) + 120]
    return captura.shot("quad_pass", 200, fov=85, tags=["quads", "temblor", "enemigo"],
                        desc="Paso de quads gigantes junto a la cámara",
                        events={-14: (lambda ww: tp(p, spot, 180.0)), 0: "TN.Beach.Quad.Now"},
                        keys=[{"f": 0, "eye": [l[0] + 1700, l[1] + 2400, l[2] + 250], "look": [l[0], l[1] - 3000, l[2] + 350]},
                              {"f": 199, "eye": [l[0] + 1500, l[1] + 2000, l[2] + 300], "look": [l[0], l[1] + 3000, l[2] + 350]}])


def mine_boom():
    w = world()
    p = pawn(w)
    mine = sorted(actors("TN_BeachMine", w), key=lambda a: xyz(a)[0])[4]
    m = xyz(mine)
    start = [m[0] - 450, m[1], m[2] + 150]
    return captura.shot("mine_boom", 120, fov=70, tags=["mina", "explosión", "tortuga"],
                        desc="La tortuga pisa una mina de juguete y sale despedida en bola",
                        events={-12: (lambda ww: tp(p, start, 0.0))},
                        each=lambda ww, i: p.add_movement_input(unreal.Vector(1, 0, 0), 0.6, True) if 0 <= i <= 45 else None,
                        keys=[{"f": 0, "eye": [m[0] - 300, m[1] - 1700, m[2] + 450], "look": [m[0], m[1], m[2] + 150]},
                              {"f": 119, "eye": [m[0] - 900, m[1] - 2100, m[2] + 750], "look": [m[0] - 500, m[1], m[2] + 250]}])


def storm_front():
    w = world()
    p = pawn(w)
    x0 = 6000.0
    start = [x0, 0, _ground_at(x0, 0, w) + 120]
    return captura.shot("storm_front", 165, fov=80, tags=["tormenta", "tortuga", "persecución"],
                        desc="La tormenta de bañistas persigue a la tortuga",
                        events={-14: (lambda ww: tp(p, start, 0.0)), -10: "TN.Beach.Storm.Start 18 420", 164: "TN.Beach.Storm.Stop"},
                        each=run_forward(p, 0.0),
                        follow=follow_actor(lambda ww: pawn(ww), (1300, 500, 350), (-900, 0, 450), smooth=0.2, fov=80))


def worm_eat():
    w = world()
    p = pawn(w)
    x0 = 30000.0
    spot = [x0, -2000, _ground_at(x0, -2000, w) + 120]

    def unhide(ww):
        pp = pawn(ww)
        pp.set_actor_hidden_in_game(False)
        pp.set_actor_enable_collision(True)
    return captura.shot("worm_eat", 120, fov=70, tags=["gusano", "tortuga", "gigante"],
                        desc="El gusano de arena gigante sale y se traga a la tortuga",
                        events={-12: (lambda ww: tp(p, spot, 0.0)), 2: "TN.Beach.Worm 0", 119: unhide},
                        keys=[{"f": 0, "eye": [x0 - 2800, -2000 - 2200, spot[2] + 500], "look": [x0, -2000, spot[2] + 900]},
                              {"f": 119, "eye": [x0 - 3600, -2000 - 2600, spot[2] + 1100], "look": [x0, -2000, spot[2] + 1600]}])


def turtles_run():
    w = world()
    p = pawn(w)
    pups = _puppets(w)
    x0 = 22000.0
    runners = [p] + pups
    for k, r in enumerate(runners):
        y = -900 + 600 * k
        tp(r, [x0, y, _ground_at(x0, y, w) + 150], 0.0)

    def push(ww, i):
        if i >= -5:
            for r in runners:
                r.add_movement_input(unreal.Vector(1, 0, 0), 1.0, True)
    return captura.shot("turtles_run", 180, fov=75, tags=["tortugas", "grupo", "carrera"],
                        desc="Cuatro tortugas corriendo juntas hacia la meta",
                        each=push,
                        follow=follow_actor(lambda ww: pawn(ww), (900, 400, 180), (-300, 350, 60), smooth=0.25, fov=75))


def _start(w):
    gen = actors("TN_BeachRaceGenerator", w)[0]
    t = gen.get_start_transform(0).translation
    return [t.x, t.y, t.z]


def eggs_hatch():
    w = world()
    s = _start(w)
    return captura.shot("eggs_hatch", 150, fov=62, tags=["salida", "huevos", "tortuga"],
                        desc="Los huevos de la salida se abren y lanzan a la cría",
                        events={-12: "TN.Beach.Egg"},
                        keys=[{"f": 0, "eye": [s[0] + 700, s[1] - 900, s[2] + 60], "look": [s[0], s[1] + 200, s[2] - 20]},
                              {"f": 149, "eye": [s[0] + 1600, s[1] - 1200, s[2] + 300], "look": [s[0] + 1500, s[1], s[2] + 100]}])


def macro_can():
    w = world()
    p = pawn(w)
    s = _start(w)
    spot = [s[0] + 1500, s[1] + 1500, _ground_at(s[0] + 1500, s[1] + 1500, w) + 110]
    return captura.shot("macro_can", 150, fov=55, tags=["macro", "escala", "tortuga"],
                        desc="La cría diminuta junto a una lata gigante",
                        events={-16: (lambda ww: tp(p, spot, 0.0)), -12: "TN.Beach.Place SodaCan 1 0 7"},
                        keys=[{"f": 0, "eye": [spot[0] - 420, spot[1] - 220, spot[2] - 50], "look": [spot[0] + 500, spot[1], spot[2] + 200]},
                              {"f": 149, "eye": [spot[0] - 300, spot[1] - 120, spot[2] - 60], "look": [spot[0] + 500, spot[1], spot[2] + 380]}])


def enemies_parade():
    return captura.shot("enemies_parade", 150, fov=75, tags=["enemigos", "cangrejo", "tanque", "erizo"],
                        desc="Travelling bajo entre cangrejos, erizos, lagartos y tanques de juguete",
                        keys=[{"f": 0, "eye": [13500, -9800, 3650], "look": [18500, -6500, 3100]},
                              {"f": 75, "eye": [16000, -4000, 3500], "look": [19500, -500, 3050]},
                              {"f": 149, "eye": [17500, 2500, 3450], "look": [21000, 4500, 3000]}])


def cliff_dive():
    w = world()
    p = pawn(w)
    edge_x = 80000.0
    x0 = edge_x - 1400.0
    spot = [x0, 0, _ground_at(x0, 0, w) + 150]
    return captura.shot("cliff_dive", 150, fov=78, tags=["meta", "zambullida", "agua"],
                        desc="Salto de cabeza desde el acantilado al mar",
                        events={-12: (lambda ww: tp(p, spot, 0.0))},
                        each=run_forward(p, 0.0),
                        follow=follow_actor(lambda ww: pawn(ww), (-900, -1300, 350), (400, 0, -200), smooth=0.15, fov=78))


# ── Segunda tanda: acciones que se disparan sobre la tortuga del jugador, con la cámara siguiéndola ──

def _open_spot(w, x, y=0.0):
    return [x, y, _ground_at(x, y, w) + 120]


def crab_charge2():
    """Un cangrejo gigante recién puesto delante de la tortuga la oye, embiste y la derriba."""
    w = world()
    p = pawn(w)
    s = _start(w)
    spot = _open_spot(w, s[0] + 900, s[1])
    return captura.shot("crab_charge", 165, fov=74, tags=["cangrejo", "ragdoll", "enemigo"],
                        desc="Cangrejo gigante que embiste a la tortuga y la manda por los aires",
                        events={-16: (lambda ww: tp(p, spot, 0.0)), -12: "TN.Beach.Place GiantCrab 1 0 5"},
                        each=lambda ww, i: p.add_movement_input(unreal.Vector(1, 0, 0), 0.35, True) if 0 <= i <= 50 else None,
                        follow=follow_actor(lambda ww: pawn(ww), (-700, -1500, 420), (500, 0, 80), smooth=0.1, fov=74))


def macro_can2():
    w = world()
    p = pawn(w)
    s = _start(w)
    spot = _open_spot(w, s[0] + 1500, s[1] + 1500)
    return captura.shot("macro_can", 150, fov=55, tags=["macro", "escala", "tortuga"],
                        desc="La cría diminuta junto a una lata gigante",
                        events={-16: (lambda ww: tp(p, spot, 0.0)), -12: "TN.Beach.Place SodaCan 1 0 7"},
                        keys=[{"f": 0, "eye": [spot[0] - 380, spot[1] - 260, spot[2] - 40], "look": [spot[0] + 350, spot[1] + 40, spot[2] + 60]},
                              {"f": 149, "eye": [spot[0] - 260, spot[1] - 160, spot[2] - 55], "look": [spot[0] + 350, spot[1] + 40, spot[2] + 140]}])


def trampoline2():
    w = world()
    p = pawn(w)
    tr = sorted(actors("TN_BeachTrampoline", w), key=lambda a: xyz(a)[0])[1]
    t = xyz(tr)
    return captura.shot("trampoline_chain", 150, fov=76, tags=["trampolín", "tortuga", "vuelo"],
                        desc="Rebote en un trampolín",
                        events={-10: (lambda ww: tp(p, [t[0] - 100, t[1], t[2] + 1100], 0.0))},
                        follow=follow_actor(lambda ww: pawn(ww), (-900, -1400, 300), (0, 0, 100), smooth=0.12, fov=76))


def worm_eat2():
    w = world()
    p = pawn(w)
    s = _start(w)
    spot = _open_spot(w, s[0] + 2600, s[1] - 800)

    def unhide(ww):
        pp = pawn(ww)
        pp.set_actor_hidden_in_game(False)
        pp.set_actor_enable_collision(True)
    return captura.shot("worm_eat", 130, fov=72, tags=["gusano", "tortuga", "gigante"],
                        desc="El gusano de arena gigante sale y se traga a la tortuga",
                        events={-14: (lambda ww: tp(p, spot, 180.0)), 4: "TN.Beach.Worm 0", 129: unhide},
                        keys=[{"f": 0, "eye": [spot[0] + 1700, spot[1] - 1300, spot[2] + 300], "look": [spot[0], spot[1], spot[2] + 500]},
                              {"f": 129, "eye": [spot[0] + 2300, spot[1] - 1700, spot[2] + 700], "look": [spot[0], spot[1], spot[2] + 1300]}])


def quad_pass2():
    w = world()
    p = pawn(w)
    lane = sorted(actors("TN_BeachQuadLane", w), key=lambda a: xyz(a)[0])[0]
    l = xyz(lane)
    spot = _open_spot(w, l[0], l[1] + 600)
    return captura.shot("quad_pass", 210, fov=90, tags=["quads", "temblor", "enemigo"],
                        desc="Paso de quads gigantes junto a la tortuga",
                        events={-14: (lambda ww: tp(p, spot, 90.0)), 0: "TN.Beach.Quad.Now"},
                        keys=[{"f": 0, "eye": [l[0] + 1500, l[1] + 900, spot[2] + 150], "look": [l[0], l[1] - 1500, spot[2] + 300]},
                              {"f": 209, "eye": [l[0] + 1400, l[1] + 700, spot[2] + 200], "look": [l[0], l[1] + 1500, spot[2] + 300]}])


def mine_boom2():
    w = world()
    p = pawn(w)
    s = _start(w)
    spot = _open_spot(w, s[0] + 1200, s[1] - 2500)
    return captura.shot("mine_boom", 120, fov=70, tags=["mina", "explosión", "tortuga"],
                        desc="La tortuga pisa una mina de juguete y sale despedida en bola",
                        events={-16: (lambda ww: tp(p, spot, 0.0)), -12: "TN.Beach.Place Mine 1 0 3"},
                        each=lambda ww, i: p.add_movement_input(unreal.Vector(1, 0, 0), 0.5, True) if 0 <= i <= 70 else None,
                        follow=follow_actor(lambda ww: pawn(ww), (-300, -1300, 350), (300, 0, 80), smooth=0.12, fov=70))


def storm_front2():
    w = world()
    p = pawn(w)
    s = _start(w)
    spot = _open_spot(w, s[0] + 6000, s[1])
    return captura.shot("storm_front", 165, fov=82, tags=["tormenta", "tortuga", "persecución"],
                        desc="La tormenta de bañistas persigue a la tortuga",
                        events={-16: (lambda ww: tp(p, spot, 0.0)), -12: "TN.Beach.Storm.Start 16 430", 164: "TN.Beach.Storm.Stop"},
                        each=run_forward(p, 0.0),
                        follow=follow_actor(lambda ww: pawn(ww), (1200, 450, 320), (-1200, 0, 550), smooth=0.2, fov=82))


def turtles_run2():
    w = world()
    p = pawn(w)
    pups = _puppets(w)
    s = _start(w)
    x0 = s[0] + 1800
    runners = [p] + pups
    for k, r in enumerate(runners):
        y = s[1] - 700 + 450 * k
        tp(r, [x0, y, _ground_at(x0, y, w) + 150], 0.0)

    def push(ww, i):
        if i >= -8:
            for r in runners:
                r.add_movement_input(unreal.Vector(1, 0, 0), 1.0, True)
    return captura.shot("turtles_run", 150, fov=72, tags=["tortugas", "grupo", "carrera"],
                        desc="Cuatro tortugas corriendo juntas hacia la meta", each=push,
                        follow=follow_actor(lambda ww: pawn(ww), (800, 650, 170), (-200, 600, 70), smooth=0.25, fov=72))


def cliff_dive2():
    w = world()
    p = pawn(w)
    gen = actors("TN_BeachRaceGenerator", w)[0]
    x0 = 80000.0 - 1600.0
    spot = _open_spot(w, x0, 0.0)
    return captura.shot("cliff_dive", 165, fov=78, tags=["meta", "zambullida", "agua"],
                        desc="Salto de cabeza desde el acantilado al mar",
                        events={-14: (lambda ww: tp(p, spot, 0.0))},
                        each=run_forward(p, 0.0),
                        follow=follow_actor(lambda ww: pawn(ww), (-700, -1500, 300), (500, 0, -250), smooth=0.14, fov=78))


def worm_eat3():
    w = world()
    p = pawn(w)
    spot = _open_spot(w, 42000.0, -1500.0)

    def unhide(ww):
        pp = pawn(ww)
        pp.set_actor_hidden_in_game(False)
        pp.set_actor_enable_collision(True)
    return captura.shot("worm_eat", 130, fov=70, tags=["gusano", "tortuga", "gigante"],
                        desc="El gusano de arena gigante sale y se traga a la tortuga",
                        events={-18: "TN.Beach.Storm.Stop", -14: (lambda ww: tp(p, spot, 180.0)), 4: "TN.Beach.Worm 0", 129: unhide},
                        keys=[{"f": 0, "eye": [spot[0] + 1600, spot[1] - 1600, spot[2] + 900], "look": [spot[0], spot[1], spot[2] + 400]},
                              {"f": 129, "eye": [spot[0] + 2200, spot[1] - 2000, spot[2] + 1300], "look": [spot[0], spot[1], spot[2] + 1100]}])


def cliff_dive3():
    w = world()
    p = pawn(w)
    return captura.shot("cliff_dive", 180, fov=80, tags=["meta", "zambullida", "agua"],
                        desc="Salto de cabeza desde el acantilado al mar",
                        events={-18: "TN.Beach.Storm.Stop", -14: "TN.Beach.Go acantilado"},
                        each=run_forward(p, 0.0),
                        follow=follow_actor(lambda ww: pawn(ww), (-900, -1300, 650), (400, 0, -300), smooth=0.14, fov=80))


def turtles_run3():
    w = world()
    p = pawn(w)
    pups = _puppets(w)
    x0 = 30000.0
    runners = [p] + pups

    def place(ww):
        for k, r in enumerate(runners):
            y = -700 + 450 * k
            tp(r, [x0, y, _ground_at(x0, y, ww) + 150], 0.0)

    def push(ww, i):
        if i >= -8:
            for r in runners:
                r.add_movement_input(unreal.Vector(1, 0, 0), 1.0, True)
    return captura.shot("turtles_run", 150, fov=70, tags=["tortugas", "grupo", "carrera"],
                        desc="Cuatro tortugas corriendo juntas hacia la meta", each=push,
                        events={-18: "TN.Beach.Storm.Stop", -14: place},
                        follow=follow_actor(lambda ww: pawn(ww), (1000, 700, 750), (0, 600, 40), smooth=0.25, fov=70))


def mine_boom3():
    w = world()
    p = pawn(w)
    mine = sorted(actors("TN_BeachMine", w), key=lambda a: xyz(a)[0])[len(actors("TN_BeachMine", w)) // 2]
    m = xyz(mine)
    start = [m[0] - 500, m[1], m[2] + 150]
    return captura.shot("mine_boom", 120, fov=68, tags=["mina", "explosión", "tortuga"],
                        desc="La tortuga pisa una mina de juguete y sale despedida en bola",
                        events={-18: "TN.Beach.Storm.Stop", -14: (lambda ww: tp(p, start, 0.0))},
                        each=lambda ww, i: p.add_movement_input(unreal.Vector(1, 0, 0), 0.5, True) if 0 <= i <= 70 else None,
                        keys=[{"f": 0, "eye": [m[0] - 400, m[1] - 1400, m[2] + 700], "look": [m[0], m[1], m[2] + 100]},
                              {"f": 119, "eye": [m[0] - 700, m[1] - 1700, m[2] + 900], "look": [m[0] - 300, m[1], m[2] + 300]}])
