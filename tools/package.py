"""Build release artifacts into dist/:
  dist/SpaceBarPause-v<version>.zip      - GitHub / ModDB download
  dist/workshop/content/                 - Steam Workshop item content
  dist/workshop/workshop_item.vdf        - steamcmd +workshop_build_item script

Usage: python package.py   (run build.sh first)
"""
import os
import re
import shutil
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PKG = os.path.join(ROOT, "package")
DIST = os.path.join(ROOT, "dist")
FOC_APPID = "32470"

FILES = [
    (os.path.join(ROOT, "build", "winmm.dll"), "winmm.dll"),
    (os.path.join(PKG, "Install.bat"), "Install.bat"),
    (os.path.join(PKG, "Uninstall.bat"), "Uninstall.bat"),
    (os.path.join(PKG, "install.ps1"), "install.ps1"),
    (os.path.join(PKG, "ReadMe.txt"), "ReadMe.txt"),
    (os.path.join(PKG, "SpaceBarPause.ini.example"), "SpaceBarPause.ini.example"),
    (os.path.join(ROOT, "LICENSE"), "LICENSE.txt"),
]


def version():
    src = open(os.path.join(ROOT, "src", "spacebar_pause.c")).read()
    return re.search(r'#define MOD_VERSION "([^"]+)"', src).group(1)


def main():
    ver = version()
    os.makedirs(DIST, exist_ok=True)

    zip_path = os.path.join(DIST, f"SpaceBarPause-v{ver}.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for src, name in FILES:
            z.write(src, f"SpaceBarPause/{name}")
    print("wrote", zip_path)

    content = os.path.join(DIST, "workshop", "content")
    shutil.rmtree(content, ignore_errors=True)
    os.makedirs(content)
    for src, name in FILES:
        shutil.copy2(src, os.path.join(content, name))

    # KeyValues escape handling varies between tools, so keep the values escape-free:
    # forward slashes in paths, and no backslashes or double quotes in the description.
    desc = open(os.path.join(PKG, "workshop_description.bbcode"), encoding="utf-8").read()
    if "\\" in desc or '"' in desc:
        raise SystemExit("workshop_description.bbcode must not contain backslashes or double quotes")
    slash = lambda p: p.replace("\\", "/")
    # The Workshop item ID (written after the first upload) makes later uploads update it.
    id_file = os.path.join(PKG, "workshop_item_id.txt")
    item_id = open(id_file).read().strip() if os.path.exists(id_file) else "0"
    # New items start private for review; updates keep whatever visibility the item has.
    visibility = '\t"visibility"\t"2"\n' if item_id == "0" else ""
    vdf = f'''"workshopitem"
{{
\t"appid"\t\t"{FOC_APPID}"
\t"publishedfileid"\t"{item_id}"
\t"contentfolder"\t"{slash(content)}"
\t"previewfile"\t"{slash(os.path.join(PKG, "art", "preview_512.png"))}"
{visibility}\t"title"\t\t"Space Bar Pause (+ orders while paused)"
\t"description"\t"{desc}"
\t"changenote"\t"v{ver}"
}}
'''
    with open(os.path.join(DIST, "workshop", "workshop_item.vdf"), "w", encoding="utf-8") as f:
        f.write(vdf)
    print("wrote", os.path.join(DIST, "workshop"))


if __name__ == "__main__":
    main()
