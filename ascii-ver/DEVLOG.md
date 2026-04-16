# STARFLIGHT — ASCII Terminal Edition
## Developer Log & Technical Reference

---

## What Is This?

This is a tribute ASCII terminal re-implementation of **Starflight** (Binary Systems / Electronic Arts, 1986 PC / 1989 C64).  
It runs in a Linux/macOS terminal using the **ncurses** library and is written in a single C file.

---

## How to Build & Play

```bash
# Install ncurses dev headers if needed (Debian/Ubuntu)
sudo apt install libncurses-dev

# Build
gcc -o starflight starflight.c -lncurses -lm

# Run (terminal must be at least 80 columns × 30 rows)
./starflight
```

---

## Game Overview

| Aspect | Detail |
|--------|--------|
| Year | 4620 — humanity rediscovers spaceflight |
| Goal | Discover why stars are flaring; stop the Crystal Planet |
| Currency | Resource Units (RU) |
| Fuel | Endurium — mined on planets, bought at starport |
| Crew | Up to 6 members: Captain, Nav, Eng, Med, Sci, Gun |
| Map | 50 procedurally seeded star systems, ~200 planets |

---

## Screen Layout

```
┌─ STARFLIGHT: ASCII TERMINAL EDITION ─────── Hull:100 Shld:60 Fuel:250 RU:2500 ─┐  row 0
│                                       │ CREW                                     │
│                                       │ COMMANDER  Cap 100%                      │
│   MAIN VIEW                           │ Zax Dorn   Nav  85%                      │
│   (changes per game state)            │                                          │  rows 1-19
│                                       │ CARGO: 5/50                              │
│                                       │ Nexidium  x3                             │
│                                       │ Cobalt    x8                             │
├─ CAPTAIN'S LOG ────────────────────────────────────────────────────────────────── │  row 20
│ > Year 4620 — The people of Arth rediscover spaceflight.                         │
│ > Hire your crew, outfit your ship, explore the galaxy.                          │  rows 21-25
│ > Find the source of the stellar flares. Save your world.                        │
├─ CONTROLS ─────────────────────────────────────────────────────────────────────── │  row 26
│ [W/A/S/D] Move   [ENTER] Confirm   [L] Land   [H] Hail   [ESC/Q] Back           │  rows 27-29
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Game States (State Machine)

```
                        ┌──────────┐
                        │  TITLE   │
                        └────┬─────┘
                             │ ENTER
                             ▼
                      ┌─────────────┐
              ┌──────▶│  STARPORT   │◀──────────────────────┐
              │       └──────┬──────┘                        │
              │ [P]          │ [8] Launch                    │
              │              ▼                               │
              │       ┌─────────────┐  on star              │
              │       │  HYPERSPACE │──────────────┐        │
              │       └──────┬──────┘              │        │
              │         ENTER│                     │ alien  │
              │              ▼                     ▼        │
              │       ┌─────────────┐      ┌─────────────┐  │
              │       │    ORBIT    │─────▶│  ENCOUNTER  │  │
              │       └──────┬──────┘ [H]  └──────┬──────┘  │
              │          [L] │                    │[C] attack │
              │              ▼                    ▼          │
              │       ┌─────────────┐      ┌─────────────┐  │
              │       │   SURFACE   │      │   COMBAT    │  │
              │       │(terrain TV) │      │(turn-based) │  │
              │       └──────┬──────┘      └──────┬──────┘  │
              │         [ESC]│                [4] │retreat   │
              └─────────────┘◀───────────────────┘         │
                    return to orbit                         │
                             │ [ESC] leave orbit            │
                             └──────────────────────────────┘
```

---

## Code Architecture

### File Structure (single file)

```
starflight.c
│
├── CONSTANTS        — screen dims, limits, fuel costs
├── ENUMS            — GState, Race, Role, PType, AType
├── STRUCTS          — Crew, Cargo, Ship, Star, Surf
├── GLOBALS          — ship, gal[], surf, game state vars
├── STRING TABLES    — race/role/planet/alien names, art
│
├── UTILITIES        — logmsg(), clamp()
├── SETUP            — setup_colors(), init_galaxy(),
│                      init_ship(), make_hire_pool(),
│                      gen_surface()
│
├── DRAW HELPERS     — draw_box(), draw_status(),
│                      draw_right_panel(), draw_log(),
│                      draw_controls()
│
├── STARPORT         — draw_starport(), handle_starport()
├── HYPERSPACE       — draw_hyperspace(), handle_hyperspace()
├── ORBIT            — draw_orbit(), handle_orbit()
├── SURFACE          — draw_surface(), handle_surface()
├── ENCOUNTER        — draw_encounter(), handle_encounter()
├── COMBAT           — draw_combat(), handle_combat()
│
├── DISPATCH         — draw_all(), handle_input()
└── main()
```

### Key Data Structures

```c
Ship  — hull, shields, weapons, engines, fuel, RU, position,
        crew[6], cargo[8]

Star  — position, name, star class, planets[], alien presence,
        alien mood (0=hostile..100=friendly), visited flag

Surf  — ASCII terrain map[14][52], mineral map[14][52],
        terrain vehicle position, atmosphere/gravity/temp

