#include "../features.hpp"
#include "../sdk.hpp"

extern bool gui_fastplace_enabled;
extern int  gui_fastplace_held_item;

namespace features::misc::fastplace
{
    void run(mapper::__minecraft& minecraft)
    {
        if (!sdk::jni) return;
        if (!gui_fastplace_enabled) return;

        auto current_screen = minecraft.get_current_screen();
        if (current_screen.object != nullptr) return;

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        auto hs = local_player.get_held_item_stack();
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }

        bool should_fastplace = false;

        if (gui_fastplace_held_item == 0) {
            // All items
            should_fastplace = true;
        } else if (hs.object) {
            auto item = hs.get_item();
            if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }
            if (item.object) {
                if (gui_fastplace_held_item == 1 && item.is_block()) {
                    should_fastplace = true;
                } else if (gui_fastplace_held_item == 2 && !item.is_block()) {
                    should_fastplace = true;
                }
            }
        }

        if (should_fastplace) {
            int delay = minecraft.get_right_click_delay_timer();
            if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }
            
            // FastPlace a 0 delay
            if (delay > 0) {
                minecraft.set_right_click_delay_timer(0);
                if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }
            }
        }
    }
}
