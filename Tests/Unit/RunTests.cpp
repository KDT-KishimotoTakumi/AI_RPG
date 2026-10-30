#include "Application/Run.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

int TestRun()
{
    using namespace GirlsSpire;
    int failed = 0, checks = 0;
    const auto check = [&](bool condition, const char* label) {
        ++checks;
        if (!condition) { ++failed; if (failed <= 12) std::cerr << "FAIL Run: " << label << '\n'; }
    };
    std::set<NodeKind> seen;
    std::set<std::vector<int>> parties;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        Run run(seed, Difficulty::Beginner, HeroRole::Sword);
        Run repeat(seed, Difficulty::Beginner, HeroRole::Sword);
        int events = 0, midBosses = 0, finalBosses = 0;
        while (run.Snapshot().Phase == RunPhase::Route && events < 25) {
            const auto before = run.Snapshot();
            const auto rng = run.RandomState();
            check(before == repeat.Snapshot() && rng == repeat.RandomState(), "同一シードの経路一致");
            check(before.EventsToday >= 1 && before.EventsToday <= 2, "一日1～2件");
            check(!run.CompleteEvent(before.Revision), "未選択の完了拒否");
            check(!run.ChooseNode(before.Revision, -1), "負の選択拒否");
            check(!run.ChooseNode(before.Revision, static_cast<int>(before.Choices.size())), "範囲外拒否");
            check(!run.ChooseNode(before.Revision + 1, 0), "古い画面からの入力拒否");
            check(run.Snapshot() == before && run.RandomState() == rng, "無効操作で状態・乱数不変");
            if (before.Choices.empty()) { check(false, "経路の生成"); break; }
            for (const auto& node : before.Choices) seen.insert(node.Kind);
            if (before.Day <= 4 && before.EventIndex == 0) {
                std::set<int> candidates;
                for (const auto& node : before.Choices) {
                    check(node.Kind == NodeKind::Rescue && node.Candidate >= 0 && node.Candidate < 8, "初期4日は救出必須");
                    check(std::find(before.Recruited.begin(), before.Recruited.end(), node.Candidate) == before.Recruited.end(), "加入者再救出なし");
                    candidates.insert(node.Candidate);
                }
                check(candidates.size() >= 2 && candidates.size() == before.Choices.size(), "救出候補の異なる分岐");
            }
            const int selected = static_cast<int>(seed % before.Choices.size());
            const auto node = before.Choices[static_cast<std::size_t>(selected)];
            check(run.ChooseNode(before.Revision, selected), "ノード選択");
            repeat.ChooseNode(before.Revision, selected);
            check(run.Snapshot().Phase == RunPhase::Event, "イベント中状態");
            const auto active = run.Snapshot();
            check(!run.ChooseNode(active.Revision, 0), "イベント中の経路変更禁止");
            check(!run.CompleteEvent(before.Revision), "完了の二重入力防止");
            check(run.Snapshot() == active, "無効完了で不変");
            if (node.Kind == NodeKind::MidBoss) { ++midBosses; check(before.Day == 6, "中ボス日"); }
            if (node.Kind == NodeKind::FinalBoss) { ++finalBosses; check(before.Day == 12, "最終ボス日"); }
            check(run.CompleteEvent(active.Revision), "イベント完了・救出対象が倒れても勝利なら加入");
            repeat.CompleteEvent(active.Revision);
            check(run.Snapshot().Recruited.size() <= 4, "最大4人");
            if (run.Snapshot().Day >= 5) check(run.Snapshot().Recruited.size() == 4, "5日目以降5人編成");
            ++events;
        }
        check(run.Snapshot().Phase == RunPhase::Won && run.Snapshot().Day == 12, "12日最終戦後のみ勝利");
        check(events >= 12 && events <= 24 && midBosses == 1 && finalBosses == 1, "イベント数と必須ボス");
        const auto ended = run.Snapshot();
        check(!run.ChooseNode(ended.Revision, 0) && !run.CompleteEvent(ended.Revision), "終了後操作拒否");
        const auto remaining = run.Unrecruited();
        std::set<int> all(remaining.begin(), remaining.end());
        all.insert(ended.Recruited.begin(), ended.Recruited.end());
        check(remaining.size() == 4 && all.size() == 8, "未加入4人を最終戦に供給");
        parties.insert(ended.Recruited);
    }
    check(seen.size() == 9, "全ノード種を生成");
    check(parties.size() > 1, "シード・経路による加入順変化");
    for (int level = 0; level < 3; ++level) for (int role = 0; role < 4; ++role) {
        Run lost(42, static_cast<Difficulty>(level), static_cast<HeroRole>(role));
        check(lost.Snapshot().Level == static_cast<Difficulty>(level) && lost.Snapshot().Role == static_cast<HeroRole>(role), "難易度・役割保持");
        lost.ChooseNode(lost.Snapshot().Revision, 0);
        check(lost.CompleteEvent(lost.Snapshot().Revision, true) && lost.Snapshot().Phase == RunPhase::Lost, "全員戦闘不能で敗北");
        check(lost.Snapshot().Recruited.empty(), "敗北時に救出を適用しない");
        check(!lost.CompleteEvent(lost.Snapshot().Revision) && !lost.ChooseNode(lost.Snapshot().Revision, 0), "敗北後再操作拒否");
    }
    for (int value : {-1, 4}) {
        try { Run invalid(0, Difficulty::Beginner, static_cast<HeroRole>(value)); check(false, "不正役割の拒否"); }
        catch (const std::invalid_argument&) { check(true, "不正役割の拒否"); }
    }
    for (int value : {-1, 3}) {
        try { Run invalid(0, static_cast<Difficulty>(value), HeroRole::Sword); check(false, "不正難易度の拒否"); }
        catch (const std::invalid_argument&) { check(true, "不正難易度の拒否"); }
    }
    std::cout << "Run: " << checks << " checks, " << failed << " failed\n";
    return failed;
}
