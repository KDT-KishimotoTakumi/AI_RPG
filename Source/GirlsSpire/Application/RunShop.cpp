#include "Run.h"
namespace GirlsSpire {
bool Run::Purchase(std::uint64_t revision,ShopOffer offer,int actorId)
{
    if(state_.Phase!=RunPhase::Event || state_.Active.Kind!=NodeKind::Shop ||
       state_.Revision!=revision || !HasMember(actorId)) return false;
    auto next=*this;
    auto& health=next.state_.Health[actorId];
    health.Hp=CurrentHp(actorId);
    if(!Buy(next.state_.Wallet,next.state_.Stock,next.state_.Accessories[actorId],
            health,BaseMaxHp(actorId),offer)) return false;
    ++next.state_.Revision;
    *this=std::move(next);return true;
}
}
