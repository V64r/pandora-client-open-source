#include "InputSupport.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace features::misc::bridge_assist {
namespace {
jobject owned_key = nullptr;
jfieldID pressed_field = nullptr, owned_pressed_field = nullptr;
int owned_code = 0, last_slot = -1, last_count = 0;
bool last_blocks = false, blocked = false;
ULONGLONG release_at = 0;
float offset_variation = 0.f, delay_variation = 1.f;
std::mt19937 random_engine{std::random_device{}()};

void release() {
    if (owned_key) {
        sdk::jni->SetBooleanField(owned_key, owned_pressed_field, input_support::physical_key(owned_code));
        sdk::jni->DeleteGlobalRef(owned_key);
        owned_key = nullptr;
    }
    release_at = 0;
}
void vary() {
    offset_variation = randomize ? std::uniform_real_distribution<float>(-0.015f, 0.015f)(random_engine) : 0.f;
    delay_variation = randomize ? std::uniform_real_distribution<float>(0.85f, 1.15f)(random_engine) : 1.f;
}
// Probe collision geometry, not block IDs, so slabs and non-full blocks work.
// A radius of zero stops at the centre crossing the edge; increasing it
// allows that much overhang before the footprint loses support.
bool supported(mapper::__world& world, mapper::__player& player,
    const mapper::__vec3& position, double radius, bool& valid) {
    valid = false;
    const auto& box_type = mapper::classes["AxisAlignedBB"];
    const auto construct = input_support::method("AxisAlignedBB", "(DDDDDD)V", {"<init>"});
    const auto collisions = input_support::method("WorldClient",
        "(" + mapper::classes["Entity"].signature + box_type.signature + ")Ljava/util/List;",
        {"getCollidingBoundingBoxes", "func_72945_a", "a"});
    const auto size = input_support::method("List", "()I", {"size"});
    if (!box_type.klass || !construct || !collisions || !size) return false;
    JNIEnv* env = sdk::jni;
    jobject box = env->NewObject(box_type.klass, construct, position.x - radius, position.y - 0.08,
        position.z - radius, position.x + radius, position.y, position.z + radius);
    if (!box || env->ExceptionCheck()) {
        if (box) env->DeleteLocalRef(box);
        return false;
    }
    jobject list = env->CallObjectMethod(world.object, collisions, player.object, box);
    env->DeleteLocalRef(box);
    if (!list || env->ExceptionCheck()) {
        if (list) env->DeleteLocalRef(list);
        return false;
    }
    const int count = env->CallIntMethod(list, size);
    env->DeleteLocalRef(list);
    valid = !env->ExceptionCheck();
    return count > 0;
}
}
bool placement_blocked() { return blocked; }
void cancel() {
    release();
    blocked = false;
    last_slot = -1;
    last_count = 0;
    last_blocks = false;
    offset_variation = 0.f;
    delay_variation = 1.f;
}
void run(mapper::__minecraft& minecraft) {
    blocked = false;
    if (!enabled) { cancel(); return; }
    JNIEnv* env = sdk::jni;
    auto player = minecraft.get_local_player();
    auto world = minecraft.get_world();
    auto settings = minecraft.get_settings();
    if (!player.object || !world.object || !settings.object || env->ExceptionCheck()) return;
    const auto key_field = input_support::field("GameSettings", mapper::classes["KeyBinding"].signature,
        {"keyBindSneak", "field_74311_E", "ab"});
    pressed_field = input_support::field("KeyBinding", "Z", {"pressed", "field_74513_e", "h"});
    const auto code_field = input_support::field("KeyBinding", "I", {"keyCode", "field_74512_d", "i"});
    if (!key_field || !pressed_field || !code_field) { cancel(); return; }
    jobject key = env->GetObjectField(settings.object, key_field);
    if (!key || env->ExceptionCheck()) { if (key) env->DeleteLocalRef(key); return; }
    const int code = env->GetIntField(key, code_field);
    env->DeleteLocalRef(key);
    const auto angles = player.get_view_angles();
    if (env->ExceptionCheck()) return;
    if ((require_sneak && !input_support::physical_key(code)) ||
        (looking_down && angles.y < 45.f) || (not_forward && player.get_move_foreward() > 0.f)) {
        cancel(); return;
    }
    const int slot = player.get_current_slot();
    auto stack = player.get_held_item_stack();
    bool has_blocks = input_support::block_stack(stack);
    const int count = input_support::stack_count(stack);
    if (env->ExceptionCheck()) return;
    const bool depleted = last_blocks && last_count > 0 && slot == last_slot && count == 0;
    if (!has_blocks && (select_blocks == 2 || (select_blocks == 1 && depleted))) {
        for (int i = 0; i < 9; ++i) {
            auto candidate = player.get_inventory_slot(i);
            if (env->ExceptionCheck()) return;
            if (input_support::block_stack(candidate)) {
                player.set_current_slot(i);
                has_blocks = true;
                break;
            }
            if (env->ExceptionCheck()) return;
        }
    }
    last_slot = player.get_current_slot();
    auto selected = player.get_held_item_stack();
    last_count = input_support::stack_count(selected);
    last_blocks = has_blocks;
    if (env->ExceptionCheck()) return;
    if (holding_blocks && !has_blocks) { release(); return; }
    const bool grounded = player.get_on_ground();
    const auto motion = player.get_motion();
    if (env->ExceptionCheck()) return;
    if (!grounded && !(sneak_on_jump && motion.y > 0.0)) { release(); return; }
    auto position = player.get_position();
    if (env->ExceptionCheck() || !std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z)) return;
    // Small lookahead compensates for the following movement tick.
    const double dx = std::clamp(motion.x, -0.08, 0.08);
    const double dz = std::clamp(motion.z, -0.08, 0.08);
    position.x += dx;
    position.z += dz;
    const float configured = std::isfinite(edge_offset) ? edge_offset : 0.15f;
    const double radius = std::clamp(configured + (randomize ? offset_variation : 0.f), 0.001f, 0.29f);
    bool valid = false;
    const bool safe = supported(world, player, position, radius, valid);
    if (!valid) { if (!env->ExceptionCheck()) cancel(); return; }
    if (avoid_double_sneaking && grounded && std::abs(dx) > 0.002 && std::abs(dz) > 0.002) {
        auto x_probe = position, z_probe = position;
        x_probe.z -= dz; z_probe.x -= dx;
        const bool x_safe = supported(world, player, x_probe, radius, valid);
        if (!valid) return;
        const bool z_safe = supported(world, player, z_probe, radius, valid);
        if (!valid) return;
        blocked = x_safe != z_safe;
        if (blocked) minecraft.set_right_click_delay_timer(2);
    }
    // In require-sneak mode the physical key is an activation condition, not
    // the desired in-game pressed state. Own the binding on safe ground too.
    if (require_sneak && !owned_key) {
        key = env->GetObjectField(settings.object, key_field);
        if (!key || env->ExceptionCheck()) { if (key) env->DeleteLocalRef(key); return; }
        owned_key = env->NewGlobalRef(key);
        env->DeleteLocalRef(key);
        if (!owned_key || env->ExceptionCheck()) return;
        owned_code = code;
        owned_pressed_field = pressed_field;
    }
    const bool should_sneak = !safe || (!grounded && sneak_on_jump && motion.y > 0.0);
    const auto now = GetTickCount64();
    if (should_sneak) {
        release_at = 0;
        if (!owned_key) {
            key = env->GetObjectField(settings.object, key_field);
            if (!key || env->ExceptionCheck()) { if (key) env->DeleteLocalRef(key); return; }
            owned_key = env->NewGlobalRef(key);
            env->DeleteLocalRef(key);
            if (!owned_key || env->ExceptionCheck()) return;
            owned_code = code;
            owned_pressed_field = pressed_field;
            vary();
        }
        env->SetBooleanField(owned_key, owned_pressed_field, JNI_TRUE);
    } else if (owned_key) {
        if (!release_at) {
            const float delay = std::isfinite(unsneak_delay) ? unsneak_delay : 60.f;
            release_at = now + static_cast<ULONGLONG>(std::clamp(delay * delay_variation, 0.f, 1000.f));
        }
        if (now >= release_at) {
            if (require_sneak) env->SetBooleanField(owned_key, owned_pressed_field, JNI_FALSE);
            else release();
        }
        else env->SetBooleanField(owned_key, owned_pressed_field, JNI_TRUE);
    }
}
}
