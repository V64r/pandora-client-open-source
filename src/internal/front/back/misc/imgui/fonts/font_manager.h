#include <windows.h>
#include <windowsx.h>
#include <backends/imgui.h>
#include <backends/imgui_internal.h>
#include "../misc/freetype/imgui_freetype.h"
#include <string>
#include <unordered_map>

namespace font_manager {

    struct FontEntry {
        ImFont* font = nullptr;
        float default_size = 0.0f;
        std::string name;
    };

    class FontManager {
    private:
        static FontManager* instance;

        std::unordered_map<std::string, FontEntry> fonts;

        ImFont* default_font = nullptr;
        ImFont* ui_font = nullptr;         
        ImFont* arraylist_font = nullptr;   
        ImFont* watermark_font = nullptr;   
        ImFont* icon_font = nullptr;

        HMODULE hModule = nullptr;

        FontManager() = default;
        ~FontManager() = default;
        bool load_montserrat_semibold(ImGuiIO& io);
        bool load_arraylist_font(ImGuiIO& io);
        bool load_icon_font(ImGuiIO& io);

    public:
        static FontManager& get() {
            if (!instance) {
                instance = new FontManager();
            }
            return *instance;
        }

        bool initialize(ImGuiIO& io, HMODULE module = nullptr) {
            hModule = module;
            return initialize_impl(io);
        }

        void shutdown();

        ImFont* get_default() const { return default_font; }
        ImFont* get_ui_font() const { return ui_font; }
        ImFont* get_arraylist_font() const { return arraylist_font; }
        ImFont* get_watermark_font() const { return watermark_font; }
        ImFont* get_icon_font() const { return icon_font; }

        ImFont* get_font(const std::string& name) const;

        void push_font(const std::string& name);
        void push_ui_font() { if (ui_font) ImGui::PushFont(ui_font); }
        void push_arraylist_font() { if (arraylist_font) ImGui::PushFont(arraylist_font); }
        void push_watermark_font() { if (watermark_font) ImGui::PushFont(watermark_font); }
        void push_icon_font() { if (icon_font) ImGui::PushFont(icon_font); }
        void pop_font() { ImGui::PopFont(); }

    private:
        
        bool initialize_impl(ImGuiIO& io);
    };

} 

#define FONT_MANAGER font_manager::FontManager::get()
