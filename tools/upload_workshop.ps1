# Uploads dist/workshop to the Steam Workshop with SteamCMD.
# Run python tools/package.py first. SteamCMD prompts for your password and Steam Guard code.
param([string]$SteamCmd = 'C:\steamcmd\steamcmd.exe')

$ErrorActionPreference = 'Stop'
$vdf = Join-Path $PSScriptRoot '..\dist\workshop\workshop_item.vdf' | Resolve-Path
if (-not (Test-Path -LiteralPath $SteamCmd)) { throw "SteamCMD not found at $SteamCmd" }

$account = Read-Host 'Steam account name (login name, not display name)'
if (-not $account) { Write-Host 'Cancelled.'; exit 1 }

& $SteamCmd +login $account +workshop_build_item "$vdf" +quit

$id = Select-String -LiteralPath $vdf -Pattern '"publishedfileid"\s+"(\d+)"' |
    ForEach-Object { $_.Matches[0].Groups[1].Value }
if ($id -and $id -ne '0') {
    Write-Host ''
    Write-Host "Workshop item ID: $id"
    Write-Host "Page: https://steamcommunity.com/sharedfiles/filedetails/?id=$id"
}
