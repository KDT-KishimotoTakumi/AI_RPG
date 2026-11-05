#include "Application/Encounter.h"
#include "AI/Intent.h"
#include "Infrastructure/SavePayload.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
using namespace GirlsSpire;
struct CampaignResult {
    std::vector<std::string> Trace;
    std::set<int> Days;
    int Battles=0,Actions=0,MidBosses=0,FinalBosses=0;
};

// 能力値・地形・HPを上書きせず、出発から最終報酬まで通常APIだけを使う。
// 主人公の選択は検証用方策。仲間は実際の信頼別抽選、敵は製品の思考を使う。
CampaignResult PlayCampaign(std::uint64_t seed,bool restoreEveryStep)
{
    Run run(seed,Difficulty::Beginner,HeroRole::CommandFan);
    CampaignResult result;
    const auto require=[](bool pass){if(!pass)throw std::runtime_error("通し検証の操作または保存が拒否されました");};
    const auto record=[&](Battle* battle=nullptr,CombatControl* control=nullptr) {
        require(run.DiscoverEnemies(battle?battle->Snapshot().Units:std::vector<BattleUnit>{}));
        if(battle && battle->Snapshot().LastCombo)require(run.DiscoverCombo(battle->Snapshot().LastCombo->RecipeId));
        RunSaveState saved{run.Checkpoint()};
        if(battle)saved.Combat=SavedCombat{battle->Checkpoint(),control->Checkpoint()};
        const auto bytes=EncodeRunPayload(saved),profileBytes=EncodeProfilePayload(run.ProfileState());
        require(bytes.has_value() && profileBytes.has_value());
        result.Trace.push_back(*bytes+*profileBytes);
        if(!restoreEveryStep)return;
        const auto decoded=DecodeRunPayload(*bytes);const auto profile=DecodeProfilePayload(*profileBytes);
        require(decoded && profile && *decoded==saved && *profile==run.ProfileState());
        const auto restored=Run::Restore(decoded->Run,*profile);require(restored.has_value());run=*restored;
        if(battle){
            const auto restoredBattle=Battle::Restore(decoded->Combat->Battle);require(restoredBattle.has_value());
            const auto restoredControl=CombatControl::Restore(decoded->Combat->Control,*restoredBattle);require(restoredControl.has_value());
            *battle=*restoredBattle;*control=*restoredControl;
        }
    };
    for(int transition=0;transition<256;++transition) {
        record();const auto state=run.Snapshot();result.Days.insert(state.Day);
        if(state.Phase==RunPhase::Won){
            require(state.Day==12 && state.Recruited.size()==4 && run.ProfileState().HighestDifficulty==1);
            require(result.MidBosses==1 && result.FinalBosses==1 && result.Days.size()==12);
            require(run.EpilogueChoices()==std::vector<int>{0});return result;
        }
        require(state.Phase!=RunPhase::Lost);
        if(state.PendingRescue>=0){require(run.ConverseRescue(state.Revision,ConversationChoice::Warm));continue;}
        if(state.Phase==RunPhase::Reward){
            if(state.RewardRecipient<0)require(run.ChooseRecipient(state.Revision,0));
            else {
                int pick=0;
                for(int i=0;i<static_cast<int>(state.Rewards.size());++i)if(state.Rewards[i]==Reward::Attack)pick=i;
                require(run.ClaimReward(state.Revision,pick));
            }
            continue;
        }
        if(state.Phase==RunPhase::Route){
            int pick=0;
            for(int i=0;i<static_cast<int>(state.Choices.size());++i)if(state.Choices[i].Kind==NodeKind::Rest)pick=i;
            require(run.ChooseNode(state.Revision,pick));continue;
        }
        switch(state.Active.Kind){
        case NodeKind::Rest:require(run.Rest(state.Revision));continue;
        case NodeKind::Social:require(!state.Recruited.empty() && run.Converse(state.Revision,state.Recruited.front()+1,ConversationChoice::Warm));continue;
        case NodeKind::Shop:case NodeKind::Anomaly:require(run.CompleteEvent(state.Revision));continue;
        default:break;
        }
        if(state.Active.Kind==NodeKind::MidBoss){require(state.Day==6);++result.MidBosses;}
        if(state.Active.Kind==NodeKind::FinalBoss){require(state.Day==12);++result.FinalBosses;}
        auto battle=Encounter::Create(state,run.RandomState(),run.ProfileState());
        CombatControl control;control.ProfileDriven=true;control.ChoiceRandom=Random(battle.RandomState());
        for(int id=1;id<=8;++id)control.Trust[id]=run.ProfileState().Bonds[id-1];
        ++result.Battles;record(&battle,&control);
        for(int step=0;battle.Snapshot().Outcome==BattleOutcome::Ongoing && step<500;++step){
            const auto& current=battle.Snapshot();
            const auto actor=std::find_if(current.Units.begin(),current.Units.end(),[&](const auto& unit){return unit.Id==current.ActiveId;});
            require(actor!=current.Units.end());
            if(actor->Side==Faction::Enemy)require(ApplyBasicAction(battle,ChooseBasicAction(battle)));
            else if(actor->Id==0){
                const auto ranked=RankIntentActions(battle,{});require(!ranked.empty());
                require(ApplyBasicAction(battle,ranked.front()));
            }else{
                const auto forecast=IntentForecast::Build(battle,RankIntentActions(battle,control.Orders[actor->Id]),control.Trust[actor->Id]);
                require(forecast.Execute(battle,control.ChoiceRandom));
            }
            ++result.Actions;record(&battle,&control);
        }
        require(battle.Snapshot().Outcome==BattleOutcome::Won);
        require(run.CompleteBattle(state.Revision,battle.Snapshot()));
        require(run.Snapshot().Wallet.Coins>state.Wallet.Coins);
        const auto& level=run.Snapshot().Levels[0];
        require(level.Level>state.Levels[0].Level || level.Experience>state.Levels[0].Experience);
    }
    throw std::runtime_error("12日通し検証が有限手数内に完了しませんでした");
}
}

int TestCampaign()
{
    int checks=0,failed=0;
    const auto check=[&](bool pass,const char* message){++checks;if(!pass){++failed;std::cerr<<"FAIL Campaign: "<<message<<'\n';}};
    for(std::uint64_t seed=1;seed<=4;++seed)try{
        const auto continuous=PlayCampaign(seed,false),restored=PlayCampaign(seed,true);
        check(continuous.Trace==restored.Trace,"全行動・イベントの再構築前後で周回/恒久/戦闘/信頼/抽選乱数の全保存バイトが一致");
        check(continuous.Battles>=6 && continuous.Actions>continuous.Battles,"救出4戦と中間/最終ボスを戦闘省略なしで完了");
        check(continuous.MidBosses==1 && continuous.FinalBosses==1 && continuous.Days.size()==12,"全12日と6/12日のボスを通過");
        std::cout<<"Campaign seed="<<seed<<": "<<continuous.Battles<<" battles, "<<continuous.Actions<<" actions, "<<continuous.Trace.size()<<" checkpoints\n";
    }catch(const std::exception& error){check(false,error.what());}
    std::cout<<"Campaign: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
