#requires -Version 7.0
param([switch] $SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-unreal.ps1')
    if ($LASTEXITCODE) { throw '再起動試験用のビルドに失敗しました。' }
}
$suffix = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($projectRoot))).Substring(0,12)
$launchRoot = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    $taskAlias = Join-Path ([IO.Path]::GetTempPath()) "GirlsSpire-$suffix"
    $taskLink = Get-Item -LiteralPath $taskAlias
    if ($taskLink.LinkType -ne 'Junction' -or $taskLink.Target -ne $projectRoot) { throw '試験用の別名がプロジェクトを指していません。' }
    $launchRoot = $taskAlias
}
$taskId = [Guid]::NewGuid().ToString('N')
$relative = "Saved/Tests/ProcessResume/$taskId"
$taskSaveRoot = Join-Path $launchRoot $relative
$actualRoot = Join-Path $projectRoot $relative
foreach ($stage in @('Save','Verify')) {
    & (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire.UI.SaveProcess' -ExtraArgs @("-GirlsSpireResumeStage=$stage", "-GirlsSpireResumeRoot=$taskSaveRoot")
    if ($LASTEXITCODE) { throw "別プロセス再開の$stage 段階が失敗しました。" }
}
$markers = @(Get-ChildItem -LiteralPath $actualRoot -Filter 'point-*.done')
if ($markers.Count -ne 60 -or @($markers | Where-Object { (Get-Content -LiteralPath $_.FullName -Raw).Trim() -ne 'pass' }).Count -gt 0) {
    throw '戦闘外14・戦闘内46地点の成功記録が揃っていません。'
}
$preferencesMarker=Join-Path $actualRoot 'preferences.done'
if(-not (Test-Path -LiteralPath $preferencesMarker) -or (Get-Content -LiteralPath $preferencesMarker -Raw).Trim() -ne 'pass'){throw '独立設定の別プロセス再読込が未成功です。'}
$recoveryMarker=Join-Path $actualRoot 'generation-recovery.done'
if(-not (Test-Path -LiteralPath $recoveryMarker) -or (Get-Content -LiteralPath $recoveryMarker -Raw).Trim() -ne 'pass'){throw '共通世代の別プロセス確認復旧が未成功です。'}
$appearanceMarker=Join-Path $actualRoot 'appearance.done'
if(-not (Test-Path -LiteralPath $appearanceMarker) -or (Get-Content -LiteralPath $appearanceMarker -Raw).Trim() -ne 'pass'){throw '配色戦闘v20の別プロセス保存再開が未成功です。'}
"別UEプロセスの保存→終了→再起動→画面から消費再開: 60地点・配色戦闘1件・世代不一致復旧1件・独立設定1件成功。試験データ: $actualRoot"
