#include "../features.hpp"

#define NOMINMAX
#include <windows.h>

#include <algorithm>

namespace features::movement::snap_tap
{
    namespace
    {
        constexpr int key_forward = 17;
        constexpr int key_back = 31;
        constexpr int key_left = 30;
        constexpr int key_right = 32;
        bool previous_forward = false;
        bool previous_back = false;
        bool previous_left = false;
        bool previous_right = false;
        int last_forward_direction = 1;
        int last_strafe_direction = 1;
        bool overriding_keys = false;
        jfieldID movement_key_fields[4]{};

        int movement_key_index(int key_code)
        {
            if (key_code == key_forward) return 0;
            if (key_code == key_back) return 1;
            if (key_code == key_left) return 2;
            if (key_code == key_right) return 3;
            return -1;
        }

        mapper::__field find_key_code_field()
        {
            auto field = mapper::classes["KeyBinding"].get_field("keyCode", "I");
            if (!field.identifier) field = mapper::classes["KeyBinding"].get_field("field_151469_d", "I");
            if (!field.identifier) field = mapper::classes["KeyBinding"].get_field("d", "I");
            if (!field.identifier) field = mapper::classes["KeyBinding"].get_field("e", "I");
            return field;
        }

        mapper::__field find_pressed_field()
        {
            auto field = mapper::classes["KeyBinding"].get_field("pressed", "Z");
            if (!field.identifier) field = mapper::classes["KeyBinding"].get_field("field_74513_e", "Z");
            if (!field.identifier) field = mapper::classes["KeyBinding"].get_field("i", "Z");
            if (!field.identifier) field = mapper::classes["KeyBinding"].get_field("h", "Z");
            if (!field.identifier) field = mapper::classes["KeyBinding"].get_field("g", "Z");
            return field;
        }

        void set_movement_key(mapper::__settings& settings, int key_code, bool pressed)
        {
            if (!settings.object || !mapper::classes["GameSettings"].klass ||
                !mapper::classes["KeyBinding"].klass)
                return;

            const auto code_field = find_key_code_field();
            const auto pressed_field = find_pressed_field();
            if (!code_field.identifier || !pressed_field.identifier) return;

            const std::string key_signature = mapper::classes["KeyBinding"].signature;
            const int key_index = movement_key_index(key_code);
            if (key_index >= 0 && movement_key_fields[key_index]) {
                jobject binding = sdk::jni->GetObjectField(settings.object, movement_key_fields[key_index]);
                if (binding) {
                    sdk::jni->SetBooleanField(binding, pressed_field.identifier, pressed);
                    sdk::jni->DeleteLocalRef(binding);
                    if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                }
                return;
            }

            for (const auto& field : mapper::classes["GameSettings"].fields) {
                if (field.signature != key_signature || !field.identifier) continue;

                jobject binding = sdk::jni->GetObjectField(settings.object, field.identifier);
                if (!binding) {
                    if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                    continue;
                }

                const int binding_code = sdk::jni->GetIntField(binding, code_field.identifier);
                if (!sdk::jni->ExceptionCheck() && binding_code == key_code) {
                    sdk::jni->SetBooleanField(binding, pressed_field.identifier, pressed);
                    if (key_index >= 0) movement_key_fields[key_index] = field.identifier;
                }
                if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                sdk::jni->DeleteLocalRef(binding);
                if (binding_code == key_code) return;
            }
        }

        void restore_physical_keys(mapper::__minecraft& minecraft)
        {
            if (!overriding_keys) return;
            auto settings = minecraft.get_settings();
            set_movement_key(settings, key_forward, (GetAsyncKeyState('W') & 0x8000) != 0);
            set_movement_key(settings, key_back, (GetAsyncKeyState('S') & 0x8000) != 0);
            set_movement_key(settings, key_left, (GetAsyncKeyState('A') & 0x8000) != 0);
            set_movement_key(settings, key_right, (GetAsyncKeyState('D') & 0x8000) != 0);
            overriding_keys = false;
        }

        void reset_state(mapper::__minecraft& minecraft)
        {
            restore_physical_keys(minecraft);
            previous_forward = false;
            previous_back = false;
            previous_left = false;
            previous_right = false;
            last_forward_direction = 1;
            last_strafe_direction = 1;
        }

        void stop_for_open_screen(mapper::__minecraft& minecraft)
        {
            auto settings = minecraft.get_settings();
            set_movement_key(settings, key_forward, false);
            set_movement_key(settings, key_back, false);
            set_movement_key(settings, key_left, false);
            set_movement_key(settings, key_right, false);

            auto player = minecraft.get_local_player();
            if (player.object) {
                player.set_move_foreward(0.0f);
                player.set_move_strafing(0.0f);
            }

            overriding_keys = false;
            previous_forward = previous_back = false;
            previous_left = previous_right = false;
            last_forward_direction = 1;
            last_strafe_direction = 1;
        }
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled) {
            reset_state(minecraft);
            return;
        }

        if (!features::visual::window ||
            GetForegroundWindow() != static_cast<HWND>(features::visual::window)) {
            stop_for_open_screen(minecraft);
            return;
        }

        auto current_screen = minecraft.get_current_screen();
        if (current_screen.object != nullptr) {
            // Minecraft intentionally releases movement while a GUI is open.
            // Restoring the physical W/A/S/D state here re-enabled movement
            // through inventories, so explicitly clear both bindings and the
            // player's cached movement input instead.
            stop_for_open_screen(minecraft);
            return;
        }

        auto player = minecraft.get_local_player();
        auto settings = minecraft.get_settings();
        if (!player.object || !settings.object) {
            reset_state(minecraft);
            return;
        }

        const bool forward = (GetAsyncKeyState('W') & 0x8000) != 0;
        const bool back = (GetAsyncKeyState('S') & 0x8000) != 0;
        const bool left = (GetAsyncKeyState('A') & 0x8000) != 0;
        const bool right = (GetAsyncKeyState('D') & 0x8000) != 0;

        if (forward && !previous_forward) last_forward_direction = 1;
        if (back && !previous_back) last_forward_direction = -1;
        if (left && !previous_left) last_strafe_direction = 1;
        if (right && !previous_right) last_strafe_direction = -1;

        previous_forward = forward;
        previous_back = back;
        previous_left = left;
        previous_right = right;

        const bool forward_wins = forward && back && last_forward_direction > 0;
        const bool back_wins = forward && back && last_forward_direction < 0;
        const bool left_wins = left && right && last_strafe_direction > 0;
        const bool right_wins = left && right && last_strafe_direction < 0;

        set_movement_key(settings, key_forward, forward && (!back || forward_wins));
        set_movement_key(settings, key_back, back && (!forward || back_wins));
        set_movement_key(settings, key_left, left && (!right || left_wins));
        set_movement_key(settings, key_right, right && (!left || right_wins));
        overriding_keys = forward || back || left || right;

        const float forward_value = forward == back ?
            (forward ? static_cast<float>(last_forward_direction) : 0.0f) :
            (forward ? 1.0f : -1.0f);
        const float strafe_value = left == right ?
            (left ? static_cast<float>(last_strafe_direction) : 0.0f) :
            (left ? 1.0f : -1.0f);
        player.set_move_foreward(forward_value);
        player.set_move_strafing(strafe_value);
    }
}
