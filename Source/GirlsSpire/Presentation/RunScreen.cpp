#include "RunScreen.h"
#include "Misc/App.h"
#include "Widgets/SNullWidget.h"
#include "SpireStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Misc/Paths.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "GameAudio.h"
#include "ComboLabels.h"
#include "CharacterPortraits.h"
#include "Domain/CharacterCatalog.h"
#include "CombatScreen.h"
#include "Application/Encounter.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include <algorithm>

namespace {
const TCHAR* const Roles[] = {TEXT("太刀"), TEXT("薙刀"), TEXT("軍配"), TEXT("火縄銃")};
const TCHAR* const Levels[] = {TEXT("初級"), TEXT("中級"), TEXT("上級")};
const TCHAR* const Nodes[] = {TEXT("合戦"), TEXT("強敵"), TEXT("救出"), TEXT("交流"), TEXT("商店"),
    TEXT("休息"), TEXT("怪異"), TEXT("中間ボス — 落城姫"), TEXT("最終ボス — 歴史喰らいの城主")};
FString NodeLabel(const GirlsSpire::RouteNode& Node)
{
    FString Label = Nodes[static_cast<int>(Node.Kind)];
    if (Node.Kind == GirlsSpire::NodeKind::Rescue) {
        const int actor=Node.Candidate+1;
        Label+=FString(TEXT(" — "))+FCharacterPortraits::Name(actor);
        if(const auto definition=GirlsSpire::FindCharacter(actor)) {
            const auto unit=GirlsSpire::CreateCharacter(actor,{});
            Label+=FString::Printf(TEXT("（%s / 移動%d・射程%d）"),UTF8_TO_TCHAR(definition->RoleName.data()),unit.Movement,unit.Range);
        }
    }
    return Label;
}
}

