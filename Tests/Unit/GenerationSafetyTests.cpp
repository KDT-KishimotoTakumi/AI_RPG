#include "Application/Encounter.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

// GS-124: 戦場生成の安全性質（FR-015～018・SC-003）を、実戦編成の複数シード×全役割×全難易度で検査する。
// 製品コードは変更せず、既存挙動を性質として固定する。
namespace {
constexpr int SeedCount=100;
constexpr int ForcedSeedCount=10;
constexpr int HpSweepSeedCount=8;
constexpr int PartySize=4;
constexpr int HpSweepMax=40;
constexpr int MaxAttempts=8;
constexpr int FallbackSize=12;
constexpr int MinFieldSize=8;
constexpr int MaxFieldSize=12;
constexpr int ObstacleDurability=20;
constexpr int CoverValue=20;
constexpr int CharacterCount=8;
// 救出候補は加入済みの人物を除く必要があるため、未加入者からシードで選ぶ。
int Candidate(const std::vector<int>& recruited,int seed)
{
    std::vector<int> free;
    for(int id=0;id<CharacterCount;++id)if(std::find(recruited.begin(),recruited.end(),id)==recruited.end())free.push_back(id);
    return free[static_cast<std::size_t>(seed)%free.size()];
}
}

int TestGenerationSafety()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool pass,const char* label){++checks;if(!pass){++failed;std::cerr<<"FAIL GenerationSafety: "<<label<<'\n';}};
    Profile profile;profile.Bonds={0,24,49,50,74,75,99,100};
    std::set<int> heroRanges,allies,skillsSeen;std::set<BattleRole> enemyRoles;std::set<std::pair<int,int>> sizes;
    bool sawWall=false,sawElevation=false,sawCover=false,sawObstacle=false,sawTrap=false,sawAllTerrain=false;
    // 盤面全体の性質。到達性は占有を無視した地形上の接続（罠回避）で、全参加者の全組合せを確認する。
    const auto inspect=[&](const Battle& battle,BattleGoal goal,const char* label){
        const auto& state=battle.Snapshot();
        check(state.Width>=MinFieldSize && state.Width<=MaxFieldSize && state.Height>=MinFieldSize && state.Height<=MaxFieldSize,"寸法は8～12");
        check(state.Tiles.size()==static_cast<std::size_t>(state.Width*state.Height),"タイル数が寸法と一致");
        check(state.Outcome==BattleOutcome::Ongoing,"開始直後に勝敗が決まらない");
        bool wall=false,elevation=false,cover=false,obstacle=false,trap=false,valuesValid=true,wallsPure=true;
        for(const auto& tile:state.Tiles){
            valuesValid=valuesValid && tile.Elevation>=0 && tile.Elevation<=1 && (tile.Cover==0 || tile.Cover==CoverValue) &&
                (tile.ObstacleHp==0 || tile.ObstacleHp==ObstacleDurability);
            wallsPure=wallsPure && (!tile.Wall || (tile.Elevation==0 && tile.Cover==0 && tile.ObstacleHp==0 && !tile.Trap));
            wall=wall||tile.Wall;elevation=elevation||tile.Elevation>0;cover=cover||tile.Cover>0;obstacle=obstacle||tile.ObstacleHp>0;trap=trap||tile.Trap;
        }
        check(valuesValid,"タイル値が生成契約の範囲");
        check(wallsPure,"壁に他の地形を重ねない");
        sawWall=sawWall||wall;sawElevation=sawElevation||elevation;sawCover=sawCover||cover;sawObstacle=sawObstacle||obstacle;sawTrap=sawTrap||trap;
        sawAllTerrain=sawAllTerrain||(wall&&elevation&&cover&&obstacle&&trap);
        sizes.insert({state.Width,state.Height});
        std::set<std::pair<int,int>> occupied;std::set<int> ids;int players=0,enemies=0;
        for(const auto& unit:state.Units){
            const bool inside=unit.At.X>=0 && unit.At.X<state.Width && unit.At.Y>=0 && unit.At.Y<state.Height;
            check(inside,"全参加者の初期位置が盤内");
            check(occupied.insert({unit.At.X,unit.At.Y}).second,"初期位置が重複しない");
            check(ids.insert(unit.Id).second,"参加者IDが重複しない");
            check(unit.Hp>0 && unit.Hp<=unit.MaxHp,"初期HPが有効");
            if(inside){const auto& tile=state.Tiles[unit.At.Y*state.Width+unit.At.X];check(!tile.Wall && tile.ObstacleHp==0 && !tile.Trap,"初期位置が壁・障害物・罠でない");}
            players+=unit.Side==Faction::Player;enemies+=unit.Side==Faction::Enemy;
            if(unit.Side==Faction::Enemy)enemyRoles.insert(unit.Role);
            if(unit.Side==Faction::Player && unit.Id>=1 && unit.Id<=8)allies.insert(unit.Id);
            for(int index=0;index<SkillCount;++index)if(unit.Skills[index])skillsSeen.insert(index);
            if(unit.Side==Faction::Player)check(battle.PotentialEnemyDamage(unit.Id)<unit.Hp,"味方は初期の合計被害上限で即死しない");
        }
        check(players>0 && enemies>0,"味方と敵が存在する");
        const auto hero=std::find_if(state.Units.begin(),state.Units.end(),[](const BattleUnit& unit){return unit.Id==0;});
        if(hero==state.Units.end()){check(false,"主人公が存在する");return;}
        heroRanges.insert(hero->Range);
        const auto walkable=std::count_if(state.Tiles.begin(),state.Tiles.end(),[](const Tile& tile){return !tile.Wall && tile.ObstacleHp==0 && !tile.Trap;});
        bool connectedAll=true,reachesAll=true;
        for(const auto& from:state.Units){
            const auto connected=battle.ConnectedTerrain(from.At,true);
            connectedAll=connectedAll && connected.size()==static_cast<std::size_t>(walkable);
            for(const auto& to:state.Units)
                reachesAll=reachesAll && std::find(connected.begin(),connected.end(),to.At)!=connected.end();
        }
        check(connectedAll,"歩行可能マスが一つの領域として接続");
        check(reachesAll,label);
        // 目的別: 目的の対象へ主人公が地形上到達できる。
        const auto heroArea=battle.ConnectedTerrain(hero->At,true);
        const auto reaches=[&](const BattleUnit& unit){return std::find(heroArea.begin(),heroArea.end(),unit.At)!=heroArea.end();};
        for(const auto& unit:state.Units){
            if(goal==BattleGoal::Rescue && unit.Id==200)check(reaches(unit) && unit.Side==Faction::Neutral,"救出対象へ到達できる");
            if(goal==BattleGoal::DefeatLord && unit.Role==BattleRole::CastleLord)check(reaches(unit),"城主へ到達できる");
            if((goal==BattleGoal::Eliminate || goal==BattleGoal::Survive) && unit.Side==Faction::Enemy)check(reaches(unit),"全敵へ到達できる");
        }
        if(goal==BattleGoal::Rescue)check(std::any_of(state.Units.begin(),state.Units.end(),[](const BattleUnit& unit){return unit.Id==200;}),"救出対象が存在する");
        if(goal==BattleGoal::DefeatLord)check(std::any_of(state.Units.begin(),state.Units.end(),[](const BattleUnit& unit){return unit.Role==BattleRole::CastleLord;}),"城主が存在する");
    };
    const auto goalOf=[](NodeKind kind){return kind==NodeKind::Rescue?BattleGoal::Rescue:kind==NodeKind::FinalBoss?BattleGoal::DefeatLord:BattleGoal::Eliminate;};
    const std::vector<std::vector<int>> parties={{},{0,2,4,6},{1,3,5,7},{0,1,2,3}};
    int automatic=0,fallbacks=0;
    for(int difficulty=0;difficulty<3;++difficulty)for(int role=0;role<4;++role)for(int seed=0;seed<SeedCount;++seed){
        RunSnapshot run;run.Phase=RunPhase::Event;run.Level=static_cast<Difficulty>(difficulty);run.Role=static_cast<HeroRole>(role);
        run.Recruited=parties[seed%parties.size()];
        for(auto kind:{NodeKind::Battle,NodeKind::Elite,NodeKind::Rescue,NodeKind::MidBoss,NodeKind::FinalBoss}){
            run.Active={kind,Candidate(run.Recruited,seed)};
            run.Day=kind==NodeKind::MidBoss?6:kind==NodeKind::FinalBoss?12:(seed%3==0?1:seed%3==1?5:9);
            for(int mode=0;mode<2;++mode){
                GenerationResult details;
                const auto battle=mode==0?Encounter::Create(run,seed,&details):Encounter::Create(run,seed,profile,&details);
                inspect(battle,goalOf(kind),"全参加者が相互に地形上到達可能");
                const auto again=mode==0?Encounter::Create(run,seed):Encounter::Create(run,seed,profile);
                check(again.Checkpoint()==battle.Checkpoint(),"同一シード・同一入力で戦場と戦闘が再現");
                check(details.Attempts>=1 && details.Attempts<=MaxAttempts,"試行回数は1～8");
                check(details.Layout.Tiles==battle.Snapshot().Tiles && details.Layout.Width==battle.Snapshot().Width,"生成結果が戦闘へ反映");
                if(details.UsedFallback){++fallbacks;check(details.Attempts==MaxAttempts,"退避は8回失敗後だけ");}else ++automatic;
            }
        }
    }
    check(automatic>0 && automatic>fallbacks,"通常は自動生成が採用され固定戦場へ退避しない");
    check(sawWall && sawElevation && sawCover && sawObstacle && sawTrap,"壁・高低差・遮蔽・障害物・罠をすべて生成");
    check(sawAllTerrain,"全地形種を同時に含む戦場がある");
    check(sizes.size()>1,"複数の寸法を生成");
    check(std::all_of(sizes.begin(),sizes.end(),[](const auto& size){return size.first>=MinFieldSize && size.second>=MinFieldSize;}),"最小寸法を下回らない");
    check(heroRanges==std::set<int>{1,2,3,5},"全4武器の戦場を含む");
    check(allies.size()==8,"全8仲間を含む戦場");
    const std::set<BattleRole> expectedRoles={BattleRole::Standard,BattleRole::Archer,BattleRole::Gunner,BattleRole::Cavalry,BattleRole::Ninja,BattleRole::Oni,
        BattleRole::CurseMonk,BattleRole::Fox,BattleRole::Spider,BattleRole::FallenPrincess,BattleRole::CastleLord};
    check(std::includes(enemyRoles.begin(),enemyRoles.end(),expectedRoles.begin(),expectedRoles.end()),"全敵役割を含む戦場");
    for(auto expected:expectedRoles)if(!enemyRoles.contains(expected))std::cerr<<"  未出現の敵役割: "<<static_cast<int>(expected)<<'\n';
    check(skillsSeen.contains(static_cast<int>(Skill::PowerStrike)) && skillsSeen.contains(static_cast<int>(Skill::FirstAid)),"特殊技を持つ参加者を含む戦場");
    std::cout<<"GenerationSafety: automatic="<<automatic<<" fallback="<<fallbacks<<" sizes="<<sizes.size()<<" skills="<<skillsSeen.size()<<'\n';

    // 8回失敗後の固定戦場: 全種別・全役割・全難易度で、固定戦場上でも同じ性質を満たす。
    for(int difficulty=0;difficulty<3;++difficulty)for(int role=0;role<4;++role)for(int seed=0;seed<ForcedSeedCount;++seed){
        RunSnapshot run;run.Phase=RunPhase::Event;run.Level=static_cast<Difficulty>(difficulty);run.Role=static_cast<HeroRole>(role);run.Recruited=parties[seed%parties.size()];
        for(auto kind:{NodeKind::Battle,NodeKind::Elite,NodeKind::Rescue,NodeKind::MidBoss,NodeKind::FinalBoss}){
            run.Active={kind,Candidate(run.Recruited,seed)};run.Day=kind==NodeKind::MidBoss?6:kind==NodeKind::FinalBoss?12:5;
            for(int mode=0;mode<2;++mode){
                GenerationResult details;
                const auto battle=mode==0?Encounter::Create(run,seed,&details,true):Encounter::Create(run,seed,profile,&details,true);
                check(details.UsedFallback && details.Attempts==MaxAttempts,"強制失敗は8回試行後に退避");
                check(details.Layout.Width==FallbackSize && details.Layout.Height==FallbackSize && details.Layout.Tiles==std::vector<Tile>(FallbackSize*FallbackSize),"退避先は平坦な12×12");
                inspect(battle,goalOf(kind),"固定戦場でも全参加者が到達可能");
            }
        }
    }
    // 目的 Survive: 実戦編成の初期配置を使い、生成と検証が全役割・全難易度のシードで成立する。
    for(int difficulty=0;difficulty<3;++difficulty)for(int role=0;role<4;++role)for(int seed=0;seed<SeedCount;++seed){
        RunSnapshot run;run.Phase=RunPhase::Event;run.Level=static_cast<Difficulty>(difficulty);run.Role=static_cast<HeroRole>(role);run.Recruited=parties[seed%parties.size()];
        run.Active={NodeKind::Battle,1};run.Day=5;
        const auto units=Encounter::Create(run,seed,profile).Snapshot().Units;
        const auto field=Battlefield::Generate(seed,units,BattleGoal::Survive,MaxAttempts,{},true);
        check(Battlefield::Validate(field.Layout,units,BattleGoal::Survive,true)==FieldFailure::None,"Surviveでも生成結果が検証を通る");
        check(field==Battlefield::Generate(seed,units,BattleGoal::Survive,MaxAttempts,{},true),"Surviveの生成が再現");
        const Battle battle(field.Layout.Width,field.Layout.Height,field.Layout.Tiles,units,seed,BattleGoal::Survive);
        inspect(battle,BattleGoal::Survive,"Surviveの全参加者が到達可能");
    }
    // 低HPの味方: 初期即死を避けるため敵を離す／固定戦場でも成立し、例外にならない。
    for(int difficulty=0;difficulty<3;++difficulty)for(int role=0;role<4;++role)for(int seed=0;seed<ForcedSeedCount;++seed)for(int hp:{1,8,25}){
        RunSnapshot run;run.Phase=RunPhase::Event;run.Level=static_cast<Difficulty>(difficulty);run.Role=static_cast<HeroRole>(role);run.Recruited={0,2,4,6};
        for(auto& health:run.Health)health.Hp=hp;
        for(auto kind:{NodeKind::Battle,NodeKind::Elite,NodeKind::Rescue,NodeKind::MidBoss,NodeKind::FinalBoss}){
            run.Active={kind,Candidate(run.Recruited,seed)};run.Day=kind==NodeKind::MidBoss?6:kind==NodeKind::FinalBoss?12:5;
            for(int mode=0;mode<4;++mode)try{
                const bool formal=mode%2==1,forced=mode>=2;
                const auto battle=formal?Encounter::Create(run,seed,profile,nullptr,forced):Encounter::Create(run,seed,nullptr,forced);
                inspect(battle,goalOf(kind),"低HPでも初期配置が安全で到達可能");
            }catch(const std::exception& error){
                check(false,"低HPの味方でも戦場生成が成立");
                std::cerr<<"  再現: difficulty="<<difficulty<<" role="<<role<<" seed="<<seed<<" hp="<<hp<<" kind="<<static_cast<int>(kind)<<" mode="<<mode<<" : "<<error.what()<<'\n';
            }
        }
    }
    // 味方HPの境界走査: 1から上限付近まで連続で、全8仲間を回転した4人編成・全役割・全難易度・全種別・通常/正式・退避の全経路を検査する。
    for(int difficulty=0;difficulty<3;++difficulty)for(int role=0;role<4;++role)for(int seed=0;seed<HpSweepSeedCount;++seed)for(int hp=1;hp<=HpSweepMax;++hp){
        RunSnapshot run;run.Phase=RunPhase::Event;run.Level=static_cast<Difficulty>(difficulty);run.Role=static_cast<HeroRole>(role);for(int offset=0;offset<PartySize;++offset)run.Recruited.push_back((seed+offset)%CharacterCount);
        for(auto& health:run.Health)health.Hp=hp;
        for(auto kind:{NodeKind::Battle,NodeKind::Elite,NodeKind::Rescue,NodeKind::MidBoss,NodeKind::FinalBoss}){
            run.Active={kind,Candidate(run.Recruited,seed)};run.Day=kind==NodeKind::MidBoss?6:kind==NodeKind::FinalBoss?12:5;
            for(int mode=0;mode<4;++mode)try{
                const bool formal=mode%2==1,forced=mode>=2;
                const auto battle=formal?Encounter::Create(run,seed,profile,nullptr,forced):Encounter::Create(run,seed,nullptr,forced);
                inspect(battle,goalOf(kind),"HP境界走査でも初期配置が安全で到達可能");
            }catch(const std::exception& error){
                check(false,"HP境界走査で戦場生成が成立");
                std::cerr<<"  再現: difficulty="<<difficulty<<" role="<<role<<" seed="<<seed<<" hp="<<hp<<" kind="<<static_cast<int>(kind)<<" mode="<<mode<<" : "<<error.what()<<'\n';
            }
        }
    }
    // 入力の拒否: 初期位置の重複・ID重複・敵なしは生成前に明示的に拒否する。
    {
        BattleUnit hero;hero.Id=0;hero.At={1,1};hero.Hp=hero.MaxHp=80;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={4,1};enemy.Attack=10;
        const auto rejects=[&](std::vector<BattleUnit> units,BattleGoal goal,const char* label){
            try{(void)Battlefield::Generate(7,units,goal);check(false,label);}catch(const std::invalid_argument&){check(true,label);}
        };
        auto overlapped=std::vector<BattleUnit>{hero,enemy};overlapped[1].At=hero.At;
        rejects(overlapped,BattleGoal::Eliminate,"初期位置の重複を拒否");
        auto sameId=std::vector<BattleUnit>{hero,enemy};sameId[1].Id=0;
        rejects(sameId,BattleGoal::Eliminate,"ID重複を拒否");
        rejects({hero},BattleGoal::Eliminate,"敵のいない戦場を拒否");
        rejects({hero,enemy},BattleGoal::Rescue,"救出対象のいない救出戦を拒否");
        rejects({hero,enemy},BattleGoal::DefeatLord,"城主のいない城主戦を拒否");
        auto negative=std::vector<BattleUnit>{hero,enemy};negative[1].At={-1,1};
        rejects(negative,BattleGoal::Survive,"負の座標を拒否");
    }
    std::cout<<"GenerationSafety: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
