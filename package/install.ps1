# Galactic Active Pause - installer / uninstaller for Star Wars: Empire at War - Forces of Corruption (Steam).
# Copies winmm.dll next to corruption\StarWarsG.exe, or removes it with -Uninstall.
param(
    [switch]$Uninstall,
    [string]$GameDir
)

$ErrorActionPreference = 'Stop'
$Marker = 'SpaceBarPause'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Test-GameDir([string]$dir) {
    if (-not $dir) { return $false }
    try {
        # Skip missing or empty drives (unplugged disks, card readers) instead of failing.
        $root = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($dir))
        if ($root -match '^[A-Za-z]:\\$' -and -not ([IO.DriveInfo]::new($root)).IsReady) { return $false }
        return [IO.File]::Exists([IO.Path]::Combine($dir, 'StarWarsG.exe'))
    } catch {
        return $false
    }
}

function Test-OurDll([string]$path) {
    $text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($path))
    $text.Contains($Marker)
}

function Find-GameDir {
    $candidates = @()
    # Steam Workshop download: <library>\steamapps\workshop\content\32470\<id>\
    $candidates += Join-Path $Here '..\..\..\..\common\Star Wars Empire at War\corruption'
    # Every Steam library listed in libraryfolders.vdf.
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -Name SteamPath -ErrorAction SilentlyContinue).SteamPath
    if ($steam) {
        $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
        $libraries = @($steam)
        if (Test-Path -LiteralPath $vdf) {
            $libraries += Select-String -LiteralPath $vdf -Pattern '"path"\s+"([^"]+)"' -AllMatches |
                ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value -replace '\\\\', '\' }
        }
        foreach ($lib in $libraries) {
            # Plain string join: Join-Path fails on drives that no longer exist (old libraries).
            $candidates += [IO.Path]::Combine($lib, 'steamapps\common\Star Wars Empire at War\corruption')
        }
    }
    foreach ($c in $candidates) {
        if (Test-GameDir $c) { return (Resolve-Path -LiteralPath $c).Path }
    }
    return $null
}

if (-not (Test-GameDir $GameDir)) { $GameDir = Find-GameDir }
while (-not (Test-GameDir $GameDir)) {
    Write-Host 'Could not find Forces of Corruption automatically.'
    $GameDir = Read-Host 'Paste the path of the "corruption" folder (the one containing StarWarsG.exe), or leave empty to cancel'
    if (-not $GameDir) { Write-Host 'Cancelled.'; exit 1 }
    $GameDir = $GameDir.Trim('"')
}

$target = Join-Path $GameDir 'winmm.dll'
Write-Host "Game folder: $GameDir"

if ($Uninstall) {
    if (-not (Test-Path -LiteralPath $target)) { Write-Host 'Galactic Active Pause is not installed.'; exit 0 }
    if (-not (Test-OurDll $target)) {
        Write-Host 'The winmm.dll in the game folder belongs to another mod; leaving it alone.'
        exit 1
    }
    Remove-Item -LiteralPath $target
    foreach ($f in 'SpaceBarPause.log') {
        $p = Join-Path $GameDir $f
        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p }
    }
    # Dialog layout the mod generated (adds the Pause/Play row to the keyboard options).
    $rc = Join-Path $GameDir 'Data\Resources\GUIDialog\GUIDialogs.rc'
    if ((Test-Path -LiteralPath $rc) -and (Select-String -LiteralPath $rc -SimpleMatch 'IDC_EDIT_GAME_OPTIONS' -Quiet)) {
        Remove-Item -LiteralPath $rc
    }
    Write-Host 'Galactic Active Pause uninstalled. (SpaceBarPause.ini, if you created one, was kept.)'
    exit 0
}

if ((Test-Path -LiteralPath $target) -and -not (Test-OurDll $target)) {
    Write-Host 'The game folder already contains a winmm.dll from another mod or tool.'
    $answer = Read-Host 'Replace it? A backup will be saved as winmm.dll.bak (y/N)'
    if ($answer -notmatch '^[yY]') { Write-Host 'Cancelled.'; exit 1 }
    Copy-Item -LiteralPath $target -Destination "$target.bak" -Force
}

Copy-Item -LiteralPath (Join-Path $Here 'winmm.dll') -Destination $target -Force
Write-Host 'Galactic Active Pause installed. Start Forces of Corruption and press Pause on the galactic map.'
