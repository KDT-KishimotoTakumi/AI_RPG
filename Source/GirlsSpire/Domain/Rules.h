#pragma once

namespace GirlsSpire {
struct Prediction {
    int CandidateLimit;
    bool ShowProbability;
    bool Certain;
    bool SpecialUnlocked;
};
int ClampHitChance(int percent);
Prediction GetPrediction(int bond);
int AddComboGauge(int current, int participants);
bool SpendUltimate(int& gauge, bool wholeParty);
}
