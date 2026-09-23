#pragma once
#include "mapper.hpp"
#include <string>
#include <utility>
#include <vector>
#include <atomic>
namespace features::visual::outline {
    inline bool enabled = false;
    inline float color[4] = { 228.f / 255.f, 228.f / 255.f, 231.f / 255.f, 1.f };
    inline float thickness = 2.f;
    inline bool glow = false;
    inline float strength = 0.5f;

    void set_hook_runtime(bool available);
    void sync_hooks(mapper::__minecraft& minecraft);
    void render();
    void release_gl();
    void release_jni();
    void reset();
}

namespace features
{
    namespace movement {
        namespace timer_speed {
            inline bool enabled = false, require_damage = false, only_weapon = false, moving = false;
            inline float speed = 1.f;
            void run(mapper::__minecraft& minecraft);
            void cancel();
        }
        namespace bunnyhop {
            inline bool enabled = false, liquid_check = true, only_moving = true;
            inline float jump_delay = 0.f, jump_height = 0.42f, power = 1.45f;
            inline float speed_multiplier = 1.6f, slowdown_factor = 0.66f, friction = 159.f, direction_threshold = 10.f;
            void run(mapper::__minecraft& minecraft);
            void cancel();
        }
    }
    namespace misc {
        namespace bard_helper {
            inline bool enabled = false, return_last_slot = true;
            inline bool resistance = true, strength = true, regeneration = true, speed = true, jump = true;
            inline float switch_delay = 100.f;
            inline int bind = 0;
            void run(mapper::__minecraft& minecraft);
            void cancel();
        }
        void run_inputs_on_game_frame();
        void cancel_inputs_on_game_frame();
        namespace right_clicker {
            inline bool enabled = false, only_click = true, ignore_obsidian = false, only_blocks = false;
            inline float min_cps = 10.f, max_cps = 12.f;
            void run(mapper::__minecraft& minecraft);
            void cancel();
        }
        namespace bridge_assist {
            inline bool enabled = false, randomize = false, sneak_on_jump = false, avoid_double_sneaking = false;
            inline bool require_sneak = false, holding_blocks = true, looking_down = true, not_forward = true;
            inline float edge_offset = 0.15f, unsneak_delay = 60.f;
            inline int select_blocks = 0;
            void run(mapper::__minecraft& minecraft);
            void cancel();
            bool placement_blocked();
        }
    }
    void run_on_run_tick(mapper::__minecraft& minecraft);
}
namespace features::combat::reach
{
    inline bool enabled = false;
    inline float min_distance = 3.0f;
    inline float max_distance = 3.10f;
    inline bool hitbox_enabled = false;
    inline float hitbox_size = 0.20f;
    inline float chance = 100.0f;
    inline bool ground_only = false;
    inline bool weapon_only = false;
    inline bool liquid_check = false;
    inline bool combo_mode = false;
    inline bool hit_through_walls = false;

    void run(mapper::__minecraft& minecraft);
}
namespace features::combat::velocity
{
    inline bool  enabled = false;
    inline int   mode = 0;               
    inline bool  air_only = false;
    inline bool  moving_only = false;
    inline bool  weapon_only = false;
    inline bool  push_back = false;
    inline bool  clicking_only = false;
    inline float horizontal = 100.0f;
    inline float vertical = 100.0f;
    inline float chance = 100.0f;
    inline float delay = 0.0f;

    void run(mapper::__minecraft& minecraft);
}

// --- NO HIT DELAY ---
namespace features::combat::no_hit_delay {
    extern bool enabled;
    void run(mapper::__minecraft& mc);
}
// --- BLOCKHIT ---
namespace features::combat::block_hit {
    void run(mapper::__minecraft& minecraft);
}

// --- MACROS ---
namespace features::combat::macros {
    void run(mapper::__minecraft& minecraft);
}
// --- ARMOR SWITCHER ---
namespace features::combat::armor_switcher {
    void run_on_game_frame();
    void cancel_on_game_frame();
    void run(mapper::__minecraft& minecraft);
}

