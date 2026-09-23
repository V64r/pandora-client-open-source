#include "../features.hpp"
#include "backends/imgui.h"
#include "backends/imgui_internal.h"
#include <GL/gl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstring>
#include <atomic>
#include <mutex>
#include "../../helper/blur/gl_blur.hpp"
#include "../../../back/misc/imgui/fonts/font_manager.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "../../../../interface/w_imgui_port/render/stb/stb_image.hh"

#include <GL/gl.h>

extern bool g_AltManagerMode;

extern std::string g_CachedPlayerName;
extern std::string g_CachedServerIP;
extern std::mutex g_CachedIdentityMutex;
extern bool  gui_arraylist_enabled;
extern bool  gui_watermark_enabled;
extern float gui_watermark_pos_x;
extern float gui_watermark_pos_y;
extern bool  gui_watermark_show_player;
extern bool  gui_watermark_show_server;
extern bool  gui_watermark_show_fps;
extern bool  gui_watermark_show_name;
extern bool  gui_watermark_show_time;
extern bool  gui_watermark_background;
extern bool  gui_watermark_background_shadow;
extern bool  gui_watermark_split_background;
extern bool  gui_watermark_blur;
extern float gui_watermark_blur_opacity;
extern float gui_watermark_color[3];
extern float gui_watermark_color_b[3];
extern int   gui_watermark_color_mode;
extern float gui_arraylist_color_b[3];
extern float gui_arraylist_color_c[3];
extern float gui_arraylist_color_space;
extern float gui_arraylist_bar_width;
extern bool  gui_arraylist_bracket_flags;
namespace font { extern ImFont* default_icon; }
extern bool  gui_watermark_text_shadow;
extern bool  gui_noslow_enabled;
extern bool  gui_nojumpdelay_enabled;
extern bool  gui_noitemrelease_enabled;
extern bool  gui_sprint_enabled;
extern float gui_tracers_thickness;
extern float gui_blink_timer_limit;

extern float gui_arraylist_scale;
extern float gui_arraylist_speed;
extern float gui_arraylist_pos_x;
extern float gui_arraylist_pos_y;
extern float gui_arraylist_pad_x;
extern float gui_arraylist_pad_y;
extern float gui_arraylist_radius;
extern bool  gui_arraylist_background;
extern bool  gui_arraylist_colorbar;
extern bool  gui_arraylist_watermark;
extern float gui_arraylist_color[3];
extern float gui_arraylist_info_color[3];
extern int   gui_arraylist_color_mode;

extern bool gui_arraylist_show_info;
extern float gui_arraylist_bg_color_4[4];
extern bool  gui_arraylist_lowercase;
extern bool  gui_arraylist_shadows;
extern bool  gui_arraylist_background_shadow;
extern float gui_arraylist_shadow_strength;

extern bool  gui_arraylist_blur;
extern float gui_arraylist_blur_opacity;
extern std::atomic<bool> g_MenuVisible;
extern std::atomic<bool> g_PlayerInGui;
extern std::atomic<bool> g_MinecraftWorldLoaded;
extern float g_AccentColor[4];

#include <set>
extern std::set<std::string> g_arraylist_hidden_modules;

extern bool  gui_nohitdelay_enabled;
extern bool  gui_blockhit_enabled;
extern bool  gui_autoarmor_enabled;
extern bool  gui_macros_enabled;
extern int   gui_macros_mode;
extern bool  gui_armorswitcher_enabled;
extern int   gui_armorswitcher_kit1;
extern int   gui_armorswitcher_kit2;
extern bool  gui_fastplace_enabled;
extern bool  gui_autotool_enabled;
extern bool  gui_esp_enabled;
extern int   gui_esp_mode;
extern int   gui_esp_draw_mode;
extern bool  gui_friends_enabled;
extern bool gui_config_just_loaded;
extern int   gui_velo_mode;

extern float g_system_toast_height;

namespace features::visual::arraylist
{
    static std::string NormalizeModuleName(const char* name)
    {
        std::string normalized;
        if (!name) return normalized;
        for (const unsigned char ch : std::string(name)) {
            if (std::isalnum(ch))
                normalized.push_back((char)std::tolower(ch));
        }
        return normalized;
    }

    static bool IsModuleHidden(const char* name)
    {
        const std::string target = NormalizeModuleName(name);
        for (const std::string& hidden : g_arraylist_hidden_modules) {
            if (NormalizeModuleName(hidden.c_str()) == target)
                return true;
        }
        return false;
    }

    struct __feature_data
    {
        const char* name;
        bool* is_enabled;
        float       animation_progress = 0.f;
        float cascade_progress = 0.f;
        float wave_phase = 0.f;
        bool  was_enabled = false;
        float cascade_reset = 0.f;
        std::function<std::string()> get_value;
    };

