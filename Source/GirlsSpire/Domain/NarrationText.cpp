#include "NarrationText.h"
namespace GirlsSpire {
namespace {
constexpr int MidBossDay=6;
constexpr int FinalBossDay=12;
constexpr std::string_view SetupText="十二日後、死者の城が目を覚ます。落ちた者たちを呼び戻し、歴史そのものを書き換える城だ。蓮は、わずかな仲間と共に出陣する。";
constexpr std::string_view MidBossText="落城姫の合戦が始まる。この一戦で勝敗が変われば、歴史は書き換えられる。";
constexpr std::string_view FinalBossText="城の主は、歴史を喰らう鬼。斬るたびに、仲間の記憶が一つずつ奪われていく。";
}
std::string_view SetupNarration(){return SetupText;}
std::string_view DayNarration(int day){return day==MidBossDay?MidBossText:day==FinalBossDay?FinalBossText:std::string_view{};}
}
