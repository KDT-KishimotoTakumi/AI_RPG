#include "Domain/Appearance.h"
#include "Infrastructure/SavePayload.h"
#include <algorithm>
#include <iostream>
int TestAppearance()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool pass,const char* message){++checks;if(!pass){++failed;std::cerr<<"FAIL Appearance: "<<message<<'\n';}};
    Profile fresh;
    check(RunPayloadSchema==19 && ProfilePayloadSchema==7,"周回v19を維持し外見を恒久v7へ保存");
    for(int id=0;id<=8;++id){
        check(CanChooseAppearance(fresh,id,AppearanceVariant::Standard),"標準は全員選択可能");
        auto copy=fresh;
        check(!ChooseAppearance(copy,id,AppearanceVariant::Alternate) && copy==fresh,"未解放差分は状態を変えず拒否");
    }
    for(int id:{-1,9})for(auto variant:{AppearanceVariant::Standard,AppearanceVariant::Alternate}){
        auto copy=fresh;check(!CanChooseAppearance(copy,id,variant) && !ChooseAppearance(copy,id,variant) && copy==fresh,"人物範囲外を拒否");
    }
    for(int raw:{-1,2}){auto copy=fresh;check(!ChooseAppearance(copy,0,static_cast<AppearanceVariant>(raw)) && copy==fresh,"外見値の範囲外を拒否");}
    auto advanced=fresh;advanced.HighestDifficulty=2;
    check(!CanChooseAppearance(advanced,0,AppearanceVariant::Alternate),"旧難易度全解放だけでは勝利を推測しない");
    for(int id=0;id<=8;++id){
        auto unlocked=fresh;if(id==0)unlocked.VictoryRecorded=true;else unlocked.SpecialConversations[id-1]=true;
        const auto before=unlocked;
        check(CanChooseAppearance(unlocked,id,AppearanceVariant::Alternate) && unlocked==before,"解放照会は状態を変更しない");
        check(ChooseAppearance(unlocked,id,AppearanceVariant::Alternate) && unlocked.SelectedAppearances[id]==AppearanceVariant::Alternate,"承認条件を満たした人物だけ差分選択");
        check(ValidProfile(unlocked),"正しい解放済み選択は有効なProfile");
        const auto encoded=EncodeProfilePayload(unlocked);const auto decoded=encoded?DecodeProfilePayload(*encoded):std::nullopt;
        check(decoded && *decoded==unlocked,"勝利根拠と人物別選択を全保存往復");
        for(int other=0;other<=8;++other)if(other!=id)check(!CanChooseAppearance(unlocked,other,AppearanceVariant::Alternate),"他人物へ解放を波及させない");
        check(ChooseAppearance(unlocked,id,AppearanceVariant::Standard) && unlocked==before,"標準へ戻しても解放根拠を保持");
    }
    for(int id=0;id<=8;++id){auto old=fresh;old.Epilogues[id]=true;check(CanChooseAppearance(old,0,AppearanceVariant::Alternate),"既存の全後日談は主人公勝利の確認済み根拠");}
    for(int id=0;id<=8;++id){
        auto invalid=fresh;invalid.SelectedAppearances[id]=AppearanceVariant::Alternate;
        check(!ValidProfile(invalid) && !ValidAppearanceSelection(invalid),"未解放選択の不正Profileを拒否");
        for(int value:{-1,2}){invalid.SelectedAppearances[id]=static_cast<AppearanceVariant>(value);check(!ValidProfile(invalid),"保存内の不正外見値を拒否");}
    }
    auto corrupt=fresh;corrupt.Bonds[0]=101;const auto before=corrupt;
    check(!CanChooseAppearance(corrupt,0,AppearanceVariant::Standard) && !ChooseAppearance(corrupt,0,AppearanceVariant::Standard) && corrupt==before,"他の恒久値が壊れたProfileを変更しない");
    Run winner(42,Difficulty::Beginner,HeroRole::Sword);
    for(int event=0;event<24 && winner.Snapshot().Phase==RunPhase::Route;++event){
        winner.ChooseNode(winner.Snapshot().Revision,0);winner.CompleteEvent(winner.Snapshot().Revision);
    }
    check(winner.Snapshot().Phase==RunPhase::Won && winner.ProfileState().VictoryRecorded && CanChooseAppearance(winner.ProfileState(),0,AppearanceVariant::Alternate),"初勝利を後日談選択前に確定する");
    auto retained=winner.ProfileState();retained.SpecialConversations[0]=true;
    ChooseAppearance(retained,0,AppearanceVariant::Alternate);ChooseAppearance(retained,1,AppearanceVariant::Alternate);
    Run loser(43,Difficulty::Beginner,HeroRole::Sword,retained);loser.ChooseNode(0,0);loser.CompleteEvent(loser.Snapshot().Revision,true);
    check(loser.Snapshot().Phase==RunPhase::Lost && loser.ProfileState()==retained,"敗北で外見解放・選択を失わない");
    Run firstLoss(44,Difficulty::Beginner,HeroRole::Sword);firstLoss.ChooseNode(0,0);firstLoss.CompleteEvent(firstLoss.Snapshot().Revision,true);
    check(!firstLoss.ProfileState().VictoryRecorded && !CanChooseAppearance(firstLoss.ProfileState(),0,AppearanceVariant::Alternate),"初敗北で主人公の衣装を解放しない");
    // 旧v6を独立した固定レイアウトで組み立て、現在のエンコーダに依存しない。
    for(bool won:{false,true}){
        std::string bytes;const auto put=[&](int value){for(int b=0;b<4;++b)bytes.push_back(static_cast<char>((value>>(8*b))&255));};
        put(2);put(6);for(int id=0;id<8;++id)put(0);bytes.append(8,'\0');bytes.append(9,'\0');if(won)bytes[48]=1;put(0);put(2);put(0);
        const auto old=DecodeProfilePayload(bytes);
        check(old && InspectPayload(bytes,SaveKind::Profile)==SaveError::None && old->HighestDifficulty==2 && CanChooseAppearance(*old,0,AppearanceVariant::Alternate)==won && std::all_of(old->SelectedAppearances.begin(),old->SelectedAppearances.end(),[](auto v){return v==AppearanceVariant::Standard;}),"旧v6は原画を維持し確認できる勝利だけ解放");
    }
    auto all=fresh;all.VictoryRecorded=true;all.SpecialConversations.fill(true);all.SelectedAppearances.fill(AppearanceVariant::Alternate);
    if(const auto bytes=EncodeProfilePayload(all)){
        for(std::size_t length=0;length<bytes->size();++length)check(!DecodeProfilePayload(bytes->substr(0,length)),"外見保存の全切断位置を拒否");
        check(!DecodeProfilePayload(*bytes+"x"),"外見保存の余剰を拒否");
        auto malformed=*bytes;malformed[malformed.size()-37]=2;
        check(!DecodeProfilePayload(malformed),"勝利フラグの不正な真偽値を拒否");
        malformed=*bytes;malformed[malformed.size()-4]=2;
        check(!DecodeProfilePayload(malformed),"保存された不正な外見列挙を拒否");
    }else check(false,"全員解放済みの保存を作成");
    std::cout<<"Appearance: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
