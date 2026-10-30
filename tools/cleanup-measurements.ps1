#requires -Version 7.0
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{32}$')][string]$ReportId)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$coverageRoot = (Resolve-Path -LiteralPath (Join-Path $projectRoot 'Saved/Coverage')).Path
$directory = Get-Item -LiteralPath (Join-Path $coverageRoot $ReportId)
if ($directory.LinkType -or -not $directory.PSIsContainer -or
    -not $directory.FullName.StartsWith($coverageRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw '整理対象は作業ツリー内の通常の計測フォルダに限ります。'
}
$gate = Get-Content -LiteralPath (Join-Path $directory.FullName 'gate.json') -Raw | ConvertFrom-Json
if (-not $gate.Passed -or @($gate.Missing).Count -ne 0) { throw '未合格・未計測のレポートは整理しません。' }
foreach ($metric in @('lines','functions','branches')) {
    if ($gate.Metrics.$metric.Count -le 0 -or $gate.Metrics.$metric.Percent -lt 90) { throw '各90%以上の集計が必要です。' }
}
foreach ($name in @('coverage.json','coverage.profdata')) {
    if ((Get-Item -LiteralPath (Join-Path $directory.FullName $name)).Length -le 0) { throw '集計データが空です。' }
}
$removed = @()
foreach ($file in Get-ChildItem -LiteralPath $directory.FullName -File -Filter '*.profraw') {
    if ($file.LinkType) { throw 'リンクされた生プロファイルは整理しません。' }
    if ($PSCmdlet.ShouldProcess($file.FullName, '集計済み生プロファイルを削除')) {
        Remove-Item -LiteralPath $file.FullName
        $removed += [pscustomobject]@{ Name=$file.Name; Bytes=$file.Length }
    }
}
if (-not $WhatIfPreference -and $removed.Count -gt 0) {
    [pscustomobject]@{ ReportId=$ReportId; Removed=$removed; Bytes=($removed | Measure-Object Bytes -Sum).Sum } |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $directory.FullName 'raw-cleanup.json')
}
"集計済み生プロファイル整理: $($removed.Count)件 / $ReportId（JSON・profdata・実行物を保持）"
