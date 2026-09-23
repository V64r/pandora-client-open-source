#include "features.hpp"
#include <windows.h>
extern std::atomic<bool> g_MenuVisible;
extern bool g_ConfigInputActive;
extern bool g_NumericEditActive;

namespace features
{
    void run_on_run_tick(mapper::__minecraft& minecraft)
    {
        if (!minecraft.is_valid() || (g_MenuVisible && (g_ConfigInputActive || g_NumericEditActive))) return;

        //combat
        combat::auto_click::run(minecraft);
        combat::aim_assist::run(minecraft);
        combat::reach::run(minecraft);
        combat::velocity::run(minecraft);
        combat::block_hit::run(minecraft);
        combat::no_hit_delay::run(minecraft);
        combat::macros::run(minecraft);

        //movement
        movement::sprint::run(minecraft);
        movement::no_slow::run(minecraft);
        movement::no_item_release::run(minecraft);
        movement::no_jump_delay::run(minecraft);
        movement::snap_tap::run(minecraft);
        movement::instant_stop::run(minecraft);


        // ============================================================
        // MISC
        // ============================================================
        misc::fastplace::run(minecraft);
        misc::auto_armor::run(minecraft);
        latency::blink::run(minecraft);
        friends::run(minecraft);
    }

}