// --- COMBATE ---
namespace features::combat::auto_click
{
    inline bool enabled = false;
    inline double min_cps = 12.0;
    inline double max_cps = 14.0;
    inline bool inventory_enabled = false;
    inline bool prevent_unrefill = false;
    inline double inventory_cps = 20.0; // CPS for inventory fill (fast but server-safe)
    inline bool click_sound = false;
    inline int sound = 0;
    inline float click_volume = 50.f;
    inline bool weapons_only = false;
    inline bool break_blocks = false;

    // Click Method: 0=Normal, 1=Jitter, 2=Butterfly
    inline int click_method = 0;

    // Conditions
    inline bool target_only = false;
    inline bool randomization = false;
    inline float drop_chance = 5.0f;   // 0 - 100%
    inline float spike_chance = 2.0f;  // 0 - 100%

    // Click counter (W TAP)
    void shutdown();

    void run(mapper::__minecraft& minecraft);
}
namespace features::combat::aim_assist
{
    inline bool enabled = false;
    inline int mode = 0;              // 0=Regular, 1=Lock On
    inline int aim_mode = 2;          // 0=Horizontal, 1=Vertical, 2=Both
    inline int priority = 0;          // 0=FOV, 1=Distance, 2=Health
    inline int target_mode = 1;       // 0=Single, 1=Switch
    inline double minimum_distance = 1.0;
    inline double maximum_distance = 4.0;
    inline double minimum_fov = 30.0;
    inline double maximum_fov = 180.0;
    inline double horizontal_speed = 10.0;
    inline double vertical_speed = 10.0;
    inline bool clicking_only = false;
    inline bool weapons_only = false;
    inline bool break_blocks = false;
    inline bool through_walls = false;
    inline bool ignore_invisible = false;
    inline float angle_x_changes = 0.f;  // rotacion horizontal calculada
    inline float angle_y_changes = 0.f;  // rotacion vertical calculada
    inline bool silent = true;

    void run(mapper::__minecraft& minecraft);
}
namespace features::combat::refill
{
    inline bool enabled = false;
    inline int bind = 0;
    inline int delay_ms = 80;
    void run(mapper::__minecraft& minecraft);
    void run_on_game_frame();
    void cancel_on_game_frame();
}

// --- MISC / PLAYER ---
namespace features::misc::auto_armor
{
    void run(mapper::__minecraft& minecraft);
}

namespace features::misc::fastplace
{
    void run(mapper::__minecraft& minecraft);
}

// --- MISC / PLAYER ---
namespace features::latency::blink
{
    inline bool enabled = false;
    inline int  mode = 0;           // 0=Smooth, 1=Freeze
    inline bool show_path = true;
    inline bool show_timer = true;
    inline float path_color[3] = { 0.2f, 0.6f, 1.0f };
    inline float timer_limit = 5.0f; // in seconds
    inline int bind = 0;

    extern std::atomic_bool is_blinking;

    void run(mapper::__minecraft& minecraft);
    void render_ui();
}
// --- MOVEMENT ---
namespace features::movement::sprint
{
    inline bool enabled = false;

    void run(mapper::__minecraft& minecraft);
}

namespace features::movement::no_slow
{
    inline bool enabled = false;

    void run(mapper::__minecraft& minecraft);
}

namespace features::movement::no_item_release
{
    inline bool sword = false, bow = false, potions = false;
    inline bool enabled = false;
    inline bool food = false;

    void run(mapper::__minecraft& minecraft);
}

namespace features::movement::no_jump_delay
{
    inline bool enabled = false;

    void run(mapper::__minecraft& minecraft);
}
namespace features::movement::snap_tap
{
    inline bool enabled = false;
    void run(mapper::__minecraft& minecraft);
}
namespace features::movement::instant_stop
{
    inline bool enabled = false;
    inline bool only_on_ground = true;
    inline bool stop_on_sneak = false;
    inline float stop_strength = 1.0f;
    void run(mapper::__minecraft& minecraft);
}
// --- MOVEMENT ---
// --- ARRAYLIST (HUD VISUAL) ---
namespace features::visual::arraylist
{
    inline bool enabled = false;
    inline bool watermark = false;
    inline float color[3] = { 0.85f, 0.05f, 0.80f };
    void run();
}

// ============================================================================
// VISUALES / MOTOR GRAFICO
// ============================================================================

namespace features::visual
{
    extern void* window;

