# CG Lab Project — Grid Breach 2D

> Qt 6 Widgets · C++17 · First-person raycasting · 2D grid world

**Grid Breach** is a first-person survival shooter built from a two-dimensional tile map. Move through the facility, aim down corridors, manage ammunition, and survive five waves ending in a boss encounter. A tactical map shows the same grid used for walls, collision, visibility, and enemy navigation.

The first-person view uses **raycasting**: a software renderer projects a 2D grid into a perspective image. The game stores positions in two dimensions and draws with `QPainter`; it does not use OpenGL, 3D meshes, or a 3D physics engine. This is the retro, sometimes called “2.5D,” approach to a first-person game.

This is a separate project alongside `FirstPersonShooter_CG`. It retains the Qt/C++ file structure of that project and adds movement, combat, enemy behavior, progression, and an ending.

## 1. Project overview

| Feature | Behavior |
|---|---|
| World | A 24 × 24 tile facility with solid walls and navigable corridors |
| View | First-person raycasting plus a toggleable tactical grid map |
| Objective | Clear five waves and defeat the final Sentinel |
| Difficulty | Easy, Standard, and Hard |
| Weapons | Rifle and shotgun with separate magazines and ammunition reserves |
| Movement | Walk, strafe, sprint, and dash |
| Survival | Health, shields, ammunition pickups, and explosive barrels |
| Enemies | Guards, fast Runners, explosive Bombers, and the Sentinel boss |
| Progression | Choose upgrades between waves |
| Feedback | Weapon recoil, muzzle flashes, hit markers, damage flashes, and sound |
| Records | Local best scores saved separately for each difficulty |

## 2. Repository layout

```text
GridBreach2D/
├── GridBreach2D.pro       # Qt Widgets / C++17 qmake project
├── main.cpp              # QApplication entry point
├── gamewidget.h          # Game state, entities, and widget declaration
├── gamewidget.cpp        # Input, movement, combat, AI, waves, and persistence
├── rendering.cpp         # Raycast world, sprites, weapon, HUD, and menus
├── soundbank.h           # Bounded asynchronous sound playback interface
├── soundbank.cpp         # Platform audio-player processes
├── sounds.qrc            # Bundled WAV resource manifest
├── sounds/
│   ├── *.wav             # Ten original synthesized sound effects
│   └── generate_sounds.py # Reproducible asset generator; Python standard library
├── tests/                # Game checks
├── build/                # Generated build artifacts, ignored by Git
└── README.md
```

## 3. Building and running

You need **Qt 6 with Widgets**, its `qmake`, a **C++17 compiler**, and **make**. Qt Multimedia is not required.

From this project directory:

```bash
mkdir -p build
cd build
"${QMAKE:-qmake}" ../GridBreach2D.pro
make -j8
```

If Qt is not on your `PATH`, set `QMAKE` to your Qt installation before those commands. For the Qt installation used in this workspace:

```bash
export QMAKE=/Users/rittickdutta/Qt/6.11.1/macos/bin/qmake
```

Run on macOS from `build/`:

```bash
open GridBreach2D.app
```

Run on Linux from `build/`:

```bash
./GridBreach2D
```

Alternatively, open `GridBreach2D.pro` in Qt Creator, select a Qt 6 desktop kit, and run it. Build products are local to this folder; the earlier game has its own project and build directory.

Run `./run.sh` from this folder to build and launch automatically on macOS or Linux. Set `QMAKE` if Qt 6 is installed outside the usual locations.

## 4. Controls

| Input | Action |
|---|---|
| W / S | Move forward / backward |
| A / D | Strafe left / right |
| Mouse | Turn and aim while playing |
| Left / Right arrow | Turn using the keyboard |
| Left mouse button | Fire; hold for repeated shots |
| R | Reload the selected weapon |
| 1 / 2 | Select rifle / shotgun |
| Shift | Sprint |
| Space | Dash |
| Esc | Pause / resume and release / capture mouse aiming |
| Tab | Toggle the tactical map |
| M | Mute / enable sound |
| F | Toggle fullscreen |