    static std::vector<__feature_data> feature_data_array = {
        { "Right Clicker", &features::misc::right_clicker::enabled, 0.f,0.f,0.f,false,0.f, nullptr },
        { "Bridge Assist", &features::misc::bridge_assist::enabled, 0.f,0.f,0.f,false,0.f, nullptr },
        { "Timer Speed", &features::movement::timer_speed::enabled, 0.f,0.f,0.f,false,0.f, nullptr },
        { "Bunny Hop", &features::movement::bunnyhop::enabled, 0.f,0.f,0.f,false,0.f, nullptr },
        { "Bard Helper", &features::misc::bard_helper::enabled, 0.f,0.f,0.f,false,0.f, nullptr },
        // COMBAT
        { "Aim Assist",    &features::combat::aim_assist::enabled,    0.f,0.f,0.f,false,0.f, []() {
            return features::combat::aim_assist::mode == 1
                ? std::string("Lock On") : std::string("Regular");
        } },
        { "Left Clicker",  &features::combat::auto_click::enabled,    0.f,0.f,0.f,false,0.f, []() {
            extern float gui_max_cps;
            extern int gui_ac_click_method;
            static const char* methods[] = { "Smart", "Jitter", "Butterfly" };
            const int method = std::clamp(gui_ac_click_method, 0, 2);
            char b[64];
            sprintf_s(b, "%s %.1f", methods[method], gui_max_cps);
            return std::string(b);
        } },
        { "Reach",         &features::combat::reach::enabled,         0.f,0.f,0.f,false,0.f, []() {
            extern float gui_reach_min_distance;
            extern float gui_reach_max_distance;
            char b[32]; sprintf_s(b, "%.1f-%.1f", gui_reach_max_distance, gui_reach_min_distance); return std::string(b);
        }},
        { "Velocity",      &features::combat::velocity::enabled,      0.f,0.f,0.f,false,0.f, []() {
            extern float gui_velo_horizontal;
            char b[16]; sprintf_s(b, "%.0f%%", gui_velo_horizontal); return std::string(b);
        }},
        { "Refill",        &features::combat::refill::enabled,        0.f,0.f,0.f,false,0.f, nullptr },
        { "Block Hit",      &gui_blockhit_enabled,                     0.f,0.f,0.f,false,0.f, []() {
            extern int gui_blockhit_mode;
            static const char* modes[] = {"Manual", "Predict", "Auto", "Lag"};
            int m = (gui_blockhit_mode >= 0 && gui_blockhit_mode < 4) ? gui_blockhit_mode : 0;
            return std::string(modes[m]);
        }},
        { "Outline", &features::visual::outline::enabled, 0.f,0.f,0.f,false,0.f, nullptr },
        { "No Hit Delay",   &gui_nohitdelay_enabled,                    0.f,0.f,0.f,false,0.f, nullptr },
        { "Sprint",        &gui_sprint_enabled,                       0.f,0.f,0.f,false,0.f, nullptr },
        { "NoSlowdown",    &gui_noslow_enabled,                       0.f,0.f,0.f,false,0.f, nullptr },
        { "No Jump Delay",   &gui_nojumpdelay_enabled,                  0.f,0.f,0.f,false,0.f, nullptr },
        { "No Item Release", &gui_noitemrelease_enabled,                 0.f,0.f,0.f,false,0.f, nullptr },
        { "Snap Tap",      &features::movement::snap_tap::enabled,   0.f,0.f,0.f,false,0.f, nullptr },
        { "Instant Stop",      &features::movement::instant_stop::enabled,   0.f,0.f,0.f,false,0.f, []() {
            char b[16]; sprintf_s(b, "%.0f%%", features::movement::instant_stop::stop_strength * 100.0f); return std::string(b);
        }},
        { "Fast Place",     &gui_fastplace_enabled,                    0.f,0.f,0.f,false,0.f, nullptr },
        { "ESP",           &gui_esp_enabled,                          0.f,0.f,0.f,false,0.f, []() {
            static const char* dimensions[] = { "2D", "3D" };
            const int dimension = (gui_esp_mode >= 0 && gui_esp_mode < 2) ? gui_esp_mode : 0;
            return std::string(dimensions[dimension]);
        }},
        { "Tracers",       &features::visual::tracers::enabled,       0.f,0.f,0.f,false,0.f, []() {
            char b[16]; sprintf_s(b, "%.1fpx", gui_tracers_thickness); return std::string(b);
        }},
        { "Nametags",      &features::visual::nametags::enabled,      0.f,0.f,0.f,false,0.f, nullptr },
        { "Hit Markers",    &features::visual::hit_markers::enabled,   0.f,0.f,0.f,false,0.f, []() {
            return features::visual::hit_markers::mode == 0 ? std::string("2D") : std::string("3D");
        }},
        // MISC
        { "Blink",         &features::latency::blink::enabled,           0.f,0.f,0.f,false,0.f, []() {
            char b[16]; sprintf_s(b, "%.0fs", gui_blink_timer_limit); return std::string(b);
        }},
        // PLAYER
        { "AutoArmor",     &gui_autoarmor_enabled,                    0.f,0.f,0.f,false,0.f, nullptr },
        { "Macros",        &gui_macros_enabled,                       0.f,0.f,0.f,false,0.f, []() {
            static const char* modes[] = {"Bow", "Fireball", "Gap", "Pot"};
            int m = (gui_macros_mode >= 0 && gui_macros_mode < 4) ? gui_macros_mode : 0;
            return std::string(modes[m]);
        }},
        { "ArmorSwitcher", &gui_armorswitcher_enabled,                 0.f,0.f,0.f,false,0.f, []() {
            static const char* kits[] = {"Diamond", "Iron", "Gold", "Chain", "Leather"};
            int first = (gui_armorswitcher_kit1 >= 0 && gui_armorswitcher_kit1 < 5) ? gui_armorswitcher_kit1 : 0;
            int second = (gui_armorswitcher_kit2 >= 0 && gui_armorswitcher_kit2 < 5) ? gui_armorswitcher_kit2 : 1;
            return std::string(kits[first]) + " / " + kits[second];
        }},
        { "Friends",       &gui_friends_enabled,                      0.f,0.f,0.f,false,0.f, nullptr },
    };

    // ----------------------------------------------------------------
    // Toast system
    // ----------------------------------------------------------------
    struct Toast {
        std::string text = {};
        bool        enabled_state = false;
        int         keybind = 0;
        float       alpha = 0.f;
        float       slide = 0.f;
        float       life = 0.f;
        float       text_w = 0.f;
        float       slideY = 0.f;
    };

    static std::vector<Toast>    g_toasts;

    struct WatchEntry { bool* ptr; bool prev; const char* name; };
    static std::vector<WatchEntry> g_watch;
    static bool g_watch_init = false;

    static void render_toasts(ImDrawList* dl, ImFont* font, float sw, float sh, float dt);
    static void check_and_push(ImFont* font, float fsz);
    static void init_watch();
    void PushToastEvent(const std::string& txt, bool st);

