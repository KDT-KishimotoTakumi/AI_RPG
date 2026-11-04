#pragma once
#include "Run.h"
#include "Domain/Battle.h"
#include "Generation/Battlefield.h"

namespace GirlsSpire {
// 仮編成を検証済みの生成戦場へ配置。正式人物・敵データへの置換はT023。
class Encounter {
public:
    static Battle Create(const RunSnapshot& run, std::uint64_t seed,
        GenerationResult* details = nullptr, bool forceGenerationFailure = false);
    static Battle Create(const RunSnapshot& run, std::uint64_t seed, const Profile& profile,
        GenerationResult* details = nullptr, bool forceGenerationFailure = false);
private:
    static Battle CreateConfigured(const RunSnapshot& run,std::uint64_t seed,const Profile* profile,
        GenerationResult* details,bool forceGenerationFailure);
};
}
