#requires -Version 7.0
param([switch] $ConfigureCodex)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$prefix = Join-Path $projectRoot 'Saved/Tools/ContextMode'
. (Join-Path $PSScriptRoot 'context-mode-runtime.ps1')
$node = Get-ContextModeNode
$version = [version]((& $node --version).TrimStart('v'))
if ($version -lt [version]'22.5.0') { throw 'Node.js 22.5.0以上が必要です。' }
New-Item -ItemType Directory -Force -Path $prefix | Out-Null
# 他ホストの設定を変更するpostinstallや不要なネイティブビルドを実行しない。
# 対応Nodeの組込みSQLiteを使う。接続検証はDEV_SKILLS.md参照。
$npm = Join-Path (Split-Path -Parent $node) 'node_modules/npm/bin/npm-cli.js'
if (-not (Test-Path -LiteralPath $npm)) { throw 'Node.jsに対応するnpmが見つかりません。' }
& $node $npm install --prefix $prefix --ignore-scripts --no-audit --no-fund --save-exact context-mode@1.0.169
if ($LASTEXITCODE) { throw 'Context Modeの導入に失敗しました。' }
if ($ConfigureCodex) {
    $config = Join-Path $projectRoot '.codex/config.toml'
    if ((Test-Path -LiteralPath $config) -and
        (Get-Content -LiteralPath $config -Raw) -match '(?m)^\[mcp_servers\.context-mode\]') {
        Write-Output '既存のcontext-mode設定を保持しました。'
        return
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $config) | Out-Null
    $nodeQuoted = ConvertTo-Json -InputObject $node -Compress
    $launcherQuoted = ConvertTo-Json -InputObject (Join-Path $PSScriptRoot 'start-context-mode.mjs') -Compress
    # この端末で解決したパスはローカル設定だけに保存し、配布物へ固定しない。
    $section = @"

[mcp_servers.context-mode]
command = $nodeQuoted
args = [$launcherQuoted]
startup_timeout_sec = 30
tool_timeout_sec = 120
enabled_tools = ["ctx_execute", "ctx_execute_file", "ctx_index", "ctx_search", "ctx_fetch_and_index", "ctx_batch_execute", "ctx_stats"]
"@
    Add-Content -LiteralPath $config -Value $section -Encoding utf8
}
