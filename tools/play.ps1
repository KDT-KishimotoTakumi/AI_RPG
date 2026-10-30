#requires -Version 7.0
param([switch] $SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-unreal.ps1')
    if ($LASTEXITCODE) { throw '通常ゲームのビルドに失敗しました。' }
}
$manifest = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
$engine = (Get-Content $manifest -Raw | ConvertFrom-Json).InstallationList | Where-Object AppName -eq 'UE_5.7' | Select-Object -First 1
if (-not $engine) { throw '登録済みのUE5.7がありません。' }
$launchRoot = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    $suffix = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($projectRoot))).Substring(0,12)
    $taskAlias = Join-Path ([IO.Path]::GetTempPath()) "GirlsSpire-$suffix"
    $link = Get-Item -LiteralPath $taskAlias
    if ($link.LinkType -ne 'Junction' -or $link.Target -ne $projectRoot) { throw '起動用の別名がプロジェクトを指していません。' }
    $launchRoot = $taskAlias
}
& (Join-Path $engine.InstallLocation 'Engine/Binaries/Win64/UnrealEditor.exe') "$launchRoot/GirlsSpire.uproject" -game /Engine/Maps/Entry -windowed -ResX=1920 -ResY=1080
