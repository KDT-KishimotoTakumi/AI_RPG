#requires -Version 7.0
function Assert-AutomationResult($Report) {
    # UEは警告付き成功をsucceededとは別に集計する。失敗と未検出は成功扱いにしない。
    $successful = [int]$Report.succeeded + [int]$Report.succeededWithWarnings
    if ([int]$Report.failed -gt 0 -or $successful -lt 1) { throw 'Automation失敗またはテスト未検出。' }
    return $successful
}
