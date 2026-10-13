# Publishing checklist

First build everything:

```sh
sh build.sh
python tools/package.py
```

This produces:
- `dist/SpaceBarPause-v<version>.zip`
- `dist/workshop/content/`
- `dist/workshop/workshop_item.vdf`

## GitHub

```sh
git tag v<version> && git push --tags
gh release create v<version> dist/SpaceBarPause-v<version>.zip --title "Space Bar Pause v<version>" --notes "..."
```

## Steam Workshop (Forces of Corruption, app 32470)

Steam has no in-game uploader for EaW, so the upload goes through Valve's **SteamCMD**.

1. One time only: download SteamCMD from Valve. The page is <https://developer.valvesoftware.com/wiki/SteamCMD>, and the file is `steamcmd.zip` from `steamcdn-a.akamaihd.net`. Extract it to, for example, `C:\steamcmd`.
2. One time only: accept the Steam Workshop agreement at <https://steamcommunity.com/sharedfiles/workshoplegalagreement>. Steam refuses uploads until you do.
3. Run the upload. It asks for your Steam account name; SteamCMD then asks for your password and Steam Guard code itself.

   ```bat
   powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\upload_workshop.ps1
   ```

4. The item ID is stored in `package/workshop_item_id.txt` (currently `3819157144`), and `tools/package.py` writes it into the vdf.
   - With the ID set, every later upload updates the same item and keeps its visibility.
   - Delete the file only if you want to create a brand-new item.
5. A brand-new item is uploaded as **private** (`"visibility" "2"`). Open it at `https://steamcommunity.com/sharedfiles/filedetails/?id=<ID>` and check the text and image.
   - Optionally add `package/art/banner_1280x720.png` as an extra screenshot ("Add/edit images & videos").
   - When it looks right, set visibility to **Public**.

The Workshop can't put the DLL in the game folder, so the description tells players to run `Install.bat` once from the item's download folder.

## ModDB

Live page: <https://www.moddb.com/mods/space-bar-pause-orders-while-paused>. It is a mod profile, not an addon, because ModDB addon categories only cover content. For a new version, upload the zip from that page's **Files > Add file** (Category: Full Version), using "upload a file from a URL" with the GitHub release link.

The steps below are how the profile was first created.

1. Log in at <https://www.moddb.com>.
2. Open the addon upload form at <https://www.moddb.com/addons/add>. Pick the game **Star Wars: Empire at War: Forces of Corruption** (<https://www.moddb.com/games/star-wars-empire-at-war-forces-of-corruption>).
3. Fill in the form:
   - **Name:** `Space Bar Pause`
   - **Category:** `Other` (or `Patches`, if the form offers it)
   - **Licence:** `MIT`
   - **File:** `dist/SpaceBarPause-v<version>.zip`
   - **Image/preview:** `package/art/banner_1280x720.png`
   - **Summary:** use the summary below
   - **Description:** use the description below
4. Submit. ModDB staff review new uploads before they go public, usually within about a day. Code mods with linked source tend to pass faster.

### ModDB summary (max ~200 chars)

> Press SPACE to pause/resume the galactic map, and keep giving orders while paused: move fleets, queue production, shuttle troops between surface and orbit.

### ModDB description

```
Space Bar Pause adds a proper "active pause" to galactic conquest in Forces of Corruption.

FEATURES
- Press SPACE on the galactic map to pause, and press it again to resume. The on-screen Pause/Play button stays in sync.
- Orders while paused: move fleets, queue production on planets, and move troops between the surface and orbit while you have the game paused. Nothing moves or builds until you unpause.
- Orders are still refused while a battle is pending or a story script is holding the pause.
- Tactical battles are not affected. In battles, Space keeps its normal Camera Center function.
- Works alongside data mods such as Awakening of the Rebellion and Thrawn's Revenge, including Workshop mods, because it changes no game files.

INSTALL
Extract the zip and double-click Install.bat. It finds your Steam game folder automatically.
Manual install: copy winmm.dll into ...\steamapps\common\Star Wars Empire at War\corruption\
Uninstall: run Uninstall.bat, or delete that winmm.dll.

REQUIREMENTS
Steam 64-bit Forces of Corruption, current 2024 patch. If a future game update changes the executable, the mod turns itself off safely and the game runs normally.

SETTINGS
Rename SpaceBarPause.ini.example to SpaceBarPause.ini (in the corruption folder) to change the key or turn off orders while paused.

OPEN SOURCE
MIT licensed. Full source and technical notes: https://github.com/nandusmasta/swfoc-spacebar-pause
It is a small DLL that changes the game's code in memory, so some antivirus programs may flag it. You can read and build the full source yourself.

By nanduskaiser.
```