    // ----------------------------------------------------------------
    // render_whip_watermark
    // ----------------------------------------------------------------
    static void render_whip_watermark(ImDrawList* dl, ImFont* font, float sw, float sh)
    {
        if (!gui_watermark_enabled) return;

        if (!font) {
            font = font_manager::FontManager::get().get_watermark_font();
            if (!font) font = ImGui::GetFont();
        }

        static int cachedFps = 0;
        static int fpsFrameCount = 0;
        static double fpsTimer = 0.0;
        double now = ImGui::GetTime();
        fpsFrameCount++;
        if (now - fpsTimer >= 1.0) {
            cachedFps = fpsFrameCount;
            fpsFrameCount = 0;
            fpsTimer = now;
        }

        if (gui_watermark_pos_x > 0.98f || gui_watermark_pos_x < 0.0f) gui_watermark_pos_x = 0.015f;
        if (gui_watermark_pos_y > 0.98f || gui_watermark_pos_y < 0.0f) gui_watermark_pos_y = 0.015f;

        const float s = 1.35f;
        float fontSz = 13.0f * s;
        float padX = 11.0f * s;
        float padY = 7.0f * s;
        float totalH = fontSz + (padY * 2.0f);
        float rounding = totalH * 0.5f;

        ImU32 cWhite = IM_COL32(244, 244, 247, 250);
        ImU32 cText = IM_COL32(194, 195, 204, 242);
        ImU32 cBar = IM_COL32(
            (int)(g_AccentColor[0] * 255.0f),
            (int)(g_AccentColor[1] * 255.0f),
            (int)(g_AccentColor[2] * 255.0f), 145);
        ImU32 cAccent = IM_COL32(
            (int)(g_AccentColor[0] * 255.0f),
            (int)(g_AccentColor[1] * 255.0f),
            (int)(g_AccentColor[2] * 255.0f), 250);

        struct Seg { std::string text; ImU32 color; bool is_icon = false; ImTextureID texture = nullptr; };
        std::vector<Seg> segs;
		segs.reserve(9);

        auto addInfo = [&](const std::string& text, ImU32 color, bool is_icon = false) {
            if (!segs.empty() && !is_icon && !gui_watermark_split_background) segs.push_back({ " | ", cWhite });
            segs.push_back({ text, color, is_icon });
            };

        if (gui_watermark_show_name) {
            segs.push_back({ "Swift", IM_COL32(
                (int)(gui_watermark_color[0] * 255.f),
                (int)(gui_watermark_color[1] * 255.f),
                (int)(gui_watermark_color[2] * 255.f), 250), false });
        }

        if (gui_watermark_show_fps) {
            char fpsBuf[32];
            snprintf(fpsBuf, sizeof(fpsBuf), "%d fps", cachedFps);
            addInfo(fpsBuf, cText);
        }
        if (gui_watermark_show_server) {
            std::string server;
            {
                std::lock_guard<std::mutex> lock(g_CachedIdentityMutex);
                server = g_CachedServerIP.empty() ? "Singleplayer" : g_CachedServerIP;
            }
            addInfo(server, cText);
        }
        if (gui_watermark_show_player) {
            std::string user;
            {
                std::lock_guard<std::mutex> lock(g_CachedIdentityMutex);
                user = g_CachedPlayerName.empty() ? "Player" : g_CachedPlayerName;
            }
            addInfo(user, cText);
        }
        if (gui_watermark_show_time) {
            time_t tNow = ::time(nullptr);
            tm tmNow;
            localtime_s(&tmNow, &tNow);
            char timeBuf[16];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
            addInfo(timeBuf, cText);
        }

        if (segs.empty()) {
            segs.push_back({ "Watermark", cAccent, false });
        }

        float totalTextW = 0.0f;
        for (const auto& sg : segs) {
            ImFont* f = sg.is_icon ? FONT_MANAGER.get_icon_font() : font;
            if (!f) f = font;
            totalTextW += f->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, sg.text.c_str()).x;
        }

        const bool split = gui_watermark_split_background && segs.size() > 1;
        const float splitGap = split ? 6.0f : 0.0f;
        std::vector<float> segmentWidths;
        segmentWidths.reserve(segs.size());
        for (const auto& sg : segs) {
            ImFont* f = sg.is_icon ? FONT_MANAGER.get_icon_font() : font;
            if (!f) f = font;
            segmentWidths.push_back(f->CalcTextSizeA(fontSz, FLT_MAX, 0.f, sg.text.c_str()).x);
        }
        float totalW = totalTextW + padX * 2.0f;
        if (split) {
            totalW = splitGap * (segs.size() - 1);
            for (float width : segmentWidths) totalW += width + padX * 2.0f;
        }

        float barX = gui_watermark_pos_x * sw;
        float barY = gui_watermark_pos_y * sh;

        barX = (std::max)(5.0f, (std::min)(barX, sw - totalW - 5.0f));
        barY = (std::max)(5.0f, (std::min)(barY, sh - totalH - 5.0f));

        if (g_MenuVisible && !g_AltManagerMode) {
            static bool   wmDragging = false;
            static ImVec2 wmDragOff;

            const ImVec2 mouse = ImGui::GetMousePos();
            const bool   down = ImGui::IsMouseDown(0);
            const bool   clicked = ImGui::IsMouseClicked(0);

            const ImVec2 editMin(barX - 4.0f, barY - 4.0f);
            const ImVec2 editMax(barX + totalW + 4.0f, barY + totalH + 4.0f);

            const bool inBox = mouse.x >= editMin.x && mouse.x <= editMax.x && mouse.y >= editMin.y && mouse.y <= editMax.y;
            if (clicked) {
                if (inBox) {
                    wmDragging = true;
                    wmDragOff = ImVec2(barX - mouse.x, barY - mouse.y);
                }
            }
            if (!down) {
                wmDragging = false;
            }
            static float wmDragAnim = 0.0f;
            wmDragAnim = ImLerp(wmDragAnim, wmDragging ? 1.0f : 0.0f,
                (std::min)(1.0f, ImGui::GetIO().DeltaTime * 15.0f));
            if (wmDragging && down) {
                float newX = mouse.x + wmDragOff.x;
                float newY = mouse.y + wmDragOff.y;
                float centerX = newX + totalW * 0.5f;
                float centerY = newY + totalH * 0.5f;
                const float snapPointsX[] = { sw * 0.3333f, sw * 0.5f, sw * 0.6666f };
                const float snapPointsY[] = { sh * 0.3333f, sh * 0.5f, sh * 0.6666f };
                float snappedX = -1.f, snappedY = -1.f;
                for (float point : snapPointsX) {
                    if (std::abs(centerX - point) < 15.f) {
                        centerX = snappedX = point;
                        newX = centerX - totalW * 0.5f;
                        break;
                    }
                }
                for (float point : snapPointsY) {
                    if (std::abs(centerY - point) < 15.f) {
                        centerY = snappedY = point;
                        newY = centerY - totalH * 0.5f;
                        break;
                    }
                }
                const float follow = 1.f - std::exp(-22.f * ImGui::GetIO().DeltaTime);
                barX = ImLerp(barX, newX, follow);
                barY = ImLerp(barY, newY, follow);
                gui_watermark_pos_x = barX / sw;
                gui_watermark_pos_y = barY / sh;
                for (float point : snapPointsX)
                    dl->AddLine({point, 0.f}, {point, sh}, IM_COL32(255, 255, 255,
                        (int)((point == snappedX ? 255.f : 80.f) * wmDragAnim)), point == snappedX ? 2.f : 1.f);
                for (float point : snapPointsY)
                    dl->AddLine({0.f, point}, {sw, point}, IM_COL32(255, 255, 255,
                        (int)((point == snappedY ? 255.f : 80.f) * wmDragAnim)), point == snappedY ? 2.f : 1.f);
            }

        }

