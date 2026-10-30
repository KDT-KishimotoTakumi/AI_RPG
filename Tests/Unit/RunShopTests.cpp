#include "Application/Encounter.h"
#include <iostream>
using namespace GirlsSpire;
int TestRunShop()
{
    int checks=0,failed=0,shops=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RunShop: "<<why<<'\n';}};
    for(int seed=0;seed<100;++seed) {
        Run run(seed,Difficulty::Beginner,HeroRole::Sword);
        check(run.Snapshot().Wallet==Purse{100,0},"開始資源");
        while(run.Snapshot().Phase==RunPhase::Route) {
            const auto before=run.Snapshot();
            check(!run.Purchase(before.Revision,ShopOffer::Power,0) && run.Snapshot()==before,"店舗外拒否");
            int index=0;
            for(int i=0;i<static_cast<int>(before.Choices.size());++i) if(before.Choices[i].Kind==NodeKind::Shop) index=i;
            run.ChooseNode(before.Revision,index);
            const auto entry=run.Snapshot();const auto kind=entry.Active.Kind;
            if(kind==NodeKind::Shop) {
                ++shops;check(entry.Stock==ShopStock{},"新店は固定在庫");
                for(auto offer:{ShopOffer::Guard,ShopOffer::Medicine,ShopOffer::Heal,ShopOffer::Treat}) {
                    auto probe=run;const auto initial=probe.Snapshot();
                    const bool effect=offer==ShopOffer::Heal?probe.CurrentHp(0)<probe.MaxHp(0):offer==ShopOffer::Treat?initial.Health[0].Wounds>0:true;
                    const bool expected=initial.Wallet.Coins>=ShopPrice(offer) && effect;
                    check(probe.Purchase(initial.Revision,offer,0)==expected,"各商品の周回接続");
                    if(!expected) check(probe.Snapshot()==initial,"効果なし・不足の購入で状態不変");
                    else {
                        check(probe.Snapshot().Wallet.Coins==initial.Wallet.Coins-ShopPrice(offer),"各商品の価格反映");
                        if(offer==ShopOffer::Medicine) check(probe.Snapshot().Wallet.Medicines==initial.Wallet.Medicines+1,"薬の共有所持数");
                        if(offer==ShopOffer::Heal) check(probe.CurrentHp(0)==probe.MaxHp(0),"周回HP全快");
                        if(offer==ShopOffer::Treat) check(probe.Snapshot().Health[0].Wounds==initial.Health[0].Wounds-1 && probe.CurrentHp(0)==run.CurrentHp(0),"治療でHP不変");
                        if(offer==ShopOffer::Guard) {
                            auto fixture=probe.Snapshot();fixture.Active={NodeKind::Battle};
                            const auto encounter=Encounter::Create(fixture,seed).Snapshot();
                            for(const auto& unit:encounter.Units) if(unit.Id==0)
                                check(unit.Defense==2+fixture.Growth[0].Bonuses[2]+fixture.Levels[0].Level-1+2 &&
                                      unit.Attack==18+fixture.Growth[0].Bonuses[1]+fixture.Levels[0].Level-1,"守り装備で力効果は除去");
                        }
                    }
                }
                check(!run.Purchase(entry.Revision+1,ShopOffer::Power,0) && run.Snapshot()==entry,"古い予告拒否");
                check(!run.Purchase(entry.Revision,ShopOffer::Power,9) && run.Snapshot()==entry,"範囲外人物拒否");
                for(int id=1;id<=8;++id) if(run.MaxHp(id)==0)
                    check(!run.Purchase(entry.Revision,ShopOffer::Power,id) && run.Snapshot()==entry,"未加入者拒否");
                if(entry.Wallet.Coins>=100) {
                    auto replay=run;const auto rng=run.RandomState();
                    check(run.Purchase(entry.Revision,ShopOffer::Power,0),"護符購入");
                    replay.Purchase(entry.Revision,ShopOffer::Power,0);
                    const auto after=run.Snapshot();
                    check(after.Wallet.Coins==entry.Wallet.Coins-100 && after.Accessories[0]==Accessory::Power && after.Stock.Power==0,"資源と装備反映");
                    check(after.Day==entry.Day && after.EventIndex==entry.EventIndex && after.Phase==RunPhase::Event && after.Revision==entry.Revision+1,"購入で日程不変");
                    check(!run.Purchase(entry.Revision,ShopOffer::Power,0) && run.Snapshot()==after,"二重確定拒否");
                    check(run.Snapshot()==replay.Snapshot() && run.RandomState()==rng,"購入で乱数不変・再現");
                }
                const auto oldRev=run.Snapshot().Revision;run.CompleteEvent(oldRev);
                const auto exit=run.Snapshot();
                check(!run.Purchase(oldRev,ShopOffer::Medicine,0) && run.Snapshot()==exit,"退出後購入不可");
            } else if(kind==NodeKind::Rest || kind==NodeKind::Social || kind==NodeKind::Anomaly) {
                run.CompleteEvent(entry.Revision);
            } else {
                auto end=Encounter::Create(entry,seed).Snapshot();
                for(const auto& u:end.Units) if(u.Id==0)
                    check(u.Attack==18+entry.Growth[0].Bonuses[1]+entry.Levels[0].Level-1+(entry.Accessories[0]==Accessory::Power?3:0),"装備は次戦へ一度だけ反映");
                end.Outcome=BattleOutcome::Won;
                if(!entry.Recruited.empty()) for(auto& unit:end.Units) if(unit.Id==0) unit.Hp=0;
                auto defeat=run;auto lost=end;lost.Outcome=BattleOutcome::Lost;
                for(auto& u:lost.Units) if(u.Side==Faction::Player) u.Hp=0;
                check(defeat.CompleteBattle(entry.Revision,lost) && defeat.Snapshot().Wallet==Purse{0,0} && defeat.Snapshot().Accessories==RunSnapshot{}.Accessories,"敗北で消去");
                check(!run.CompleteBattle(entry.Revision+1,end) && run.Snapshot()==entry,"不正決着で銭を付与しない");
                check(run.CompleteBattle(entry.Revision,end),"勝利受付");
                const int reward=kind==NodeKind::Elite?75:kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss?100:50;
                check(run.Snapshot().Wallet.Coins==entry.Wallet.Coins+reward,"固定勝利報酬");
                const auto won=run.Snapshot();
                check(!run.CompleteBattle(entry.Revision,end) && run.Snapshot()==won,"二重付与拒否");
                run.ChooseRecipient(won.Revision,0);run.ClaimReward(run.Snapshot().Revision,0);
                if(run.Snapshot().PendingRescue>=0)run.ConverseRescue(run.Snapshot().Revision,ConversationChoice::Neutral);
            }
        }
        check(run.Snapshot().Phase==RunPhase::Won && run.Snapshot().Wallet==Purse{0,0} && run.Snapshot().Accessories==RunSnapshot{}.Accessories,"勝利で消去");
    }
    check(shops>0,"商店へ実際に到達");
    std::cout<<"RunShop: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
