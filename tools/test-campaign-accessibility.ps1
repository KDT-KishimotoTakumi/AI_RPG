#requires -Version 7.0
param([switch] $SkipBuild)
$ErrorActionPreference='Stop'
if(-not $SkipBuild){
    & (Join-Path $PSScriptRoot 'build-unreal.ps1')
    if($LASTEXITCODE){throw '併用設定の通し検証ビルド失敗'}
}
foreach($width in @(1920,2560)){
    $output=& (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire.UI.Campaign.併用設定' -ExtraArgs @('-GirlsSpireCampaignTest','-GirlsSpireCampaignAccessibility',"-GirlsSpireCampaignWidth=$width")
    $code=$LASTEXITCODE
    $output
    if($code){exit $code}
    if(-not ($output -match '^Automation: 1 succeeded \(with warnings: 0\); 0 failed\.')){throw "併用設定の実寸$width が警告なしで成功していません。"}
}
exit 0