        ImVec2 bMin(barX, barY);
        ImVec2 bMax(barX + totalW, barY + totalH);
        if (gui_watermark_background && gui_watermark_blur)
            GLBlur::AddBlurPass(dl, 5.5f);
        if (gui_watermark_background) {
            float bgAlpha = (gui_watermark_blur ? 0.42f : 0.88f);
            auto capsule = [&](ImVec2 mn, ImVec2 mx) {
                if (gui_watermark_background_shadow) {
                    for (int layer = 5; layer >= 1; --layer) {
                        const float spread = layer * 1.6f;
                        dl->AddRectFilled({mn.x-spread,mn.y-spread},{mx.x+spread,mx.y+spread},
                            IM_COL32(0,0,0,5 + (5-layer)*3), rounding + spread);
                    }
                }
                if (gui_watermark_blur)
                    GLBlur::DrawBlurredImage(dl, mn, mx, std::clamp(gui_watermark_blur_opacity, 0.0f, 1.0f), rounding);
                dl->AddRectFilled(mn, mx, IM_COL32(20,20,20,(int)(255*bgAlpha)), rounding);
            };
            if (split) {
                float capsuleX = bMin.x;
                for (float width : segmentWidths) {
                    const float capsuleW = width + padX * 2.0f;
                    capsule({capsuleX, bMin.y}, {capsuleX + capsuleW, bMax.y});
                    capsuleX += capsuleW + splitGap;
                }
            } else capsule(bMin, bMax);
        }

        float cx = barX + padX;
        float cy = barY + (totalH - fontSz) * 0.5f;

