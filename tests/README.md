# Simulation checks

These checks exercise the real game simulation without needing the Qt Test
module. All checks remain enabled in release builds. The executable disables
sound and mouse capture and puts its scores/settings in a temporary directory,
so it does not overwrite the player's saved records.

From `GridBreach2D`:

```sh
mkdir -p tests/build
cd tests/build
qmake ../tests.pro
make -j4
QT_QPA_PLATFORM=offscreen ./gridbreach_tests
```

Coverage includes map connectivity and boundaries, axis-aligned ray distances,
line-of-sight and wall collision, hitscan damage and scoring, ammunition and
reload conservation, weapon switching, pause/reset, pickups, shields, defeat,
all five waves and their upgrades, boss/victory, and inactive gameplay controls.

The checks inspect the simulation directly for deterministic fixtures; rendering
and sound still need a visual and listening check in the game itself.