    // Shared between the Minecraft/render hook and the swap-buffers thread.
    // Atomic prevents torn/racy visibility while a world is loaded/unloaded.
    inline std::atomic_bool render_valid{ false };
    inline unsigned long long matrix_capture_tick = 0;

    // Matrices matematicas del juego
    extern double model_view_matrix[16];
    extern double projection_matrix[16];
    extern int view_port[4];
    extern double render_camera_x;
    extern double render_camera_y;
    extern double render_camera_z;
    extern double render_local_player_x;
    extern double render_local_player_y;
    extern double render_local_player_z;
    extern bool render_local_player_valid;
    extern float render_partial_ticks;
    extern int render_world_tick;
    extern bool render_frame_snapshot_valid;

    // Declaracion de las funciones de renderizado
    auto world_to_screen(mapper::__vec3 data, bool can_reverse = false, bool ignore_z = false) -> mapper::__vec2;
    auto render_nametag(std::string name, mapper::__vec3 vec3, mapper::__vec4 color, bool draw_health, float health, bool draw_distance, double distance, bool draw_hurt_time, __int32 hurt_time) -> void;
    auto render_tracer(mapper::__vec3 vec3, mapper::__vec4 color, bool draw_distance, double distance, bool draw_hurt_time, __int32 hurt_time) -> void;

    // Helpers
}

namespace features::visual::esp
{
    void run(mapper::__minecraft& minecraft);
    void render();
    void clear();
}

namespace features::visual::nametags
{
    void run_on_game_frame();
    void initialize_hook();
    void sync_hide_vanilla_hook(bool should_attach);
    bool uninitialize_hook();
    void shutdown(mapper::__minecraft& minecraft);
    void abandon_render_resources();
    inline bool enabled = false;
    inline bool draw_health = true;
    inline bool draw_distance = true;
    inline bool draw_hurt_time = true;
    inline bool draw_invisible_players = false;
    inline bool background = true;

    inline bool use_fake_name = false;
    inline std::string fake_name = "Jugador";

    inline mapper::__vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    void run(mapper::__minecraft& minecraft);
    auto render() -> void;
    void render_minecraft_font();
    bool has_pending_minecraft_render();
}

namespace features::visual::tracers
{
    inline bool enabled = false;
    inline bool draw_distance = false;
    inline bool draw_hurt_time = false;
    inline bool draw_invisible_players = false;
    inline float thickness = 1.5f;
    inline mapper::__vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    void run(mapper::__minecraft& minecraft);
    auto render() -> void;
}


namespace features::visual::aguita
{
    inline bool enabled = false;
}

namespace features::misc::notifications
{
    inline bool enabled = true;
    inline int position = 3; // 0: Top Left, 1: Top Right, 2: Bottom Left, 3: Bottom Right
    inline float bar_color[4] = { 228.f / 255.f, 228.f / 255.f, 231.f / 255.f, 1.f };
}

namespace features::settings
{
}

// --- FRIENDS ---
namespace features::friends
{
    inline int nearby_bind = 0;
    inline float nearby_distance = 10.f;
    inline std::atomic<bool> nearby_requested{false};
    bool add_name(const std::string& name);
    extern std::vector<std::string>* list;
    extern std::vector<std::string>* uuids;
    bool is_friend(const std::string& name);
    bool is_teammate(mapper::__player& player, mapper::__player& local_player);
    void clear();
    void remove_at(size_t index);
    std::vector<std::string> snapshot_names();
    std::vector<std::pair<std::string, std::string>> snapshot_entries();
    void replace_all(const std::vector<std::string>& names, const std::vector<std::string>& ids);
    void request_uuid(const std::string& name);
    void verify_uuid(const std::string& uuid);
    void shutdown();
    void run(mapper::__minecraft& minecraft);
}

namespace features::visual::hit_markers
{
    inline bool enabled = false;
    inline int mode = 0; // 0 = 2D, 1 = 3D
    inline float color[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float size = 10.f;
    inline float line_width = 2.f;
    inline float duration = 0.5f;
    inline bool fade_out = true;
    inline bool scale_animation = true;
    inline float scale_amount = 1.5f;
    inline bool outline = true;
    inline float outline_color[4] = { 0.f, 0.f, 0.f, 1.f };
    inline float outline_width = 1.f;
    void run(mapper::__minecraft& minecraft);
    void render();
}
