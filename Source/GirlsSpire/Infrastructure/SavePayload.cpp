#include "SavePayload.h"
#include "SaveEnvelope.h"
#include <set>
#include <algorithm>
#include <type_traits>

namespace GirlsSpire {
namespace {
template<class T> struct VectorType : std::false_type {};
template<class T> struct VectorType<std::vector<T>> : std::true_type {};
template<class T> struct OptionalType : std::false_type {};
template<class T> struct OptionalType<std::optional<T>> : std::true_type {};
template<class T> struct ArrayType : std::false_type {};
template<class T,std::size_t N> struct ArrayType<std::array<T,N>> : std::true_type {};
class Wire;
template<class T> bool Fields(Wire& archive,T& value);

// パディング・ホストのエンディアン・標準ライブラリの内部表現を保存しない。
class Wire {
public:
    explicit Wire(bool reading,std::string_view input={}) : Reading(reading),Input(input) {}
    template<class... T> bool operator()(T&... values) {return (Item(values) && ...);}
    template<class T> bool Item(T& value) {
        if constexpr(std::is_same_v<T,bool>) {
            std::uint8_t raw=value?1:0;
            if(!Item(raw) || raw>1) return false;
            value=raw!=0;return true;
        } else if constexpr(std::is_enum_v<T>) {
            int raw=static_cast<int>(value);
            if(!Item(raw)) return false;
            value=static_cast<T>(raw);return true;
        } else if constexpr(std::is_integral_v<T>) {
            using Unsigned=std::make_unsigned_t<T>;
            auto raw=static_cast<Unsigned>(value);
            if(Reading) {
                if(Input.size()-Offset<sizeof(T)) return false;
                raw=0;
                for(std::size_t i=0;i<sizeof(T);++i)
                    raw|=static_cast<Unsigned>(static_cast<unsigned char>(Input[Offset++]))<<(8*i);
                value=static_cast<T>(raw);
            } else {
                if(Output.size()+sizeof(T)>MaxSavePayload) return false;
                for(std::size_t i=0;i<sizeof(T);++i) Output.push_back(static_cast<char>((raw>>(8*i))&255));
            }
            return true;
        } else if constexpr(VectorType<T>::value) {
            std::uint32_t count=static_cast<std::uint32_t>(value.size());
            if(!Item(count) || count>Budget/sizeof(typename T::value_type)) return false;
            Budget-=count*sizeof(typename T::value_type);
            // 各要素は最低1バイト必要。検査前に過大なメモリを確保しない。
            if(Reading) {if(count>Input.size()-Offset) return false;value.resize(count);}
            for(auto& item:value) if(!Item(item)) return false;
            return true;
        } else if constexpr(OptionalType<T>::value) {
            bool present=value.has_value();
            if(!Item(present)) return false;
            if(Reading) {if(present) value.emplace();else value.reset();}
            return !present || Item(*value);
        } else if constexpr(ArrayType<T>::value) {
            for(auto& item:value) if(!Item(item)) return false;
            return true;
        } else return Fields(*this,value);
    }
    bool Complete() const {return Offset==Input.size();}
    std::string Output;
    std::uint32_t Version=0;
private:
    bool Reading;
    std::string_view Input;
    std::size_t Offset=0;
    std::size_t Budget=MaxSavePayload;
};

template<class T> bool Fields(Wire& a,T& v)
{
    if constexpr(std::is_same_v<T,Position>) return a(v.X,v.Y);
    else if constexpr(std::is_same_v<T,RouteNode>) return a(v.Kind,v.Candidate);
    else if constexpr(std::is_same_v<T,GrowthProgress>) return a(v.Bonuses,v.Learned);
    else if constexpr(std::is_same_v<T,HealthProgress>) return a(v.Hp,v.Wounds);
    else if constexpr(std::is_same_v<T,LevelProgress>) return a(v.Level,v.Experience);
    else if constexpr(std::is_same_v<T,Purse>) return a(v.Coins,v.Medicines);
    else if constexpr(std::is_same_v<T,ShopStock>) return a(v.Power,v.Guard,v.Medicines);
    else if constexpr(std::is_same_v<T,RunSnapshot>) {
        if(!a(v.Day,v.EventIndex,v.EventsToday,v.Level,v.Role,v.Phase,v.Revision,v.Recruited,v.Choices,v.Active,v.Growth,v.Health,v.Levels,v.Wallet,v.Stock,v.Accessories,v.RewardRecipient,v.Rewards))return false;
        return a.Version<3 || a(v.PendingRescue);
    }
    else if constexpr(std::is_same_v<T,RunCheckpoint>) return a(v.State,v.RandomState,v.AfterReward);
    else if constexpr(std::is_same_v<T,Tile>) return a(v.Wall,v.Elevation,v.Cover,v.ObstacleHp,v.Trap);
    else if constexpr(std::is_same_v<T,TimedBoost>) return a(v.Amount,v.SkipCurrentEnd);
    else if constexpr(std::is_same_v<T,BattleUnit>) {
        if(!a(v.Id,v.Side,v.Role,v.At,v.Hp,v.MaxHp,v.Attack,v.Defense,v.Speed,v.Accuracy,v.Evasion,v.Movement,v.Range,v.Direction,v.Skills,v.Cooldowns))return false;
        if(a.Version<5 && v.Role>BattleRole::Gunner)return false;
        if(a.Version<6 && v.Role>BattleRole::Cavalry)return false;
        if(a.Version<7 && v.Role>BattleRole::Oni)return false;
        if(a.Version<8 && v.Role>BattleRole::CurseMonk)return false;
        if(a.Version<9 && v.Role>BattleRole::Fox)return false;
        if(a.Version<10 && v.Role>BattleRole::Spider)return false;
        if(a.Version<11 && v.Role>BattleRole::FallenPrincess)return false;
        if(a.Version>=4 && !a(v.CreatesCover))return false;
        if(a.Version>=7 && !a(v.AttackCursed))return false;
        if(a.Version>=9 && !a(v.MovementWebbed))return false;
        if(a.Version>=11 && !a(v.SummonerId))return false;
        return a.Version<13 || a(v.AttackBoost,v.DefenseBoost,v.AccuracyBoost);
    }
    else if constexpr(std::is_same_v<T,BattleBond>) return a(v.Combo,v.Aid);
    else if constexpr(std::is_same_v<T,ComboAction>) return a(v.Sequence,v.ActorId,v.Attributes,v.EnemyTargets);
    else if constexpr(std::is_same_v<T,ComboSnapshot>) return a(v.Round,v.Gauge,v.LastSequence,v.ResolvedSequence,v.Pending);
    else if constexpr(std::is_same_v<T,ComboResolution>) return a(v.RecipeId,v.Participants,v.GaugeGain);
    else if constexpr(std::is_same_v<T,BattleSnapshot>) {
        if(!a(v.Width,v.Height,v.Tiles,v.Units,v.Order,v.ActiveId,v.Round,v.Moved,v.Acted,v.Outcome,v.Revision,v.UsedSkill,v.Medicines,v.BondGains,v.Combos,v.LastCombo))return false;
        if(a.Version>=5 && !a(v.ChargeReady))return false;
        if(a.Version>=12 && !a(v.AbilityRules,v.StartingBonds))return false;
        if(a.Version>=17 && !a(v.PersonalityAI))return false;
        if(a.Version>=18 && !a(v.EnemyAI))return false;
        if(a.Version<14 && v.AbilityRules==AbilityRuleSet::CharacterSkills)return false;
        if(a.Version<15 && v.AbilityRules==AbilityRuleSet::CharacterAbilities)return false;
        return a.Version>=16 || v.AbilityRules!=AbilityRuleSet::CompleteCharacters;
    }
    else if constexpr(std::is_same_v<T,AttackHit>) return a(v.TargetId,v.Hit,v.Damage);
    else if constexpr(std::is_same_v<T,BattleCheckpoint>) {
        if(!a(v.State,v.RandomState,v.Goal,v.SurvivalRounds,v.TurnIndex,v.LastHits))return false;
        if(a.Version<11 && v.Goal>BattleGoal::Rescue)return false;
        return a.Version<11 || a(v.EnemyHpPercent);
    }
    else if constexpr(std::is_same_v<T,IntentOrder>) return a(v.Kind,v.TargetId,v.Point);
    else if constexpr(std::is_same_v<T,CombatControlCheckpoint>) return a(v.Orders,v.Trust,v.ChoiceRandomState,v.ProfileDriven);
    else if constexpr(std::is_same_v<T,SavedCombat>) return a(v.Battle,v.Control);
    else if constexpr(std::is_same_v<T,RunSaveState>) return a(v.Run,v.Combat,v.ResumeAvailable);
    else if constexpr(std::is_same_v<T,Profile>) {
        if(!a(v.Bonds,v.SpecialConversations,v.Epilogues))return false;
        return a.Version<4 || a(v.DiscoveredCombos);
    }
}

bool ValidState(const RunSaveState& saved)
{
    if(!Run::Restore(saved.Run,{})) return false;
    if(!saved.Combat) return true;
    const auto& run=saved.Run.State;
    const auto kind=run.Active.Kind;
    if(run.Phase!=RunPhase::Event || (kind!=NodeKind::Battle && kind!=NodeKind::Elite &&
        kind!=NodeKind::Rescue && kind!=NodeKind::MidBoss && kind!=NodeKind::FinalBoss)) return false;
    const auto& checkpoint=saved.Combat->Battle;
    const auto battle=Battle::Restore(checkpoint);
    if(!battle || !CombatControl::Restore(saved.Combat->Control,*battle)) return false;
    if(checkpoint.State.EnemyAI!=EnemyThinking::Legacy && static_cast<int>(checkpoint.State.EnemyAI)!=1+static_cast<int>(run.Level))return false;
    const auto expectedGoal=kind==NodeKind::Rescue?BattleGoal::Rescue:
        kind==NodeKind::FinalBoss && std::any_of(checkpoint.State.Units.begin(),checkpoint.State.Units.end(),[](const BattleUnit& unit){return unit.Role==BattleRole::CastleLord;})?BattleGoal::DefeatLord:BattleGoal::Eliminate;
    if(checkpoint.Goal!=expectedGoal ||
        checkpoint.State.Medicines>run.Wallet.Medicines) return false;
    std::set<int> expected{0},actual;
    for(int id:run.Recruited) expected.insert(id+1);
    for(const auto& unit:checkpoint.State.Units) if(unit.Side==Faction::Player) actual.insert(unit.Id);
    return expected==actual;
}
template<class T> std::optional<std::string> Encode(T value,std::uint32_t kind)
{
    Wire archive(false);std::uint32_t version=kind==1?RunPayloadSchema:ProfilePayloadSchema;
    archive.Version=version;
    if(!archive(kind,version,value)) return std::nullopt;
    return archive.Output;
}
template<class T> std::optional<T> Decode(std::string_view input,std::uint32_t expected)
{
    if(input.size()>MaxSavePayload) return std::nullopt;
    Wire archive(true,input);std::uint32_t kind=0,version=0;T value{};
    if(!archive(kind,version) || kind!=expected) return std::nullopt;
    archive.Version=version;
    if constexpr(std::is_same_v<T,Profile>) {
        // v1には絆だけがある。未実装だった完了記録を初期値で補い、絆を維持する。
        if(version==1) {
            if(!archive(value.Bonds) || !archive.Complete())return std::nullopt;
            return value;
        }
        if(version==2) {
            if(!archive(value.Bonds,value.SpecialConversations) || !archive.Complete())return std::nullopt;
            return value;
        }
        if(version==3) {
            if(!archive(value.Bonds,value.SpecialConversations,value.Epilogues) || !archive.Complete())return std::nullopt;
            return value;
        }
    }
    const bool supported=version==(expected==1?RunPayloadSchema:ProfilePayloadSchema) || (expected==1 && (version==2 || version==3 || version==4 || version==5 || version==6 || version==7 || version==8 || version==9 || version==10 || version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17));
    if(!supported || !archive(value) || !archive.Complete()) return std::nullopt;
    return value;
}
}
SaveError InspectPayload(std::string_view bytes,SaveKind expected)
{
    if(bytes.size()<8) return SaveError::Corrupt;
    std::uint32_t kind=0,version=0;
    for(int i=0;i<4;++i) {
        kind|=static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i]))<<(8*i);
        version|=static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i+4]))<<(8*i);
    }
    if(kind!=static_cast<std::uint32_t>(expected)) return SaveError::WrongKind;
    if(version!=(expected==SaveKind::Run?RunPayloadSchema:ProfilePayloadSchema) &&
        !(expected==SaveKind::Profile && (version==1 || version==2 || version==3)) &&
        !(expected==SaveKind::Run && (version==2 || version==3 || version==4 || version==5 || version==6 || version==7 || version==8 || version==9 || version==10 || version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17))) return SaveError::UnsupportedVersion;
    return SaveError::None;
}
std::optional<std::string> EncodeRunPayload(const RunSaveState& state)
{
    if(!ValidState(state)) return std::nullopt;
    return Encode(state,1);
}
std::optional<RunSaveState> DecodeRunPayload(std::string_view bytes)
{
    const auto result=Decode<RunSaveState>(bytes,1);
    if(!result || !ValidState(*result)) return std::nullopt;
    return result;
}
std::optional<std::string> EncodeProfilePayload(const Profile& profile)
{
    if(!ValidProfile(profile)) return std::nullopt;
    return Encode(profile,2);
}
std::optional<Profile> DecodeProfilePayload(std::string_view bytes)
{
    const auto result=Decode<Profile>(bytes,2);
    if(!result || !ValidProfile(*result)) return std::nullopt;
    return result;
}
std::optional<std::string> EncodePreferencesPayload(const Preferences& preferences)
{
    if(!ValidPreferences(preferences))return std::nullopt;
    // 周回/恒久と署名・版を分け、既存形式の更新や乱数操作を伴わない。
    Wire archive(false);std::uint32_t signature=0x46505347,version=1;
    auto value=preferences;
    archive(signature,version,value.Text,value.BgmVolume,value.SfxVolume,
        value.Subtitles,value.ReducedEffects,value.SuppressIntimacy);
    return archive.Output;
}
std::optional<Preferences> DecodePreferencesPayload(std::string_view bytes)
{
    // 固定長の基本型だけで構成し、余剰・切断・過大入力を読込前に拒否する。
    if(bytes.size()!=23)return std::nullopt;
    Wire archive(true,bytes);std::uint32_t signature=0,version=0;Preferences value;
    if(!archive(signature,version,value.Text,value.BgmVolume,value.SfxVolume,
        value.Subtitles,value.ReducedEffects,value.SuppressIntimacy) ||
        signature!=0x46505347 || version!=1 || !ValidPreferences(value))return std::nullopt;
    return value;
}
}
