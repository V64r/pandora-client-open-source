#pragma once
#include <array>

namespace features::combat::auto_click::clicksounds {
inline constexpr std::array<const char*, 9> names{
    "Click A", "Click B", "Click C", "Click D", "Click E", "Click F", "Click G",
    "Click H", "Click I"
};
// Called exclusively by the click worker; shutdown before joining that worker.
void play(int selection, float gain);
void shutdown();
}
