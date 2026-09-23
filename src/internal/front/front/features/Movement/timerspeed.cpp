#include "../misc/InputSupport.hpp"
#include <algorithm>
#include <cmath>

namespace features::movement::timer_speed {
namespace {
jobject owned_timer = nullptr;
jfieldID speed_field = nullptr;
float original_speed = 1.f;
}
void cancel() {
    if (!owned_timer) return;
    sdk::jni->SetFloatField(owned_timer, speed_field, original_speed);
    sdk::jni->DeleteGlobalRef(owned_timer);
    owned_timer = nullptr;
}
void run(mapper::__minecraft& minecraft) {
    JNIEnv* env = sdk::jni;
    if (!enabled) { cancel(); return; }
    auto player = minecraft.get_local_player();
    auto timer = minecraft.get_timer();
    if (!player.object || !timer.object || env->ExceptionCheck()) return;
    bool allowed = true;
    if (require_damage) {
        const auto field = misc::input_support::field("EntityPlayerXP", "I",
            {"hurtResistantTime", "field_70172_ad"});
        allowed = field ? env->GetIntField(player.object, field) > 0 : player.get_hurt_time() > 0;
    }
    if (only_weapon && allowed) {
        auto held = player.get_held_item_stack();
        allowed = held.object != nullptr;
        if (allowed && !env->ExceptionCheck()) {
            auto item = held.get_item();
            allowed = item.object && !env->ExceptionCheck() && (item.is_sword() || item.is_axe());
        }
    }
    if (moving && allowed && !env->ExceptionCheck()) {
        const auto now = player.get_position(), old = player.get_old_position();
        allowed = std::abs(now.x - old.x) + std::abs(now.y - old.y) + std::abs(now.z - old.z) > 0.000001;
    }
    if (env->ExceptionCheck()) return;
    if (!allowed) { cancel(); return; }
    if (owned_timer && !env->IsSameObject(owned_timer, timer.object)) cancel();
    if (!owned_timer) {
        speed_field = misc::input_support::field("Timer", "F", {"timerSpeed", "field_74278_d", "d"});
        if (!speed_field) return;
        original_speed = env->GetFloatField(timer.object, speed_field);
        if (env->ExceptionCheck()) return;
        if (!std::isfinite(original_speed)) original_speed = 1.f;
        owned_timer = env->NewGlobalRef(timer.object);
        if (!owned_timer || env->ExceptionCheck()) return;
    }
    const float value = std::clamp(std::isfinite(speed) ? speed : 1.f, 0.1f, 3.f);
    env->SetFloatField(owned_timer, speed_field, value);
}
}
