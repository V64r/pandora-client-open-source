#include "../features.hpp"
#include <vector>
#include <mutex>
#include <imgui.h>
#include <cfloat>
#include <string>
#include <unordered_map>
#include <chrono>

struct Tracer_Entry
{
    mapper::__vec3 world_pos{};
    mapper::__vec4 color{};        // Color manual (fallback)
    ImU32          rank_color = 0;
    double         distance = 0.0;
    __int32        hurt_time = 0;
    bool           has_rank_color = false;
};

extern std::atomic<bool> g_PlayerInGui;
static std::vector<Tracer_Entry> g_tracer_buffer;
static std::mutex                g_tracer_mutex;
static double g_tracer_cam_x = 0.0, g_tracer_cam_y = 0.0, g_tracer_cam_z = 0.0;

// ============================================================
// ============================================================
struct TracerCachedName {
    std::string                           value;
    std::chrono::steady_clock::time_point expiry;
};
static std::unordered_map<std::string, TracerCachedName> g_tracer_name_cache;

struct TracerJNIFrame {
    JNIEnv* env;
    TracerJNIFrame(JNIEnv* e, int cap) : env(e) { if (env) env->PushLocalFrame(cap); }
    ~TracerJNIFrame() { if (env) env->PopLocalFrame(nullptr); }
};

static std::string tracer_get_formatted_name(JNIEnv* env, jobject player_obj, const std::string& player_name)
{
    if (!env || !player_obj) return "";

    auto now = std::chrono::steady_clock::now();
    auto it = g_tracer_name_cache.find(player_name);
    if (it != g_tracer_name_cache.end() && now < it->second.expiry)
        return it->second.value;

    static jmethodID mid = nullptr;
    static bool      tried = false;
    static jmethodID mid_fmt = nullptr;
    static bool      tried_fmt = false;

    jclass player_class = env->GetObjectClass(player_obj);
    if (!player_class) return "";

    if (!tried) {
        tried = true;
        mid = env->GetMethodID(player_class, "getDisplayName", "()Lnet/minecraft/util/IChatComponent;");
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(player_class, "func_145748_c_", "()Lnet/minecraft/util/IChatComponent;"); }
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(player_class, "f_=", "()Lnet/minecraft/util/IChatComponent;"); }
    }

    std::string result;
    if (mid) {
        jobject chat_comp = env->CallObjectMethod(player_obj, mid);
        if (chat_comp) {
            jclass chat_comp_class = env->GetObjectClass(chat_comp);
            if (chat_comp_class) {
                if (!tried_fmt) {
                    tried_fmt = true;
                    mid_fmt = env->GetMethodID(chat_comp_class, "getFormattedText", "()Ljava/lang/String;");
                    if (!mid_fmt) { env->ExceptionClear(); mid_fmt = env->GetMethodID(chat_comp_class, "func_150254_d", "()Ljava/lang/String;"); }
                    if (!mid_fmt) { env->ExceptionClear(); mid_fmt = env->GetMethodID(chat_comp_class, "d", "()Ljava/lang/String;"); }
                }
                if (mid_fmt) {
                    jstring jstr = (jstring)env->CallObjectMethod(chat_comp, mid_fmt);
                    if (jstr) {
                        const char* chars = env->GetStringUTFChars(jstr, nullptr);
                        if (chars) { result = chars; env->ReleaseStringUTFChars(jstr, chars); }
                        env->DeleteLocalRef(jstr);
                    }
                }
                env->DeleteLocalRef(chat_comp_class);
            }
            env->DeleteLocalRef(chat_comp);
        }
    }
    env->DeleteLocalRef(player_class);
    if (env->ExceptionCheck()) env->ExceptionClear();

    g_tracer_name_cache[player_name] = { result, now + std::chrono::milliseconds(3000) };
    return result;
}

// ============================================================
// Devuelve true y escribe out_color si encuentra un color.
// ============================================================
static bool extract_rank_color(const std::string& formatted, ImU32& out_color)
{
    const int len = (int)formatted.length();
    for (int i = 0; i < len; ++i)
    {
        char cc = 0;
        bool is_cc = false;

        if ((unsigned char)formatted[i] == 0xC2 &&
            i + 2 < len &&
            (unsigned char)formatted[i + 1] == 0xA7)
        {
            is_cc = true;
            cc = formatted[i + 2];
            i += 2;
        }
        else if (formatted[i] == '&' && i + 1 < len)
        {
            is_cc = true;
            cc = formatted[i + 1];
            i += 1;
        }

        if (!is_cc) continue;

        switch (tolower((unsigned char)cc))
        {
        case '0': out_color = IM_COL32(0, 0, 0, 255); return true;
        case '1': out_color = IM_COL32(0, 0, 170, 255); return true;
        case '2': out_color = IM_COL32(0, 170, 0, 255); return true;
        case '3': out_color = IM_COL32(0, 170, 170, 255); return true;
        case '4': out_color = IM_COL32(170, 0, 0, 255); return true;
        case '5': out_color = IM_COL32(170, 0, 170, 255); return true; // Morado
        case '6': out_color = IM_COL32(255, 170, 0, 255); return true; // Dorado
        case '7': out_color = IM_COL32(170, 170, 170, 255); return true;
        case '8': out_color = IM_COL32(85, 85, 85, 255); return true;
        case '9': out_color = IM_COL32(85, 85, 255, 255); return true;
        case 'a': out_color = IM_COL32(85, 255, 85, 255); return true;
        case 'b': out_color = IM_COL32(85, 255, 255, 255); return true;
        case 'c': out_color = IM_COL32(255, 85, 85, 255); return true;
        case 'd': out_color = IM_COL32(255, 85, 255, 255); return true; // Rosa/Magenta
        case 'e': out_color = IM_COL32(255, 255, 85, 255); return true;
        case 'f': out_color = IM_COL32(255, 255, 255, 255); return true;
        default: break;
        }
    }
    return false;
}

