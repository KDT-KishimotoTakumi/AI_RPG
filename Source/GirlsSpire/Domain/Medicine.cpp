#include "Battle.h"
#include <algorithm>
#include <cstdlib>
namespace GirlsSpire {
SkillPreview Battle::PreviewMedicine(int actorId,Position target) const
{
    if(!Active(actorId) || state_.Acted || state_.Medicines<=0 || !Inside(target)) return {};
    const auto actor=Find(actorId);
    const auto patient=Find(Occupant(target));
    if(actor->Side!=Faction::Player || !patient || patient->Side!=Faction::Player ||
       patient->Hp<=0 || patient->Hp>=patient->MaxHp ||
       std::abs(actor->At.X-target.X)+std::abs(actor->At.Y-target.Y)>1) return {};
    SkillPreview preview;preview.Legal=true;preview.TargetId=patient->Id;
    preview.Healing=std::min(30,patient->MaxHp-patient->Hp);return preview;
}
bool Battle::UseMedicine(std::uint64_t revision,int actorId,Position target)
{
    if(revision!=state_.Revision) return false;
    const auto preview=PreviewMedicine(actorId,target);
    if(!preview.Legal) return false;
    Find(preview.TargetId)->Hp+=preview.Healing;
    AddAidBond(state_.BondGains,actorId,preview.TargetId,preview.Healing);
    --state_.Medicines;state_.Acted=true;++state_.Revision;
    lastHits_.clear();state_.LastCombo.reset();
    return true;
}
}
