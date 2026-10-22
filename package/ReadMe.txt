SPACE BAR PAUSE  v1.1.0
for Star Wars: Empire at War - Forces of Corruption (Steam version)
by nanduskaiser - MIT License - https://github.com/nandusmasta/swfoc-spacebar-pause

WHAT IT DOES
  * Press the PAUSE key on the galactic map to pause. Press it again to resume.
    The "Game Paused" banner appears immediately instead of sliding in.
  * The key can be changed in the game: Options > Keyboard > Interface > Pause/Play.
    (Space is the game's default Battle Cam key, so Pause is used by default.)
  * While you have paused the game, you can still give orders: move fleets, queue
    production on planets, and move troops between the surface and orbit.
    Nothing moves or builds until you unpause.
  * The on-screen Pause/Play button stays in sync with the key.
  * Tactical battles are not affected.

INSTALL
  1. Double-click Install.bat. It finds the game automatically. If it can't,
     it asks for the "corruption" folder (the folder that contains StarWarsG.exe).
  2. Start Forces of Corruption normally.
  If you subscribed on the Steam Workshop, you still need to run Install.bat once.
  To find it, right-click the game in Steam > Manage > Browse local files, then go up
  to steamapps\workshop\content\32470\<this item's ID>\.

  Manual install: copy winmm.dll into
    ...\steamapps\common\Star Wars Empire at War\corruption\

UNINSTALL
  Double-click Uninstall.bat. It removes winmm.dll and the keyboard-options layout the
  mod adds (corruption\Data\Resources\GUIDialog\GUIDialogs.rc).

CHECKING IT WORKS
  After you start the game, corruption\SpaceBarPause.log should say:
    "Space Bar Pause v1.1.0 active: Pause/Play key in Options > Keyboard > Interface."

SETTINGS (optional)
  Rename SpaceBarPause.ini.example to SpaceBarPause.ini and put it in the corruption
  folder. You can use it to turn off orders while paused.

COMPATIBILITY
  * Requires the current Steam 64-bit release of Forces of Corruption (2024 patch).
  * Works with data mods such as Awakening of the Rebellion and Thrawn's Revenge,
    including Workshop mods. The only game file it adds is the keyboard-options layout;
    if a mod ships its own layout, the Pause/Play row is missing from that mod's options
    screen, but the key still works.
  * After a game update that changes StarWarsG.exe, the mod turns itself off safely
    and writes "Unsupported StarWarsG.exe build" to the log. The game then runs normally.
  * Not for the base game (Empire at War without the expansion).
  * Multiplayer: the game does not allow galactic pausing in multiplayer, so the mod
    does nothing there.
  * If you already use another winmm.dll (some tools do), the installer asks before
    replacing it and keeps a backup.

ANTIVIRUS NOTE
  The mod is a small DLL that changes the game's code in memory, so some antivirus
  programs may flag it. The full source code is on GitHub (link above).
