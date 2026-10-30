#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
$engine = (Get-Content $manifest -Raw | ConvertFrom-Json).InstallationList |
    Where-Object AppName -EQ 'UE_5.7' | Select-Object -First 1
if (-not $engine) { throw '登録済みのUE5.7が見つかりません。' }
$root = $projectRoot
if ($root -match '[^\x00-\x7F]') {
    $suffix = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($projectRoot))).Substring(0,12)
    $root = Join-Path ([IO.Path]::GetTempPath()) "GirlsSpire-$suffix"
    $alias = Get-Item -LiteralPath $root
    if ($alias.LinkType -ne 'Junction' -or $alias.Target -ne $projectRoot) { throw '先にbuild-unreal.ps1を実行してください。' }
}
& (Join-Path $engine.InstallLocation 'Engine/Binaries/Win64/UnrealEditor.exe') "$root/GirlsSpire.uproject" '-ExecCmds=GirlsSpire.Preview' -nosplash
