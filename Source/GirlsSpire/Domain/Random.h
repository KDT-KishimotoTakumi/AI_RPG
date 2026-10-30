#pragma once
#include <cstdint>
#include <stdexcept>

namespace GirlsSpire {
class Random {
public:
    explicit Random(std::uint64_t state) : state_(state) {}
    std::uint64_t State() const { return state_; }
    std::uint64_t Next()
    {
        // SplitMix64。標準ライブラリの分布実装差に依存しない固定演算。
        auto value = (state_ += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }
    std::uint32_t Uniform(std::uint32_t exclusiveMax)
    {
        if (exclusiveMax == 0) throw std::invalid_argument("範囲は1以上で指定してください");
        // 剰余による偏りを棄却法で除く。
        const auto bound = static_cast<std::uint64_t>(exclusiveMax);
        const auto threshold = (std::uint64_t{0} - bound) % bound;
        std::uint64_t value;
        do { value = Next(); } while (value < threshold);
        return static_cast<std::uint32_t>(value % bound);
    }
private:
    std::uint64_t state_;
};
}
