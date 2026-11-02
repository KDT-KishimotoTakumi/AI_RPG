#pragma once
#include <array>
#include <string_view>
namespace GirlsSpire {
struct SocialDialogue {
    std::string_view Opening;
    std::array<std::string_view,3> Replies;
    std::string_view Special;
};
struct EndingText {std::string_view Title,Page;};
const SocialDialogue* FindSocialDialogue(int actorId);
const EndingText* FindEndingText(int actorId);
}
