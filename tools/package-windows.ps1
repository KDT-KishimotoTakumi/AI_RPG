#requires -Version 7.0
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Windowsで実行してください。' }
if ($env:GIRLS_SPIRE_COVERAGE -eq '1') { throw 'カバレッジ計装を解除してから実行してください。' }

$root = Split-Path -Parent $PSScriptRoot
$manifest = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
$engine = (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).InstallationList |
    Where-Object AppName -eq 'UE_5.7' | Select-Object -First 1
if (-not $engine) { throw '登録済みUE 5.7が見つかりません。' }
$uat = Join-Path $engine.InstallLocation 'Engine/Build/BatchFiles/RunUAT.bat'
if (-not (Test-Path -LiteralPath $uat -PathType Leaf)) { throw 'RunUAT.batが見つかりません。' }

$commit = & git -C $root rev-parse --verify HEAD
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Gitコミットを確定できません。' }
$id = [guid]::NewGuid().ToString('N')
$candidate = Join-Path $root "Saved/ReleaseCandidates/$id"
$project = Join-Path $candidate 'Project'
$zip = Join-Path $candidate 'project-input.zip'
$inputs = @('GirlsSpire.uproject','Source','Content','Tests',
    'Config/DefaultGame.ini','Config/DefaultInput.ini','Config/Windows/WindowsEngine.ini')
New-Item -ItemType Directory -Path $candidate | Out-Null
& git -C $root archive --format=zip "--output=$zip" $commit -- @inputs
if ($LASTEXITCODE -ne 0) { throw "配布用の管理ファイルを抽出できません。証跡: $candidate" }
Expand-Archive -LiteralPath $zip -DestinationPath $project
if ((Test-Path -LiteralPath (Join-Path $project 'Config/DefaultEngine.ini')) -or
    (Test-Path -LiteralPath (Join-Path $project 'Saved'))) {
    throw '配布用入力に対象外の設定またはセーブが見つかりました。'
}

# 画像と同じフォルダにある制作資料は、クック前に隔離コピーから退避する。
$art = Join-Path $project 'Content/Art'
$notes = @(Get-ChildItem -LiteralPath $art -Filter '*.md' -File -Recurse)
foreach ($file in $notes) {
    $relative = [IO.Path]::GetRelativePath($art, $file.FullName)
    $target = Join-Path (Join-Path $candidate 'ArtAuthoringEvidence') $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Move-Item -LiteralPath $file.FullName -Destination $target
}
@{Commit=$commit; Project=$project; SourceArchive=$zip; AuthoringNotesExcluded=$notes.Count} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $candidate 'input.json')

# クック中のWindowsパス長制限を避けるため、隔離プロジェクトのみ短い名前で参照する。
$alias = Join-Path ([IO.Path]::GetTempPath()) ('GS-' + $id.Substring(0,8))
New-Item -ItemType Junction -Path $alias -Target $project | Out-Null
$link = Get-Item -LiteralPath $alias
if ($link.LinkType -ne 'Junction' -or $link.Target -ne $project) { throw '短縮パスの対象を確認できません。' }

$log = Join-Path $candidate 'package.log'
$projectPath = Join-Path $alias 'GirlsSpire.uproject'
$archive = Join-Path $alias 'Distribution'
& $uat BuildCookRun "-project=$projectPath" -target=GirlsSpire -platform=Win64 -clientconfig=Shipping `
    -map=/Engine/Maps/Entry -build -cook -stage -pak -archive "-archivedirectory=$archive" `
    -nodebuginfo -prereqs -nop4 -unattended -utf8output *> $log
if ($LASTEXITCODE -ne 0) { throw "Windows Shippingのビルドに失敗しました。証跡: $log" }

$windows = Join-Path $project 'Distribution/Windows'
& (Join-Path $PSScriptRoot 'test-windows-distribution.ps1') -WindowsRoot $windows -ArtRoot $art
@{Commit=$commit; Windows=$windows; Log=$log; ShortPath=$alias} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $candidate 'result.json')
"Windows Shipping候補: $windows"
