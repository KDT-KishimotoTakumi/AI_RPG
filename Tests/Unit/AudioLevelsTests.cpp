#include "Application/AudioLevels.h"
#include <cmath>
#include <iostream>
int TestAudioLevels()
{
    using namespace GirlsSpire;int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL AudioLevels: "<<why<<'\n';}};
    for(int bgm=0;bgm<=100;++bgm)for(int sfx=0;sfx<=100;++sfx){
        Preferences value;value.BgmVolume=bgm;value.SfxVolume=sfx;const auto before=value;const auto levels=ResolveAudioLevels(value);
        check(levels && levels->Bgm>=0 && levels->Bgm<=1 && levels->Sfx>=0 && levels->Sfx<=1 && value==before,"全整数音量を独立・無変更で解決");
        check(levels && std::abs(levels->Bgm*100-bgm)<0.0001 && std::abs(levels->Sfx*100-sfx)<0.0001,"百分率の音量と消音を保持");
    }
    Preferences value;value.BgmVolume=37;value.SfxVolume=0;const auto levels=ResolveAudioLevels(value);
    check(levels && std::abs(levels->Bgm-0.37f)<0.0001f && levels->Sfx==0,"片方だけ消音");
    for(int size=0;size<3;++size)for(int flags=0;flags<8;++flags){value.Text=static_cast<TextSize>(size);value.Subtitles=flags&1;value.ReducedEffects=flags&2;value.SuppressIntimacy=flags&4;check(ResolveAudioLevels(value)==levels,"表示設定は音量へ混入しない");}
    for(int bad:{-1,101}){value.BgmVolume=bad;check(!ResolveAudioLevels(value),"不正BGMは補正せず拒否");value.BgmVolume=37;value.SfxVolume=bad;check(!ResolveAudioLevels(value),"不正SFXは補正せず拒否");value.SfxVolume=0;}
    value.Text=static_cast<TextSize>(3);check(!ResolveAudioLevels(value),"不正設定を拒否");
    std::cout<<"AudioLevels: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
