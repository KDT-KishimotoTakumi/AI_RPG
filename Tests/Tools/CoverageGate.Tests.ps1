#requires -Version 7.0
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../tools/coverage-gate.ps1')
function Assert-Gate($report, $sources, $expected, $label) {
    if ((Test-CoverageGate $report $sources).Passed -ne $expected) { throw "失敗: $label" }
}
$file = [pscustomobject]@{ filename='C:/project/Rules.cpp'; summary=@{} }
foreach ($metric in @('lines', 'functions', 'branches')) { $file.summary[$metric] = [pscustomobject]@{count=100; covered=90} }
$report = [pscustomobject]@{data=@([pscustomobject]@{files=@($file)})}
Assert-Gate $report @('C:\project\Rules.cpp') $true '90%境界とパス区切り'
foreach ($metric in @('lines', 'functions', 'branches')) {
    $file.summary[$metric].covered = 89
    Assert-Gate $report @('C:/project/Rules.cpp') $false "$metric が基準未満"
    $file.summary[$metric].covered = 90
}
Assert-Gate $report @('C:/project/Rules.cpp', 'C:/project/Missing.cpp') $false '未計測ソース'
Assert-Gate ([pscustomobject]@{data=@()}) @('C:/project/Rules.cpp') $false '空レポート'
foreach ($metric in @('lines', 'functions', 'branches')) { $file.summary[$metric].count=0; $file.summary[$metric].covered=0 }
Assert-Gate $report @('C:/project/Rules.cpp') $false '計測件数ゼロ'
'Coverage gate: 7 checks passed（判定器の試験であり、ゲームの計測値ではありません）'