Select a difficulty and start a run from the menu. Between waves, select an upgrade to continue. The results screen appears after victory or defeat. Pausing freezes gameplay, and losing window focus pauses the run.

## 5. Architecture and game loop

```text
QApplication
└── GameWidget
    ├── QTimer + QElapsedTimer → elapsed-time simulation → repaint
    ├── State machine → menu / playing / paused / upgrade / won / lost
    ├── 2D tile map → wall collision, line of sight, raycasting, navigation
    ├── Entity vectors → enemies, pickups, barrels, projectiles, effects
    ├── QPainter → first-person world, HUD, map, and menus
    ├── SoundBank → bounded asynchronous effect playback
    └── QSettings → best score per difficulty
```

`gamewidget.cpp` owns gameplay. `rendering.cpp` draws that state without creating another world model. Movement and animation use elapsed time so their speed does not depend on how often painting occurs. The game limits simulation advancement after long stalls and clears held inputs when pausing or losing focus.

The player and all enemies have continuous `(x, y)` positions inside the tile grid. Walls remain integer tile cells. That combination allows smooth movement without losing the simple, inspectable grid representation.

## 6. Computer graphics and navigation

### Raycasting

For each vertical slice of the first-person image, cast a ray through the tile map and find its first wall intersection. Project that distance into a wall height: nearby walls occupy more screen space and distant walls occupy less. Correcting distance relative to the view direction prevents the curved-wall “fisheye” appearance. Wall-face direction and distance provide visual shading.

Enemies, pickups, barrels, and effects are projected from their world coordinates into the same camera view. Wall depth determines which parts of those objects are visible. The tactical map displays their positions from above, making the connection between the 2D map and the first-person view visible.

### Collision and visibility

The player's radius is checked against nearby wall cells. Movement is resolved along each axis, allowing the player to slide along walls. Weapon and enemy visibility checks use the tile map, so walls block shots and line of sight.

### Enemy navigation

A breadth-first search through walkable tiles builds a distance field toward the player's current cell. Enemies can follow lower-distance neighboring cells around walls instead of simply walking straight through obstacles. The field is shared between enemies and refreshed periodically.

## 7. Sound

Ten short effects are bundled as Qt resources: `rifle`, `shotgun`, `hit`, `explosion`, `hurt`, `pickup`, `reload`, `wave`, `victory`, and `defeat`. They are original synthesized mono PCM WAVs, created by the included Python script; no downloaded audio packs are needed.

`SoundBank` copies resources into a temporary directory and plays them asynchronously with macOS `afplay`, or Linux `paplay` / `aplay` when available. Two playback slots per weapon allow overlapping shots; each other effect has one slot. Busy slots skip additional requests, keeping process counts bounded and avoiding delayed sound queues. Muting or pausing stops active effects. Missing audio-player software leaves the game playable without audio.

Regenerate the assets from the project directory with:

```bash
python3 sounds/generate_sounds.py
```

Audio is disabled when `GRIDBREACH_TEST` is set or Qt uses the `offscreen` / `minimal` platform, allowing silent automated checks.

## 8. Local records

Best scores are saved with `QSettings` under organization **CG Lab**, application **Grid Breach 2D**, separately for Easy, Standard, and Hard. These records belong to this game and do not overwrite Pixel Reflex's scores. Rankings are local to the current computer; no account or network service is required.

## 9. Project scope

The game is a single-player desktop prototype designed to make grid-based graphics algorithms playable: perspective projection, wall intersections, occlusion, collision, line of sight, and breadth-first navigation. The current facility has a complete five-wave run with upgrades and win/loss states. Future extensions could add new maps, remappable controls, additional enemy attacks, and an integrated audio backend.

Shields absorb 65% of incoming damage while available. Each cleared wave supplies ammunition and a small health/shield top-up. The final wave contains the Sentinel; clearing it ends the run and awards a completion bonus.
# fps_2d
