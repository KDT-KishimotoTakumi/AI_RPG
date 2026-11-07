#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $WindowsRoot,
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string] $ExpectedTitleSHA256
)

$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Windowsで実行してください。' }
$package = Get-Item -LiteralPath $WindowsRoot
if (-not $package.PSIsContainer) { throw 'Windows配布フォルダを指定してください。' }
$exe = Join-Path $package.FullName 'GirlsSpire/Binaries/Win64/GirlsSpire-Win64-Shipping.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Shipping実行ファイルが不足しています。' }
if (Get-Process GirlsSpire,GirlsSpire-Win64-Shipping -ErrorAction SilentlyContinue) {
    throw '実行中のGirls Spireを閉じてから起動確認を実行してください。'
}

$root = Split-Path -Parent $PSScriptRoot
$id = [guid]::NewGuid().ToString('N')
$report = Join-Path $root "Saved/Tests/ShippingSmoke/$id"
$prefix = $package.FullName.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
if ($report.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {
    throw '試験保存先を配布フォルダの外に置いてください。'
}
New-Item -ItemType Directory -Path $report | Out-Null
$before = @{}
foreach ($file in Get-ChildItem -LiteralPath $package.FullName -File -Recurse -Force) {
    $before[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
}

if (-not ('GirlsSpireShippingDesktop' -as [type])) {
    Add-Type -Path (Join-Path $PSScriptRoot 'WindowsShippingSmoke.cs')
}
$result = [GirlsSpireShippingDesktop]::Run($exe,$package.FullName,$report,"GirlsSpireSmoke-$id")
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $report 'process.json')
$after = @(Get-ChildItem -LiteralPath $package.FullName -File -Recurse -Force)
if ($after.Count -ne $before.Count) { throw "配布ファイル数が変化しました。証跡: $report" }
foreach ($file in $after) {
    if ($before[$file.FullName] -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {
        throw "配布ファイルが変化しました。証跡: $report"
    }
}
if (-not $result.Captured -or -not $result.CloseRequested -or -not $result.Exited -or $result.ExitCode -ne 0) {
    throw "起動・撮影・正常終了を確認できません。証跡: $report"
}

Add-Type -AssemblyName System.Drawing
$bitmap = [Drawing.Bitmap]::new((Join-Path $report 'title.bmp'))
try {
    $colors = [Collections.Generic.HashSet[int]]::new()
    for ($y = 1; $y -lt 18; $y++) {
        for ($x = 1; $x -lt 32; $x++) {
            $pixel = $bitmap.GetPixel([int]($bitmap.Width*$x/32),[int]($bitmap.Height*$y/18))
            $null = $colors.Add($pixel.ToArgb() -band 0xffffff)
        }
    }
    if ($colors.Count -lt 16) { throw "空白に近い画像が返りました。証跡: $report" }
    $png = Join-Path $report 'title.png'
    $bitmap.Save($png,[Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}
$titleHash = (Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash
$referenceMatched = $null
if ($ExpectedTitleSHA256) {
    $referenceMatched = $titleHash -eq $ExpectedTitleSHA256
    if (-not $referenceMatched) { throw "確認済みタイトル画像と一致しません。証跡: $report" }
}
@{WindowsRoot=$package.FullName;PackageFiles=$after.Count;PackageUnchanged=$true;TitleImage=$png;
    TitleSHA256=$titleHash;VisualReferenceMatched=$referenceMatched;ExitCode=$result.ExitCode} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $report 'result.json')
"Shipping起動確認成功。証跡: $report"
