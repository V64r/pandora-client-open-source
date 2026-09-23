#include "InputSupport.hpp"
#include <algorithm>
#include <cmath>

namespace features::misc::bard_helper {
namespace {
bool active = false;
int original_slot = -1, next_effect = 0;
ULONGLONG next_action = 0;
constexpr int items[]{265, 377, 370, 353, 288};
}
void cancel() {
    enabled = false;
    active = false;
    original_slot = -1;
    next_effect = 0;
    next_action = 0;
}
void run(mapper::__minecraft& minecraft) {
    if (!enabled || !bind) { cancel(); return; }
    auto player = minecraft.get_local_player();
    if (!player.object || sdk::jni->ExceptionCheck()) return;
    const auto now = GetTickCount64();
    if (!active) {
        original_slot = player.get_current_slot();
        if (sdk::jni->ExceptionCheck()) return;
        next_effect = 0;
        next_action = now;
        active = true;
    }
    if (now < next_action) return;
    const bool effects[]{resistance, strength, regeneration, speed, jump};
    for (; next_effect < 5; ++next_effect) {
        if (!effects[next_effect]) continue;
        for (int slot = 0; slot < 9; ++slot) {
            auto stack = player.get_inventory_slot(slot);
            if (sdk::jni->ExceptionCheck()) return;
            if (!stack.object) continue;
            const int count = input_support::stack_count(stack);
            if (sdk::jni->ExceptionCheck()) return;
            if (count <= 0) continue;
            auto item = stack.get_item();
            if (sdk::jni->ExceptionCheck()) return;
            if (!item.object) continue;
            const int id = item.get_id();
            if (sdk::jni->ExceptionCheck()) return;
            if (id != items[next_effect]) continue;
            player.set_current_slot(slot);
            if (sdk::jni->ExceptionCheck()) return;
            ++next_effect;
            next_action = now + static_cast<ULONGLONG>(std::clamp(
                std::isfinite(switch_delay) ? switch_delay : 100.f, 20.f, 2000.f));
            return;
        }
    }
    if (return_last_slot && original_slot >= 0 && original_slot < 9)
        player.set_current_slot(original_slot);
    cancel();
}
}
