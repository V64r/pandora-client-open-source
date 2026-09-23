#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
extern std::atomic<bool> g_MenuVisible;
extern "C" void swiftLog(const char*, ...);

extern bool gui_armorswitcher_enabled;
extern int gui_armorswitcher_kit1;
extern int gui_armorswitcher_kit2;
extern int gui_armorswitcher_bind;
extern float gui_armorswitcher_delay;

namespace features::combat::armor_switcher {
namespace {
enum class state { idle, opening, swapping, closing };
state current_state = state::idle;
int source_slots[4]{ -1, -1, -1, -1 };
int current_piece = 0;
int click_phase = 0;
int active_kit = 0;
int last_kit = -1;
bool opened_by_us = false;
ULONGLONG next_action = 0, started_at = 0;
jweak operation_world = nullptr;

int armor_id(int kit, int piece) {
    static constexpr int bases[] = { 310, 306, 314, 302, 298 };
    return bases[(std::clamp)(kit, 0, 4)] + piece;
}

int item_id(JNIEnv* env, jobject item) {
    if (!env || !item) return -1;
    jclass klass = env->GetObjectClass(item);
    if (!klass) return -1;
    const std::string signature = "(" + mapper::classes["Item"].signature + ")I";
    jmethodID method = env->GetStaticMethodID(klass, "getIdFromItem", signature.c_str());
    if (!method) { env->ExceptionClear(); method = env->GetStaticMethodID(klass, "func_150891_b", signature.c_str()); }
    if (!method) { env->ExceptionClear(); method = env->GetStaticMethodID(klass, "b", signature.c_str()); }
    if (!method) { env->ExceptionClear(); method = env->GetStaticMethodID(klass, "a", signature.c_str()); }
    int result = method ? env->CallStaticIntMethod(klass, method, item) : -1;
    if (env->ExceptionCheck()) { env->ExceptionClear(); result = -1; }
    env->DeleteLocalRef(klass);
    return result;
}

int inventory_index(int container_slot) {
    return container_slot >= 36 ? container_slot - 36 : container_slot;
}

int equipped_count(JNIEnv* env, mapper::__player& player, int kit) {
    int count = 0;
    for (int piece = 0; piece < 4; ++piece) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        auto stack = player.get_inventory_slot(39 - piece);
        if (!stack.object) continue;
        auto item = stack.get_item();
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (item.object && item_id(env, item.object) == armor_id(kit, piece)) ++count;
    }
    return count;
}

int choose_kit(JNIEnv* env, mapper::__player& player) {
    const int first = (std::clamp)(gui_armorswitcher_kit1, 0, 4);
    const int second = (std::clamp)(gui_armorswitcher_kit2, 0, 4);
    if (first == second) return first;
    const int first_count = equipped_count(env, player, first);
    const int second_count = equipped_count(env, player, second);
    if (first_count > second_count) return second;
    if (second_count > first_count) return first;
    return last_kit == first ? second : first;
}

bool find_available_set(JNIEnv* env, mapper::__player& player, int kit) {
    std::fill(std::begin(source_slots), std::end(source_slots), -1);
    for (int slot = 9; slot <= 44; ++slot) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        auto stack = player.get_inventory_slot(inventory_index(slot));
        if (!stack.object) continue;
        auto item = stack.get_item();
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (!item.object) continue;
        const int id = item_id(env, item.object);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        for (int piece = 0; piece < 4; ++piece) {
            if (source_slots[piece] < 0 && id == armor_id(kit, piece)) {
                source_slots[piece] = slot;
                break;
            }
        }
    }
    return std::any_of(std::begin(source_slots), std::end(source_slots), [](int slot) { return slot >= 9; });
}

void tap_inventory_key() {
    const BYTE scan = static_cast<BYTE>(MapVirtualKey('E', MAPVK_VK_TO_VSC));
    keybd_event('E', scan, 0, 0);
    keybd_event('E', scan, KEYEVENTF_KEYUP, 0);
}

void finish(bool success) {
    if (success) last_kit = active_kit;
    if (operation_world) sdk::jni->DeleteWeakGlobalRef(operation_world);
    operation_world = nullptr;
    current_state = state::idle;
    current_piece = click_phase = 0;
    opened_by_us = false;
    gui_armorswitcher_enabled = false;
}
}

