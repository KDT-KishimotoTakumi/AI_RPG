#include "Encounter.h"
#include <stdexcept>
namespace GirlsSpire {
Battle Encounter::Create(const RunSnapshot& run,std::uint64_t seed,const Profile& profile,GenerationResult* details,bool forceGenerationFailure)
{
    if(!ValidProfile(profile))throw std::invalid_argument("恒久データが無効です");
    // 新しく生成する戦闘だけ、正式技能と支援を含む安全検査に通す。
    return CreateConfigured(run,seed,&profile,details,forceGenerationFailure);
}
}
