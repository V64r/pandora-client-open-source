#include "../features.hpp"
#include "sdk.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>

extern "C" void swiftLog(const char*, ...);
namespace features::combat::refill {
namespace {
    enum class State { idle, opening, moving, closing };
    State state = State::idle;
    ULONGLONG started = 0, deadline = 0, next_move = 0;
    int attempts = 0, last_slot = -1, repeated_slot = 0;
    bool opened_inventory = false;
    jweak world_identity = nullptr;

    void reset(JNIEnv* env) {
        if (world_identity) env->DeleteWeakGlobalRef(world_identity);
        world_identity = nullptr;
        state = State::idle;
        attempts = 0; last_slot = -1; repeated_slot = 0;
        opened_inventory = false;
        enabled = false;
    }
    void inventory_key() {
        // These queued input events never wait for the window procedure.
        keybd_event('E', static_cast<BYTE>(MapVirtualKey('E', MAPVK_VK_TO_VSC)), 0, 0);
        keybd_event('E', static_cast<BYTE>(MapVirtualKey('E', MAPVK_VK_TO_VSC)), KEYEVENTF_KEYUP, 0);
    }
    bool refillable(mapper::__item_stack& stack) {
        if (!stack.object) return false;
        auto item = stack.get_item();
        if (!item.object || sdk::jni->ExceptionCheck()) return false;
        const int id = item.get_id();
        return id == 282 || (id == 373 && (stack.get_item_damage() & 16384) != 0);
    }
    void finish(JNIEnv* env, ULONGLONG now) {
        if (opened_inventory) {
            inventory_key();
            state = State::closing;
            deadline = now + 800;
        } else reset(env);
    }
}
void run(mapper::__minecraft& minecraft) {
    JNIEnv* env = sdk::jni;
    if (!env) return;
    if (!enabled) { if (state != State::idle) reset(env); return; }
    if (env->ExceptionCheck()) { env->ExceptionClear(); reset(env); return; }
    DWORD foreground_pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
    if (foreground_pid != GetCurrentProcessId()) { reset(env); return; }
    auto player = minecraft.get_local_player();
    auto world = minecraft.get_world();
    if (!player.object || !world.object || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        reset(env); return;
    }
    const auto now = GetTickCount64();
    if (state != State::idle && (!env->IsSameObject(world_identity, world.object) ||
        now - started > 10000 || attempts >= 36)) {
        swiftLog("Refill cancelled: world changed or operation limit reached");
        reset(env); return;
    }
    auto screen = minecraft.get_current_screen();
    const auto& inventory_class = mapper::classes["GuiInventory"];
    const bool inventory = screen.object && inventory_class.klass &&
        env->IsInstanceOf(screen.object, inventory_class.klass);
    if (env->ExceptionCheck()) { env->ExceptionClear(); reset(env); return; }
    if (state == State::closing) {
        if (!inventory || now >= deadline) reset(env);
        return;
    }
    if (state == State::idle) {
        if (screen.object && !inventory) { reset(env); return; }
        bool has_free_slot = false;
        for (int i = 0; i < 9; ++i) {
            auto stack = player.get_inventory_slot(i);
            if (env->ExceptionCheck()) { env->ExceptionClear(); reset(env); return; }
            if (!stack.object) { has_free_slot = true; break; }
        }
        if (!has_free_slot) { reset(env); return; }
        bool has_source = false;
        for (int i = 9; i < 36; ++i) {
            auto stack = player.get_inventory_slot(i);
            if (env->ExceptionCheck()) { env->ExceptionClear(); reset(env); return; }
            if (refillable(stack)) { has_source = true; break; }
        }
        if (!has_source) { reset(env); return; }
        world_identity = env->NewWeakGlobalRef(world.object);
        if (!world_identity) { if (env->ExceptionCheck()) env->ExceptionClear(); reset(env); return; }
        started = now;
        opened_inventory = !inventory;
        state = inventory ? State::moving : State::opening;
        next_move = now;
        deadline = now + 1200;
        if (opened_inventory) inventory_key();
        swiftLog("Refill started on game thread=%lu", GetCurrentThreadId());
        return;
    }
    if (state == State::opening) {
        if (inventory) { state = State::moving; next_move = now; }
        else if (now >= deadline) { swiftLog("Refill cancelled: inventory did not open"); reset(env); }
        return;
    }
    if (!inventory) { reset(env); return; }
    if (now < next_move) return;
    next_move = now + static_cast<ULONGLONG>(std::clamp(delay_ms, 20, 1000));
    bool free_slot = false;
    for (int i = 0; i < 9; ++i) {
        auto stack = player.get_inventory_slot(i);
        if (env->ExceptionCheck()) { env->ExceptionClear(); reset(env); return; }
        if (!stack.object) { free_slot = true; break; }
    }
    if (!free_slot) { finish(env, now); return; }
    int source = -1;
    for (int i = 9; i < 36; ++i) {
        auto stack = player.get_inventory_slot(i);
        if (refillable(stack)) { source = i; break; }
        if (env->ExceptionCheck()) { env->ExceptionClear(); reset(env); return; }
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); reset(env); return; }
    if (source < 0) { finish(env, now); return; }
    repeated_slot = source == last_slot ? repeated_slot + 1 : 0;
    last_slot = source;
    if (repeated_slot >= 3) {
        swiftLog("Refill cancelled: slot %d made no progress", source);
        reset(env); return;
    }
    ++attempts;
    swiftLog("Refill: moving slot %d (attempt %d)", source, attempts);
    if (!minecraft.window_click(0, source, 0, 1, player)) {
        swiftLog("Refill cancelled: windowClick failed");
        reset(env);
    }
}
void cancel_on_game_frame() {
    JNIEnv* env = nullptr;
    if (sdk::jvm && sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env)
        reset(env);
}
void run_on_game_frame() {
    JNIEnv* env = nullptr;
    if (!sdk::jvm || sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
        !env || env->ExceptionCheck()) return;
    struct EnvScope {
        JNIEnv* previous;
        EnvScope(JNIEnv* current) : previous(sdk::jni) { sdk::jni = current; }
        ~EnvScope() { sdk::jni = previous; }
    } scope(env);
    if (!enabled && state == State::idle) return;
    mapper::__minecraft minecraft;
    if (minecraft.object) run(minecraft);
    else reset(env);
}
} // namespace features::combat::refill
