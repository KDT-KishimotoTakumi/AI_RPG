#include "Domain/Preferences.h"
#include "Infrastructure/SavePayload.h"
#include <iostream>
#include <limits>
using namespace GirlsSpire;
int TestPreferences()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL Preferences: "<<why<<'\n';}};
    const Preferences defaults;
    check(ValidPreferences(defaults) && defaults.Text==TextSize::Standard && defaults.BgmVolume==100 && defaults.SfxVolume==100 && defaults.Subtitles && !defaults.ReducedEffects && defaults.SuppressIntimacy,"初期値は有効で親密表現を抑制");
    for(int text=0;text<3;++text)for(int bgm:{0,1,50,100})for(int sfx:{0,1,50,100})for(int flags=0;flags<8;++flags) {
        const Preferences value{static_cast<TextSize>(text),bgm,sfx,(flags&1)!=0,(flags&2)!=0,(flags&4)!=0};
        const auto before=value;
        const auto encoded=EncodePreferencesPayload(value);
        check(ValidPreferences(value) && encoded && encoded->size()==23 && value==before,"設定の全組合せを副作用なく符号化");
        check(encoded && DecodePreferencesPayload(*encoded)==value,"文字サイズ・独立音量・全フラグを正確に復元");
        check(TextScalePercent(value.Text)==100+text*25,"文字サイズの倍率");
    }
    for(int bad:{std::numeric_limits<int>::min(),-1,101,std::numeric_limits<int>::max()})for(int field=0;field<2;++field) {
        auto value=defaults;if(field==0)value.BgmVolume=bad;else value.SfxVolume=bad;
        check(!ValidPreferences(value) && !EncodePreferencesPayload(value),"範囲外音量を拒否");
    }
    for(int bad:{std::numeric_limits<int>::min(),-1,3,std::numeric_limits<int>::max()}) {
        auto value=defaults;value.Text=static_cast<TextSize>(bad);
        check(!ValidPreferences(value) && !EncodePreferencesPayload(value) && TextScalePercent(value.Text)==0,"不正サイズを拒否");
    }
    const auto encoded=EncodePreferencesPayload(defaults);
    if(encoded) {
        for(std::size_t cut=0;cut<encoded->size();++cut)check(!DecodePreferencesPayload(encoded->substr(0,cut)),"全切断位置で拒否");
        check(!DecodePreferencesPayload(*encoded+'\0') && !DecodePreferencesPayload(std::string(MaxSavePayload+1,'x')),"余剰・過大データを拒否");
        for(int offset:{0,1,2,3,4,8,12,16,20,21,22}) {
            auto corrupt=*encoded;corrupt[offset]=static_cast<char>(255);
            check(!DecodePreferencesPayload(corrupt),"署名・版・列挙・音量・真偽値の破損を拒否");
        }
        check(!DecodeRunPayload(*encoded) && !DecodeProfilePayload(*encoded),"設定を周回/恒久へ混入させない");
    } else check(false,"破損試験の符号化を用意できる");
    const auto profile=EncodeProfilePayload({});
    check(profile && !DecodePreferencesPayload(*profile),"恒久記録を設定として読み込まない");
    std::cout<<"Preferences: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
