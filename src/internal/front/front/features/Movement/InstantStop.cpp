#include "../features.hpp"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

namespace features::movement::instant_stop
{
    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled || minecraft.get_current_screen().object != nullptr ||
            !features::visual::window ||
            GetForegroundWindow() != static_cast<HWND>(features::visual::window))
            return;

        auto player = minecraft.get_local_player();
        if (!player.object) return;
        if (only_on_ground && !player.get_on_ground()) return;
        if (!stop_on_sneak && player.get_flag(1)) return;

        const bool moving = (GetAsyncKeyState('W') & 0x8000) != 0 ||
            (GetAsyncKeyState('S') & 0x8000) != 0 ||
            (GetAsyncKeyState('A') & 0x8000) != 0 ||
            (GetAsyncKeyState('D') & 0x8000) != 0;
        if (moving) return;

        mapper::__vec3 motion = player.get_motion();
        const float strength = std::clamp(stop_strength, 0.0f, 1.0f);
        const double factor = 1.0 - static_cast<double>(strength);
        motion.x *= factor;
        motion.z *= factor;
        if (std::abs(motion.x) < 0.001) motion.x = 0.0;
        if (std::abs(motion.z) < 0.001) motion.z = 0.0;
        player.set_motion(motion);
    }
}
