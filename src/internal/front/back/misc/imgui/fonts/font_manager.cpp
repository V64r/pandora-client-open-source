#include "font_manager.h"
#include "Montserrat-SemiBold.h"
#include "PoppinsMedium.h"
#include "../../../../../interface/w_imgui_port/render/fonts/fa.hh"
#include <backends/imgui_impl_win32.h>

font_manager::FontManager* font_manager::FontManager::instance = nullptr;

namespace font_manager {
bool FontManager::initialize_impl(ImGuiIO& io) {
    io.Fonts->Clear();
    bool success = load_montserrat_semibold(io);
    success &= load_arraylist_font(io);
    success &= load_icon_font(io);
    if (!success) {
        default_font = io.Fonts->AddFontDefault();
        if (!ui_font) ui_font = default_font;
        if (!arraylist_font) arraylist_font = default_font;
        if (!watermark_font) watermark_font = default_font;
        if (!icon_font) icon_font = default_font;
    }
    io.Fonts->Build();
    return success;
}
bool FontManager::load_montserrat_semibold(ImGuiIO& io) {
    ImFontConfig ui_config{};
    ui_config.OversampleH = 2; ui_config.OversampleV = 2;
    ui_config.PixelSnapH = false; ui_config.FontDataOwnedByAtlas = false;
#ifdef IMGUI_ENABLE_FREETYPE
    ui_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting;
#endif
    constexpr float ui_size = 17.0f;
    ui_font = io.Fonts->AddFontFromMemoryTTF((void*)montserrat_semibold_data, (int)montserrat_semibold_size, ui_size, &ui_config);
    if (!ui_font) return false;
    default_font = ui_font;
    fonts["Default"] = { ui_font, ui_size, "Default" };
    fonts["UI"] = { ui_font, ui_size, "UI" };
    fonts["MontserratSemiBold"] = { ui_font, ui_size, "MontserratSemiBold" };
    ImFontConfig hud_config = ui_config;
    hud_config.OversampleH = 3; hud_config.OversampleV = 3;
    constexpr float watermark_size = 15.0f;
    watermark_font = io.Fonts->AddFontFromMemoryTTF((void*)montserrat_semibold_data, (int)montserrat_semibold_size, watermark_size, &hud_config);
    if (!watermark_font) return false;
    fonts["Watermark"] = { watermark_font, watermark_size, "Watermark" };
    return true;
}
bool FontManager::load_arraylist_font(ImGuiIO& io) {
    ImFontConfig config{};
    config.OversampleH = 3; config.OversampleV = 3;
    config.PixelSnapH = false; config.FontDataOwnedByAtlas = false;
#ifdef IMGUI_ENABLE_FREETYPE
    config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting;
#endif
    constexpr float arraylist_size = 24.0f;
    arraylist_font = io.Fonts->AddFontFromMemoryTTF((void*)poppins_medium_data,
        (int)poppins_medium_size, arraylist_size, &config);
    if (!arraylist_font) { arraylist_font = ui_font; return false; }
    fonts["ArrayList"] = { arraylist_font, arraylist_size, "ArrayList" };
    return true;
}
bool FontManager::load_icon_font(ImGuiIO& io) {
    ImFontConfig config{}; config.OversampleH = 3; config.OversampleV = 3;
    config.PixelSnapH = true; config.FontDataOwnedByAtlas = false;
#ifdef IMGUI_ENABLE_FREETYPE
    config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting;
#endif
    static const ImWchar ranges[] = { 0xf000, 0xf8ff, 0 };
    icon_font = io.Fonts->AddFontFromMemoryCompressedTTF(
        FA_compressed_data, static_cast<int>(FA_compressed_size), 18.0f, &config, ranges);
    if (icon_font) fonts["Icons"] = { icon_font, 18.0f, "Icons" }; else icon_font = default_font;
    return true;
}
ImFont* FontManager::get_font(const std::string& name) const {
    auto it = fonts.find(name); return it != fonts.end() ? it->second.font : default_font;
}
void FontManager::push_font(const std::string& name) {
    ImFont* font = get_font(name); ImGui::PushFont(font ? font : default_font);
}
void FontManager::shutdown() {
    fonts.clear(); default_font = ui_font = arraylist_font = watermark_font = icon_font = nullptr; hModule = nullptr;
}
}
