# Changelog

## v1.1.1 — 2026-10-23

- Fixed: the galactic clock and production kept running while paused. The orders-while-paused change had also unlocked the galactic time step; now only player orders are allowed and time stays frozen.
- Fixed: Install.bat stopped with a "cannot find drive" error when Steam still listed a library on a drive that is no longer connected.

## v1.1.0 — 2026-10-22

- Renamed from *Space Bar Pause* to **Galactic Active Pause**.
- The pause key is now **Pause** by default and can be rebound in *Options > Keyboard > Interface > Pause/Play*. Space is left to the game's Battle Cam.
- The "Game Paused" banner appears instantly instead of sliding in.
- The `Key=` setting in `SpaceBarPause.ini` is removed; set the key in the game options instead.
- Uninstall.bat also removes the keyboard-options layout the mod adds.

## v1.0.0 — 2026-10-13

- First release: pause and resume the galactic map with Space.
- While you have the game paused, you can still move fleets, queue production and move troops between the surface and orbit.
