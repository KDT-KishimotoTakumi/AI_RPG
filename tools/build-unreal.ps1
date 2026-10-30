#requires -Version 7.0
param([switch] $Coverage)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
# UE/MSVCの応答ファイルが日本語パスを誤読する環境では、同一作業ツリーへASCII別名を作る。
$buildRoot = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    $sha = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($projectRoot))
    $suffix = [Convert]::ToHexString($sha).Substring(0, 12)
    $buildRoot = Join-Path ([IO.Path]::GetTempPath()) "GirlsSpire-$suffix"
    if (Test-Path -LiteralPath $buildRoot) {
        $alias = Get-Item -LiteralPath $buildRoot
        if ($alias.LinkType -ne 'Junction' -or $alias.Target -ne $projectRoot) { throw 'ビルド別名が別の対象を参照しています。' }
    } else {
        New-Item -ItemType Junction -Path $buildRoot -Target $projectRoot | Out-Null
    }
}
$manifest = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
$engine = (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).InstallationList |
    Where-Object AppName -EQ 'UE_5.7' | Select-Object -First 1
if (-not $engine) { throw '登録済みのUE5.7が見つかりません。' }
$build = Join-Path $engine.InstallLocation 'Engine/Build/BatchFiles/Build.bat'
$oldCoverage = $env:GIRLS_SPIRE_COVERAGE
$oldLlvm = $env:LLVM_PATH
try {
    $env:GIRLS_SPIRE_COVERAGE = $(if ($Coverage) { '1' } else { $null })
    if ($Coverage) { $env:LLVM_PATH = Join-Path $buildRoot 'Saved/Tools/clang+llvm-20.1.8-x86_64-pc-windows-msvc' }
    & $build GirlsSpireEditor Win64 Development "-Project=$buildRoot/GirlsSpire.uproject" -WaitMutex -NoHotReloadFromIDE -NoUBA -NoUBTMakefiles
    if ($LASTEXITCODE) { throw "UEビルド失敗: $LASTEXITCODE" }
} finally {
    $env:GIRLS_SPIRE_COVERAGE = $oldCoverage
    $env:LLVM_PATH = $oldLlvm
}
