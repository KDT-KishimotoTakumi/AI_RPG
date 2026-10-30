#include "Domain/Random.h"
#include <iostream>
#include <stdexcept>

int TestRandom()
{
    int failed = 0;
    GirlsSpire::Random a(42), b(42), other(7);
    bool differs = false;
    for (int i = 0; i < 1000; ++i) {
        const auto value = a.Next();
        if (value != b.Next()) ++failed;
        differs |= value != other.Next();
    }
    if (!differs) ++failed;
    GirlsSpire::Random restored(a.State());
    for (int i = 0; i < 1000; ++i) if (a.Next() != restored.Next()) ++failed;
    GirlsSpire::Random zero(0);
    if (zero.Next() == 0) ++failed;
    for (int i = 0; i < 10000; ++i) if (a.Uniform(100) >= 100) ++failed;
    if (a.Uniform(1) != 0) ++failed;
    const auto before = a.State();
    try { (void)a.Uniform(0); ++failed; } catch (const std::invalid_argument&) {}
    if (a.State() != before) ++failed;
    std::cout << "Random: " << failed << " failed\n";
    return failed;
}
