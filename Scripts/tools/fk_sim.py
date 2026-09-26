"""Simulador de las poses de TN_TurtleAnimInstance sobre la postura en T (posiciones en unidades de la malla).

Sirve para diseñar y comprobar poses sin abrir el editor: reproduce Turn() de TN_TurtleAnimInstance.cpp (giro en el
espacio de la malla alrededor de la articulación, con los hijos detrás) sobre la postura de referencia de
TotugaDemo_Rig. La malla mira a +Y, arriba +Z y su izquierda es +X (unidades de malla; el personaje la escala x2,5).

Ejemplo:
    from fk_sim import *
    P = Pose()
    P.turn('LeftArm', 'Y', -78); P.turn('LeftForeArm', 'Y', -40)
    print(P.p['LeftHand'], clear(P, 'Left'))   # posición de la muñeca y holgura (unidades) con la cabeza

Datos (sacados del editor con AnimPoseExtensions y de los vértices de la malla):
    data/turtle_ref_pose.txt  hueso|?|posición en malla|giro en malla|posición local|giro local
    data/turtle_geo.json      {"v": vértices antes del skinning, "t": triángulos, "m": ranura de material}
    data/turtle_clip_samples.txt  cadera y pies de los clips Walking, Drunk_Run_Forward y Old_Man_Idle a 20 Hz
"""
import math, json, os
HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
PAR = {}
chain = lambda names: [PAR.__setitem__(names[i], names[i-1]) for i in range(1, len(names))]
chain(['Hips','Spine','Spine1','Spine2','Neck','Head','HeadTop_End'])
for s in ('Left','Right'):
    chain(['Spine2', s+'Shoulder', s+'Arm', s+'ForeArm', s+'Hand', s+'HandIndex1', s+'HandIndex2', s+'HandIndex3', s+'HandIndex4'])
    chain(['Hips', s+'UpLeg', s+'Leg', s+'Foot', s+'ToeBase', s+'Toe_End'])
REF = {}
for line in open(os.path.join(HERE, 'turtle_ref_pose.txt')):
    f = line.strip().split('|')
    if len(f) < 3: continue
    REF[f[0]] = tuple(float(v) for v in f[2].split())
def kids(b):
    out = [b]
    for c, p in PAR.items():
        if p == b: out += kids(c)
    return out
def rot(axis, deg, v):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    x, y, z = v
    if axis == 'X': return (x, y*c - z*s, y*s + z*c)
    if axis == 'Y': return (x*c + z*s, y, -x*s + z*c)
    return (x*c - y*s, x*s + y*c, z)
class Pose:
    def __init__(s): s.p = dict(REF)
    def turn(s, b, axis, deg):
        o = s.p[b]
        for k in kids(b):
            v = tuple(s.p[k][i] - o[i] for i in range(3))
            r = rot(axis, deg, v)
            s.p[k] = tuple(o[i] + r[i] for i in range(3))
    def lift(s, u):
        for k in s.p: s.p[k] = (s.p[k][0], s.p[k][1], s.p[k][2] + u)
V = json.load(open(os.path.join(HERE, 'turtle_geo.json')))['v']
HEAD = [v for v in V if v[2] > 37.5 and abs(v[0]) < 11]
def head_hit(pose, pts):
    # distancia mínima de los puntos a los vértices de la cabeza (la cabeza gira con Neck/Head: aquí solo se usa en poses sin giro de cabeza)
    return min(min(math.dist(p, h) for h in HEAD) for p in pts)
def hand_pts(pose, side):
    return [pose.p[side + n] for n in ('Hand', 'HandIndex1', 'HandIndex2', 'HandIndex3', 'HandIndex4')]
def fmt(v): return '(%.1f %.1f %.1f)' % v

# Vértices de la cabeza como hijos del hueso Head (siguen sus giros).
for i, v in enumerate(HEAD[::4]):
    PAR['hv%d' % i] = 'Head'
    REF['hv%d' % i] = tuple(v)
HEADKEYS = ['hv%d' % i for i in range(len(HEAD[::4]))]
def clear(pose, side):
    pts = [pose.p[side + n] for n in ('ForeArm', 'Hand', 'HandIndex1', 'HandIndex2', 'HandIndex3', 'HandIndex4')]
    # puntos intermedios del antebrazo
    e, w = pose.p[side + 'ForeArm'], pose.p[side + 'Hand']
    pts += [tuple(e[i] + (w[i] - e[i]) * t for i in range(3)) for t in (0.33, 0.66)]
    return min(min(math.dist(p, pose.p[h]) for h in HEADKEYS) for p in pts)