        size_t segment_index = 0;
        for (const auto& sg : segs) {
            ImFont* f = sg.is_icon && font::default_icon ? font::default_icon : font;

            if (gui_watermark_text_shadow)
                dl->AddText(f, fontSz, ImVec2(cx + 1.0f, cy + 1.0f), IM_COL32(0, 0, 0, 180), sg.text.c_str());

            if (gui_watermark_color_mode == 1 && sg.text == "Swift") {
                const float segment_width = f->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, sg.text.c_str()).x;
                float glyph_x = cx;
                for (size_t i = 0; i < sg.text.size(); ++i) {
                    const char glyph[2] = { sg.text[i], '\0' };
                    const float glyph_width = f->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, glyph).x;
                    const float position = segment_width > 0.0f ?
                        (glyph_x - cx + glyph_width * 0.5f) / segment_width : 0.0f;
                    float mix = 0.5f + 0.5f * sinf((position + (float)ImGui::GetTime() * 0.55f) * 6.2831853f);
                    mix = mix * mix * (3.0f - 2.0f * mix);
                    const float r = ImLerp(gui_watermark_color[0], gui_watermark_color_b[0], mix);
                    const float g = ImLerp(gui_watermark_color[1], gui_watermark_color_b[1], mix);
                    const float b = ImLerp(gui_watermark_color[2], gui_watermark_color_b[2], mix);
                    dl->AddText(f, fontSz, ImVec2(glyph_x, cy), IM_COL32((int)(r * 255.f), (int)(g * 255.f), (int)(b * 255.f), 250), glyph);
                    glyph_x += glyph_width;
                }
            } else {
                dl->AddText(f, fontSz, ImVec2(cx, cy), sg.color, sg.text.c_str());
            }

            cx += f->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, sg.text.c_str()).x;
            if (split && segment_index + 1 < segs.size()) cx += splitGap + padX * 2.0f;
            ++segment_index;
        }

        if (g_MenuVisible && !g_AltManagerMode) {
            const char* hint = "hold to move";
            ImFont* hintFont = FONT_MANAGER.get_arraylist_font();
            if (!hintFont) hintFont = font;
            const float hintSize = 14.5f * s;
            ImVec2 hintExtent = hintFont->CalcTextSizeA(
                hintSize, FLT_MAX, 0.0f, hint);
            ImVec2 hintPos(
                bMin.x + (totalW - hintExtent.x) * 0.5f,
                bMax.y + 6.0f * s);
            dl->AddText(
                hintFont, hintSize, ImVec2(hintPos.x + 1.0f, hintPos.y + 1.0f),
                IM_COL32(0, 0, 0, 220), hint);
            dl->AddText(
                hintFont, hintSize, hintPos,
                IM_COL32(235, 235, 239, 245), hint);
        }
    }

    // ----------------------------------------------------------------
    // ----------------------------------------------------------------
    void run()
    {
        GLBlur::Init();
        float dt = ImGui::GetIO().DeltaTime;
        float elapsed = dt * 1000.f;
        float time = (float)ImGui::GetTime();

        __int32 vp[4] = {};
        glGetIntegerv(GL_VIEWPORT, vp);
        float sw = (float)vp[2];
        float sh = (float)vp[3];

        // Notifications and Watermark
        ImFont* notifFont = FONT_MANAGER.get_watermark_font();
        if (!notifFont) notifFont = ImGui::GetFont();

        // Arraylist uses the original Montserrat SemiBold HUD font.
        ImFont* arrayFont = FONT_MANAGER.get_arraylist_font();
        if (!arrayFont) arrayFont = ImGui::GetFont();

        // Front-end/title and multiplayer screens have no loaded world.
        const bool showHud = g_MinecraftWorldLoaded.load(std::memory_order_acquire) ||
            g_MenuVisible;

        if (showHud)
            render_whip_watermark(ImGui::GetBackgroundDrawList(), notifFont, sw, sh);

        if (features::misc::notifications::enabled)
            render_toasts(ImGui::GetBackgroundDrawList(), notifFont, sw, sh, dt);
        else {
            if (g_watch_init)
                for (auto& w : g_watch) w.prev = *w.ptr;
            g_toasts.clear();
        }

        if (!showHud) return;

        enabled = ::gui_arraylist_enabled;
        if (!enabled) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        if (gui_arraylist_scale < 0.4f || gui_arraylist_scale > 3.5f) gui_arraylist_scale = 1.0f;

        float scale = gui_arraylist_scale;
        float fontSize = 20.0f * scale;
        float padX = gui_arraylist_pad_x * scale;
        gui_arraylist_pad_y = std::clamp(gui_arraylist_pad_y, 0.f, 5.f);
        float padY = gui_arraylist_pad_y * scale;
        const float horizontalSafety = 10.0f * scale;
        float barW = gui_arraylist_bar_width * scale;
        float rounding = gui_arraylist_radius * scale;
        float waveSpeed = gui_arraylist_speed;

        float spaceW = arrayFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, " ").x;
        float lerpSpeed = (std::min)(7.0f * dt, 1.0f);

        // Module color and secondary color (from settings)
        ImVec4 modColorA(gui_arraylist_color[0], gui_arraylist_color[1], gui_arraylist_color[2], 1.0f);
        ImVec4 modColorB(gui_arraylist_color_b[0], gui_arraylist_color_b[1], gui_arraylist_color_b[2], 1.0f);
        const auto gradient_color = [&](float y, float alpha) {
            const ImVec4 colors[]{modColorA, modColorB, ImVec4(gui_arraylist_color_c[0], gui_arraylist_color_c[1], gui_arraylist_color_c[2], 1.f)};
            const float spacing = std::isfinite(gui_arraylist_color_space) ? std::clamp(gui_arraylist_color_space, 20.f, 300.f) : 100.f;
            float phase = std::fmod(y / spacing + static_cast<float>(ImGui::GetTime()) * waveSpeed, 3.f);
            if (phase < 0.f) phase += 3.f;
            const int index = static_cast<int>(phase);
            float blend = phase - index;
            blend = blend * blend * (3.f - 2.f * blend);
            const auto& a = colors[index]; const auto& b = colors[(index + 1) % 3];
            return ImVec4(ImLerp(a.x,b.x,blend), ImLerp(a.y,b.y,blend), ImLerp(a.z,b.z,blend), alpha);
        };

        // Anchor: draggable position (stored as normalized 0-1)
        static bool alDragging = false;
        static ImVec2 alDragOff;

        float anchorX = gui_arraylist_pos_x * sw;
        float startY = gui_arraylist_pos_y * sh;

        anchorX = (std::max)(0.0f, (std::min)(anchorX, sw));
        startY = (std::max)(0.0f, (std::min)(startY, sh));

        // Auto-flip: if anchor is on left half, render left-aligned
        bool isLeftSide = (gui_arraylist_pos_x < 0.5f);

        struct ItemToDraw {
            std::string name;
            std::string flags;
            float totalW;
            float totalH;
            ImVec2 nameSz;
            float inkTop;
            float inkHeight;
            float anim;
            int index;
        };
        std::vector<ItemToDraw> activeList;
		activeList.reserve(feature_data_array.size());

        for (int i = 0; i < (int)feature_data_array.size(); i++) {
            __feature_data& fd = feature_data_array[i];
            const bool hiddenByCurrentName = IsModuleHidden(fd.name);
            const bool hiddenByLegacyClickerName = strcmp(fd.name, "Left Clicker") == 0
                && g_arraylist_hidden_modules.count("Auto Clicker") != 0;
            const bool hiddenByLegacyName = strcmp(fd.name, "NoSlowdown") == 0
                && g_arraylist_hidden_modules.count("NoSlow") != 0;
            bool st = (fd.is_enabled && *fd.is_enabled) && !hiddenByCurrentName
                && !hiddenByLegacyClickerName && !hiddenByLegacyName;
            float targetAnim = st ? 1.0f : 0.0f;

            fd.animation_progress += (targetAnim - fd.animation_progress) * lerpSpeed;
            if (st && fd.animation_progress > 0.995f)
                fd.animation_progress = 1.0f;
            if (fd.animation_progress < 0.01f && !st) { fd.animation_progress = 0.0f; continue; }

            std::string nameStr = fd.name;
            std::string flagStr = "";
            if (gui_arraylist_show_info && fd.get_value) {
                flagStr = fd.get_value();
                if (!flagStr.empty() && gui_arraylist_bracket_flags) {
                    flagStr = "[" + flagStr + "]";
                }
            }
            const bool dimensional_flag =
                (flagStr == "2D" || flagStr == "3D" || flagStr == "[2D]" || flagStr == "[3D]");
            if (gui_arraylist_lowercase) {
                for (auto& c : nameStr) c = (char)std::tolower((unsigned char)c);
                if (!dimensional_flag)
                    for (auto& c : flagStr) c = (char)std::tolower((unsigned char)c);
            }

            ImVec2 nSz = arrayFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, nameStr.c_str());
            ImVec2 fSz = flagStr.empty() ? ImVec2(0, 0) : arrayFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, flagStr.c_str());
            const float textHeight = (std::max)(nSz.y, fSz.y);
            float inkTop = 0.f, inkBottom = textHeight;
            const float glyphScale = fontSize / arrayFont->FontSize;
            auto include_ink = [&](const std::string& text) {
                for (unsigned char ch : text) {
                    if (const ImFontGlyph* glyph = arrayFont->FindGlyph(ch)) {
                        inkTop = (std::min)(inkTop, glyph->Y0 * glyphScale);
                        inkBottom = (std::max)(inkBottom, glyph->Y1 * glyphScale);
                    }
                }
            };
            include_ink(nameStr);
            include_ink(flagStr);
            const float inkHeight = inkBottom - inkTop;
            const float rowSafety = 4.0f * scale;
            float tW = nSz.x + (flagStr.empty() ? 0.0f : (spaceW + fSz.x)) +
                (padX * 2.0f) + (gui_arraylist_colorbar ? barW : 0.0f) +
                (gui_arraylist_shadows ? 1.0f : 0.0f) + horizontalSafety;
            const float tH = std::ceil(inkHeight) + (padY * 2.0f) + rowSafety;

            const float animation = fd.animation_progress * fd.animation_progress * (3.f - 2.f * fd.animation_progress);
            activeList.push_back({ nameStr, flagStr, std::ceil(tW), std::ceil(tH), nSz, inkTop, inkHeight, animation, i });
        }

        std::stable_sort(activeList.begin(), activeList.end(), [](const ItemToDraw& a, const ItemToDraw& b) {
            if (std::abs(a.totalW - b.totalW) > 0.01f)
                return a.totalW > b.totalW;
            return a.index < b.index;
        });

        // Nearly equal widths share an edge instead of producing one-pixel
        // ledges with a radius larger than the available step.
        for (size_t i = 1; i < activeList.size(); ++i)
            if (activeList[i - 1].totalW - activeList[i].totalW < std::max(rounding, 2.f))
                activeList[i].totalW = activeList[i - 1].totalW;

        auto row_radius = [&](int i) {
            if (i + 1 == static_cast<int>(activeList.size())) return rounding;
            return std::min(rounding, std::max(0.f,
                activeList[i].totalW - activeList[i + 1].totalW));
        };
        float currentY = startY;

        auto get_corners = [&](int i) -> ImDrawFlags {
            ImDrawFlags c = 0;
            if (isLeftSide) {
                if (row_radius(i) > 0.f) c |= ImDrawFlags_RoundCornersBottomRight;
                if (i == 0) c |= ImDrawFlags_RoundCornersTopRight;
            } else {
                if (row_radius(i) > 0.f) c |= ImDrawFlags_RoundCornersBottomLeft;
                if (i == 0) c |= ImDrawFlags_RoundCornersTopLeft;
            }
            return c == 0 ? ImDrawFlags_RoundCornersNone : c;
        };

        // ============================================================
        // ============================================================
        if (gui_arraylist_background && gui_arraylist_blur) {
            GLBlur::AddBlurPass(dl, 5.5f);
        }

        // ============================================================
        // ============================================================
        // Rasterize the union of expanded row silhouettes once per layer.
        // Shared corners must not accumulate alpha from neighbouring rows.
        if (gui_arraylist_background && gui_arraylist_background_shadow) {
            struct ShadowBox { float left, top, right, bottom, radius, alpha; };
            std::vector<ShadowBox> boxes;
            currentY = startY;
            float bottom = startY;
            for (int i = 0; i < static_cast<int>(activeList.size()); ++i) {
                const auto& item = activeList[i];
                if (item.anim < 0.01f) continue;
                const float anchor = std::round(anchorX);
                const float left = std::round(isLeftSide
                    ? anchor - (1.f - item.anim) * (item.totalW + 10.f)
                    : anchor - item.totalW + (1.f - item.anim) * (item.totalW + 10.f));
                bottom = std::round(currentY + item.totalH);
                boxes.push_back({left, std::round(currentY), isLeftSide ? left + item.totalW : anchor,
                    bottom, row_radius(i), item.anim});
                currentY += item.totalH * item.anim;
            }
            const float strength = ImClamp(gui_arraylist_shadow_strength, 0.f, 100.f) * 0.01f;
            if (strength > 0.f) {
                const float thickness = (5.f + 11.f * strength) * scale;
                for (const auto& box : boxes) {
                    constexpr int layers = 7;
                    for (int layer = layers; layer >= 1; --layer) {
                        const float t = static_cast<float>(layer) / layers;
                        const float spread = thickness * t;
                        const float falloff = 1.f - t;
                        const int alpha = static_cast<int>(100.f * strength * box.alpha * (0.18f + falloff * falloff));
                        dl->AddRectFilled({box.left - spread, box.top - spread},
                            {box.right + spread, box.bottom + spread}, IM_COL32(0, 0, 0, alpha),
                            box.radius + spread);
                    }
                }
            }
        }

        // Render backgrounds after the black Whip-style shadow pass.
        if (gui_arraylist_background) {
            currentY = startY;
            for (int i = 0; i < (int)activeList.size(); i++) {
                ItemToDraw& item = activeList[i];
                float anim = item.anim;
                if (anim < 0.01f) continue;

                float itemH = item.totalH;
                float itemW = item.totalW;

                const float alignedAnchorX = std::round(anchorX);
                float boxX;
                if (isLeftSide) {
                    boxX = alignedAnchorX - (1.0f - anim) * (itemW + 10.0f);
                }
                else {
                    boxX = alignedAnchorX - itemW + (1.0f - anim) * (itemW + 10.0f);
                }
                boxX = std::round(boxX);
                float boxY = std::round(currentY);

                ImVec2 iMin(boxX, boxY);
                ImVec2 iMax(boxX + itemW, boxY + itemH);
                if (!isLeftSide)
                    iMax.x = alignedAnchorX;
                if (anim >= 0.995f)
                    iMax.y = std::round(currentY + itemH);

                // ============================================================
                // ============================================================
                ImDrawFlags corners = get_corners(i);

                // Draw the pre-calculated blur texture for this item's area
                if (gui_arraylist_blur) {
                    GLBlur::DrawBlurredImage(dl, iMin, iMax,
                        anim * (std::max)(0.0f, (std::min)(gui_arraylist_blur_opacity, 1.0f)),
                        row_radius(i), corners);
                }

                // Semi-transparent tinted overlay on top of the blur
                ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    gui_arraylist_bg_color_4[0],
                    gui_arraylist_bg_color_4[1],
                    gui_arraylist_bg_color_4[2],
                    gui_arraylist_bg_color_4[3] * anim));
                dl->AddRectFilled(iMin, iMax, bgCol, row_radius(i), corners);

                currentY += itemH * anim;
            }
        }

        // ============================================================
        // ============================================================
        if (gui_arraylist_colorbar) {
            currentY = startY;
            for (int i = 0; i < (int)activeList.size(); i++) {
                ItemToDraw& item = activeList[i];
                float anim = item.anim;
                if (anim < 0.01f) {
                    currentY += item.totalH * anim;
                    continue;
                }

                float itemH = item.totalH;
                float itemW = item.totalW;

                const float alignedAnchorX = std::round(anchorX);
                float boxX;
                if (isLeftSide) {
                    boxX = alignedAnchorX - (1.0f - anim) * (itemW + 10.0f);
                }
                else {
                    boxX = alignedAnchorX - itemW + (1.0f - anim) * (itemW + 10.0f);
                }
                boxX = std::round(boxX);
                float boxY = std::round(currentY);

                ImVec2 iMin(boxX, boxY);
                ImVec2 iMax(boxX + itemW, boxY + itemH);
                if (!isLeftSide)
                    iMax.x = alignedAnchorX;
                if (anim >= 0.995f)
                    iMax.y = std::round(currentY + itemH);

                // Wave color based on Mode
                ImVec4 waveColor;
                int waveDelay = -(i * 12);
                if (gui_arraylist_color_mode == 0) {
                    waveColor = modColorA;
                    waveColor.w = anim;
                }
                else if (gui_arraylist_color_mode == 1) {
                    float hue = fmodf(time * waveSpeed + (float)waveDelay * 0.01f, 1.0f);
                    float r, g, b;
                    ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.9f, r, g, b);
                    waveColor = ImVec4(r, g, b, anim);
                }
                else if (gui_arraylist_color_mode == 2) {
                    waveColor = gradient_color(currentY - startY, anim);
                }
                else if (gui_arraylist_color_mode == 3) {
                    float t = (std::sin(time * waveSpeed * 4.0f + (float)i * 0.45f) + 1.0f) * 0.5f;
                    waveColor = ImVec4(
                        modColorA.x + (modColorB.x - modColorA.x) * t,
                        modColorA.y + (modColorB.y - modColorA.y) * t,
                        modColorA.z + (modColorB.z - modColorA.z) * t,
                        anim);
                }
                else {
                    float t = (std::sin(time * 3.0f - (float)i * 0.35f) + 1.0f) * 0.5f;
                    float factor = 0.80f + 0.20f * t;
                    waveColor = ImVec4(
                        g_AccentColor[0] * factor,
                        g_AccentColor[1] * factor,
                        g_AccentColor[2] * factor,
                        anim);
                }
                ImU32 itemCol = ImGui::ColorConvertFloat4ToU32(waveColor);

                ImDrawFlags corners = get_corners(i);
                if (isLeftSide) {
                    ImVec2 bMin(iMin.x, iMin.y);
                    ImVec2 bMax(iMin.x + barW, iMax.y);
                    ImDrawFlags cbCorners = 0;
                    if (corners & ImDrawFlags_RoundCornersTopLeft) cbCorners |= ImDrawFlags_RoundCornersTopLeft;
                    if (corners & ImDrawFlags_RoundCornersBottomLeft) cbCorners |= ImDrawFlags_RoundCornersBottomLeft;
                    dl->AddRectFilled(bMin, bMax, itemCol, rounding, cbCorners == 0 ? ImDrawFlags_RoundCornersNone : cbCorners);
                }
                else {
                    ImVec2 bMin(iMax.x - barW, iMin.y);
                    ImVec2 bMax(iMax.x, iMax.y);
                    ImDrawFlags cbCorners = 0;
                    if (corners & ImDrawFlags_RoundCornersTopRight) cbCorners |= ImDrawFlags_RoundCornersTopRight;
                    if (corners & ImDrawFlags_RoundCornersBottomRight) cbCorners |= ImDrawFlags_RoundCornersBottomRight;
                    dl->AddRectFilled(bMin, bMax, itemCol, rounding, cbCorners == 0 ? ImDrawFlags_RoundCornersNone : cbCorners);
                }

                currentY += itemH * anim;
            }
        }

        // ============================================================
        // ============================================================
        currentY = startY;
        for (int i = 0; i < (int)activeList.size(); i++) {
            ItemToDraw& item = activeList[i];
            float anim = item.anim;
            if (anim < 0.01f) continue;

            float itemH = item.totalH;
            float itemW = item.totalW;

            const float alignedAnchorX = std::round(anchorX);
            float boxX;
            if (isLeftSide) {
                boxX = alignedAnchorX - (1.0f - anim) * (itemW + 10.0f);
            }
            else {
                boxX = alignedAnchorX - itemW + (1.0f - anim) * (itemW + 10.0f);
            }
            boxX = std::round(boxX);
            float boxY = std::round(currentY);

            ImVec2 iMin(boxX, boxY);
            ImVec2 iMax(isLeftSide ? boxX + itemW : alignedAnchorX, std::round(currentY + itemH));

            // Re-calculate waveColor for text
            ImVec4 waveColor;
            int waveDelay = -(i * 12);
            if (gui_arraylist_color_mode == 0) {
                waveColor = modColorA; waveColor.w = anim;
            }
            else if (gui_arraylist_color_mode == 1) {
                float hue = fmodf(time * waveSpeed + (float)waveDelay * 0.01f, 1.0f);
                float r, g, b; ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.9f, r, g, b);
                waveColor = ImVec4(r, g, b, anim);
            }
            else if (gui_arraylist_color_mode == 2) {
                    waveColor = gradient_color(currentY - startY, anim);
                }
            else if (gui_arraylist_color_mode == 3) {
                float t = (std::sin(time * waveSpeed * 4.0f + (float)i * 0.45f) + 1.0f) * 0.5f;
                waveColor = ImVec4(modColorA.x + (modColorB.x - modColorA.x) * t,
                    modColorA.y + (modColorB.y - modColorA.y) * t,
                    modColorA.z + (modColorB.z - modColorA.z) * t, anim);
            }
            else {
                float t = (std::sin(time * 3.0f - (float)i * 0.35f) + 1.0f) * 0.5f;
                float factor = 0.80f + 0.20f * t;
                waveColor = ImVec4(g_AccentColor[0] * factor,
                    g_AccentColor[1] * factor,
                    g_AccentColor[2] * factor, anim);
            }
            ImU32 itemCol = ImGui::ColorConvertFloat4ToU32(waveColor);

            // Text position
            float tx = std::round(iMin.x + padX + horizontalSafety * 0.5f +
                (isLeftSide && gui_arraylist_colorbar ? barW : 0.0f));
            float ty = std::round(boxY + (itemH - item.inkHeight) * 0.5f - item.inkTop);

            // Text and its shadow must never escape the row background. The row
            // height already includes descender safety, so extending this clip
            // produced visible glyphs outside rounded bottom edges.
            dl->PushClipRect(iMin, iMax, true);
            // Module name with text shadow
            if (gui_arraylist_shadows) {
                dl->AddText(arrayFont, fontSize, ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, (int)(220 * anim)), item.name.c_str());
            }
            dl->AddText(arrayFont, fontSize, ImVec2(tx, ty), itemCol, item.name.c_str());

            // Flag text in dimmer color
            if (!item.flags.empty()) {
                float fx = std::round(tx + item.nameSz.x + spaceW);
                float flagAlpha = 1.f * anim;
                ImU32 flagCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    gui_arraylist_info_color[0],
                    gui_arraylist_info_color[1],
                    gui_arraylist_info_color[2],
                    flagAlpha));
                if (gui_arraylist_shadows) {
                    dl->AddText(arrayFont, fontSize, ImVec2(fx + 1, ty + 1), IM_COL32(0, 0, 0, (int)(180 * anim)), item.flags.c_str());
                }
                dl->AddText(arrayFont, fontSize, ImVec2(fx, ty), flagCol, item.flags.c_str());
            }
            dl->PopClipRect();

            currentY += itemH * anim;
        }

        // --- hold to move (only when menu is open) ---
        if (g_MenuVisible && !g_AltManagerMode && !activeList.empty()) {
            float listTop = gui_arraylist_pos_y * sh;
            float listW = activeList[0].totalW;
            float listH = currentY - listTop;
            float listLeft, listRight;
            if (isLeftSide) {
                listLeft = anchorX;
                listRight = anchorX + listW;
            }
            else {
                listRight = anchorX;
                listLeft = anchorX - listW;
            }

            ImVec2 dragMin(listLeft - 8.0f, listTop - 8.0f);
            ImVec2 dragMax(listRight + 8.0f, listTop + listH + 8.0f);

            const ImVec2 mouse = ImGui::GetMousePos();
            const bool down = ImGui::IsMouseDown(0);
            const bool clicked = ImGui::IsMouseClicked(0);
            const bool inBox = mouse.x >= dragMin.x && mouse.x <= dragMax.x && mouse.y >= dragMin.y && mouse.y <= dragMax.y;

            if (clicked && inBox) {
                alDragging = true;
                alDragOff = ImVec2(anchorX - mouse.x, listTop - mouse.y);
            }
            if (!down) alDragging = false;

            static float alDragAnim = 0.0f;
            alDragAnim = ImLerp(alDragAnim, alDragging ? 1.0f : 0.0f,
                (std::min)(1.0f, ImGui::GetIO().DeltaTime * 15.0f));
            if (alDragging && down) {
                float newX = mouse.x + alDragOff.x;
                float newY = mouse.y + alDragOff.y;
                float centerX = isLeftSide ? newX + listW * 0.5f : newX - listW * 0.5f;
                float centerY = newY + listH * 0.5f;
                const float snapPointsX[] = { sw * 0.3333f, sw * 0.5f, sw * 0.6666f };
                const float snapPointsY[] = { sh * 0.3333f, sh * 0.5f, sh * 0.6666f };
                float snappedX = -1.f, snappedY = -1.f;
                for (float point : snapPointsX) {
                    if (std::abs(centerX - point) < 15.f) {
                        centerX = snappedX = point;
                        newX = isLeftSide ? centerX - listW * 0.5f : centerX + listW * 0.5f;
                        break;
                    }
                }
                for (float point : snapPointsY) {
                    if (std::abs(centerY - point) < 15.f) {
                        centerY = snappedY = point;
                        newY = centerY - listH * 0.5f;
                        break;
                    }
                }
                const float follow = 1.f - std::exp(-22.f * ImGui::GetIO().DeltaTime);
                gui_arraylist_pos_x = ImLerp(anchorX, newX, follow) / sw;
                gui_arraylist_pos_y = ImLerp(listTop, newY, follow) / sh;
                gui_arraylist_pos_x = (std::max)(0.0f, (std::min)(gui_arraylist_pos_x, 1.0f));
                gui_arraylist_pos_y = (std::max)(0.0f, (std::min)(gui_arraylist_pos_y, 0.95f));
                for (float point : snapPointsX)
                    dl->AddLine({point, 0.f}, {point, sh}, IM_COL32(255, 255, 255,
                        (int)((point == snappedX ? 255.f : 80.f) * alDragAnim)), point == snappedX ? 2.f : 1.f);
                for (float point : snapPointsY)
                    dl->AddLine({0.f, point}, {sw, point}, IM_COL32(255, 255, 255,
                        (int)((point == snappedY ? 255.f : 80.f) * alDragAnim)), point == snappedY ? 2.f : 1.f);
            }

            const char* hint = "hold to move";
            float hintSz = 14.5f * scale;
            ImVec2 hintExt = arrayFont->CalcTextSizeA(hintSz, FLT_MAX, 0.0f, hint);
            float hintX = isLeftSide ? listLeft : (listRight - hintExt.x);
            float hintY = listTop + listH + 4.0f;
            dl->AddText(arrayFont, hintSz, ImVec2(hintX + 1, hintY + 1), IM_COL32(0, 0, 0, 200), hint);
            dl->AddText(arrayFont, hintSz, ImVec2(hintX, hintY), IM_COL32(200, 200, 210, 220), hint);
        }
    }

    // ----------------------------------------------------------------
    // Sistema toast
    // ----------------------------------------------------------------
    static void init_watch()
    {
        static const struct { bool* ptr; const char* name; } entries[] = {
            { &features::movement::timer_speed::enabled, "Timer Speed" },
            { &features::movement::bunnyhop::enabled, "Bunny Hop" },
            { &features::misc::right_clicker::enabled, "Right Clicker" },
            { &features::misc::bridge_assist::enabled, "Bridge Assist" },
            { &features::combat::auto_click::enabled,     "AutoClick"     },
            { &features::combat::aim_assist::enabled,     "Aim Assist"    },
            { &features::combat::reach::enabled,          "Reach"         },
            { &features::combat::velocity::enabled,       "Velocity"      },
            { &features::combat::refill::enabled,         "Refill"        },
            { &gui_nohitdelay_enabled,                    "NoHitDelay"    },
            { &gui_blockhit_enabled,                      "BlockHit"      },
            { &gui_friends_enabled,                       "Friends"       },
            { &features::movement::sprint::enabled,       "Sprint"        },
            { &features::movement::no_jump_delay::enabled,"NoJumpDelay"   },
            { &features::movement::snap_tap::enabled,    "SnapTap"      },
            { &features::movement::instant_stop::enabled,    "InstantStop"      },
            { &gui_fastplace_enabled,                     "FastPlace"     },
            { &gui_esp_enabled,                           "ESP"           },
            { &features::visual::nametags::enabled,       "Nametags"      },
            { &features::visual::tracers::enabled,        "Tracers"       },
            { &features::visual::hit_markers::enabled,    "Hit Markers"   },
            { &features::visual::arraylist::enabled,      "ArrayList"     },
            { &features::latency::blink::enabled,            "Blink"         },
            { &gui_autotool_enabled,                      "AutoTool"      },
            { &gui_autoarmor_enabled,                     "AutoArmor"     },
            { &gui_macros_enabled,                        "Macros"        },
            { &gui_armorswitcher_enabled,                 "ArmorSwitcher" },
        };
        int n = (int)(sizeof(entries) / sizeof(entries[0]));
        g_watch.resize(n);
        for (int i = 0; i < n; i++)
            g_watch[i] = { entries[i].ptr, *entries[i].ptr, entries[i].name };
        g_watch_init = true;
    }

    static void check_and_push(ImFont* font, float fsz)
    {
        if (!g_watch_init) { init_watch(); return; }

        if (::gui_config_just_loaded) {
            for (auto& w : g_watch) w.prev = *w.ptr;
            ::gui_config_just_loaded = false;
            return;
        }

        for (auto& w : g_watch) {
            w.prev = *w.ptr;
        }
    }

    void PushToastEvent(const std::string& txt, bool st, int kb = 0);

    static void render_toasts(ImDrawList* dl, ImFont* font, float sw, float sh, float dt)
    {
        // Toasts are now handled by the system notification system
        // This function is kept empty for compatibility
    }
}

extern void TriggerNotification(const char* title, const char* body, const char* tag);

namespace features::visual::arraylist {
    void PushToastEvent(const std::string& txt, bool st, int kb)
    {
        ::TriggerNotification(txt.c_str(), st ? "Enabled" : "Disabled", "SYSTEM");
    }
}

void PushModuleToast(const std::string& name, bool state, int keybind = 0) {
    features::visual::arraylist::PushToastEvent(name, state, keybind);
}
