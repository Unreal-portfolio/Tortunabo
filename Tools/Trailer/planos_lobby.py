"""Planos del lobby para el tráiler (PIE en LVL_Lobby): castillo de arena, tienda, general y tortugas disfrazadas."""
import unreal

import captura
from rodaje import actors, follow_actor, orbit, pawn, tp, world, xyz


def _puppets(w):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Pawn) if a.actor_has_tag("TrailerPuppet")]


def lobby_castle():
    return captura.shot("lobby_castle", 150, fov=60, tags=["lobby", "castillo"],
                        desc="Órbita alrededor del castillo de arena del lobby",
                        keys=orbit([0, 0, 0], 3600.0, 1300.0, 120.0, 230.0, 150, fov=60, look_z=500.0))


def lobby_shop():
    w = world()
    k = xyz(actors("TN_ShopKeeper", w)[0])
    p = pawn(w)
    tp(p, [k[0] - 60, k[1] - 380, k[2] + 100], 90.0)
    return captura.shot("lobby_shop", 120, fov=55, tags=["tienda", "disfraces", "lobby"],
                        desc="La tienda de disfraces con su tendero",
                        keys=[{"f": 0, "eye": [k[0] + 700, k[1] - 900, k[2] + 260], "look": [k[0], k[1], k[2] + 120]},
                              {"f": 119, "eye": [k[0] + 350, k[1] - 600, k[2] + 180], "look": [k[0], k[1], k[2] + 110]}])


def lobby_general():
    w = world()
    g = xyz(actors("TN_GeneralBriefing", w)[0])
    return captura.shot("lobby_general", 120, fov=55, tags=["general", "lobby", "militar"],
                        desc="El general de Tortunavy en su mesa de mando",
                        keys=[{"f": 0, "eye": [g[0] - 700, g[1] - 800, g[2] + 280], "look": [g[0], g[1], g[2] + 120]},
                              {"f": 119, "eye": [g[0] - 250, g[1] - 650, g[2] + 200], "look": [g[0], g[1], g[2] + 130]}])


def lobby_lineup():
    """Cuatro tortugas disfrazadas en fila delante del castillo (viste a tu tortuga)."""
    w = world()
    p = pawn(w)
    hats = ["Helmet_Pirate", "Helmet_Captain", "Helmet_Straw", "Helmet_Bronze"]
    turtles = [p] + _puppets(w)
    for k, t in enumerate(turtles[:4]):
        tp(t, [-300 + 200 * k, 1300, 110], -90.0)
        try:
            t.update_helmet_mesh(hats[k % len(hats)])
        except Exception:  # noqa: BLE001
            pass
    return captura.shot("lobby_lineup", 135, fov=50, tags=["disfraces", "tortugas", "lobby", "grupo"],
                        desc="Tortugas con distintos gorros en fila",
                        events={40: (lambda ww: [t.request_wheel_emote(2) for t in turtles[:4] if hasattr(t, "request_wheel_emote")])},
                        keys=[{"f": 0, "eye": [-550, 800, 170], "look": [-200, 1300, 110]},
                              {"f": 134, "eye": [550, 850, 160], "look": [200, 1300, 110]}])
