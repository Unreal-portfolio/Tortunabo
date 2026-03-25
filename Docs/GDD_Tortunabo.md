# [INSERTAR IMAGEN: PORTADA DEL JUEGO]

# Tortunabo — Game Design Document (GDD)

---

## 1. Portada

**Título:** Tortunabo  
**Versión:** [POR DEFINIR]  
**Fecha:** 23/03/2026  
**Autor:** [POR DEFINIR]  

[INSERTAR IMAGEN: LOGO DEL JUEGO]

---

## 2. Ficha Técnica

| Elemento                | Detalle                                  |
|------------------------|------------------------------------------|
| Género                 | Cooperativo, Party Game, Aventura        |
| Jugadores              | 1-4 (multijugador online, Steam)         |
| Plataforma             | PC (Unreal Engine 5.6)                   |
| Motor                  | Unreal Engine 5.6                        |
| Estado                 | [POR DEFINIR]                            |
| Público objetivo       | [POR DEFINIR]                            |
| Estilo visual          | Tercera persona, cartoon, colorido       |
| Perspectiva            | 3ª persona (over-the-shoulder)           |
| Control                | Teclado y ratón                          |
| Idioma                 | [POR DEFINIR]                            |
| Clasificación          | [POR DEFINIR]                            |

---

## 3. Dirección de Arte

### Estilo Visual
- Estética cartoon, colorida y amigable.
- Personajes animales antropomórficos (tortugas, cangrejos, etc.).
- Entornos playeros, tropicales y veraniegos.
- Interfaces limpias y legibles.

### UI/UX
- HUD minimalista: barra de vida, estamina, hotbar de inventario, indicadores de estado.
- Menús claros, navegación intuitiva.
- Indicadores visuales para interacción, revive, emotes y penalizaciones.

### Referencias Visuales
- [INSERTAR IMAGEN: MOODBOARD GENERAL]
- [INSERTAR IMAGEN: UI/HUD EJEMPLO]
- [INSERTAR IMAGEN: PERSONAJES PRINCIPALES]
- [INSERTAR IMAGEN: ENTORNOS Y OBSTÁCULOS]

---

## 4. Narrativa

### Premisa
[PENDIENTE DE DEFINIR]

### Tono
- Humorístico, ligero, competitivo y cooperativo.

### Personajes / Roles
- Jugadores: tortugas personalizables.
- NPCs: sargento, socorristas, vendedores, aliados y enemigos (ver sección de enemigos/aliados).

### Progresión Narrativa por Niveles
- [POR DEFINIR]

---

## 5. Jugabilidad

### Core Loop
1. Los jugadores aparecen en el lobby.
2. Personalizan su personaje y se preparan.
3. Inician la partida y atraviesan niveles con obstáculos, enemigos y aliados.
4. Colaboran y compiten para llegar al final o sobrevivir.
5. Al terminar, vuelven al lobby para repetir el ciclo.

### Cámara
- Tercera persona (over-the-shoulder), con spring arm y FOV dinámico.

### Controles
- [INSERTAR PLACEHOLDER: ESQUEMA DE CONTROLES]

### Mecánicas Principales
- **Andar:** movimiento base en strafe.
- **Correr:** Shift para sprintar (consume estamina, no ilimitado).
- **Saltar:** impulso vertical para superar obstáculos.
- **Coger/Soltar/Lanzar Objetos:** interactuar, almacenar en inventario, soltar o lanzar (con trayectoria visible).
- **Consumir:** uso de ítems consumibles para obtener beneficios.
- **Interactuar con Entorno:** botón de interacción (palancas, NPCs, etc.).
- **Empujar Objetos:** aplicar fuerza para moverlos.
- **Reanimar:** revivir compañeros caídos durante un tiempo limitado.
- **Arrastrarse Abatido:** movimiento lento en estado de inconsciencia.
- **Emotes:** bailes y gestos mediante rueda radial.
- **Personalización:** cambio de skins y cosméticos.

### Sistemas del Jugador
- **Vida:** barra de vida, reducción por daño; muerte si llega a cero.
- **Estamina:** se consume al correr, se recupera caminando/parado o con consumibles.
- **Revivir:** elemento interactuable en el nivel permite revivir sin coste.
- **Inventario:** máximo 3 objetos, hotbar central, cambio de slot con números/scroll.
- **Peso:** cada objeto suma peso; exceso limita velocidad/movimiento.
- **Deshidratación:** agua corporal disminuye con el tiempo y bajo sol; sin agua no regenera estamina.
- **Spawn de Objetos:** objetos aparecen por zonas específicas.

