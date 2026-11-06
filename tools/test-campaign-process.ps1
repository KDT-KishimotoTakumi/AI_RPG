#requires -Version 7.0
param([switch] $SkipBuild,[switch] $Guards)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not $SkipBuild){
    & (Join-Path $PSScriptRoot 'build-unreal.ps1')
    if($LASTEXITCODE){throw '通し再起動試験のビルド失敗'}
}
$launchRoot=$root
if($root -match '[^\x00-\x7F]'){
    $suffix=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($root))).Substring(0,12)
    $launchRoot=Join-Path ([IO.Path]::GetTempPath()) "GirlsSpire-$suffix"
    $alias=Get-Item -LiteralPath $launchRoot
    if($alias.LinkType -ne 'Junction' -or $alias.Target -ne $root){throw '試験用別名の対象不一致'}
}
$relative='Saved/Tests/ProcessCampaign/'+[guid]::NewGuid().ToString('N')
$saveRoot=Join-Path $launchRoot $relative
$actualRoot=Join-Path $root $relative
foreach($stage in @('Start','Continue','Finish')){
    $output=& (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire.UI.Campaign.別プロセス' -ExtraArgs @('-GirlsSpireCampaignTest',"-GirlsSpireCampaignStage=$stage","-GirlsSpireCampaignRoot=$saveRoot")
    $code=$LASTEXITCODE
    $output
    if($code){throw "12日通しの$stage 段階で失敗"}
    if(-not ($output -match '^Automation: 1 succeeded \(with warnings: 0\); 0 failed\.')){throw '各段階の1件成功が不足'}
    $marker=Join-Path $actualRoot "$stage.done"
    if(-not(Test-Path -LiteralPath $marker) -or (Get-Content -LiteralPath $marker -Raw).Trim() -ne 'pass'){throw "$stage 段階の引継ぎ成功記録が不足"}
    Copy-Item -LiteralPath (Join-Path $root 'Saved/Logs/FoundationTests.log') -Destination (Join-Path $actualRoot "$stage-engine.log")
}
if($Guards){
    function Test-RejectedStage([string]$stage,[string]$cliRoot,[string]$diskRoot,[string]$expected){
        $slots=Join-Path $diskRoot 'slots'
        $original=@{}
        if(Test-Path -LiteralPath $slots){foreach($file in Get-ChildItem -LiteralPath $slots -File){$original[$file.Name]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}}
        $reports=Join-Path $root 'Saved/Automation'
        $before=@(Get-ChildItem -LiteralPath $reports -Directory|Select-Object -ExpandProperty Name)
        $log=Join-Path $actualRoot "guard-$stage.log"
        $caught=$false
        try{
            & (Join-Path $PSScriptRoot 'test-unreal.ps1') -UI -Filter 'GirlsSpire.UI.Campaign.別プロセス' -ExtraArgs @('-GirlsSpireCampaignTest',"-GirlsSpireCampaignStage=$stage","-GirlsSpireCampaignRoot=$cliRoot") *> $log
        }catch{$caught=$true;$_|Out-String|Add-Content -LiteralPath $log}
        $added=@(Get-ChildItem -LiteralPath $reports -Directory|Where-Object Name -notin $before)
        if(-not $caught -or $added.Count -ne 1){throw '期待した段階拒否のレポートではない'}
        $result=Get-Content -LiteralPath (Join-Path $added[0].FullName 'index.json') -Raw|ConvertFrom-Json
        $messages=@($result.tests|ForEach-Object {$_.entries.event.message})
        if($result.failed -ne 1 -or $result.succeeded -ne 0 -or -not($messages -like "*$expected*")){throw '意図した境界条件の拒否ではない'}
        $after=@(if(Test-Path -LiteralPath $slots){Get-ChildItem -LiteralPath $slots -File})
        if($after.Count -ne $original.Count){throw '拒否時に保存ファイルが変化'}
        foreach($file in $after){if(-not $original.ContainsKey($file.Name) -or (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne $original[$file.Name]){throw '拒否時に原本が変化'}}
        Copy-Item -LiteralPath (Join-Path $root 'Saved/Logs/FoundationTests.log') -Destination (Join-Path $actualRoot "guard-$stage-engine.log")
        @{Stage=$stage;Rejected=$true;FilesUnchanged=$true;Report=$added[0].Name;SaveRoot=$diskRoot}|ConvertTo-Json|Set-Content (Join-Path $actualRoot "guard-$stage.json")
        "段階拒否の確認: $stage / $expected / 原本不変"
    }
    Test-RejectedStage 'Start' $saveRoot $actualRoot '既存の通し保存を上書きしない'
    $empty='Saved/Tests/ProcessCampaign/'+[guid]::NewGuid().ToString('N')
    Test-RejectedStage 'Finish' (Join-Path $launchRoot $empty) (Join-Path $root $empty) '前プロセスの全保存/恒久/確定世代と独立期待記録が一致'
}
"別UEプロセス3段階の実マウス12日通し: 成功。保存先: $actualRoot"
exit 0
