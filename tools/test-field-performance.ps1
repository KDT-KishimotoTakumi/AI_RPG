#requires -Version 7.0
param([switch] $SkipBuild)
$ErrorActionPreference='Stop'
if(-not $SkipBuild){
    & (Join-Path $PSScriptRoot 'build-unreal.ps1')
    if($LASTEXITCODE){throw '性能検証の通常ビルド失敗'}
}
$output=& (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire.Performance.Field' -ExtraArgs @('-GirlsSpirePerformanceTest','-NoVSync')
$code=$LASTEXITCODE
$output
if($code){exit $code}
if(-not($output -match '^Automation: 2 succeeded \(with warnings: 0\); 0 failed\.')){throw '通常/最大編成の性能2件が警告なしで成功していません。'}
exit 0