### Estados Especiales
- **Inconsciencia:** tras perder toda la vida, solo puede arrastrarse; cuenta atrás para ser reanimado.
- **Muerte:** si no es reanimado a tiempo, entra en modo espectador.
- **Espectador:** observa a otros jugadores tras morir.

### Emotes y Personalización
- Rueda radial de emotes (bailes, gestos).
- Personalización de skins y accesorios desde el lobby.

### Wireframe
- [INSERTAR IMAGEN: WIREFRAME GENERAL DE HUD Y MENÚS]

---

## 6. División por Niveles

### Lobby (Zona Central)
- **Punto de spawn:** llegada de jugadores.
- **Taquilla de personalización:** selección de cosméticos.
- **Punto de ayuda:** NPCs con información útil.
- **Tienda:** canjeo de puntos por cosméticos.
- **Sargento:** información sobre el siguiente nivel.
- **Circuito de obstáculos:** práctica de mecánicas.
- **Arena:** minijuego opcional de empujar fuera del círculo.
- **Zonas adicionales:** coleccionables, álbum de logros, etc. [PENDIENTE]
- [INSERTAR IMAGEN: SKETCH DEL LOBBY]

#### Zonas del Lobby
1. **Personalización**
   - Tienda de accesorios.
   - Vestuarios para cambiar objetos.
2. **Minijuegos**
   - Campo de tiro para probar objetos.
   - Campo de entrenamiento para practicar habilidades.
3. **Ambientación**
   - Baños.
   - Puesto de mando (NPC general).
   - Torres de vigilancia (inicio de misión).

### Niveles de Juego
- [INSERTAR PLACEHOLDER: LISTADO DE NIVELES SEGÚN SHEET]
- [INSERTAR IMAGEN: SKETCHES DE NIVELES]

#### Obstáculos, Enemigos y Entorno
- **Aliados:**
  - Anélido poliqueto: boost de estamina.
  - Mini charcos de pesca.
  - Rampas inclinadas.
- **Enemigos Aéreos:**
  - Gaviota: ataque de agarre y bombardeo.
- **Enemigos Terrestres:**
  - Cangrejo dinámico: rutas laterales, persecución.
  - Medusa estática: trampolín y veneno.
- **Enemigos Bajo Tierra:**
  - Cangrejo estático: pinza sorpresa.
  - Erizo: veneno al pisar.
- **Entorno Hostil:**
  - Arenas movedizas, socorristas, tormentas, gas tóxico, tormenta de arena.
- **Neutros:**
  - Algas, basura, plataformas rompibles, marea, remolino de arena, alambre de espino, muros, erizos checos, trincheras, búnker, minas.

#### Objetos
- **No consumibles:**
  - Mochila: aumenta capacidad de inventario.
- **Consumibles:**
  - Medusas: estamina infinita temporal, luego veneno.
  - Cocos: aumenta estamina máxima temporalmente.
  - Camarones: recuperan hambre.
  - Pez globo: reduce daño recibido, luego mareo.
  - Barrita de algas: elimina hambre.
  - Detector de objetos: pitidos según cercanía.
  - Caña de pescar: rescate y pesca de objetos.
- **Puzles:** [PLACEHOLDER]

---

## 7. Conectividad / Multiplayer

### Infraestructura
- Arquitectura servidor-cliente (Unreal Engine).
- Host local (usuario principal).
- Conexión mediante Steam.
- Máximo 4 jugadores por sala.

### Tipos de Sala
- **Públicas:** acceso libre, aparecen en lista.
- **Privadas:** acceso por código de 4 letras (450.976 combinaciones), no aparecen en lista pública.

### Métodos de Conexión
- Invitación por Steam.
- Búsqueda de sala pública.
- Introducción de clave de acceso.

### Flujo de Conexión
- Host invita o crea sala (pública/privada).
- Jugadores se unen por invitación, búsqueda o clave.
- En lobby, todos deben marcar "preparados" para iniciar.
- Al iniciar partida, se cierra la sala a nuevas conexiones.
- Al finalizar partida (victoria, derrota o decisión del host), todos regresan al lobby.
- Si el host abandona, todos vuelven al menú principal.

---

## 8. Referencias

- **Peak** (referencia principal de peso e inventario)
- **Repo** (referencia de mecánicas y tono)
- [INSERTAR IMAGEN: REFERENCIAS VISUALES DE OTROS JUEGOS]
- [INSERTAR LINK: SHEET DE NIVELES Y DISEÑO]
  - https://docs.google.com/spreadsheets/d/1Yjgaw_VrzjqObQIcKl2F9EstFzelipZQUI6FiDdln2c/edit?gid=0#gid=0

---

[FIN DEL DOCUMENTO]

