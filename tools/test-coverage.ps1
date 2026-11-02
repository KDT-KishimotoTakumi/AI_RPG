#requires -Version 7.0
param([switch] $CoreOnly)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'coverage-gate.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
$suffix = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($projectRoot))).Substring(0,12)
$root = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    $root = Join-Path ([IO.Path]::GetTempPath()) "GirlsSpire-$suffix"
    $alias = Get-Item -LiteralPath $root
    if ($alias.LinkType -ne 'Junction' -or $alias.Target -ne $projectRoot) { throw '先にbuild-unreal.ps1で正しい別名を作成してください。' }
}
$llvm = Join-Path $root 'Saved/Tools/clang+llvm-20.1.8-x86_64-pc-windows-msvc'
foreach ($tool in @('clang-cl', 'llvm-cov', 'llvm-profdata')) {
    if (-not (Test-Path "$llvm/bin/$tool.exe")) { throw "LLVM 20.1.8の$tool がありません。" }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
Import-Module (Join-Path $vsRoot 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vsRoot -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$out = Join-Path $root ('Saved/Coverage/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$sources = @(Get-ChildItem "$root/Tests/Unit" -Filter '*.cpp' | Select-Object -ExpandProperty FullName)
foreach ($layer in @('Domain','Application','AI','Generation','Infrastructure')) {
    if (Test-Path "$root/Source/GirlsSpire/$layer") { $sources += @(Get-ChildItem "$root/Source/GirlsSpire/$layer" -Filter '*.cpp' -Recurse | Select-Object -ExpandProperty FullName) }
}
$oldProfile = $env:LLVM_PROFILE_FILE
Push-Location $out
try {
    & "$llvm/bin/clang-cl.exe" /nologo /std:c++20 /EHsc /W4 /WX /utf-8 /Od /MT /clang:-fprofile-instr-generate /clang:-fcoverage-mapping "/I$root/Source/GirlsSpire" $sources /Fe:CoreTests.exe
    if ($LASTEXITCODE) { throw '計装コンパイルに失敗しました。' }
    $env:LLVM_PROFILE_FILE = "$out/core-%p.profraw"
    & "$out/CoreTests.exe"
    if ($LASTEXITCODE) { throw '単体テストが失敗しました。' }
    $objects = @("$out/CoreTests.exe")
    if (-not $CoreOnly) {
        & (Join-Path $PSScriptRoot 'build-unreal.ps1') -Coverage
        if ($LASTEXITCODE) { throw 'UE計装ビルドに失敗しました。' }
        $env:LLVM_PROFILE_FILE = "$out/unreal-%p.profraw"
        & (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire'
        if ($LASTEXITCODE) { throw 'UEテストに失敗しました。' }
        $env:LLVM_PROFILE_FILE = "$out/runtime-%p.profraw"
        & (Join-Path $PSScriptRoot 'test-runtime.ps1') -SkipBuild
        if ($LASTEXITCODE) { throw '通常ゲーム起動の計測試験に失敗しました。' }
        $runtimeProfiles=@(Get-ChildItem $out -Filter 'runtime-*.profraw')
        if($runtimeProfiles.Count -lt 1 -or ($runtimeProfiles | Measure-Object Length -Sum).Sum -le 0){throw '通常起動の計測原本が空です。'}
        $env:LLVM_PROFILE_FILE = "$out/runtimeaudio-%p.profraw"
        & (Join-Path $PSScriptRoot 'test-runtime.ps1') -SkipBuild -Audio
        if ($LASTEXITCODE) { throw '音声有効の通常ゲーム・別プロセス再開に失敗しました。' }
        $runtimeAudioProfiles=@(Get-ChildItem $out -Filter 'runtimeaudio-*.profraw')
        if($runtimeAudioProfiles.Count -ne 2 -or @($runtimeAudioProfiles | Where-Object Length -le 0).Count){throw '音声有効の起動/再開の計測原本が空または不足です。'}
        $env:LLVM_PROFILE_FILE = "$out/audio-%p.profraw"
        & (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Audio -Filter 'GirlsSpire.Audio.Engine' -ExtraArgs @('-GirlsSpireAudioTest','-AllowCommandletAudio')
        if ($LASTEXITCODE) { throw '音声有効時の再生試験に失敗しました。' }
        $audioProfiles=@(Get-ChildItem $out -Filter 'audio-*.profraw')
        if($audioProfiles.Count -lt 1 -or ($audioProfiles | Measure-Object Length -Sum).Sum -le 0){throw '音声経路の計測原本が空です。'}
        $objects += '-object'; $objects += "$root/Binaries/Win64/UnrealEditor-GirlsSpire.dll"
    }
    $profiles = @(Get-ChildItem $out -Filter '*.profraw' | Select-Object -ExpandProperty FullName)
    & "$llvm/bin/llvm-profdata.exe" merge -sparse $profiles -o "$out/coverage.profdata"
    if ($LASTEXITCODE) { throw 'プロファイル統合に失敗しました。' }
    # 宣言専用ファイルは内容まで照合する。後から実装が足された場合は必ず失敗させる。
    $declarations = Get-Content (Join-Path $PSScriptRoot 'coverage-declarations.json') -Raw | ConvertFrom-Json
    $declarationPaths = @($declarations | ForEach-Object {
        $path = Join-Path $root $_.path
        $content = (Get-Content -LiteralPath $path -Raw).Replace("`r`n", "`n")
        $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($content)))
        if ($hash -ne $_.sha256) { throw "宣言専用ファイルが変更されました。計測対象を再確認してください: $path" }
        $path
    })
    $inventory = @(Get-ChildItem "$root/Source/GirlsSpire" -Recurse -File | Where-Object { $_.Extension -in '.cpp','.h' -and $_.FullName -notin $declarationPaths } | Select-Object -ExpandProperty FullName)
    $json = & "$llvm/bin/llvm-cov.exe" export @objects "-instr-profile=$out/coverage.profdata" $inventory 2> "$out/coverage-diagnostics.txt"
    if ($LASTEXITCODE) { throw 'レポート作成に失敗しました。' }
    if ((Get-Content "$out/coverage-diagnostics.txt" -Raw) -match 'mismatched|hash-mismatch') { throw '計測データの不一致があります。ビルドとテストを見直してください。' }
    $json | Set-Content "$out/coverage.json" -Encoding utf8
    $gate = Test-CoverageGate ($json | ConvertFrom-Json) $inventory
    $gate | ConvertTo-Json -Depth 5 | Set-Content "$out/gate.json" -Encoding utf8
    $gate | ConvertTo-Json -Depth 5
    "レポート: $out"
    "宣言のみ（内容ハッシュ照合済み）: $($declarations.path -join ', ')"
    if (-not $gate.Passed) { throw '全自作C++の90%ゲートは未達です。' }
    # 全体計測に合格し集計を保持できた回だけ、自身の生データを整理する。
    if (-not $CoreOnly) { & (Join-Path $PSScriptRoot 'cleanup-measurements.ps1') -ReportId (Split-Path -Leaf $out) }
} finally {
    $env:LLVM_PROFILE_FILE = $oldProfile
    Pop-Location
}
