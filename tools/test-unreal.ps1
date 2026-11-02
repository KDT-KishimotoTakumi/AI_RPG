#requires -Version 7.0
param([string] $Filter = 'GirlsSpire.Foundation', [switch] $UI, [switch] $Audio, [string[]] $ExtraArgs = @())
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'automation-result.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
$engine = (Get-Content $manifest -Raw | ConvertFrom-Json).InstallationList |
    Where-Object AppName -EQ 'UE_5.7' | Select-Object -First 1
if (-not $engine) { throw '登録済みのUE5.7が見つかりません。' }
$sha = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($projectRoot))
$suffix = [Convert]::ToHexString($sha).Substring(0, 12)
$alias = Join-Path ([IO.Path]::GetTempPath()) "GirlsSpire-$suffix"
$launchRoot = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    if (-not (Test-Path $alias)) { throw '先にbuild-unreal.ps1を実行してください。' }
    $item = Get-Item -LiteralPath $alias
    if ($item.LinkType -ne 'Junction' -or $item.Target -ne $projectRoot) { throw 'ビルド別名の対象が不正です。' }
    $launchRoot = $alias
}
$report = Join-Path $launchRoot ('Saved/Automation/' + [Guid]::NewGuid().ToString('N'))
$render = if ($UI) { @('-RenderOffscreen', '-ResX=1280', '-ResY=900') } else { @('-NullRHI') }
$sound = if($Audio){@("-AudioMixer")}else{@("-nosound")}
$oldProfile = $env:LLVM_PROFILE_FILE
try {
    # 個別の動作確認では巨大な既定プロファイルを残さない。本計測の指定先は維持する。
    if ([string]::IsNullOrWhiteSpace($oldProfile)) { $env:LLVM_PROFILE_FILE = 'NUL' }
    & (Join-Path $engine.InstallLocation 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') "$launchRoot/GirlsSpire.uproject" -unattended @render @sound -nocef @ExtraArgs "-ExecCmds=Automation RunTests $Filter" '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" '-log=FoundationTests.log'
} finally {
    $env:LLVM_PROFILE_FILE = $oldProfile
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$resultPath = Join-Path $report 'index.json'
if (-not (Test-Path $resultPath)) { throw 'Automationレポートがありません。成功扱いにしません。' }
$result = Get-Content $resultPath -Raw | ConvertFrom-Json
$successful = Assert-AutomationResult $result
"Automation: $successful succeeded (with warnings: $([int]$result.succeededWithWarnings)); $($result.failed) failed. $resultPath"
