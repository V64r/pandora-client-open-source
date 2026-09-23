#include "InputSupport.hpp"
#include <algorithm>
#include <cmath>
#include <random>

extern std::atomic<bool> g_MenuVisible;
extern bool g_ConfigInputActive;
extern bool g_NumericEditActive;
namespace features::misc::right_clicker {
namespace {
HWND pressed_window = nullptr;
ULONGLONG next_click = 0, release_at = 0;
std::mt19937 random_engine{std::random_device{}()};
}
void cancel() {
    if (pressed_window) PostMessageW(pressed_window, WM_RBUTTONUP, 0, 0);
    pressed_window = nullptr;
    next_click = release_at = 0;
}
void run(mapper::__minecraft& minecraft) {
    const auto now = GetTickCount64();
    if (pressed_window && now >= release_at) {
        PostMessageW(pressed_window, WM_RBUTTONUP, 0, 0);
        pressed_window = nullptr;
    }
    if (!enabled || (only_click && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) ||
        bridge_assist::placement_blocked()) { cancel(); return; }
    auto player = minecraft.get_local_player();
    if (!player.object) { cancel(); return; }
    auto stack = player.get_held_item_stack();
    if ((only_blocks && !input_support::block_stack(stack)) || sdk::jni->ExceptionCheck()) {
        cancel(); return;
    }
    if (ignore_obsidian && stack.object) {
        auto item = stack.get_item();
        if (!item.object || sdk::jni->ExceptionCheck() || item.get_id() == 49) { cancel(); return; }
    }
    if (sdk::jni->ExceptionCheck() || pressed_window || now < next_click) return;
    const float low = std::clamp(std::isfinite(min_cps) ? min_cps : 10.f, 1.f, 25.f);
    const float high = std::clamp(std::isfinite(max_cps) ? max_cps : low, low, 25.f);
    const double interval = 1000.0 / std::uniform_real_distribution<float>(low, high)(random_engine);
    HWND window = static_cast<HWND>(features::visual::window);
    POINT point{};
    if (!window || !GetCursorPos(&point) || !ScreenToClient(window, &point)) { cancel(); return; }
    minecraft.set_right_click_delay_timer(0);
    if (sdk::jni->ExceptionCheck()) return;
    if (PostMessageW(window, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(point.x, point.y))) {
        pressed_window = window;
        release_at = now + static_cast<ULONGLONG>(interval * 0.30);
    }
    next_click = (next_click && now - next_click < 100 ? next_click : now) + static_cast<ULONGLONG>(interval);
}
}

namespace features::misc {
namespace {
jweak input_world = nullptr;
struct EnvScope {
    JNIEnv* previous;
    explicit EnvScope(JNIEnv* env) : previous(sdk::jni) { sdk::jni = env; }
    ~EnvScope() { sdk::jni = previous; }
};
void cancel_all(JNIEnv* env) {
    right_clicker::cancel();
    bridge_assist::cancel();
    bard_helper::cancel();
    movement::timer_speed::cancel();
    movement::bunnyhop::cancel();
    if (input_world) env->DeleteWeakGlobalRef(input_world);
    input_world = nullptr;
}
}
void cancel_inputs_on_game_frame() {
    JNIEnv* env = nullptr;
    if (!sdk::jvm || sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
        !env || env->ExceptionCheck()) { right_clicker::cancel(); return; }
    EnvScope scope(env);
    cancel_all(env);
}
void run_inputs_on_game_frame() {
    JNIEnv* env = nullptr;
    if (!sdk::jvm || sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
        !env || env->ExceptionCheck()) return;
    EnvScope scope(env);
    if (g_MenuVisible && (g_ConfigInputActive || g_NumericEditActive)) {
        mapper::__minecraft mc;
        if (mc.object) {
            auto settings = mc.get_settings();
            const auto pressed = input_support::field("KeyBinding", "Z", {"pressed","field_74513_e","h"});
            const char* names[]{"keyBindForward","keyBindBack","keyBindLeft","keyBindRight","keyBindJump","keyBindSneak","keyBindSprint"};
            const char* aliases[]{"field_74351_w","field_74368_y","field_74370_x","field_74366_z","field_74314_A","field_74311_E","field_151444_V"};
            for (int i=0;settings.object && pressed && i<7 && !env->ExceptionCheck();++i) {
                const auto field=input_support::field("GameSettings",mapper::classes["KeyBinding"].signature,{names[i],aliases[i]});
                if(!field) continue;
                jobject key=env->GetObjectField(settings.object,field);
                if(key && !env->ExceptionCheck()) env->SetBooleanField(key,pressed,JNI_FALSE);
                if(key) env->DeleteLocalRef(key);
            }
        }
        if(env->ExceptionCheck()) env->ExceptionClear();
    }
    if ((!right_clicker::enabled && !bridge_assist::enabled && !movement::timer_speed::enabled && !movement::bunnyhop::enabled && !bard_helper::enabled) || g_MenuVisible) {
        cancel_all(env); return;
    }
    HWND window = static_cast<HWND>(features::visual::window);
    HWND foreground = GetForegroundWindow();
    if (!window || !foreground || GetAncestor(window, GA_ROOT) != GetAncestor(foreground, GA_ROOT)) {
        cancel_all(env); return;
    }
    mapper::__minecraft minecraft;
    if (!minecraft.object) { cancel_all(env); return; }
    auto world = minecraft.get_world();
    auto player = minecraft.get_local_player();
    auto screen = minecraft.get_current_screen();
    if (env->ExceptionCheck()) { env->ExceptionClear(); cancel_all(env); return; }
    if (!world.object || !player.object || screen.object) { cancel_all(env); return; }
    if (input_world && !env->IsSameObject(input_world, world.object)) cancel_all(env);
    if (!input_world) input_world = env->NewWeakGlobalRef(world.object);
    if (!input_world || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        cancel_all(env); return;
    }
    bard_helper::run(minecraft);
    if (!env->ExceptionCheck()) movement::timer_speed::run(minecraft);
    if (!env->ExceptionCheck()) movement::bunnyhop::run(minecraft);
    if (!env->ExceptionCheck()) bridge_assist::run(minecraft);
    if (!env->ExceptionCheck()) right_clicker::run(minecraft);
    if (env->ExceptionCheck()) { env->ExceptionClear(); cancel_all(env); }
}
}
