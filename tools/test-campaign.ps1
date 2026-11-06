#requires -Version 7.0
param([switch] $Matrix)
$ErrorActionPreference = 'Stop'
# 初期周回のみ固定シードで準備し、全12日の進行は実マウス操作で検証する。
$arguments=@('-GirlsSpireCampaignTest')
if($Matrix){$arguments+='-GirlsSpireCampaignMatrix'}
$output=& (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire.UI.Campaign' -ExtraArgs $arguments
$code=$LASTEXITCODE
$output
if($code){exit $code}
$expected=if($Matrix){15}else{3}
if(-not ($output -match "^Automation: $expected succeeded \(with warnings: 0\); 0 failed\.")){throw "12日通しの期待件数$expected 件が警告なしで成功していません。"}
