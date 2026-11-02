#include "RunScreen.h"
#include "GameAudio.h"
#include "Widgets/SBoxPanel.h"
#include <algorithm>
void SRunScreen::ShowPreferences(bool resetDraft)
{
    if(resetDraft){
        const auto loaded=PreferencesService->Load();PreferencesLoadIssue=loaded.Issue;
        if(loaded.Issue==GirlsSpire::PreferencesIssue::None && loaded.Value!=DisplayPreferences){DisplayPreferences=loaded.Value;if(Audio)Audio->Apply(DisplayPreferences);}
        DraftPreferences=DisplayPreferences;PreferencesMessage.Empty();
    }
    RefreshPreferences();
}
void SRunScreen::RefreshPreferences()
{
    using namespace GirlsSpire;Body->ClearChildren();AddText(TEXT("設定"),"settings.heading",28);
    if(!PreferencesMessage.IsEmpty())AddText(PreferencesMessage,"settings.message",18);
    if(PreferencesLoadIssue!=PreferencesIssue::None && PreferencesLoadIssue!=PreferencesIssue::Empty){
        AddText(TEXT("設定を読み込めません。元のファイルは変更していません。"),"settings.error");
        AddAction(TEXT("読み込みを再試行"),"settings.retry",[this]{ShowPreferences();});
        if(PreferencesLoadIssue==PreferencesIssue::Corrupt)AddAction(TEXT("原本を保全して標準設定へ戻す"),"settings.reset",[this]{ConfirmPreferencesReset();});
    }else{
        AddText(FString::Printf(TEXT("文字サイズ: %d%%"),TextScalePercent(DraftPreferences.Text)),"settings.text.value");
        const auto row=SNew(SHorizontalBox);Body->AddSlot().AutoHeight()[row];
        const TCHAR* names[]={TEXT("標準 100%"),TEXT("大 125%"),TEXT("最大 150%")};
        for(int i=0;i<3;++i)AddAction(names[i],*FString::Printf(TEXT("settings.text.%d"),i),[this,i]{DraftPreferences.Text=static_cast<TextSize>(i);ShowPreferences(false);},row);
        for(bool bgm:{true,false}){
            AddText(FString::Printf(TEXT("%s音量: %d%%"),bgm?TEXT("BGM"):TEXT("効果"),bgm?DraftPreferences.BgmVolume:DraftPreferences.SfxVolume),bgm?"settings.bgm.value":"settings.sfx.value");
            const auto volume=SNew(SHorizontalBox);Body->AddSlot().AutoHeight()[volume];
            for(int change:{-10,0,10})AddAction(change<0?TEXT("−10"):change==0?TEXT("消音"):TEXT("＋10"),*FString::Printf(TEXT("settings.%s.%s"),bgm?TEXT("bgm"):TEXT("sfx"),change<0?TEXT("down"):change==0?TEXT("mute"):TEXT("up")),[this,bgm,change]{auto& value=bgm?DraftPreferences.BgmVolume:DraftPreferences.SfxVolume;value=change==0?0:std::clamp(value+change,0,100);ShowPreferences(false);},volume);
        }
        AddAction(DraftPreferences.Subtitles?TEXT("字幕: 入"):TEXT("字幕: 切"),"settings.subtitles",[this]{DraftPreferences.Subtitles=!DraftPreferences.Subtitles;ShowPreferences(false);});
        AddAction(DraftPreferences.ReducedEffects?TEXT("演出軽減: 入"):TEXT("演出軽減: 切"),"settings.effects",[this]{DraftPreferences.ReducedEffects=!DraftPreferences.ReducedEffects;ShowPreferences(false);});
        AddAction(DraftPreferences.SuppressIntimacy?TEXT("親密表現の抑制: 入"):TEXT("親密表現の抑制: 切"),"settings.intimacy",[this]{DraftPreferences.SuppressIntimacy=!DraftPreferences.SuppressIntimacy;ShowPreferences(false);});
        AddText(TEXT("戦闘の重要情報は、色や音だけに頼らず常に文字でも表示します。"),"settings.notice",16);
        AddAction(TEXT("標準設定を選ぶ"),"settings.defaults",[this]{DraftPreferences={};ShowPreferences(false);});
        AddAction(TEXT("保存して適用"),"settings.save",[this]{SavePreferences();});
    }
    AddAction(TEXT("変更せず戻る"),"settings.cancel",[this]{ShowTitle();});
}
void SRunScreen::SavePreferences()
{
    using namespace GirlsSpire;const auto issue=PreferencesService->Save(DraftPreferences);
    if(issue==PreferencesIssue::None){DisplayPreferences=DraftPreferences;if(Audio)Audio->Apply(DisplayPreferences);PreferencesLoadIssue=PreferencesIssue::None;ShowTitle();return;}
    if(issue==PreferencesIssue::Corrupt || issue==PreferencesIssue::ReadFailed)PreferencesLoadIssue=issue;
    PreferencesMessage=TEXT("設定を保存できませんでした。適用済みの設定は変えていません。再試行できます。");RefreshPreferences();
}
void SRunScreen::ConfirmPreferencesReset()
{
    Body->ClearChildren();AddText(TEXT("読めない設定の原本を保全して、標準設定に戻しますか？ 周回と恒久記録は変更しません。"),"settings.reset.question");
    AddAction(TEXT("原本を保全して戻す"),"settings.reset.confirm",[this]{
        const auto issue=PreferencesService->RecoverDefaults();
        if(issue==GirlsSpire::PreferencesIssue::None){DisplayPreferences={};if(Audio)Audio->Apply(DisplayPreferences);PreferencesLoadIssue=issue;ShowTitle();return;}
        PreferencesMessage=TEXT("設定を復旧できませんでした。原本を変更せず停止しました。");RefreshPreferences();
    });
    AddAction(TEXT("戻る"),"settings.reset.cancel",[this]{ShowPreferences();});
}
