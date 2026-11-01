function Get-ContextModeNode {
    $command = Get-Command node -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    # MCP子プロセスはシェルのプロファイルを読み込まない。既存環境から補完する。
    $candidates = @($env:CONTEXT_MODE_NODE)
    if ($env:HERMES_HOME) { $candidates += Join-Path $env:HERMES_HOME 'node/node.exe' }
    if ($env:LOCALAPPDATA) { $candidates += Join-Path $env:LOCALAPPDATA 'hermes/node/node.exe' }
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $candidate }
    }
    throw 'Node.jsが見つかりません。PATHまたはCONTEXT_MODE_NODEで既存実行物を指定してください。'
}