// ============================================================
// ============================================================
auto features::visual::tracers::run(mapper::__minecraft& minecraft) -> void
{
    auto clear_buffer = [&]() {
        std::lock_guard<std::mutex> lock(g_tracer_mutex);
        g_tracer_buffer.clear();
        };

    if (!features::visual::tracers::enabled) { clear_buffer(); return; }


    auto timer = minecraft.get_timer();
    if (timer.object == nullptr) return;
    float partial_ticks = timer.get_partial_ticks();

    auto world = minecraft.get_world();
    auto local_player = minecraft.get_local_player();
    if (world.object == nullptr || local_player.object == nullptr) { clear_buffer(); return; }

    auto local_pos = local_player.get_view_position(partial_ticks);
    auto world_players = world.get_players();

    auto rm = minecraft.get_render_manager();
    double cam_x = 0.0, cam_y = 0.0, cam_z = 0.0;
    if (rm.object) {
        auto local_pos = local_player.get_view_position(partial_ticks);
        cam_x = rm.get_render_pos_x();
        cam_y = rm.get_render_pos_y();
        cam_z = rm.get_render_pos_z();
    }

    std::vector<Tracer_Entry> temp;
    temp.reserve(world_players.size());

    for (auto& player : world_players)
    {
        if (player.object == nullptr) continue;
        if (sdk::jni->IsSameObject(local_player.object, player.object)) continue;

        float health = player.get_health();
        if (health <= 0.f) continue;

        if (!features::visual::tracers::draw_invisible_players && player.get_flag(5)) continue;

        TracerJNIFrame frame(sdk::jni, 20);

        auto player_pos = player.get_view_position(partial_ticks);
        double distance = local_pos.get_distance_to_vec3(player_pos);
        if (distance > 255.0) continue;

        Tracer_Entry entry;

        entry.world_pos.x = player_pos.x;
        entry.world_pos.y = player_pos.y + 0.9f;
        entry.world_pos.z = player_pos.z;

        entry.color = features::visual::tracers::color;
        entry.distance = distance;
        entry.hurt_time = features::visual::tracers::draw_hurt_time ? player.get_hurt_time() : 0;

        std::string name = player.get_name();
        entry.rank_color = 0;
        entry.has_rank_color = false;

        if (!name.empty())
        {
            std::string fmt = tracer_get_formatted_name(sdk::jni, player.object, name);
            if (!fmt.empty())
                entry.has_rank_color = extract_rank_color(fmt, entry.rank_color);
        }

        temp.push_back(std::move(entry));
    }

    // Limpiar cache expirado cada ~60 ticks
    static int cache_tick = 0;
    if (++cache_tick >= 60) {
        cache_tick = 0;
        auto now = std::chrono::steady_clock::now();
        for (auto it = g_tracer_name_cache.begin(); it != g_tracer_name_cache.end(); )
            it = (now >= it->second.expiry) ? g_tracer_name_cache.erase(it) : ++it;
    }

    std::lock_guard<std::mutex> lock(g_tracer_mutex);
    g_tracer_buffer = std::move(temp);
    g_tracer_cam_x = cam_x;
    g_tracer_cam_y = cam_y;
    g_tracer_cam_z = cam_z;
}

// ============================================================
// ============================================================
auto features::visual::tracers::render() -> void
{
    if (!features::visual::tracers::enabled) return;
    if (features::visual::view_port[2] == 0) return;

    double rm_x, rm_y, rm_z;
    thread_local std::vector<Tracer_Entry> local_buf;
    {
        std::lock_guard<std::mutex> lock(g_tracer_mutex);
        if (g_tracer_buffer.empty()) return;
        local_buf = g_tracer_buffer;
        rm_x = g_tracer_cam_x;
        rm_y = g_tracer_cam_y;
        rm_z = g_tracer_cam_z;
    }

    auto* draw = ImGui::GetBackgroundDrawList();

    float  center_x = (float)features::visual::view_port[2] / 2.0f;
    float  center_y = (float)features::visual::view_port[3] / 2.0f;
    ImVec2 start_pos = { center_x, center_y };

    for (auto& e : local_buf)
    {
        mapper::__vec3 rel_pos;
        rel_pos.x = (float)(e.world_pos.x - rm_x);
        rel_pos.y = (float)(e.world_pos.y - rm_y);
        rel_pos.z = (float)(e.world_pos.z - rm_z);

        auto screen = features::visual::world_to_screen(rel_pos, true);
        if (screen.x == FLT_MAX || screen.y == FLT_MAX) continue;



        ImU32 line_color;
        if (e.has_rank_color)
        {
            float user_alpha = static_cast<float>(e.color.w);

            float r = ((e.rank_color >> 0) & 0xFF) / 255.f;
            float g = ((e.rank_color >> 8) & 0xFF) / 255.f;
            float b = ((e.rank_color >> 16) & 0xFF) / 255.f;
            line_color = ImGui::ColorConvertFloat4ToU32({ r, g, b, user_alpha });
        }
        else
        {
            line_color = ImGui::ColorConvertFloat4ToU32({
                static_cast<float>(e.color.x),
                static_cast<float>(e.color.y),
                static_cast<float>(e.color.z),
                static_cast<float>(e.color.w)
                });
        }

        if (features::visual::tracers::draw_hurt_time && e.hurt_time > 0)
        {
            float alpha = static_cast<float>(e.hurt_time) / 10.f * 0.8f;
            line_color = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.2f, 0.2f, alpha });
        }

        draw->AddLine(start_pos, { screen.x, screen.y }, line_color, features::visual::tracers::thickness);
    }
}
