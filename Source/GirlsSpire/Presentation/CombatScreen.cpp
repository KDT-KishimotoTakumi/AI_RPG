#include "CombatScreen.h"
#include "GameAudio.h"
#include "Application/BoardCues.h"
#include "CharacterPortraits.h"
#include "AI/BasicTurn.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include <algorithm>

void SCombatScreen::Construct(const FArguments& Args)
{
    Audio=Args._Audio;DisplayPreferences=Args._DisplayPreferences;Combat=Args._Combat; OnFinished=Args._OnFinished;
    OnCheckpoint=Args._OnCheckpoint;OnSuspend=Args._OnSuspend;
    GenerationSummary=Args._GenerationSummary;
    if(Args._BondProfile)KnownCombos=Args._BondProfile->DiscoveredCombos;
    check(Combat.IsValid());
    if(Args._SavedControl) {
        const auto restored=GirlsSpire::CombatControl::Restore(*Args._SavedControl,*Combat);
        if(!restored) {
            ChildSlot [ SNew(STextBlock).Text(FText::FromString(TEXT("作戦の保存データを復元できません。")))
                .AddMetaData(FDriverMetaData::Id("save.control.error")) ];
            return;
        }
        Control=*restored;
    } else {
        Control.ProfileDriven=Args._BondProfile.has_value();
        if(Control.ProfileDriven) for(int id=1;id<=8;++id) Control.Trust[id]=Args._BondProfile->Bonds[id-1];
        Control.ChoiceRandom=GirlsSpire::Random(Combat->RandomState());
    }
    MedicineVisible=Combat->Snapshot().Medicines>0;
    PortraitResources=Args._Portraits?Args._Portraits:MakeShared<FCharacterPortraits>();
    ChildSlot [ SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0,0,20,0)
            [ SNew(SScrollBox).AddMetaData(FDriverMetaData::Id("battle.board.scroll"))
              + SScrollBox::Slot()
              [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight() [ SAssignNew(Grid,SUniformGridPanel).SlotPadding(2) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,6)[SAssignNew(PortraitPanel,SHorizontalBox)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,6)
                    [ SAssignNew(ComboGauge,STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(18)))
                        .AddMetaData(FDriverMetaData::Id("battle.combo.gauge")) ]
                + SVerticalBox::Slot().AutoHeight()
                    [ SAssignNew(ComboResult,STextBlock).WrapTextAt(390).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(14)))
                        .AddMetaData(FDriverMetaData::Id("battle.combo.result")) ] ] ]
        + SHorizontalBox::Slot().FillWidth(1)
            [ SNew(SBox).HeightOverride(490)
                [ SNew(SScrollBox).AddMetaData(FDriverMetaData::Id("battle.scroll")) + SScrollBox::Slot() [ SAssignNew(Panel,SVerticalBox) ] ] ] ];
    AdvanceEnemies();
    Refresh();
}

void SCombatScreen::Text(const FString& label,const FName& id,int size)
{
    Panel->AddSlot().AutoHeight().Padding(0,0,0,10)
    [ SNew(STextBlock).Text(FText::FromString(label)).AutoWrapText(true)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(size))).AddMetaData(FDriverMetaData::Id(id)) ];
}

void SCombatScreen::Action(const FString& label,const FName& id,TFunction<void()> callback,bool enabled,TSharedPtr<SHorizontalBox> row)
{
    const auto button=SNew(SButton).ContentPadding(FMargin(10,6)).IsEnabled(enabled).AddMetaData(FDriverMetaData::Id(id))
        .OnClicked_Lambda([this,callback=MoveTemp(callback)] { if(Audio)Audio->Cue(EGameCue::Confirm);callback(); return FReply::Handled(); })
        [ SNew(STextBlock).Text(FText::FromString(label)).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(16))) ];
    if(row) row->AddSlot().AutoWidth().Padding(0,0,6,0) [button];
    else Panel->AddSlot().AutoHeight().Padding(0,0,0,7) [button];
}

