#pragma once
#include "SaveSlot.h"
#include <array>
namespace GirlsSpire {
struct RecoveryPair {
    SaveEnvelope Run,Profile;
    std::uint64_t HighestGeneration=0;
};
struct RecoverySelection {
    SlotIssue Issue=SlotIssue::Corrupt;
    std::optional<RecoveryPair> Pair;
};
// 順番は周回本体/周回バックアップ/恒久本体/恒久バックアップ。
// 選択のみを行い、入力の変更・保全・書込・確認操作は行わない。
RecoverySelection SelectRecoveryPair(const std::array<SlotBytes,4>& files);
}
