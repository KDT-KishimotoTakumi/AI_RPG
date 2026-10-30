# カバレッジ未計測ファイルを合格にしないため、レポートと実ソース一覧を照合する。
function Test-CoverageGate {
    param([Parameter(Mandatory)] $Report, [Parameter(Mandatory)] [string[]] $Sources)
    $files = @($Report.data | ForEach-Object { $_.files })
    $reportedPaths = @($files | ForEach-Object { $_.filename.Replace('\', '/') })
    $missing = @($Sources | Where-Object { $_.Replace('\', '/') -notin $reportedPaths })
    $totals = @{}
    foreach ($metric in @('lines', 'functions', 'branches')) {
        $count = 0L; $covered = 0L
        foreach ($file in $files) {
            if ($file.filename.Replace('\', '/') -in @($Sources | ForEach-Object { $_.Replace('\', '/') })) {
                $count += $file.summary.$metric.count
                $covered += $file.summary.$metric.covered
            }
        }
        $totals[$metric] = [pscustomobject]@{ Count=$count; Covered=$covered; Percent=$(if ($count) { 100.0 * $covered / $count } else { 0.0 }) }
    }
    [pscustomobject]@{
        Passed = ($missing.Count -eq 0 -and $totals.lines.Percent -ge 90 -and $totals.functions.Percent -ge 90 -and $totals.branches.Percent -ge 90)
        Missing = $missing
        Metrics = $totals
    }
}
