#pragma once
#include <string_view>
namespace GirlsSpire {
// 物語の中核設定を伝える地の文。状態を持たず、表示側が読むだけの承認済み本文。
std::string_view SetupNarration();
// 6日目（落城姫）と12日目（城主）だけ本文を返す。それ以外の日は空。
std::string_view DayNarration(int day);
}