Crew  — name, race, role, hp, skill, morale (all 0-100)
```

---

## Procedural Generation

### Galaxy Map

Seeds from **`srand(42)`** (fixed) to ensure the same galaxy every run.  
50 stars are placed at random `(x,y)` within a 72×18 grid.  
Star 0 ("Arth") is always placed at the centre.

```
Each star gets:
  cls     = rand() % 6          → "OBAFGK" star class
  ch      = SC[cls]             → display char: .*+oO@
  np      = 1 + rand() % 6      → 1–6 planets
  alien   = (rand() % 3 == 0)   → 33% chance of alien presence
  mood    = 30 + rand() % 50    → initial diplomacy score
  planets[p] = rand() % 7       → Rocky/Ocean/Volcanic/etc.
```

### Planet Surface

Seeds from `star_index * 13 + planet_index * 7 + 42`.  
Generates a 52×14 ASCII terrain map + mineral deposit positions.

```
For each tile:
  r = rand() % 10
  if r < 5  → base terrain char for planet type
  if r < 8  → '#' (rock/obstacle)
  else      → '.' (flat ground)

  18% chance of mineral deposit (type = rand() % 7)
```

---

## Game Systems

### Economy Loop

```
Mine minerals on surface
    ↓
Return to Arth Starport
    ↓
Sell cargo at starport  ──→  earn RU
    ↓
Spend RU on:
  • Fuel (Endurium) — 100 RU for 50 units
  • Weapons upgrade  — 500 × (level+1) RU
  • Shield upgrade   — 500 × (level+1) RU
  • Engine upgrade   — 500 × (level+1) RU
  • Crew hire        — 500 RU each
  • Hull repair      — 5 RU per point
```

### Mineral Values (RU per unit)

| Mineral | RU/unit |
|---------|---------|
| Endurium | 5 |
| Cobalt | 8 |
| Fluorine | 6 |
| Silicon | 3 |
| Nexidium | 12 |
| Zenon | 15 |
| Plutanium | 20 |

### Diplomacy System

Each alien race has a `mood` value (0–100).  
Your tone choices shift it:

```
Friendly  → +15 mood
Neutral   → no change
Hostile   → -25 mood

mood < 20  → alien attacks automatically
mood > 50  → trade offers accepted
mood > 40  → information sharing unlocked
```

### Combat (Turn-Based)

Each turn the player picks an action, then the enemy retaliates:

```
Player action:
  [1] Fire   → 50% hit chance, dmg = 8 + weap×5 + rand(10)
  [2] Shield → restore 20 shield points
  [3] Evade  → 40% + eng×10% chance to avoid enemy attack
  [4] Retreat → exit combat, return to orbit

Enemy attack:
  50% hit chance, dmg = 5 + rand(10 + alien_type×2)
  Shields absorb damage first, then hull
  hull ≤ 0 → game over
```

---

## Alien Races

| Race | Base Mood | Personality |
|------|-----------|-------------|
| Velox | 60 | Calm, observational |
| Thrynn | 40 | Territorial, aggressive |
| Elowan | 70 | Peaceful, empathic |
| Spemin | 55 | Cowardly, submissive |
| Gazurtoid | 30 | Predatory, territorial |
| Mechans | 35 | Logical, hostile to organics |

---

## Planet Types

| Type | Surface Char | Notes |
|------|-------------|-------|
| Rocky | `.` | Most common, good minerals |
| Ocean | `~` | Difficult terrain |
| Volcanic | `^` | Rare minerals, dangerous |
| Arctic | `*` | Cold, moderate minerals |
| Jungle | `T` | Dense terrain `#` |
| Desert | `Z` | Sparse minerals |
| Toxic | `%` | Dangerous atmosphere |

---

## Known Limitations / Future Work

- [ ] Endurium twist: reveal late-game that fuel is sentient
- [ ] Main story quest chain (Crystal Planet / stellar flares)
- [ ] Ruins discovery system on surface
- [ ] Lifeform capture mechanic
- [ ] Full alien dialogue trees (not just tone choices)
- [ ] Save/load game state
- [ ] Star chart overlay showing visited systems
- [ ] Crew skill effects on gameplay (navigator reduces fuel cost, etc.)
- [ ] Doctor skill for crew healing between turns

---

## ASCII Flowchart — Full Turn Sequence

```
  ┌─────────────────────────────────────────────────────┐
  │                   GAME TURN                         │
  └──────────────────────┬──────────────────────────────┘
                         │
           ┌─────────────┼─────────────┐
           ▼             ▼             ▼
      STARPORT       HYPERSPACE     SURFACE
           │             │             │
    buy/hire/sell    move ship       drive TV
           │             │             │
           │      reach star?     tile = mineral?
           │         YES │           YES │
           │             ▼             ▼
           │          ORBIT          MINE
           │         /     \       (add to cargo)
           │      land     hail
           │       │         │
           │    SURFACE   ENCOUNTER
           │                 │
           │          mood < 20?
           │            YES │
           │                ▼
           │             COMBAT
           │            /       \
           │        win          die
           │         │            │
           │      get RU       GAME OVER
           │         │
           └─────────┘
               loop
```

---

*Built with ncurses. Tribute to Binary Systems & Electronic Arts.*  
*"Endurium — it was alive the whole time."*
