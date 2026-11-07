#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $WindowsRoot,
    [string] $ArtRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Content/Art')
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$package = Get-Item -LiteralPath $WindowsRoot
if (-not $package.PSIsContainer) { throw 'Windows配布フォルダを指定してください。' }

$files = @(Get-ChildItem -LiteralPath $package.FullName -File -Recurse -Force)
foreach ($file in $files) {
    $relative = [IO.Path]::GetRelativePath($package.FullName, $file.FullName).Replace('\','/')
    if ($file.Extension -in '.md','.pdb' -or $relative -match '(^|/)SaveGames/' -or
        $relative -match '(^|/)Config/DefaultEngine\.ini$') {
        throw "配布対象外ファイル: $relative"
    }
}
foreach ($relative in @(
    'GirlsSpire.exe',
    'GirlsSpire/Binaries/Win64/GirlsSpire-Win64-Shipping.exe',
    'Engine/Extras/Redist/en-us/vc_redist.x64.exe',
    'GirlsSpire/Content/Paks/GirlsSpire-Windows.utoc',
    'GirlsSpire/Content/Paks/GirlsSpire-Windows.ucas',
    'GirlsSpire/Content/Paks/GirlsSpire-Windows.pak',
    'GirlsSpire/Content/Paks/global.utoc',
    'GirlsSpire/Content/Paks/global.ucas'
)) {
    $required = Get-Item -LiteralPath (Join-Path $package.FullName $relative) -ErrorAction SilentlyContinue
    if (-not $required -or $required.PSIsContainer -or $required.Length -le 0) {
        throw "配布必須ファイルが不足: $relative"
    }
}

$art = (Resolve-Path -LiteralPath $ArtRoot).Path
$packagedArt = Join-Path $package.FullName 'GirlsSpire/Content/Art'
$images = @(Get-ChildItem -LiteralPath $art -Filter '*.png' -File -Recurse)
if (-not $images.Count) { throw '照合元のゲーム画像がありません。' }
foreach ($source in $images) {
    $relative = [IO.Path]::GetRelativePath($art, $source.FullName)
    $target = Join-Path $packagedArt $relative
    if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "配布画像が不足: $relative" }
    if ((Get-FileHash -LiteralPath $source.FullName).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
        throw "画像SHA256不一致: $relative"
    }
}

$launcherManifest = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
$engine = (Get-Content -LiteralPath $launcherManifest -Raw | ConvertFrom-Json).InstallationList |
    Where-Object AppName -eq 'UE_5.7' | Select-Object -First 1
if (-not $engine) { throw '登録済みUE 5.7が見つかりません。' }

$report = Join-Path $projectRoot ('Saved/Tests/DistributionAudit/' + [guid]::NewGuid().ToString('N'))
$packagePrefix = $package.FullName.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
if ($report.StartsWith($packagePrefix,[StringComparison]::OrdinalIgnoreCase)) {
    throw '検査結果を配布フォルダの外に保存できません。'
}
New-Item -ItemType Directory -Path $report | Out-Null
$csv = Join-Path $report 'container.csv'
$container = Join-Path $package.FullName 'GirlsSpire/Content/Paks/GirlsSpire-Windows.utoc'
& (Join-Path $engine.InstallLocation 'Engine/Binaries/Win64/UnrealPak.exe') "-ListContainer=$container" "-Csv=$csv" *> (Join-Path $report 'container.log')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $csv -PathType Leaf)) {
    throw "IoStore一覧の取得に失敗しました。証跡: $report"
}
$sounds = @('BGM_Castle_V2','SFX_Confirm_V2','SFX_Impact_V2')
foreach ($name in $sounds) {
    if (-not (Select-String -LiteralPath $csv -SimpleMatch "/Audio/$name.uasset" -Quiet)) {
        throw "配布音声が不足: $name"
    }
}
@{WindowsRoot=$package.FullName; PackageFiles=$files.Count; ImageHashesMatched=$images.Count; AudioEntries=$sounds} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $report 'result.json')
"配布同梱検査成功: 画像 $($images.Count) 点、現行音声 $($sounds.Count) 点。証跡: $report"
