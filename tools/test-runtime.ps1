#requires -Version 7.0
[CmdletBinding()]
param([switch] $SkipBuild,[switch] $Audio)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-unreal.ps1')
    if ($LASTEXITCODE) { throw 'ゲーム起動試験のビルドに失敗しました。' }
}
$launchRoot = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    $suffix = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($projectRoot))).Substring(0,12)
    $taskAlias = Join-Path ([IO.Path]::GetTempPath()) "GirlsSpire-$suffix"
    $link = Get-Item -LiteralPath $taskAlias
    if ($link.LinkType -ne 'Junction' -or $link.Target -ne $projectRoot) { throw 'ゲーム試験用の別名が不正です。' }
    $launchRoot = $taskAlias
}
$taskId = [Guid]::NewGuid().ToString('N')
$relative = "Saved/Tests/Runtime/$taskId"
$testRoot = Join-Path $launchRoot $relative
$runtimeArgs=@('-game','/Engine/Maps/Entry','-GirlsSpireRuntimeTest',"-GirlsSpireRuntimeRoot=$testRoot")
if($Audio){$runtimeArgs+=@('-GirlsSpireRuntimeAudioTest','-AllowCommandletAudio')}
& (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Audio:$Audio -Filter 'GirlsSpire.Runtime.GameEntry' -ExtraArgs $runtimeArgs
if ($LASTEXITCODE) { throw '通常ゲーム起動のUI試験が失敗しました。' }
if($Audio){
    & (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Audio -Filter 'GirlsSpire.Runtime.GameEntry' -ExtraArgs ($runtimeArgs+@('-GirlsSpireRuntimeAudioResume'))
    if($LASTEXITCODE){throw '音声有効の別プロセス再開試験が失敗しました。'}
    "音声有効の通常ゲーム・独立音量保存・別プロセス再開: 成功。"
}
"通常ゲーム起動・実戦操作・保存・中断: 成功。試験保存先: $(Join-Path $projectRoot $relative)"