void SRunScreen::Construct(const FArguments& Args)
{
    ExitGame=Args._OnExitGame;
    FullField=Args._FullField || FApp::IsGame();
    SaveService=Args._SaveService;PreferencesService=Args._PreferencesService;
    if(PreferencesService){const auto loaded=PreferencesService->Load();PreferencesLoadIssue=loaded.Issue;if(loaded.Issue==GirlsSpire::PreferencesIssue::None)DisplayPreferences=loaded.Value;}
    Audio=Args._Audio;if(Audio){Audio->Apply(DisplayPreferences);Audio->Start();}
    DevelopmentTools=Args._DevelopmentTools;
    TitleBackdrop=FSpireStyle::Backdrop(FPaths::ProjectContentDir()/TEXT("Art/Backgrounds/title-castle-v1.png"));
    ChildSlot [
        SNew(SOverlay)
        +SOverlay::Slot()
        [SNew(SImage).Image(TitleBackdrop.Get()).AddMetaData(FDriverMetaData::Id("title.art"))
            .ColorAndOpacity_Lambda([this]{return FLinearColor(1,1,1,CurrentRun?0.12f:1.f);})
            .Visibility(TitleBackdrop?EVisibility::HitTestInvisible:EVisibility::Collapsed)]
        +SOverlay::Slot()
        [
        SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .Visibility_Lambda([this]{return FullFieldActive?EVisibility::Collapsed:EVisibility::Visible;})
        .AddMetaData(FDriverMetaData::Id("run.frame"))
        .BorderBackgroundColor(FLinearColor(0.012f,0.023f,0.045f,0.25f)).Padding(36)
        [ SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [ SNew(STextBlock).Text(FText::FromString(TEXT("死者の城 — 十二日間の戦記")))
                .Font_Lambda([this]{return FCoreStyle::GetDefaultFontStyle("Bold",FontSize(18));}).ColorAndOpacity(FLinearColor(0.94f, 0.79f, 0.58f)) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 24)
            [ SNew(STextBlock).Text(FText::FromString(DevelopmentTools?TEXT("開発版 — 戦闘・作戦予告・三択報酬 / 正式素材は開発中"):TEXT("十二日後、死者の城が歴史を書き換える。")))
                .Font_Lambda([this]{return FCoreStyle::GetDefaultFontStyle("Regular",FontSize(16));}).ColorAndOpacity(FLinearColor(0.8f, 0.65f, 0.65f)) ]
            + SVerticalBox::Slot().FillHeight(1)
            [ SNew(SScrollBox).AddMetaData(FDriverMetaData::Id("run.scroll")) + SScrollBox::Slot() [ SAssignNew(Body, SVerticalBox) ] ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
            [ SNew(STextBlock).Text(FText::FromString(TEXT("音声の一部またはすべてを利用できません。音なしでもゲームを続けられます。")))
                .AutoWrapText(true).Font_Lambda([this]{return FCoreStyle::GetDefaultFontStyle("Regular",FontSize(14));})
                .Visibility_Lambda([this]{return Audio && !Audio->Ready()?EVisibility::Visible:EVisibility::Collapsed;})
                .AddMetaData(FDriverMetaData::Id("audio.unavailable")) ]
        ] ]
        +SOverlay::Slot()[SAssignNew(FieldHost,SBox).Visibility_Lambda([this]{return FullFieldActive?EVisibility::Visible:EVisibility::Collapsed;})]
    ];
    if(Args._InitialRun) {CurrentRun=Args._InitialRun;RefreshRun();}
    else ShowTitle();
}

void SRunScreen::AddText(const FString& Text, const FName& Id, int Size)
{
    Body->AddSlot().AutoHeight().Padding(0, 0, 0, 16)
    [ SNew(STextBlock).Text(FText::FromString(Text)).AutoWrapText(true).WrapTextAt(CurrentRun?0.f:620.f)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", FontSize(Size)))
        .ColorAndOpacity(FLinearColor(0.92f, 0.89f, 0.83f)).AddMetaData(FDriverMetaData::Id(Id)) ];
}

void SRunScreen::AddAction(const FString& Text, const FName& Id, TFunction<void()> Action, TSharedPtr<SHorizontalBox> Row)
{
    const auto Button = SNew(SButton).ContentPadding(FMargin(20, 12)).HAlign(HAlign_Left)
        .ButtonStyle(&FSpireStyle::Command())
        .AddMetaData(FDriverMetaData::Id(Id))
        .OnClicked_Lambda([this,Action = MoveTemp(Action)] { if(Audio)Audio->Cue(EGameCue::Confirm);Action(); return FReply::Handled(); })
        [ SNew(STextBlock).Text(FText::FromString(Text)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular", FontSize(19)))
            .ColorAndOpacity(FLinearColor(0.98f, 0.94f, 0.85f)) ];
    if (Row.IsValid()) Row->AddSlot().FillWidth(1).Padding(0, 0, 8, 10) [ Button ];
    else if(!CurrentRun) Body->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0,0,0,10)
        [SNew(SBox).WidthOverride(620)[Button]];
    else Body->AddSlot().AutoHeight().Padding(0, 0, 0, 10) [ Button ];
}

void SRunScreen::ShowTitle()
{
    HideFieldCombat();
    if(CurrentRun) {
        RetainedProfile=CurrentRun->ProfileState();
        CurrentRun.reset();
        if(SaveService) {FinishRunToTitle();return;}
    }
    CurrentRun.reset();
    SocialActor=-1;SocialChoice=-1;SocialMessage.Empty();
    Body->ClearChildren();
    if(!LoadTitleState()) return;
    AddText(TEXT("GIRLS SPIRE"), "title.heading", 64);
    AddText(TEXT("四人の仲間と挑む、戦国怪異RPG。"), "title.description");
    AddText(DevelopmentTools?TEXT("経路を選び、盤面で移動・攻撃して仲間を救出します。進行確認用の結果入力も利用できます。"):
        TEXT("自動保存対応・戦闘中も中断できます。"), "title.notice");
    if(StoredRun) AddAction(StoredRun->ResumeAvailable?TEXT("中断した周回を再開"):TEXT("直近の状態から復旧"),
        StoredRun->ResumeAvailable?"title.resume":"title.recover",[this]{ResumeSaved();});
    if(StorageRecovered) AddText(TEXT("保存記録を復旧しました。直近の有効な状態を使用しています。"),"save.recovered",16);
    if(!SaveNotice.IsEmpty()) AddText(SaveNotice,"save.reset.result",16);
    AddAction(TEXT("新しい周回へ"), "title.start", [this] {if(StoredRun) ConfirmAbandon();else ShowSetup();});
    if(PreferencesService) AddAction(TEXT("設定"),"title.settings",[this]{ShowPreferences();});
    if(PreferencesLoadIssue==GirlsSpire::PreferencesIssue::Corrupt || PreferencesLoadIssue==GirlsSpire::PreferencesIssue::ReadFailed) AddText(TEXT("設定を読み込めません。設定画面から確認できます。"),"title.settings.error",16);
    if(std::any_of(RetainedProfile.Epilogues.begin(),RetainedProfile.Epilogues.end(),[](bool read){return read;}))
        AddAction(TEXT("後日談の本を開く"),"title.epilogues",[this]{ShowEpilogueLibrary();});
    AddExitAction();
    AddText(ProfileLabel(),"title.bonds",14);
    FString discoveries=TEXT("発見済み連携: ");
    if(RetainedProfile.DiscoveredCombos.empty())discoveries+=TEXT("なし");
    for(int id:RetainedProfile.DiscoveredCombos)discoveries+=ComboDisplayName(id)+TEXT(" / ");
    AddText(discoveries,"title.combos",14);
}
void SRunScreen::AddExitAction()
{
    if(ExitGame.IsBound())AddAction(TEXT("ゲームを終了する"),"title.quit",[this]{ExitGame.ExecuteIfBound();});
}

void SRunScreen::ShowSetup()
{
    SaveNotice.Empty();
    Body->ClearChildren();
    AddText(TEXT("出陣の支度"), "setup.heading", 28);
    AddText(TEXT("難易度"), "setup.difficulty");
    TSharedPtr<SHorizontalBox> LevelRow;
    Body->AddSlot().AutoHeight() [ SAssignNew(LevelRow, SHorizontalBox) ];
    for (int Index = 0; Index < 3; ++Index) {
        const FString Mark = static_cast<int>(Difficulty) == Index ? TEXT("● ") : TEXT("○ ");
        AddAction(Mark + Levels[Index], *FString::Printf(TEXT("difficulty.%d"), Index), [this, Index] {
            Difficulty = static_cast<GirlsSpire::Difficulty>(Index); ShowSetup();
        }, LevelRow);
    }
    AddText(TEXT("主人公の武器・役割"), "setup.role");
    TSharedPtr<SHorizontalBox> RoleRow;
    Body->AddSlot().AutoHeight() [ SAssignNew(RoleRow, SHorizontalBox) ];
    for (int Index = 0; Index < 4; ++Index) {
        const FString Mark = static_cast<int>(Role) == Index ? TEXT("● ") : TEXT("○ ");
        AddAction(Mark + Roles[Index], *FString::Printf(TEXT("role.%d"), Index), [this, Index] {
            Role = static_cast<GirlsSpire::HeroRole>(Index); ShowSetup();
        }, RoleRow);
    }
    AddAction(TEXT("この編成で出発する"), "role.confirm", [this] { StartRun(); });
    AddAction(TEXT("タイトルへ戻る"), "setup.back", [this] { ShowTitle(); });
}

void SRunScreen::StartRun()
{
    const auto seed=DevelopmentTools?UINT64_C(42):static_cast<std::uint64_t>(FDateTime::UtcNow().GetTicks())^FPlatformTime::Cycles64();
    CurrentRun = std::make_shared<GirlsSpire::Run>(seed, Difficulty, Role,RetainedProfile);
    RefreshRun();
}

void SRunScreen::RefreshRun()
{
    HideFieldCombat();
    if(!SaveCurrent()) {ShowSaveError([this]{RefreshRun();});return;}
    using namespace GirlsSpire;
    const auto State = CurrentRun->Snapshot();
    Body->ClearChildren();
    AddText(FString::Printf(TEXT("第 %d 日 / 十二日　 |　 %s ・ %s"), State.Day,
        Levels[static_cast<int>(State.Level)], Roles[static_cast<int>(State.Role)]), "run.day", 27);
    FString Party = FString::Printf(TEXT("%s ＋ 仲間%d人"),FCharacterPortraits::Name(0),static_cast<int>(State.Recruited.size()));
    for (int Candidate : State.Recruited) Party += FString(TEXT("　/　")) + FCharacterPortraits::Name(Candidate+1);
    AddText(Party, "run.party", 18);
    AddText(FString::Printf(TEXT("所持金%d銭 / 傷薬%d/5"),State.Wallet.Coins,State.Wallet.Medicines),"run.wallet",16);
    if(SaveService) AddAction(TEXT("保存して中断する"),"save.suspend",[this]{SuspendRun();});
    if(State.Phase==RunPhase::Route || State.Phase==RunPhase::Reward) ShowGrowth();
    if(State.PendingRescue>=0){RefreshRescueConversation();return;}
    if(State.Phase==RunPhase::Reward) { RefreshRewards();return; }
    if (State.Phase == RunPhase::Route) {
        AddText(FString::Printf(TEXT("行き先を選ぶ　—　本日の %d / %d 件目"), State.EventIndex + 1, State.EventsToday), "route.heading");
        for (int Index = 0; Index < static_cast<int>(State.Choices.size()); ++Index) {
            AddAction(NodeLabel(State.Choices[Index]), *FString::Printf(TEXT("route.node.%d"), Index),
                [this, Revision = State.Revision, Index] {
                    CurrentRun->ChooseNode(Revision, Index);SocialMessage.Empty(); RefreshRun();
                });
        }
        AddText(TEXT("1～4日目：仲間の救出　　6日目：落城姫　　12日目：城主との決戦"), "route.schedule", 17);
        if(!SocialMessage.IsEmpty()) AddText(SocialMessage,"social.result",16);
        AddText(ProfileLabel(),"run.profile",14);
    } else if (State.Phase == RunPhase::Event) {
        AddText(NodeLabel(State.Active), "event.kind", 26);
        if(State.Active.Kind==NodeKind::Rest) {RefreshRest();return;}
        if(State.Active.Kind==NodeKind::Shop) {RefreshShop();return;}
        if(State.Active.Kind==NodeKind::Social) {RefreshSocial();return;}
        const bool combat=State.Active.Kind==NodeKind::Battle || State.Active.Kind==NodeKind::Elite ||
            State.Active.Kind==NodeKind::Rescue || State.Active.Kind==NodeKind::MidBoss || State.Active.Kind==NodeKind::FinalBoss;
        if(combat) AddAction(TEXT("戦闘を始める"),DevelopmentTools?"preview.battle":"battle.start",[this] { StartBattle(); });
        if(combat && DevelopmentTools) AddAction(TEXT("検証：生成を失敗させ固定戦場へ"),"preview.fallback",[this] { StartBattle(true); });
        if(DevelopmentTools) AddText(TEXT("以下は進行確認用です。戦闘・報酬を操作せず結果だけを反映します。"), "event.notice");
        if (State.Active.Kind == NodeKind::Rescue)
            AddText(TEXT("救出対象は倒れても死亡せず、戦闘に勝てば加入します。"), "event.rescue");
        if(DevelopmentTools || !combat) AddAction(DevelopmentTools?TEXT("検証：勝利・達成として進む"):TEXT("先へ進む"), DevelopmentTools?"preview.complete":"event.resolve", [this, Revision = State.Revision] {
            CurrentRun->CompleteEvent(Revision); RefreshRun();
        });
        if(DevelopmentTools) AddAction(TEXT("検証：味方全員戦闘不能"), "preview.defeat", [this, Revision = State.Revision] {
            CurrentRun->CompleteEvent(Revision, true); RefreshRun();
        });
    } else {
        const bool Won = State.Phase == RunPhase::Won;
        AddText(Won ? TEXT("周回成功") : TEXT("周回敗北"), Won ? "result.won" : "result.lost", 30);
        if(DevelopmentTools) AddText(TEXT("正式な敵・素材・会話は開発中です。"), "result.notice");
        AddAction(Won?TEXT("後日談を選ぶ"):TEXT("タイトルへ戻る"), "result.continue", [this,Won] {if(Won)ShowEpilogueChoices();else ShowTitle();});
    }
}

void SRunScreen::StartBattle(bool forceGenerationFailure)
{
    const auto state=CurrentRun->Snapshot();
    GirlsSpire::GenerationResult generation;
    const auto combat=MakeShared<GirlsSpire::Battle>(GirlsSpire::Encounter::Create(state,CurrentRun->RandomState(),CurrentRun->ProfileState(),&generation,forceGenerationFailure));
    const auto description=FString::Printf(TEXT("%s %d×%d / 生成試行%d回"),
        generation.UsedFallback?TEXT("固定戦場へ切替"):TEXT("自動生成"),
        generation.Layout.Width,generation.Layout.Height,generation.Attempts);
    ShowCombat(combat,description);
}

void SRunScreen::HideFieldCombat()
{
    FullFieldActive=false;
    if(FieldHost.IsValid())FieldHost->SetContent(SNullWidget::NullWidget);
}
