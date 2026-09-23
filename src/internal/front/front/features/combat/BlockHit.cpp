#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <random>

extern bool  gui_blockhit_enabled;
extern int   gui_blockhit_mode;
extern bool  gui_blockhit_require_mouse_down;
extern float gui_blockhit_block_ticks;
extern float gui_blockhit_unblock_ticks;
extern float gui_blockhit_chance;
extern bool  gui_blockhit_only_sword;
extern bool  gui_blockhit_visual_only;
extern std::atomic<bool> g_MenuVisible;

namespace features::combat::block_hit
{
    static bool s_is_blocking = false;
    static ULONGLONG s_block_start = 0;
    static std::mt19937 s_rng(std::random_device{}());
    static bool s_was_swinging = false;
    static int s_block_duration_ms = 50;
    static ULONGLONG s_cooldown_end = 0;

    static bool roll_chance(float val)
    {
        if (val >= 100.0f) return true;
        if (val <= 0.0f)   return false;
        std::uniform_real_distribution<float> d(0.0f, 100.0f);
        return d(s_rng) <= val;
    }

    static int random_ms(int min_ms, int max_ms) {
        if (min_ms >= max_ms) return min_ms;
        std::uniform_int_distribution<int> d(min_ms, max_ms);
        return d(s_rng);
    }

    static int humanize(int base)
    {
        if (base <= 1) return 1;
        std::uniform_int_distribution<int> d(-1, 1);
        int result = base + d(s_rng);
        return result < 1 ? 1 : result;
    }

    // NO tocamos keyCode, NO llamamos a rightClickMouse().
    static void set_key_use_item(mapper::__minecraft& minecraft, bool state)
    {
        if (!sdk::jni) return;

        auto settings = minecraft.get_settings();
        if (!settings.object) return;

        std::string sig = "L" + mapper::classes["KeyBinding"].name + ";";

        mapper::__field kb_fid = mapper::classes["GameSettings"].get_field("keyBindUseItem", sig);
        if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("field_74313_G", sig);
        if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("ag", sig);
        if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("Y", sig);

        if (!kb_fid.identifier) return;

        jobject kb_obj = sdk::jni->GetObjectField(settings.object, kb_fid.identifier);
        if (!kb_obj) return;

        mapper::__field pr_fid = mapper::classes["KeyBinding"].get_field("pressed", "Z");
        if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("field_74513_e", "Z");
        if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("i", "Z");
        if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("h", "Z");
        if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("g", "Z");

        if (pr_fid.identifier) {
            sdk::jni->SetBooleanField(kb_obj, pr_fid.identifier, state);
        }

        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        sdk::jni->DeleteLocalRef(kb_obj);

        s_is_blocking = state;
    }

    static void reset_state(mapper::__minecraft& minecraft)
    {
        if (s_is_blocking) set_key_use_item(minecraft, false);
        s_was_swinging = false;
        s_is_blocking = false;
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (!gui_blockhit_enabled || !sdk::jni) {
            reset_state(minecraft);
            return;
        }
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        if (g_MenuVisible) {
            reset_state(minecraft);
            return;
        }

        auto current_screen = minecraft.get_current_screen();
        if (current_screen.object != nullptr) {
            reset_state(minecraft);
            return;
        }

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) {
            reset_state(minecraft);
            return;
        }

        auto hs = local_player.get_held_item_stack();
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }
        if (!hs.object) { reset_state(minecraft); return; }

        auto item = hs.get_item();
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }
        if (!item.object) { reset_state(minecraft); return; }

        if (gui_blockhit_only_sword && !item.is_sword()) {
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
            reset_state(minecraft);
            return;
        }

        bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (gui_blockhit_require_mouse_down && !lmb) {
            reset_state(minecraft);
            return;
        }

        int hurt_time = local_player.get_hurt_time();
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        bool is_swinging = local_player.is_swing_in_progress();
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        bool just_swung = is_swinging && !s_was_swinging;
        s_was_swinging = is_swinging;

        // --- SISTEMA INTELIGENTE DE DETECCION DE COMBATE ---
        bool hit_detected = false;
        bool enemy_near = false;
        mapper::__vec3 local_pos = local_player.get_position();
        auto world = minecraft.get_world();
        if (world.object) {
            for (auto& player : world.get_players()) {
                if (sdk::jni->IsSameObject(local_player.object, player.object)) continue;
                
                float dist = local_pos.get_distance_to_vec3(player.get_position());
                if (dist <= 3.5f) {
                    enemy_near = true;
                    int ht = player.get_hurt_time();
                    if (ht == 9 || ht == 10) {
                        hit_detected = true;
                        break;
                    }
                }
            }
        }
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        ULONGLONG now = GetTickCount64();
        int block_t = (int)gui_blockhit_block_ticks;
        int unblock_t = (int)gui_blockhit_unblock_ticks;

        if (just_swung && s_is_blocking) {
            set_key_use_item(minecraft, false);
            s_cooldown_end = now;
        }

        switch (gui_blockhit_mode)
        {
        case 0: // Manual
            break;

        case 1:
        {
            if (hurt_time >= 7 && hurt_time <= 10 && enemy_near) {
                if (!s_is_blocking && now >= s_cooldown_end && roll_chance(gui_blockhit_chance)) {
                    set_key_use_item(minecraft, true);
                    s_block_start = now;
                    s_block_duration_ms = humanize(block_t) * 50;
                }
            }
            
            if (s_is_blocking && now >= s_block_start + s_block_duration_ms) {
                set_key_use_item(minecraft, false);
                s_cooldown_end = now + (humanize(unblock_t) * 50);
            }
            break;
        }

        case 2: // Auto (Legit) - Bloqueo perfecto post-hit
        case 3: // Lag - Bloqueo rapido en cada swing
        {
            if (!lmb || !enemy_near) {
                reset_state(minecraft);
                break;
            }

            // Desbloqueo preciso
            if (s_is_blocking) {
                if (now >= s_block_start + s_block_duration_ms) {
                    set_key_use_item(minecraft, false);
                    int cd_ms = (gui_blockhit_mode == 3) ? random_ms(15, 30) : (humanize(unblock_t) * random_ms(30, 50));
                    s_cooldown_end = now + cd_ms;
                }
            }
            else {
                // Bloqueo
                if (now >= s_cooldown_end) {
                    bool should_block = false;
                    
                    if (gui_blockhit_mode == 2) {
                        should_block = hit_detected || (just_swung && roll_chance(70.0f)); 
                    } else if (gui_blockhit_mode == 3) {
                        should_block = just_swung || hit_detected;
                    }

                    if (should_block && roll_chance(gui_blockhit_chance)) {
                        set_key_use_item(minecraft, true);
                        s_block_start = now;
                        
                        int base_dur = (gui_blockhit_mode == 3) ? random_ms(15, 35) : (humanize(block_t) * random_ms(20, 45));
                        if (base_dur < 15) base_dur = 15;
                        if (base_dur > 200) base_dur = 200; // Cap de seguridad
                        
                        s_block_duration_ms = base_dur;
                    }
                }
            }
            break;
        }
        }

        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    }
}