void run(mapper::__minecraft& minecraft) {
    JNIEnv* env = sdk::jni;
    if (!env) return;
    if (env->ExceptionCheck()) env->ExceptionClear();

    // The normal menu keybind handler owns activation; do not toggle twice.
    if (!gui_armorswitcher_enabled || g_MenuVisible) {
        if (current_state != state::idle) finish(false);
        return;
    }
    DWORD foreground_pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
    if (foreground_pid != GetCurrentProcessId()) { finish(false); return; }

    auto player = minecraft.get_local_player();
    if (!player.object) { finish(false); return; }
    auto world = minecraft.get_world();
    auto screen = minecraft.get_current_screen();
    const auto& inventory_class = mapper::classes["GuiInventory"];
    const bool screen_open = screen.object && inventory_class.klass &&
        env->IsInstanceOf(screen.object, inventory_class.klass);
    if (!world.object || (screen.object && !screen_open) || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        finish(false); return;
    }
    const ULONGLONG now = GetTickCount64();

    if (current_state != state::idle && (!env->IsSameObject(operation_world, world.object) ||
        now - started_at > 15000)) {
        swiftLog("ArmorSwitcher cancelled: world changed or timeout");
        finish(false); return;
    }
    if (current_state == state::idle) {
        operation_world = env->NewWeakGlobalRef(world.object);
        if (!operation_world) { if (env->ExceptionCheck()) env->ExceptionClear(); finish(false); return; }
        started_at = now;
        swiftLog("ArmorSwitcher started on game thread=%lu", GetCurrentThreadId());
        active_kit = choose_kit(env, player);
        if (!find_available_set(env, player, active_kit)) {
            swiftLog("ArmorSwitcher: no pieces found for kit %d (inventory unchanged)", active_kit);
            finish(false); return;
        }
        swiftLog("ArmorSwitcher kit=%d slots=%d,%d,%d,%d", active_kit, source_slots[0], source_slots[1], source_slots[2], source_slots[3]);
        current_piece = click_phase = 0;
        opened_by_us = !screen_open;
        if (opened_by_us) {
            tap_inventory_key();
            current_state = state::opening;
            next_action = now + 1000;
        } else {
            current_state = state::swapping;
            next_action = now;
        }
        return;
    }

    if (current_state == state::opening) {
        if (screen_open) { current_state = state::swapping; next_action = now; }
        else if (now >= next_action) finish(false);
        return;
    }
    if (!screen_open) { finish(false); return; }
    if (now < next_action) return;

    if (current_state == state::swapping) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        while (current_piece < 4 && source_slots[current_piece] < 0) ++current_piece;
        if (current_piece >= 4) { current_state = state::closing; return; }
        const int source = source_slots[current_piece];
        const int armor_slot = 5 + current_piece;
        const int slot = click_phase == 1 ? armor_slot : source;
        if (!minecraft.window_click(0, slot, 0, 0, player)) { finish(false); return; }
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (++click_phase == 3) {
            click_phase = 0;
            if (++current_piece == 4) current_state = state::closing;
        }
        next_action = now + static_cast<ULONGLONG>((std::clamp)(std::isfinite(gui_armorswitcher_delay) ? gui_armorswitcher_delay : 80.f, 20.f, 1000.f));
        return;
    }

    if (current_state == state::closing) {
        if (opened_by_us) tap_inventory_key();
        finish(true);
    }
}
void cancel_on_game_frame() {
    JNIEnv* env = nullptr;
    if (!sdk::jvm || sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
        !env || env->ExceptionCheck()) return;
    JNIEnv* previous = sdk::jni;
    sdk::jni = env;
    finish(false);
    sdk::jni = previous;
}
void run_on_game_frame() {
    JNIEnv* env = nullptr;
    if (!sdk::jvm || sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
        !env || env->ExceptionCheck()) return;
    struct EnvScope {
        JNIEnv* previous;
        explicit EnvScope(JNIEnv* current) : previous(sdk::jni) { sdk::jni = current; }
        ~EnvScope() { sdk::jni = previous; }
    } scope(env);
    if (!gui_armorswitcher_enabled && current_state == state::idle) return;
    mapper::__minecraft minecraft;
    if (minecraft.object) run(minecraft);
    else finish(false);
    if (env->ExceptionCheck()) { env->ExceptionClear(); finish(false); }
}

}
