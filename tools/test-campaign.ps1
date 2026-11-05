#requires -Version 7.0
$ErrorActionPreference = 'Stop'
# 初期周回のみ固定シードで準備し、全12日の進行は実マウス操作で検証する。
& (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire.UI.Campaign' -ExtraArgs @('-GirlsSpireCampaignTest')
exit $LASTEXITCODE
