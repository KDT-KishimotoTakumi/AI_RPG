#include "Encounter.h"
#include <stdexcept>
namespace GirlsSpire {
Battle Encounter::Create(const RunSnapshot& run,std::uint64_t seed,const Profile& profile,GenerationResult* details,bool forceGenerationFailure)
{
    if(!ValidProfile(profile))throw std::invalid_argument("恒久データが無効です");
    // 生成手順と乱数位置を共有し、新しく作った戦闘だけに正式解釈を付ける。
    auto checkpoint=Create(run,seed,details,forceGenerationFailure).Checkpoint();
    checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
    checkpoint.State.StartingBonds=profile.Bonds;
    checkpoint.State.PersonalityAI=true;
    checkpoint.State.EnemyAI=static_cast<EnemyThinking>(1+static_cast<int>(run.Level));
    auto battle=Battle::Restore(checkpoint);
    if(!battle)throw std::logic_error("生成した正式戦闘が無効です");
    return std::move(*battle);
}
}
