#requires -Version 7.0
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio検出ツールが見つかりません。' }
$vsRoot = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if (-not $vsRoot) { throw 'C++ビルドツールが見つかりません。' }
# LauncherのJSON再検出は日本語出力の文字コードに依存するため、検出済みパスを公式モジュールへ渡す。
Import-Module (Join-Path $vsRoot 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vsRoot -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$outDir = Join-Path $projectRoot 'Saved/Tests/Core'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$sources = @(Get-ChildItem (Join-Path $projectRoot 'Tests/Unit') -Filter '*.cpp' | Select-Object -ExpandProperty FullName)
foreach ($layer in @('Domain','Application','AI','Generation','Infrastructure')) {
    $path = Join-Path $projectRoot "Source/GirlsSpire/$layer"
    if (Test-Path $path) { $sources += @(Get-ChildItem $path -Filter '*.cpp' -Recurse | Select-Object -ExpandProperty FullName) }
}
Push-Location $outDir
try {
    & cl.exe /nologo /std:c++20 /EHsc /W4 /WX /utf-8 /Od /MP4 "/I$projectRoot/Source/GirlsSpire" $sources /Fe:CoreTests.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $outDir 'CoreTests.exe')
    exit $LASTEXITCODE
} finally { Pop-Location }