void SCombatScreen::Refresh(bool persist)
{
    if(persist && !PersistCheckpoint()) {ShowSaveFailure();return;}
    using namespace GirlsSpire;
    const auto& state=Combat->Snapshot();
    RefreshPortraits();
    RefreshCombos();
    Grid->ClearChildren(); Panel->ClearChildren();
    const auto cues=ReadBoardCues(*Combat,ShowThreats);
    const bool large=state.Width>8 || state.Height>8;
    for(int y=0;y<state.Height;++y) for(int x=0;x<state.Width;++x) {
        const Position at{x,y};
        const auto unit=std::find_if(state.Units.begin(),state.Units.end(),[&](const BattleUnit& candidate) {
            return candidate.Hp>0 && candidate.At==at;
        });
        const auto& tile=state.Tiles[y*state.Width+x];
        const auto& cue=cues[y*state.Width+x];
        FString label=FString::Printf(TEXT("%d,%d"),x,y);
        if(tile.Elevation>0) label=FString::Printf(TEXT("高%d"),tile.Elevation);
        else if(tile.Cover>0) label=FString::Printf(TEXT("遮%d"),tile.Cover);
        if(tile.Trap) label=TEXT("罠10");
        FLinearColor color(0.12f,0.11f,0.15f);
        if(tile.Wall) { label=TEXT("壁"); color=FLinearColor(0.035f,0.035f,0.035f); }
        else if(tile.ObstacleHp>0) { label=FString::Printf(TEXT("障%d"),tile.ObstacleHp); color=FLinearColor(0.3f,0.2f,0.08f); }
        else if(unit!=state.Units.end()) {
            const TCHAR* side=unit->Side==Faction::Player?TEXT("味"):unit->Side==Faction::Enemy?
                (unit->Role==BattleRole::Cavalry?TEXT("敵騎"):unit->Role==BattleRole::Ninja?TEXT("敵忍"):unit->Role==BattleRole::Oni?TEXT("敵鬼"):unit->Role==BattleRole::CurseMonk?TEXT("敵僧"):unit->Role==BattleRole::Fox?TEXT("敵狐"):unit->Role==BattleRole::Spider?TEXT("敵絡"):unit->Role==BattleRole::CastleLord?TEXT("敵城"):unit->Role==BattleRole::FallenPrincess?TEXT("敵姫"):TEXT("敵")):TEXT("救");
            label=FString::Printf(TEXT("%s\n%d"),side,unit->Hp);
            color=unit->Side==Faction::Player?FLinearColor(0.05f,0.23f,0.4f):unit->Side==Faction::Enemy?FLinearColor(0.4f,0.09f,0.1f):FLinearColor(0.36f,0.28f,0.08f);
        } else if(cue.Move) {
            label+=TEXT("\n移動"); color=FLinearColor(0.06f,0.28f,0.22f);
        }
        FString detail=label;
        if(unit!=state.Units.end() && unit->Side==Faction::Enemy && unit->Role==BattleRole::FallenPrincess)
            detail=TEXT("落城姫 / 移動2・射程2・素早さ12\nHP50%以下で通常攻撃が対象中心の上下左右1マスを含む範囲攻撃へ変化。味方巻込み・壁越し攻撃・追加手番はありません。");
        if(unit!=state.Units.end() && unit->Side==Faction::Enemy && unit->Role==BattleRole::Spider)
            detail=TEXT("絡新婦 / 移動2・射程3・攻撃10\n通常命中で相手の次手番終了まで移動力−2（下限0）。重複せず再命中で更新し、行動後移動にも適用します。");
        if(unit!=state.Units.end() && unit->Side==Faction::Enemy && unit->Role==BattleRole::Fox)
            detail=TEXT("妖狐 / 移動3・射程3・攻撃8\n自身を含むHP50%以下の味方から、回復射程3・射線内でHP割合が最も低い者へ応急手当（回復20・再使用待ち2）を優先。不可なら射撃・接近します。");
        if(unit!=state.Units.end() && unit->Side==Faction::Enemy && unit->Role==BattleRole::CurseMonk)
            detail=TEXT("呪詛僧 / 移動2・射程3・攻撃10\n通常攻撃が命中すると相手の次の手番終了まで攻撃力−4（下限0）。重複せず再命中で更新します。");
        if(unit!=state.Units.end() && unit->Side==Faction::Enemy && unit->Role==BattleRole::Oni)
            detail=TEXT("鬼武者 / 移動2・射程1・攻撃16\n通常攻撃より予告ダメージが高ければ強撃（攻撃力＋8・再使用待ち2）を優先します。味方や壁越しの対象は攻撃しません。");
        if(unit!=state.Units.end() && unit->Side==Faction::Enemy && unit->Role==BattleRole::Ninja)
            detail=TEXT("骨忍 / 移動4・射程1\n攻撃前に移動していなければ攻撃後に退避。罠を含む被害が最小、次にプレイヤーとの距離が最大の位置を選びます。致死罠の経路は選びません。");
        if(unit!=state.Units.end() && unit->Role==BattleRole::Cavalry)
            detail=TEXT("騎馬亡者 / 移動5・射程1\n実際の移動経路が直線3マス以上なら、その手番の通常攻撃の攻撃力＋6。\nスキル対象外。行動後は移動できません。");
        if(unit!=state.Units.end() && unit->AttackCursed){label+=TEXT(" 呪");detail+=TEXT("\n呪詛：攻撃力−4（下限0）、自身の手番終了まで。");}
        if(unit!=state.Units.end() && unit->MovementWebbed){label+=TEXT(" 糸");detail+=TEXT("\n糸縛り：移動力−2（下限0）、自身の手番終了まで。");}
        detail+=FString::Printf(TEXT("\n経路の罠被害：%d"),cue.RouteTrapDamage);
        if(ShowThreats)detail+=FString::Printf(TEXT("\n現在地/合法移動先の敵被害上限：%d（実際の敵行動を確約しません）"),cue.EnemyDamage);
        if(MonochromeBoard)color=FLinearColor(0.12f,0.12f,0.12f);
        Grid->AddSlot(x,y)
        [ SNew(SBox).WidthOverride(FontSize(large?36:48)).HeightOverride(FontSize(large?48:60))
            [ SNew(SButton).ContentPadding(0).NormalPaddingOverride(FMargin(0)).PressedPaddingOverride(FMargin(0))
                .ToolTipText(FText::FromString(detail))
                .HAlign(HAlign_Fill).ButtonColorAndOpacity(color)
                .IsEnabled(state.Outcome==BattleOutcome::Ongoing)
                .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.cell.%d.%d"),x,y)))
                .OnClicked_Lambda([this,at,revision=state.Revision] { SelectCell(at,revision); return FReply::Handled(); })
                [ SNew(SVerticalBox)
                  +SVerticalBox::Slot().AutoHeight()
                  [ SNew(STextBlock).Text(FText::FromString(label)).Justification(ETextJustify::Center)
                    .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.celllabel.%d.%d"),x,y)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(large?8:11))).ColorAndOpacity(FLinearColor::White) ]
                  +SVerticalBox::Slot().AutoHeight()
                  [ SNew(STextBlock).Text(FText::FromString(UTF8_TO_TCHAR(cue.Symbols().c_str()))).Justification(ETextJustify::Center)
                    .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.cues.%d.%d"),x,y)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(large?8:11))).ColorAndOpacity(FLinearColor::White) ] ] ] ];
    }
    Text(FString::Printf(TEXT("第%dラウンド"),state.Round),"battle.round",23);
    Text(GenerationSummary,"battle.generation",13);
    for(const auto& unit:state.Units)if(unit.Hp>0 && unit.Role==BattleRole::CastleLord) {
        const auto summoned=std::count_if(state.Units.begin(),state.Units.end(),[id=unit.Id](const BattleUnit& other){return other.SummonerId==id && other.Hp>0;});
        Text(FString::Printf(TEXT("歴史喰らいの城主：HP%d/%d / 3の倍数ラウンドの手番開始に足軽1体召喚（存命%d/3）。召喚後も通常行動。城主撃破で勝利。"),unit.Hp,unit.MaxHp,static_cast<int>(summoned)),*FString::Printf(TEXT("battle.finalboss.%d"),unit.Id),14);
    }
    for(const auto& unit:state.Units)if(unit.Hp>0 && unit.Role==BattleRole::FallenPrincess)
        Text(FString::Printf(TEXT("落城姫：HP%d/%d / %s"),unit.Hp,unit.MaxHp,Combat->NormalAttackRadius(unit.Id)>0?TEXT("通常範囲攻撃（中心＋上下左右1マス）"):TEXT("通常単体攻撃（HP50%以下で範囲化）")),*FString::Printf(TEXT("battle.midboss.%d"),unit.Id),14);
    for(const auto& unit:state.Units)if(unit.Hp>0 && unit.MovementWebbed)
        Text(FString::Printf(TEXT("人物%d：糸縛り / 現在の移動力%d（−2・下限0）・自身の手番終了まで。"),unit.Id,Combat->MovementAllowance(unit.Id)),*FString::Printf(TEXT("battle.web.%d"),unit.Id),14);
    for(const auto& unit:state.Units)if(unit.Hp>0 && unit.AttackCursed)
        Text(FString::Printf(TEXT("人物%d：呪詛 / 攻撃力−4（下限0）・自身の手番終了まで。通常・技・必殺に反映。"),unit.Id),*FString::Printf(TEXT("battle.curse.%d"),unit.Id),14);
    for(const auto& unit:state.Units)if(unit.Hp>0 && (unit.AttackBoost.Amount || unit.DefenseBoost.Amount || unit.AccuracyBoost.Amount)) {
        const auto expiry=[](const GirlsSpire::TimedBoost& boost){return !boost.Amount?TEXT("なし"):boost.SkipCurrentEnd?TEXT("使用手番を除外し次の自身の手番終了まで"):TEXT("自身の手番終了まで");};
        Text(FString::Printf(TEXT("人物%d：強化 攻撃+%d / 防御+%d / 命中+%d\n攻撃：%s / 防御：%s / 命中：%s"),unit.Id,unit.AttackBoost.Amount,unit.DefenseBoost.Amount,unit.AccuracyBoost.Amount,expiry(unit.AttackBoost),expiry(unit.DefenseBoost),expiry(unit.AccuracyBoost)),*FString::Printf(TEXT("battle.boost.%d"),unit.Id),14);
    }
    for(const auto& unit:state.Units)if(unit.Hp>0 && unit.CreatesCover)
        Text(FString::Printf(TEXT("絡繰鎧（敵%d） 防御%d：偶数ラウンドの手番開始時に遮蔽1個を生成。生成後も通常行動します。"),unit.Id,unit.Defense),*FString::Printf(TEXT("battle.cover.rule.%d"),unit.Id),14);
    if(OnSuspend) Action(TEXT("保存して中断する"),"save.suspend",[this] {
        const auto keepAlive=SharedThis(this);
        SuspensionPending=true;if(!PersistCheckpoint()) Refresh();
    });
    if(state.Outcome!=BattleOutcome::Ongoing) {
        Text(state.Outcome==BattleOutcome::Won?TEXT("戦闘勝利"):TEXT("全員戦闘不能"),
            state.Outcome==BattleOutcome::Won?"battle.won":"battle.lost",27);
        Text(Message,"battle.log",15);
        Action(TEXT("経路へ戻る"),"battle.return",[this] { OnFinished.ExecuteIfBound(); });
        if(Control.ProfileDriven) {
            FString bonds=TEXT("今回の絆（経路へ戻る際に確定）");
            for(const auto& unit:state.Units) if(unit.Side==Faction::Player && unit.Id>0 && unit.Id<=8) {
                const int id=unit.Id,combo=state.BondGains.Combo[id-1],aid=state.BondGains.Aid[id-1];
                const int after=std::min(100,Control.Trust[id]+combo+aid);
                bonds+=FString::Printf(TEXT("\n味方%d: 連携+%d / 援護+%d / 絆%d → %d（実増加+%d）"),id,combo,aid,Control.Trust[id],after,after-Control.Trust[id]);
            }
            Text(bonds,"battle.bond.result",14);
        }
        return;
    }
    Text(FString::Printf(TEXT("味方%dの手番　移動:%s / 行動:%s"),state.ActiveId,
        state.Moved?TEXT("済"):TEXT("未"),state.Acted?TEXT("済"):TEXT("未")),"battle.turn");
    if(SelectingUltimate) { RefreshUltimate(); Text(Message,"battle.log",14); return; }
    RefreshMedicine();
    if(SelectingMedicine) {Text(Message,"battle.log",14);return;}
    if(state.ActiveId>0 && state.ActiveId<9) { RefreshIntent(); return; }
    Text(TEXT("人物マスの数値はHP。移動先・攻撃対象をクリックし、予告を確認して確定します。"),"battle.help",15);
    if(MoveTarget) {
        const auto movement=Combat->PreviewMove(state.ActiveId,*MoveTarget);
        FString route=TEXT("移動経路: ");
        for(const auto at:movement.Path) route+=FString::Printf(TEXT("(%d,%d) "),at.X,at.Y);
        route+=FString::Printf(TEXT("/ 罠被害%d"),movement.Damage);
        if(movement.KnockedOut) route+=TEXT(" / 警告: 途中で戦闘不能になり停止します");
        Text(route,"battle.move.preview",15);
        Action(movement.KnockedOut?TEXT("戦闘不能を了承して移動"):TEXT("移動を確定"),"battle.move.confirm",
            [this,revision=state.Revision] { ConfirmMove(revision); },movement.Legal);
        Action(TEXT("移動を取り消す"),"battle.move.cancel",[this] { MoveTarget.reset(); Refresh(); });
    }
    if(Selected && !SelectedSkill) {
        const auto prediction=Combat->PreviewAttack(state.ActiveId,*Selected);
        FString preview=TEXT("この対象には攻撃できません");
        if(prediction.Legal) {
            if(prediction.Hits.empty()) {
                const auto hp=state.Tiles[Selected->Y*state.Width+Selected->X].ObstacleHp;
                preview=FString::Printf(TEXT("障害物　確定命中 / 耐久 %d → %d"),hp,hp-prediction.TerrainDamage);
            } else preview=FString::Printf(TEXT("敵%d　命中%d%% / 予想%dダメージ"),
                prediction.Hits[0].TargetId,prediction.Hits[0].Chance,prediction.Hits[0].Damage);
        }
        Text(preview,"battle.preview");
        Action(TEXT("攻撃を確定"),"battle.confirm",[this,revision=state.Revision] { ConfirmAttack(revision); },prediction.Legal);
    }
    for(const auto& target:state.Units) {
        if(!Combat->PreviewAttack(state.ActiveId,target.At).Legal) continue;
        const TCHAR* kind=target.Side==Faction::Enemy && target.Id>0 && target.Id<=8?FCharacterPortraits::Name(target.Id):target.CreatesCover?TEXT("絡繰鎧・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::CastleLord?TEXT("歴史喰らいの城主・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::FallenPrincess?TEXT("落城姫・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::Spider?TEXT("絡新婦・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::Fox?TEXT("妖狐・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::CurseMonk?TEXT("呪詛僧・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::Oni?TEXT("鬼武者・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::Ninja?TEXT("骨忍・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::Cavalry?TEXT("騎馬亡者・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::Archer?TEXT("弓兵亡者・敵"):
            target.Side==Faction::Enemy && target.Role==BattleRole::Gunner?TEXT("鉄砲亡者・敵"):TEXT("敵");
        Action(FString::Printf(TEXT("%s%dを狙う（HP%d）"),kind,target.Id,target.Hp),
            *FString::Printf(TEXT("battle.target.%d"),target.Id),
            [this,at=target.At,revision=state.Revision] { SelectCell(at,revision); });
    }
    const TCHAR* labels[]={TEXT("北を向いて待機"),TEXT("東を向いて待機"),TEXT("南を向いて待機"),TEXT("西を向いて待機")};
    const TCHAR* ids[]={TEXT("battle.wait.north"),TEXT("battle.wait.east"),TEXT("battle.wait.south"),TEXT("battle.wait.west")};
    for(int facing=0;facing<4;++facing)
        Action(labels[facing],ids[facing],[this,facing,revision=state.Revision] { Wait(static_cast<Facing>(facing),revision); });
    RefreshSkills();
    RefreshUltimate();
    Text(Message,"battle.log",14);
    Text(TEXT("○ 味方 / △ 敵 / ◇ 中立 / ＋ 移動先 / × 通常攻撃可能 / ！ 罠・予測被害"),"battle.cues.legend",13);
    Action(MonochromeBoard?TEXT("盤面：単色（切替）"):TEXT("盤面：色付き（切替）"),"battle.cues.monochrome",[this]{MonochromeBoard=!MonochromeBoard;Refresh(false);});
    Action(ShowThreats?TEXT("敵被害上限の予測：表示中（切替）"):TEXT("敵被害上限の予測：非表示（切替）"),"battle.cues.threats",[this]{ShowThreats=!ShowThreats;Refresh(false);});
    if(ShowThreats)Text(TEXT("！は現在地と合法移動先の被害上限も含みます。敵の実際の行動・命中を確約する予測ではありません。詳細は各マスに表示。"),"battle.cues.scope",13);
}

void SCombatScreen::SelectCell(GirlsSpire::Position cell,std::uint64_t revision)
{
    const auto& state=Combat->Snapshot();
    if(revision!=state.Revision) return;
    if(SelectingUltimate) { Selected=cell; Refresh(); return; }
    if(SelectingMedicine) {Selected=cell;Refresh();return;}
    if(state.ActiveId>0 && state.ActiveId<9) { SelectIntentCell(cell); Refresh(); return; }
    Selected.reset(); MoveTarget.reset();
    if(SelectedSkill) { Selected=cell; Refresh(); return; }
    const auto occupant=std::find_if(state.Units.begin(),state.Units.end(),[&](const GirlsSpire::BattleUnit& unit) {
        return unit.Hp>0 && unit.At==cell && unit.Side!=GirlsSpire::Faction::Player;
    });
    if(occupant!=state.Units.end() || state.Tiles[cell.Y*state.Width+cell.X].ObstacleHp>0) { Selected=cell; Message.Empty(); }
    else {
        if(Combat->PreviewMove(state.ActiveId,cell).Legal) { MoveTarget=cell; Message.Empty(); }
        else Message=TEXT("そのマスには移動できません");
    }
    Refresh();
}

void SCombatScreen::DescribeHits()
{
    if(Audio && std::any_of(Combat->LastHits().begin(),Combat->LastHits().end(),[](const auto& hit){return hit.Hit && hit.Damage>0;}))Audio->Cue(EGameCue::Impact);
    Message.Empty();
    for(const auto& hit:Combat->LastHits())
        Message+=hit.Hit?FString::Printf(TEXT("対象%dに%dダメージ。"),hit.TargetId,hit.Damage):FString::Printf(TEXT("対象%dは回避。"),hit.TargetId);
}

void SCombatScreen::ConfirmMove(std::uint64_t revision)
{
    if(!MoveTarget) return;
    const auto actor=Combat->Snapshot().ActiveId;
    const auto preview=Combat->PreviewMove(actor,*MoveTarget);
    if(Combat->Move(revision,actor,*MoveTarget))
        Message=FString::Printf(TEXT("%s / 罠被害%d"),preview.KnockedOut?TEXT("罠で戦闘不能・移動を中断しました"):TEXT("移動しました"),preview.Damage);
    MoveTarget.reset();
    AdvanceEnemies();
    Refresh();
}

void SCombatScreen::ConfirmAttack(std::uint64_t revision)
{
    if(!Selected) return;
    const auto& state=Combat->Snapshot();
    const int tileIndex=Selected->Y*state.Width+Selected->X;
    const bool obstacle=state.Tiles[tileIndex].ObstacleHp>0;
    if(Combat->Attack(revision,state.ActiveId,*Selected)) {
        if(obstacle) {
            const int hp=Combat->Snapshot().Tiles[tileIndex].ObstacleHp;
            Message=hp==0?TEXT("障害物を破壊しました"):FString::Printf(TEXT("障害物の残り耐久%d"),hp);
        } else DescribeHits();
    }
    Selected.reset();
    Refresh();
}

void SCombatScreen::Wait(GirlsSpire::Facing facing,std::uint64_t revision)
{
    Combat->EndTurn(revision,Combat->Snapshot().ActiveId,facing);
    SelectingUltimate=false;
    SelectingMedicine=false;
    SelectedSkill.reset();
    Selected.reset();
    MoveTarget.reset();
    AdvanceEnemies();
    Refresh();
}

void SCombatScreen::AdvanceEnemies()
{
    if(!PersistCheckpoint()) return;
    using namespace GirlsSpire;
    const auto limit=Combat->Snapshot().Units.size()*6;
    for(std::size_t step=0;step<limit;++step) {
        const auto& state=Combat->Snapshot();
        if(state.Outcome!=BattleOutcome::Ongoing) return;
        const auto actor=std::find_if(state.Units.begin(),state.Units.end(),[&](const BattleUnit& unit) { return unit.Id==state.ActiveId; });
        if(actor->Side==Faction::Player) return;
        const auto action=ChooseBasicAction(*Combat);
        const auto recovery=action.Kind==BasicActionKind::UseSkill?Combat->PreviewSkill(state.ActiveId,action.Ability,action.Target):SkillPreview{};
        if(!ApplyBasicAction(*Combat,action)) return;
        if(!PersistCheckpoint()) return;
        if(recovery.Healing>0)Message=FString::Printf(TEXT("応急手当で人物%dを%d回復しました。"),recovery.TargetId,recovery.Healing);
        else if(action.Kind==BasicActionKind::Attack || action.Kind==BasicActionKind::UseSkill) DescribeHits();
    }
}
