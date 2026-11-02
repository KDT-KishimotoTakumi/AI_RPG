#pragma once
#include <array>
#include <string_view>
namespace GirlsSpire {
struct RescueDialogue {
    std::string_view Opening;
    std::array<std::string_view,3> Replies;
};
const RescueDialogue* FindRescueDialogue(int actorId);
}
