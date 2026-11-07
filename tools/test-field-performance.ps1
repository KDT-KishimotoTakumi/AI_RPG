#requires -Version 7.0
[CmdletBinding()]
param([switch] $SkipBuild,[switch] $Combat)
$ErrorActionPreference='Stop'
if(-not $SkipBuild){
    & (Join-Path $PSScriptRoot 'build-unreal.ps1')
    if($LASTEXITCODE){throw '性能検証の通常ビルド失敗'}
}
$arguments=@('-GirlsSpirePerformanceTest','-NoVSync')
if($Combat){$arguments+='-GirlsSpirePerformanceCombat'}
$output=& (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire.Performance.Field' -ExtraArgs $arguments
$code=$LASTEXITCODE
$output
if($code){exit $code}
$summary=@($output | Where-Object {$_ -match '^Automation: 2 succeeded \(with warnings: 0\); 0 failed\. .+index\.json$'})
if($summary.Count -ne 1){throw '通常/最大編成の性能2件が警告なしで成功していません。'}
$path=$summary[0] -replace '^Automation: 2 succeeded \(with warnings: 0\); 0 failed\. ',''
$report=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
$mode=if($Combat){'実戦操作付き'}else{'待機中'}
foreach($enemies in @(3,10)){
    if(@($report.tests|Where-Object {$_.fullTestPath -like "*5対${enemies}の${mode}*"}).Count -ne 1){throw "指定した計測条件の記録がありません: $enemies / $mode"}
}
exit 0
