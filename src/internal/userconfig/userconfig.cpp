#include "userconfig.h"
#include <ShlObj.h>
#pragma comment(lib, "shell32.lib")

// ============================================================
// SWIFT CONFIG SYSTEM
// ============================================================
std::map<std::string, std::string> g_InMemoryConfigs;
// Profile currently selected in the UI. Selection never implies persistence;
// only an explicit SaveConfig call may overwrite its snapshot.
std::string g_ActiveConfig;

static std::map<std::string, int> g_KeybindMap;

// Nested ImGui children own separate draw lists. Share the animated reveal
// rectangle with them so their contents cannot outlive the closing menu shell.
static bool g_MenuRevealClipActive = false;

struct Setting {
    enum Type { TOGGLE, SLIDER, COLOR, COLOR4, LABEL, BUTTON, DROPDOWN, MULTIBOX };
    Type type = TOGGLE;
    std::string name = "";
    bool* boolPtr = nullptr;
    float* floatPtr = nullptr;
    float  min = 0.0f, max = 0.0f;
    const char* format = "";
    float* colorPtr = nullptr;
    int* intPtr = nullptr;
    std::vector<std::string> dropdownItems;
    std::function<bool()> visibleCondition = nullptr;
    std::vector<int> dropdownValues;
    bool inlineWithPrevious = false;
    std::vector<std::pair<std::string, bool*>> multiboxItems;
};

struct Module {
    std::string name;
    int   category = 0;
    int   keybind = 0;
    bool* enabledPtr = nullptr;
    std::string description = "";
    std::vector<Setting> settings;
    bool hidden = false;
};

std::vector<Module> modules;

Module CreateMod(std::string name, std::string desc, int cat, bool* enabledPtr = nullptr) {
    Module m; m.name = name; m.description = desc; m.category = cat; m.enabledPtr = enabledPtr; return m;
}

static std::string ConfigModuleKey(const std::string& name) {
    std::string key = "module_";
    key.reserve(key.size() + name.size() + 8);
    for (unsigned char ch : name) {
        if (std::isalnum(ch)) key.push_back((char)std::tolower(ch));
        else if (key.back() != '_') key.push_back('_');
    }
    key += "_enabled";
    return key;
}

static std::string ConfigSettingKey(const std::string& moduleName, const std::string& settingName) {
    std::string key = "setting_" + moduleName + "_" + settingName;
    for (char& ch : key) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '_';
    }
    return key;
}

void RefreshConfigs() {
    std::vector<std::string> new_list;
    // Keep existing items in their current order
    for (auto& cfg : g_ConfigList) {
        if (g_InMemoryConfigs.count(cfg)) {
            new_list.push_back(cfg);
        }
    }
    // Append new items
    for (auto& kv : g_InMemoryConfigs) {
        if (std::find(new_list.begin(), new_list.end(), kv.first) == new_list.end()) {
            new_list.push_back(kv.first);
        }
    }
    g_ConfigList = new_list;
}

// ============================================================
// CONFIG PERSISTENCE - Registro oculto
// Clave disfrazada como entrada COM de Windows, pasa desapercibida
// ============================================================
#pragma comment(lib, "advapi32.lib")
#define CFG_REG_KEY "Software\\Swift\\Configs"

static std::string LegacyConfigProductName() {
    static constexpr char name[] = {
        0x70, 0x61, 0x6E, 0x64, 0x6F, 0x72, 0x61, 0x43,
        0x6C, 0x69, 0x65, 0x6E, 0x74, 0x00
    };
    return name;
}

static std::string LegacyConfigRegistryKey() {
    return "Software\\" + LegacyConfigProductName() + "\\Configs";
}

static std::string ConfigFolderPath() {
    static const std::string cached_path = []() -> std::string {
    char appData[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    std::string base = (len > 0 && len < MAX_PATH)
        ? std::string(appData) : std::string(".");
    char documents[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, documents))) return base + "\\Swift\\Configs";
    std::string root = std::string(documents) + "\\swift";
    std::string configs = root + "\\configs";
    const bool destination_exists = GetFileAttributesA(configs.c_str()) != INVALID_FILE_ATTRIBUTES;
    CreateDirectoryA(root.c_str(), nullptr);
    CreateDirectoryA(configs.c_str(), nullptr);

    static bool migrated = false;
    if (!migrated && !destination_exists) {
        migrated = true;
        const std::string legacyFolders[] = { base + "\\Swift\\Configs",
            base + "\\" + LegacyConfigProductName().substr(0, 7) + "\\Configs" };
        for (const auto& legacyFolder : legacyFolders) {
        WIN32_FIND_DATAA findData = {};
        HANDLE find = FindFirstFileA((legacyFolder + "\\*").c_str(), &findData);
        if (find != INVALID_HANDLE_VALUE) {
            do {
                if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
                const std::string source = legacyFolder + "\\" + findData.cFileName;
                const std::string destination = configs + "\\" + findData.cFileName;
                CopyFileA(source.c_str(), destination.c_str(), TRUE);
            } while (FindNextFileA(find, &findData));
            FindClose(find);
        }
        }
    }
    return configs;
    }();
    return cached_path;
}

static std::string SafeConfigFileName(const std::string& name) {
    std::string safe = name;
    for (char& c : safe) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
            c == '\\' || c == '|' || c == '?' || c == '*') c = '_';
    }
    while (!safe.empty() && (safe.back() == ' ' || safe.back() == '.'))
        safe.pop_back();
    return safe.empty() ? "config" : safe;
}

static uint32_t ConfigChecksum(const std::string& data) {
    uint32_t hash = 2166136261u;
    for (unsigned char byte : data) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

static std::string EncodeConfigData(const std::string& plain) {
    // This format is lightweight obfuscation with accidental-corruption
    // detection. It is not encryption and stores no account credentials.
    static constexpr unsigned char magic[8] = { 'S', 'T', 'R', '$', '$', '$', 1, 0 };
    std::string encoded(reinterpret_cast<const char*>(magic), sizeof(magic));
    auto append_u32 = [&](uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8)
            encoded.push_back(static_cast<char>((value >> shift) & 0xFFu));
    };
    append_u32(static_cast<uint32_t>(plain.size()));
    append_u32(ConfigChecksum(plain));

    uint32_t state = 0x9E3779B9u ^ static_cast<uint32_t>(plain.size());
    encoded.reserve(encoded.size() + plain.size());
    for (unsigned char byte : plain) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        encoded.push_back(static_cast<char>(byte ^ static_cast<unsigned char>(state >> 24)));
    }
    return encoded;
}

static bool DecodeConfigData(const std::string& encoded, std::string& plain) {
    static constexpr unsigned char magic[8] = { 'S', 'T', 'R', '$', '$', '$', 1, 0 };
    static constexpr unsigned char legacyMagic[8] = {
        0x50, 0x4E, 0x44, 0x24, 0x24, 0x24, 0x01, 0x00
    };
    if (encoded.size() < 16 ||
        (std::memcmp(encoded.data(), magic, sizeof(magic)) != 0 &&
         std::memcmp(encoded.data(), legacyMagic, sizeof(legacyMagic)) != 0))
        return false;
    auto read_u32 = [&](size_t offset) {
        uint32_t value = 0;
        for (int shift = 0; shift < 32; shift += 8)
            value |= static_cast<uint32_t>(static_cast<unsigned char>(encoded[offset++])) << shift;
        return value;
    };
    const uint32_t size = read_u32(8);
    const uint32_t checksum = read_u32(12);
    if (encoded.size() != 16ull + size) return false;

    plain.clear();
    plain.reserve(size);
    uint32_t state = 0x9E3779B9u ^ size;
    for (size_t i = 0; i < size; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        plain.push_back(static_cast<char>(
            static_cast<unsigned char>(encoded[16 + i]) ^
            static_cast<unsigned char>(state >> 24)));
    }
    if (ConfigChecksum(plain) != checksum) {
        plain.clear();
        return false;
    }
    return true;
}

static std::string ConfigFilePath(const std::string& name) {
    return ConfigFolderPath() + "\\" + SafeConfigFileName(name) + ".$$$";
}

static void WriteConfigFile(const std::string& name, const std::string& data) {
    std::ofstream file(ConfigFilePath(name), std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
        const std::string encoded = EncodeConfigData(data);
        file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    }
}

void OpenConfigFolder() {
    const std::string folder = ConfigFolderPath();
    ShellExecuteA(nullptr, "open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void Registry_SaveConfig(const std::string& name, const std::string& data) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, CFG_REG_KEY, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        const std::string encoded = EncodeConfigData(data);
        RegSetValueExA(hKey, name.c_str(), 0, REG_BINARY,
            reinterpret_cast<const BYTE*>(encoded.data()), static_cast<DWORD>(encoded.size()));
        RegCloseKey(hKey);
    }
    WriteConfigFile(name, data);
}

static void Registry_DeleteConfig(const std::string& name) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CFG_REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueA(hKey, name.c_str());
        RegCloseKey(hKey);
    }
    DeleteFileA(ConfigFilePath(name).c_str());
    const std::string legacy = ConfigFolderPath() + "\\" + SafeConfigFileName(name) + ".pcfg";
    DeleteFileA(legacy.c_str());
}

static void ReloadConfigsFromDisk() {
    // The visible Configs folder is authoritative. The registry remains only as
    // a mirror written by SaveConfig; it must never recreate files deleted by
    // the user.
    const std::string folder = ConfigFolderPath();
    std::map<std::string, std::string> diskConfigs;
    std::vector<std::pair<std::string, std::string>> legacy_configs;
    auto load_pattern = [&](const char* pattern, size_t extension_size, bool legacy) {
        WIN32_FIND_DATAA findData = {};
        HANDLE find = FindFirstFileA((folder + "\\" + pattern).c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE) return;
        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
            const std::string fileName = findData.cFileName;
            if (fileName.size() <= extension_size) continue;
            const std::string configName = fileName.substr(0, fileName.size() - extension_size);
            if (legacy && diskConfigs.count(configName)) continue;
            std::ifstream file(folder + "\\" + fileName, std::ios::binary);
            if (!file.is_open()) continue;
            const std::string raw((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            std::string data;
            if (!legacy) {
                if (!DecodeConfigData(raw, data)) continue;
            } else {
                data = raw;
            }
            const std::string typePrefix = "// CLOUD_CONFIG_TYPE=";
            if (data.compare(0, typePrefix.size(), typePrefix) == 0) {
                const size_t newline = data.find('\n');
                if (newline != std::string::npos) data = data.substr(newline + 1);
            }
            if (data.empty()) continue;
            diskConfigs[configName] = data;
            if (legacy) legacy_configs.emplace_back(configName, std::move(data));
        } while (FindNextFileA(find, &findData));
        FindClose(find);
    };
    load_pattern("*.$$$", 4, false);
    load_pattern("*.pcfg", 5, true);

    for (const auto& legacy : legacy_configs) {
        WriteConfigFile(legacy.first, legacy.second);
        const std::string oldPath = folder + "\\" + SafeConfigFileName(legacy.first) + ".pcfg";
        DeleteFileA(oldPath.c_str());
    }
    g_InMemoryConfigs = std::move(diskConfigs);
    RefreshConfigs();
}

void PollConfigFolderChanges() {
    struct FolderWatch {
        HANDLE handle = INVALID_HANDLE_VALUE;
        std::string path;
        ULONGLONG reload_at = 0, retry_at = 0;
        ~FolderWatch() { if (handle != INVALID_HANDLE_VALUE) FindCloseChangeNotification(handle); }
    };
    static FolderWatch watch;
    const std::string path = ConfigFolderPath();
    if (path != watch.path || (watch.handle == INVALID_HANDLE_VALUE && GetTickCount64() >= watch.retry_at)) {
        if (watch.handle != INVALID_HANDLE_VALUE) FindCloseChangeNotification(watch.handle);
        watch.path = path;
        watch.retry_at = GetTickCount64() + 5000;
        watch.handle = FindFirstChangeNotificationA(path.c_str(), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
        if (watch.handle != INVALID_HANDLE_VALUE) watch.reload_at = GetTickCount64() + 150;
    }
    if (watch.handle != INVALID_HANDLE_VALUE && WaitForSingleObject(watch.handle, 0) == WAIT_OBJECT_0) {
        // Coalesce writes/renames instead of reading a partially written config.
        watch.reload_at = GetTickCount64() + 150;
        if (!FindNextChangeNotification(watch.handle)) {
            FindCloseChangeNotification(watch.handle);
            watch.handle = INVALID_HANDLE_VALUE;
        }
    }
    if (watch.reload_at && GetTickCount64() >= watch.reload_at) {
        watch.reload_at = 0;
        ReloadConfigsFromDisk();
    }
}

void SaveConfig(std::string name) {
    if (name.empty()) return;
    std::string out;
    out += "{\n";
    auto wb = [&](const char* k, bool v) { out += std::string("  \"") + k + "\": " + (v ? "1" : "0") + ",\n"; };
    auto wf = [&](const char* k, float v) { std::string s = std::to_string(v); for (char& c : s) if (c == ',') c = '.'; out += std::string("  \"") + k + "\": " + s + ",\n"; };
    auto wi = [&](const char* k, int v) { out += std::string("  \"") + k + "\": " + std::to_string(v) + ",\n"; };
    auto wh = [&](const char* k, const char* value) {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string encoded;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
            encoded.push_back(digits[*p >> 4]);
            encoded.push_back(digits[*p & 0x0F]);
        }
        out += std::string("  \"") + k + "\": \"" + encoded + "\",\n";
    };
    wb("en_autoclick", features::combat::auto_click::enabled);
    wb("en_aimassist", features::combat::aim_assist::enabled);
    wb("en_reach", gui_reach_enabled);
    wb("en_velocity", gui_velo_enabled);
    wb("en_nohitdelay", gui_nohitdelay_enabled);
    wb("en_refill", features::combat::refill::enabled);
    wb("en_sprint", gui_sprint_enabled);
    wb("en_noslow", gui_noslow_enabled);
    wb("en_nojumpdelay", gui_nojumpdelay_enabled);
    wb("en_noitemrelease", gui_noitemrelease_enabled);
    wb("esp_enabled", gui_esp_enabled);
    wi("esp_mode", gui_esp_mode);
    wi("esp_draw_mode", gui_esp_draw_mode);
    wi("esp_2d_style", gui_esp_2d_style);
    wf("esp_corner_size", gui_esp_corner_size);
    wf("esp_line_thickness", gui_esp_line_thickness);
    wf("esp_render_distance", gui_esp_render_distance);
    wb("esp_pulse", gui_esp_pulse);
    wf("esp_pulse_speed", gui_esp_pulse_speed);
    wf("esp_pulse_min_alpha", gui_esp_pulse_min_alpha);
    wf("esp_pulse_max_alpha", gui_esp_pulse_max_alpha);
    wb("esp_healthbar", gui_esp_healthbar);
    wi("esp_healthbar_position", gui_esp_healthbar_position);
    wi("esp_healthbar_style", gui_esp_healthbar_style);
    wf("esp_healthbar_thickness", gui_esp_healthbar_thickness);
    wb("esp_health_number", gui_esp_health_number);
    wf("esp_healthbar_segments", gui_esp_healthbar_segments);
    wb("esp_hurt_color", gui_esp_hurt_color);
    wi("esp_fill_mode", gui_esp_fill_mode);
    wb("en_nametags", gui_nametags_enabled);
    wb("en_tracers", gui_tracers_enabled);

    wb("wm_en", gui_watermark_enabled);
    wi("wm_color_mode", gui_watermark_color_mode);
    wf("wm_color_b0", gui_watermark_color_b[0]); wf("wm_color_b1", gui_watermark_color_b[1]); wf("wm_color_b2", gui_watermark_color_b[2]);
    wf("wm_pos_x", gui_watermark_pos_x);
    wf("wm_pos_y", gui_watermark_pos_y);
    wb("wm_ply", gui_watermark_show_player);
    wb("wm_srv", gui_watermark_show_server);
    wb("wm_fps", gui_watermark_show_fps);
    wb("wm_name", gui_watermark_show_name);
    wb("wm_time", gui_watermark_show_time);
    wb("wm_bg", gui_watermark_background);
    wb("wm_bg_shadow", gui_watermark_background_shadow);
    wb("wm_split_bg", gui_watermark_split_background);
    wb("wm_text_shadow", gui_watermark_text_shadow);

    wb("en_arraylist", gui_arraylist_enabled);
    wf("al_scale", gui_arraylist_scale);
    wf("al_bgc0", gui_arraylist_bg_color_4[0]); wf("al_bgc1", gui_arraylist_bg_color_4[1]); wf("al_bgc2", gui_arraylist_bg_color_4[2]); wf("al_bgc3", gui_arraylist_bg_color_4[3]);
    wb("al_low", gui_arraylist_lowercase);
    wb("al_shd", gui_arraylist_shadows);
    wb("al_cbar", gui_arraylist_colorbar);
    wf("al_c0", gui_arraylist_color[0]); wf("al_c1", gui_arraylist_color[1]); wf("al_c2", gui_arraylist_color[2]);
    wf("al_ic0", gui_arraylist_info_color[0]); wf("al_ic1", gui_arraylist_info_color[1]); wf("al_ic2", gui_arraylist_info_color[2]);
    wf("al_spd", gui_arraylist_speed);
    wf("al_rad", gui_arraylist_radius);
    wf("al_px", gui_arraylist_pad_x); wf("al_py", gui_arraylist_pad_y);
    wf("al_posx", gui_arraylist_pos_x); wf("al_posy", gui_arraylist_pos_y);
    wb("al_inf", gui_arraylist_show_info);
    wb("al_not", features::misc::notifications::enabled);
    wi("not_pos", features::misc::notifications::position);
    wb("outline_enabled", features::visual::outline::enabled);
    wb("outline_glow", features::visual::outline::glow);
    wf("outline_thickness", features::visual::outline::thickness);
    wf("outline_strength", features::visual::outline::strength);
    wb("al_drg", g_ArrayListDragMode);
    wb("al_bg", gui_arraylist_background);
    wb("al_bgsh", gui_arraylist_background_shadow);
    wb("al_watermark", gui_arraylist_watermark);
    wf("al_bgsh_strength", gui_arraylist_shadow_strength);
    wi("al_mod", gui_arraylist_color_mode);
    wb("al_blur", gui_arraylist_blur);
    wf("al_bopac", gui_arraylist_blur_opacity);
    // Save hidden modules as pipe-separated string
    {
        std::string hidden_str;
        for (auto& h : g_arraylist_hidden_modules) {
            if (!hidden_str.empty()) hidden_str += "|";
            hidden_str += h;
        }
        out += std::string("  \"al_hidden\": \"") + hidden_str + "\",\n";
    }
    for (int i = 0; i < 3; ++i) g_AccentColor[i] = gui_guicolor_custom[i];
    wf("ac_r", g_AccentColor[0]); wf("ac_g", g_AccentColor[1]); wf("ac_b", g_AccentColor[2]);
    wf("gui_col_r", gui_guicolor_custom[0]); wf("gui_col_g", gui_guicolor_custom[1]); wf("gui_col_b", gui_guicolor_custom[2]);
    wb("menu_header_animation", g_MenuHeaderAnim);
    wf("menu_header_background_r", g_MenuHeaderBgColor[0]);
    wf("menu_header_background_g", g_MenuHeaderBgColor[1]);
    wf("menu_header_background_b", g_MenuHeaderBgColor[2]);
    wf("menu_header_stars_r", g_MenuHeaderAnimColor[0]);
    wf("menu_header_stars_g", g_MenuHeaderAnimColor[1]);
    wf("menu_header_stars_b", g_MenuHeaderAnimColor[2]);
    wi("menu_key", g_MenuKey);
    wi("gui_scale_index", g_GuiScaleIndex);
    wb("background_dim", g_BackgroundDim);
    wb("en_hitmarkers", features::visual::hit_markers::enabled);
    wb("en_armorswitcher", gui_armorswitcher_enabled);
    wi("blink_mode", features::latency::blink::mode);
    wf("al_bar_width", gui_arraylist_bar_width);
    wb("al_bracket_flags", gui_arraylist_bracket_flags);
    wb("wm_blur", gui_watermark_blur);
    wf("wm_blur_opacity", gui_watermark_blur_opacity);
    wf("wm_c0", gui_watermark_color[0]); wf("wm_c1", gui_watermark_color[1]); wf("wm_c2", gui_watermark_color[2]);
    wf("al_c3_0", gui_arraylist_color_c[0]); wf("al_c3_1", gui_arraylist_color_c[1]); wf("al_c3_2", gui_arraylist_color_c[2]);
    wf("al_color_space", gui_arraylist_color_space);
    wf("al_c2_0", gui_arraylist_color_b[0]); wf("al_c2_1", gui_arraylist_color_b[1]); wf("al_c2_2", gui_arraylist_color_b[2]);
    wf("blink_path_c0", gui_blink_path_color[0]); wf("blink_path_c1", gui_blink_path_color[1]); wf("blink_path_c2", gui_blink_path_color[2]);
    for (int i = 0; i < 4; ++i) {
        wf(("esp_outline_" + std::to_string(i)).c_str(), gui_esp_outline_color[i]);
        wf(("esp_filled_" + std::to_string(i)).c_str(), gui_esp_filled_color[i]);
        wf(("esp_friend_" + std::to_string(i)).c_str(), gui_esp_friend_color[i]);
        wf(("esp_hurt_effect_" + std::to_string(i)).c_str(), gui_esp_hurt_effect_color[i]);
        wf(("esp_health_top_" + std::to_string(i)).c_str(), gui_esp_healthbar_gradient_top[i]);
        wf(("esp_health_bottom_" + std::to_string(i)).c_str(), gui_esp_healthbar_gradient_bottom[i]);
        wf(("esp_fill_top_" + std::to_string(i)).c_str(), gui_esp_fill_gradient_top[i]);
        wf(("esp_fill_bottom_" + std::to_string(i)).c_str(), gui_esp_fill_gradient_bottom[i]);
        wf(("notif_bar_" + std::to_string(i)).c_str(), features::misc::notifications::bar_color[i]);
        wf(("outline_color_" + std::to_string(i)).c_str(), features::visual::outline::color[i]);
    }
    wb("en_autoarmor", gui_autoarmor_enabled);
    wf("autoarmor_delay", gui_autoarmor_delay);
    wb("autoarmor_better", gui_autoarmor_only_better);
    wb("en_blink", gui_blink_enabled);

    wb("en_blockhit", gui_blockhit_enabled);
    wi("gui_blockhit_mode", gui_blockhit_mode);
    wb("gui_blockhit_mouse", gui_blockhit_require_mouse_down);
    wf("gui_blockhit_block_ticks", gui_blockhit_block_ticks);
    wf("gui_blockhit_unblock_ticks", gui_blockhit_unblock_ticks);
    wf("gui_blockhit_chance", gui_blockhit_chance);
    wb("gui_blockhit_only_sword", gui_blockhit_only_sword);
    wb("gui_blockhit_visual", gui_blockhit_visual_only);
    wb("en_fastplace", gui_fastplace_enabled);
    wi("gui_fastplace_held", gui_fastplace_held_item);
    wb("en_autotool", gui_autotool_enabled);
    wf("gui_autotool_delay", gui_autotool_swap_delay);
    wb("gui_autotool_swap_weapon", gui_autotool_swap_weapon);
    wb("gui_autotool_instant", gui_autotool_instant_swap);
    wb("gui_autotool_back", gui_autotool_swap_back);
    wb("gui_autotool_mouse", gui_autotool_require_mouse_down);
    wb("gui_autotool_sneak", gui_autotool_only_sneaking);
    wb("gui_ac_break", gui_ac_break_blocks);
    // -- New modules --
    wb("en_macros", gui_macros_enabled);
    wi("gui_macros_mode", gui_macros_mode);
    wi("gui_macros_bind", gui_macros_bind);
    wf("gui_macros_switch_delay", gui_macros_switch_delay);
    wf("gui_macros_use_delay", gui_macros_use_delay);
    wb("gui_macros_auto_back", gui_macros_auto_switch_back);
    wi("gui_armorsw_kit1", gui_armorswitcher_kit1);
    wi("gui_armorsw_kit2", gui_armorswitcher_kit2);
    wi("gui_armorsw_bind", gui_armorswitcher_bind);
    wf("gui_armorsw_delay", gui_armorswitcher_delay);
    


    wf("gui_refill_delay", gui_refill_delay);
    wf("gui_refill_silent_delay", gui_refill_silent_delay);
    wf("gui_refill_silent_ticks", gui_refill_silent_ticks);

    wf("gui_min_cps", gui_min_cps);
    wf("gui_max_cps", gui_max_cps);
    wf("gui_inv_cps", gui_inv_cps);
    wb("ac_break_blocks", gui_ac_break_blocks);
    wb("ac_inv_en", features::combat::auto_click::inventory_enabled);
    wb("ac_weap_only", features::combat::auto_click::weapons_only);
    wb("ac_target_only", features::combat::auto_click::target_only);
    wb("ac_prevent_unrefill", features::combat::auto_click::prevent_unrefill);
    wb("ac_random", features::combat::auto_click::randomization);
    wf("ac_drop", features::combat::auto_click::drop_chance);
    wf("ac_spike", features::combat::auto_click::spike_chance);
    wi("ac_click_method", gui_ac_click_method);
    wf("bard_delay", features::misc::bard_helper::switch_delay);
    wb("bard_return", features::misc::bard_helper::return_last_slot);
    wb("bard_resistance", features::misc::bard_helper::resistance);
    wb("bard_strength", features::misc::bard_helper::strength);
    wb("bard_regeneration", features::misc::bard_helper::regeneration);
    wb("bard_speed", features::misc::bard_helper::speed);
    wb("bard_jump", features::misc::bard_helper::jump);
    wi("friends_nearby_bind", features::friends::nearby_bind);
    wf("friends_nearby_distance", features::friends::nearby_distance);
    wb("nir_potions", features::movement::no_item_release::potions);
    wb("nir_sword", features::movement::no_item_release::sword);
    wb("nir_bow", features::movement::no_item_release::bow);
    wb("nir_food", gui_noitemrelease_food);
    wb("en_snaptap", features::movement::snap_tap::enabled);
    wb("en_instantstop", features::movement::instant_stop::enabled);
    wb("is_only_on_ground", features::movement::instant_stop::only_on_ground);
    wb("is_stop_on_sneak", features::movement::instant_stop::stop_on_sneak);
    wf("is_stop_strength", features::movement::instant_stop::stop_strength);
    wb("ts_enabled", features::movement::timer_speed::enabled);
    wf("ts_speed", features::movement::timer_speed::speed);
    wb("ts_damage", features::movement::timer_speed::require_damage);
    wb("ts_weapon", features::movement::timer_speed::only_weapon);
    wb("ts_moving", features::movement::timer_speed::moving);
    wb("bh_enabled", features::movement::bunnyhop::enabled);
    wb("bh_liquid", features::movement::bunnyhop::liquid_check);
    wb("bh_moving", features::movement::bunnyhop::only_moving);
    wf("bh_delay", features::movement::bunnyhop::jump_delay);
    wf("bh_height", features::movement::bunnyhop::jump_height);
    wf("bh_power", features::movement::bunnyhop::power);
    wf("bh_multiplier", features::movement::bunnyhop::speed_multiplier);
    wf("bh_slowdown", features::movement::bunnyhop::slowdown_factor);
    wf("bh_friction", features::movement::bunnyhop::friction);
    wf("bh_threshold", features::movement::bunnyhop::direction_threshold);
    wb("ac_sound_enabled", features::combat::auto_click::click_sound);
    wi("ac_sound", features::combat::auto_click::sound);
    wf("ac_volume", features::combat::auto_click::click_volume);
    wb("rc_enabled", features::misc::right_clicker::enabled);
    wf("rc_min_cps", features::misc::right_clicker::min_cps);
    wf("rc_max_cps", features::misc::right_clicker::max_cps);
    wb("rc_only_click", features::misc::right_clicker::only_click);
    wb("rc_ignore_obsidian", features::misc::right_clicker::ignore_obsidian);
    wb("rc_only_blocks", features::misc::right_clicker::only_blocks);
    wb("ba_enabled", features::misc::bridge_assist::enabled);
    wf("ba_offset", features::misc::bridge_assist::edge_offset);
    wf("ba_delay", features::misc::bridge_assist::unsneak_delay);
    wi("ba_select_blocks", features::misc::bridge_assist::select_blocks);
    wb("ba_randomize", features::misc::bridge_assist::randomize);
    wb("ba_sneak_jump", features::misc::bridge_assist::sneak_on_jump);
    wb("ba_avoid_double", features::misc::bridge_assist::avoid_double_sneaking);
    wb("ba_require_sneak", features::misc::bridge_assist::require_sneak);
    wb("ba_holding_blocks", features::misc::bridge_assist::holding_blocks);
    wb("ba_looking_down", features::misc::bridge_assist::looking_down);
    wb("ba_not_forward", features::misc::bridge_assist::not_forward);

    wb("gui_aa_silent", gui_aa_silent);
    wf("gui_aa_min_dist", gui_aa_min_dist); wf("gui_aa_max_dist", gui_aa_max_dist);
    wf("gui_aa_min_fov", gui_aa_min_fov); wf("gui_aa_max_fov", gui_aa_max_fov);
    wf("gui_aa_horizontal_speed", gui_aa_horizontal_speed); wf("gui_aa_vertical_speed", gui_aa_vertical_speed);
    wf("gui_reach_min_distance", gui_reach_min_distance); wf("gui_reach_max_distance", gui_reach_max_distance);
    wb("gui_reach_hitbox_enabled", gui_reach_hitbox_enabled); wf("gui_reach_hitbox_size", gui_reach_hitbox_size);
    wf("gui_reach_chance", gui_reach_chance);
    wb("gui_reach_ground_only", gui_reach_ground_only);
    wb("gui_reach_weapon_only", gui_reach_weapon_only);
    wb("gui_reach_liquid_check", gui_reach_liquid_check);
    wb("gui_reach_combo_mode", gui_reach_combo_mode);
    wb("gui_reach_hit_through_walls", gui_reach_hit_through_walls);
    wi("gui_velo_mode", gui_velo_mode);
    wf("gui_velo_horizontal", gui_velo_horizontal);
    wf("gui_velo_vertical", gui_velo_vertical);
    wf("gui_velo_chance", gui_velo_chance);
    wf("gui_velo_delay", gui_velo_delay);
    wb("gui_velo_air_only", gui_velo_air_only);
    wb("gui_velo_moving_only", gui_velo_moving_only);
    wb("gui_velo_weapon_only", gui_velo_weapon_only);
    wb("gui_velo_push_back", gui_velo_push_back);
    wb("gui_velo_clicking_only", gui_velo_clicking_only);
    
    wf("gui_tracers_thickness", gui_tracers_thickness);
    wb("gui_tracers_draw_distance", gui_tracers_draw_distance);
    wb("gui_tracers_draw_hurt_time", gui_tracers_draw_hurt_time);
    wb("gui_tracers_draw_invisible", gui_tracers_draw_invisible);
    wf("gui_blink_timer_limit", gui_blink_timer_limit);
    wb("gui_blink_show_path", gui_blink_show_path);
    wb("gui_blink_show_timer", gui_blink_show_timer);
    wb("gui_nametags_draw_health", gui_nametags_draw_health);
    wi("gui_nametags_health_format", gui_nametags_health_format);
    wf("gui_nametags_health_segments", gui_nametags_health_segments);
    wb("gui_nametags_show_name", gui_nametags_show_name);
    wb("gui_nametags_show_own", gui_nametags_show_own);
    wb("gui_nametags_hide_vanilla", gui_nametags_hide_vanilla);
    wb("gui_nametags_show_equipment", gui_nametags_show_equipment);
    wb("gui_nametags_show_enchantments", gui_nametags_show_enchantments);
    wi("hitmarkers_mode", features::visual::hit_markers::mode);
    wf("hitmarkers_size", features::visual::hit_markers::size);
    wf("hitmarkers_width", features::visual::hit_markers::line_width);
    wf("hitmarkers_duration", features::visual::hit_markers::duration);
    wb("hitmarkers_fade", features::visual::hit_markers::fade_out);
    wb("hitmarkers_scale_anim", features::visual::hit_markers::scale_animation);
    wf("hitmarkers_scale_amount", features::visual::hit_markers::scale_amount);
    wb("hitmarkers_outline", features::visual::hit_markers::outline);
    wf("hitmarkers_outline_width", features::visual::hit_markers::outline_width);
    for (int i = 0; i < 4; ++i) { wf(("hitmarkers_color" + std::to_string(i)).c_str(), features::visual::hit_markers::color[i]); wf(("hitmarkers_outline_color" + std::to_string(i)).c_str(), features::visual::hit_markers::outline_color[i]); }
    wb("gui_nametags_draw_distance", gui_nametags_draw_distance);
    wb("gui_nametags_draw_hurt_time", gui_nametags_draw_hurt_time);
    wb("gui_nametags_draw_invisible", gui_nametags_draw_invisible);
    wb("gui_nametags_background", gui_nametags_background);    wb("gui_nametags_use_fake_name", gui_nametags_use_fake_name);
    wf("gui_nametags_scale", gui_nametags_scale);
    wb("gui_nametags_distance_scaling", gui_nametags_distance_scaling);
    wi("gui_aa_mode", features::combat::aim_assist::mode);
    wi("gui_aa_aim_mode", features::combat::aim_assist::aim_mode);
    wi("gui_aa_priority", features::combat::aim_assist::priority);
    wi("gui_aa_target_mode", features::combat::aim_assist::target_mode);
    wb("gui_aa_weapons_only", features::combat::aim_assist::weapons_only);
    wb("gui_aa_break_blocks", features::combat::aim_assist::break_blocks);
    wb("gui_aa_clicking_only", features::combat::aim_assist::clicking_only);
    wb("gui_aa_through_walls", features::combat::aim_assist::through_walls);
    wb("gui_aa_ignore_invisible", features::combat::aim_assist::ignore_invisible);
    wb("friends_enabled", gui_friends_enabled);
    wi("friends_add_bind", gui_friends_add_bind);
    // Colors (RGBA)
    wf("tracers_c0", gui_tracers_color_4[0]); wf("tracers_c1", gui_tracers_color_4[1]); wf("tracers_c2", gui_tracers_color_4[2]); wf("tracers_c3", gui_tracers_color_4[3]);
    wf("nametags_c0", gui_nametags_color[0]); wf("nametags_c1", gui_nametags_color[1]); wf("nametags_c2", gui_nametags_color[2]); wf("nametags_c3", gui_nametags_color[3]);
    wh("nametags_fake_name", gui_nametags_fake_name);

    for (const auto& module : modules) {
        if (module.enabledPtr) wb(ConfigModuleKey(module.name).c_str(), *module.enabledPtr);
        // Generic persistence keeps every current/future module option covered,
        // even when an explicit legacy key was not added to this function.
        for (const auto& setting : module.settings) {
            const std::string key = ConfigSettingKey(module.name, setting.name);
            switch (setting.type) {
            case Setting::TOGGLE:
                if (setting.boolPtr) wb(key.c_str(), *setting.boolPtr);
                break;
            case Setting::SLIDER:
                if (setting.floatPtr) wf(key.c_str(), *setting.floatPtr);
                break;
            case Setting::DROPDOWN:
                if (setting.intPtr) wi(key.c_str(), *setting.intPtr);
                break;
            case Setting::COLOR:
                if (setting.colorPtr) {
                    for (int i = 0; i < 3; ++i)
                        wf((key + "_" + std::to_string(i)).c_str(), setting.colorPtr[i]);
                }
                break;
            case Setting::COLOR4:
                if (setting.colorPtr) {
                    for (int i = 0; i < 4; ++i)
                        wf((key + "_" + std::to_string(i)).c_str(), setting.colorPtr[i]);
                }
                break;
            default:
                break;
            }
        }
    }

    for (auto& m : modules) g_KeybindMap[m.name] = m.keybind;
    for (auto& kv : g_KeybindMap) {
        std::string key = "bind_";
        key += kv.first;
        for (char& c : key) if (c == ' ') c = '_';
        wi(key.c_str(), kv.second);
    }

    {
        std::string tlist;
        const auto friendsSnapshot = features::friends::snapshot_entries();
        for (size_t i = 0; i < friendsSnapshot.size(); ++i) {
            if (!tlist.empty()) tlist += "|";
            tlist += friendsSnapshot[i].first;
            if (!friendsSnapshot[i].second.empty()) {
                tlist += "@";
                tlist += friendsSnapshot[i].second;
            }
        }
        out += std::string("  \"friends_list\": \"") + tlist + "\",\n";
    }
    // remove trailing comma from last entry for valid JSON
    if (out.size() >= 2 && out[out.size() - 2] == ',')
        out[out.size() - 2] = ' ';
    out += "}\n";
    if (g_ConfigDateMap.find(name) == g_ConfigDateMap.end()) {
        time_t t = time(0);
        struct tm now; localtime_s(&now, &t);
        char buf[80]; strftime(buf, sizeof(buf), "%m/%d/%Y", &now);
        g_ConfigDateMap[name] = buf;
    }
    g_InMemoryConfigs[name] = out;
    g_ActiveConfig = name;
    Registry_SaveConfig(name, out);
    RefreshConfigs();
}

bool gui_config_just_loaded = false;
uint64_t g_ConfigLoadRevision = 0;

void LoadConfig(std::string name) {
    auto it = g_InMemoryConfigs.find(name);
    if (it == g_InMemoryConfigs.end()) return;
    g_ActiveConfig = name;
    gui_config_just_loaded = true;
    std::istringstream ss(it->second);
    std::string line;
    bool loadedAccentColor = false;
    bool loadedGuiColor = false;
    auto readFloat = [&](const std::string& key, float& val) {
        std::string prefix = "\"" + key + "\": ";
        size_t pos = line.find(prefix);
        if (pos != std::string::npos) {
            size_t val_start = pos + prefix.length();
            while (val_start < line.length() && (line[val_start] == ' ' || line[val_start] == '\t')) val_start++;
            size_t end = val_start;
            while (end < line.length() && (isdigit(line[end]) || line[end] == '.' || line[end] == ',' || line[end] == '-')) end++;
            if (end > val_start) {
                std::string s_val = line.substr(val_start, end - val_start);
                float result = 0.0f, sign = 1.0f, fraction = 0.1f;
                bool in_frac = false;
                for (char c : s_val) {
                    if (c == '-') sign = -1.0f;
                    else if (c == '.' || c == ',') in_frac = true;
                    else if (isdigit(c)) {
                        if (!in_frac) result = result * 10.0f + (c - '0');
                        else { result += (c - '0') * fraction; fraction *= 0.1f; }
                    }
                }
                val = result * sign;
            }
        }
    };
    auto readBool = [&](const std::string& key, bool& val) {
        std::string prefix = "\"" + key + "\": ";
        size_t pos = line.find(prefix);
        if (pos != std::string::npos) {
            size_t val_start = pos + prefix.length();
            while (val_start < line.length() && (line[val_start] == ' ' || line[val_start] == '\t')) val_start++;
            size_t end = val_start;
            while (end < line.length() && line[end] != ',' && line[end] != '}' && line[end] != ' ' && line[end] != '\t' && line[end] != '\r') end++;
            if (end > val_start) {
                std::string s_val = line.substr(val_start, end - val_start);
                if (s_val == "true" || s_val == "1") val = true;
                else if (s_val == "false" || s_val == "0") val = false;
            }
        }
    };
    auto readInt = [&](const std::string& key, int& val) {
        std::string prefix = "\"" + key + "\": ";
        size_t pos = line.find(prefix);
        if (pos != std::string::npos) {
            size_t val_start = pos + prefix.length();
            while (val_start < line.length() && (line[val_start] == ' ' || line[val_start] == '\t')) val_start++;
            size_t end = val_start;
            while (end < line.length() && (isdigit(line[end]) || line[end] == '-')) end++;
            if (end > val_start) {
                try { val = std::stoi(line.substr(val_start, end - val_start)); }
                catch (...) {}
            }
        }
    };
    auto readHexString = [&](const std::string& key, char* destination, size_t capacity) {
        const std::string prefix = "\"" + key + "\": \"";
        const size_t pos = line.find(prefix);
        if (pos == std::string::npos || capacity == 0) return;
        const size_t begin = pos + prefix.size();
        const size_t end = line.find('"', begin);
        if (end == std::string::npos) return;
        const std::string encoded = line.substr(begin, end - begin);
        const size_t byteCount = (std::min)(encoded.size() / 2, capacity - 1);
        auto nibble = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
            if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(c - 'A' + 10);
            if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(c - 'a' + 10);
            return 0;
        };
        for (size_t i = 0; i < byteCount; ++i)
            destination[i] = static_cast<char>((nibble(encoded[i * 2]) << 4) | nibble(encoded[i * 2 + 1]));
        destination[byteCount] = '\0';
    };
    while (std::getline(ss, line)) {
        readInt("menu_key", g_MenuKey);
        readInt("gui_scale_index", g_GuiScaleIndex);
        readBool("background_dim", g_BackgroundDim);
        readBool("en_hitmarkers", features::visual::hit_markers::enabled);
        readBool("en_armorswitcher", gui_armorswitcher_enabled);
        readInt("blink_mode", features::latency::blink::mode);
        readFloat("al_bar_width", gui_arraylist_bar_width);
        readBool("al_bracket_flags", gui_arraylist_bracket_flags);
        readBool("wm_blur", gui_watermark_blur);
        readFloat("wm_blur_opacity", gui_watermark_blur_opacity);
        readFloat("wm_c0", gui_watermark_color[0]); readFloat("wm_c1", gui_watermark_color[1]); readFloat("wm_c2", gui_watermark_color[2]);
        readFloat("al_c3_0", gui_arraylist_color_c[0]); readFloat("al_c3_1", gui_arraylist_color_c[1]); readFloat("al_c3_2", gui_arraylist_color_c[2]);
        readFloat("al_color_space", gui_arraylist_color_space);
        readFloat("al_c2_0", gui_arraylist_color_b[0]); readFloat("al_c2_1", gui_arraylist_color_b[1]); readFloat("al_c2_2", gui_arraylist_color_b[2]);
        readFloat("blink_path_c0", gui_blink_path_color[0]); readFloat("blink_path_c1", gui_blink_path_color[1]); readFloat("blink_path_c2", gui_blink_path_color[2]);
        for (int i = 0; i < 4; ++i) {
            readFloat("esp_outline_" + std::to_string(i), gui_esp_outline_color[i]);
            readFloat("esp_filled_" + std::to_string(i), gui_esp_filled_color[i]);
            readFloat("esp_friend_" + std::to_string(i), gui_esp_friend_color[i]);
            readFloat("esp_hurt_effect_" + std::to_string(i), gui_esp_hurt_effect_color[i]);
            readFloat("esp_health_top_" + std::to_string(i), gui_esp_healthbar_gradient_top[i]);
            readFloat("esp_health_bottom_" + std::to_string(i), gui_esp_healthbar_gradient_bottom[i]);
            readFloat("esp_fill_top_" + std::to_string(i), gui_esp_fill_gradient_top[i]);
            readFloat("esp_fill_bottom_" + std::to_string(i), gui_esp_fill_gradient_bottom[i]);
            readFloat("notif_bar_" + std::to_string(i), features::misc::notifications::bar_color[i]);
            readFloat("outline_color_" + std::to_string(i), features::visual::outline::color[i]);
        }
        readBool("outline_enabled", features::visual::outline::enabled);
        readBool("outline_glow", features::visual::outline::glow);
        readFloat("outline_thickness", features::visual::outline::thickness);
        readFloat("outline_strength", features::visual::outline::strength);
        readBool("en_autoclick", features::combat::auto_click::enabled);
        readBool("en_aimassist", features::combat::aim_assist::enabled);
        readBool("en_reach", gui_reach_enabled);
        readBool("en_velocity", gui_velo_enabled);
        readBool("en_nohitdelay", gui_nohitdelay_enabled);
        readBool("en_refill", features::combat::refill::enabled);
        readBool("en_sprint", gui_sprint_enabled);
        readBool("en_noslow", gui_noslow_enabled);
        readBool("en_nojumpdelay", gui_nojumpdelay_enabled);
        readBool("en_noitemrelease", gui_noitemrelease_enabled);
        readBool("esp_enabled", gui_esp_enabled);
        readInt("esp_mode", gui_esp_mode);
        gui_esp_mode = ImClamp(gui_esp_mode, 0, 1);
        readInt("esp_draw_mode", gui_esp_draw_mode);
        gui_esp_draw_mode = ImClamp(gui_esp_draw_mode, 0, 2);
        readInt("esp_2d_style", gui_esp_2d_style);
        gui_esp_2d_style = ImClamp(gui_esp_2d_style, 0, 2);
        readFloat("esp_corner_size", gui_esp_corner_size);
        gui_esp_corner_size = ImClamp(gui_esp_corner_size, 0.10f, 0.50f);
        readFloat("esp_line_thickness", gui_esp_line_thickness);
        gui_esp_line_thickness = ImClamp(gui_esp_line_thickness, 0.5f, 6.0f);
        readFloat("esp_render_distance", gui_esp_render_distance);
        gui_esp_render_distance = ImClamp(gui_esp_render_distance, 8.0f, 256.0f);
        readBool("esp_pulse", gui_esp_pulse);
        readFloat("esp_pulse_speed", gui_esp_pulse_speed);
        readFloat("esp_pulse_min_alpha", gui_esp_pulse_min_alpha);
        readFloat("esp_pulse_max_alpha", gui_esp_pulse_max_alpha);
        readBool("esp_healthbar", gui_esp_healthbar);
        readInt("esp_healthbar_position", gui_esp_healthbar_position);
        gui_esp_healthbar_position = ImClamp(gui_esp_healthbar_position, 0, 1);
        readInt("esp_healthbar_style", gui_esp_healthbar_style);
        gui_esp_healthbar_style = ImClamp(gui_esp_healthbar_style, 0, 1);
        readFloat("esp_healthbar_thickness", gui_esp_healthbar_thickness);
        gui_esp_healthbar_thickness = ImClamp(gui_esp_healthbar_thickness, 1.0f, 6.0f);
        readBool("esp_health_number", gui_esp_health_number);
        readFloat("esp_healthbar_segments", gui_esp_healthbar_segments);
        gui_esp_healthbar_segments = ImClamp(gui_esp_healthbar_segments, 2.0f, 20.0f);
        readBool("esp_hurt_color", gui_esp_hurt_color);
        readInt("esp_fill_mode", gui_esp_fill_mode);
        gui_esp_fill_mode = ImClamp(gui_esp_fill_mode, 0, 1);
        readBool("en_nametags", gui_nametags_enabled);
        readBool("en_tracers", gui_tracers_enabled);
        readBool("wm_en", gui_watermark_enabled);
        readInt("wm_color_mode", gui_watermark_color_mode);
        readFloat("wm_color_b0", gui_watermark_color_b[0]); readFloat("wm_color_b1", gui_watermark_color_b[1]); readFloat("wm_color_b2", gui_watermark_color_b[2]);
        readFloat("wm_pos_x", gui_watermark_pos_x);
        readFloat("wm_pos_y", gui_watermark_pos_y);
        readBool("wm_ply", gui_watermark_show_player);
        readBool("wm_srv", gui_watermark_show_server);
        readBool("wm_fps", gui_watermark_show_fps);
        readBool("wm_name", gui_watermark_show_name);
        readBool("wm_time", gui_watermark_show_time);
        readBool("wm_bg", gui_watermark_background);
        readBool("wm_bg_shadow", gui_watermark_background_shadow);
        readBool("wm_split_bg", gui_watermark_split_background);
        readBool("wm_text_shadow", gui_watermark_text_shadow);
        readBool("en_arraylist", gui_arraylist_enabled);
        readFloat("al_scale", gui_arraylist_scale);
        gui_arraylist_scale = ImClamp(gui_arraylist_scale, 0.5f, 3.0f);
        readFloat("al_bgc0", gui_arraylist_bg_color_4[0]); readFloat("al_bgc1", gui_arraylist_bg_color_4[1]); readFloat("al_bgc2", gui_arraylist_bg_color_4[2]); readFloat("al_bgc3", gui_arraylist_bg_color_4[3]);
        
        
        // "al_pos_x": 5.0,
        readBool("al_low", gui_arraylist_lowercase);
        readBool("al_shd", gui_arraylist_shadows);
        readBool("al_cbar", gui_arraylist_colorbar);
        readFloat("al_c0", gui_arraylist_color[0]); readFloat("al_c1", gui_arraylist_color[1]); readFloat("al_c2", gui_arraylist_color[2]);
        readFloat("al_ic0", gui_arraylist_info_color[0]); readFloat("al_ic1", gui_arraylist_info_color[1]); readFloat("al_ic2", gui_arraylist_info_color[2]);
        readFloat("al_spd", gui_arraylist_speed);
        readFloat("al_rad", gui_arraylist_radius);
        readFloat("al_px", gui_arraylist_pad_x); readFloat("al_py", gui_arraylist_pad_y);
        readFloat("al_posx", gui_arraylist_pos_x); readFloat("al_posy", gui_arraylist_pos_y);
        readBool("al_inf", gui_arraylist_show_info);
        readBool("al_not", features::misc::notifications::enabled);
        readInt("not_pos", features::misc::notifications::position);
        readBool("al_drg", g_ArrayListDragMode);
        readBool("al_blur", gui_arraylist_blur);
        readBool("al_bgsh", gui_arraylist_background_shadow);
        readFloat("al_bgsh_strength", gui_arraylist_shadow_strength);
        gui_arraylist_shadow_strength = ImClamp(gui_arraylist_shadow_strength, 0.0f, 100.0f);
        readFloat("al_bopac", gui_arraylist_blur_opacity);
        readBool("al_bg", gui_arraylist_background);
        readBool("al_watermark", gui_arraylist_watermark);
        readInt("al_mod", gui_arraylist_color_mode);
        // Load hidden modules from pipe-separated string
        if (line.find("\"al_hidden\"") != std::string::npos) {
            size_t q1 = line.find(": \"");
            size_t q2 = line.rfind("\"");
            if (q1 != std::string::npos && q2 > q1 + 3) {
                std::string hlist = line.substr(q1 + 3, q2 - q1 - 3);
                g_arraylist_hidden_modules.clear();
                std::string tok;
                for (char ch : hlist) {
                    if (ch == '|') { if (!tok.empty()) g_arraylist_hidden_modules.insert(tok); tok.clear(); }
                    else tok += ch;
                }
                if (!tok.empty()) g_arraylist_hidden_modules.insert(tok);
            }
        }
        if (line.find("\"ac_r\"") != std::string::npos ||
            line.find("\"ac_g\"") != std::string::npos ||
            line.find("\"ac_b\"") != std::string::npos)
            loadedAccentColor = true;
        if (line.find("\"gui_col_r\"") != std::string::npos ||
            line.find("\"gui_col_g\"") != std::string::npos ||
            line.find("\"gui_col_b\"") != std::string::npos)
            loadedGuiColor = true;
        readFloat("ac_r", g_AccentColor[0]); readFloat("ac_g", g_AccentColor[1]); readFloat("ac_b", g_AccentColor[2]);
        readFloat("gui_col_r", gui_guicolor_custom[0]); readFloat("gui_col_g", gui_guicolor_custom[1]); readFloat("gui_col_b", gui_guicolor_custom[2]);
        readBool("menu_header_animation", g_MenuHeaderAnim);
        readFloat("menu_header_background_r", g_MenuHeaderBgColor[0]);
        readFloat("menu_header_background_g", g_MenuHeaderBgColor[1]);
        readFloat("menu_header_background_b", g_MenuHeaderBgColor[2]);
        readFloat("menu_header_stars_r", g_MenuHeaderAnimColor[0]);
        readFloat("menu_header_stars_g", g_MenuHeaderAnimColor[1]);
        readFloat("menu_header_stars_b", g_MenuHeaderAnimColor[2]);
        readBool("en_autoarmor", gui_autoarmor_enabled);
        readFloat("autoarmor_delay", gui_autoarmor_delay);
        readBool("autoarmor_better", gui_autoarmor_only_better);
        readBool("en_blink", gui_blink_enabled);

        readBool("en_blockhit", gui_blockhit_enabled);
        readInt("gui_blockhit_mode", gui_blockhit_mode);
        readBool("gui_blockhit_mouse", gui_blockhit_require_mouse_down);
        readFloat("gui_blockhit_block_ticks", gui_blockhit_block_ticks);
        readFloat("gui_blockhit_unblock_ticks", gui_blockhit_unblock_ticks);
        readFloat("gui_blockhit_chance", gui_blockhit_chance);
        readBool("gui_blockhit_only_sword", gui_blockhit_only_sword);
        readBool("gui_blockhit_visual", gui_blockhit_visual_only);
        readBool("en_fastplace", gui_fastplace_enabled);
        readInt("gui_fastplace_held", gui_fastplace_held_item);
        readBool("en_autotool", gui_autotool_enabled);
        readFloat("gui_autotool_delay", gui_autotool_swap_delay);
        readBool("gui_autotool_swap_weapon", gui_autotool_swap_weapon);
        readBool("gui_autotool_instant", gui_autotool_instant_swap);
        readBool("gui_autotool_back", gui_autotool_swap_back);
        readBool("gui_autotool_mouse", gui_autotool_require_mouse_down);
        readBool("gui_autotool_sneak", gui_autotool_only_sneaking);
        readBool("gui_ac_break", gui_ac_break_blocks);
        // -- New modules --
        readBool("en_macros", gui_macros_enabled);
        readInt("gui_macros_mode", gui_macros_mode);
        readInt("gui_macros_bind", gui_macros_bind);
        readFloat("gui_macros_switch_delay", gui_macros_switch_delay);
        readFloat("gui_macros_use_delay", gui_macros_use_delay);
        readBool("gui_macros_auto_back", gui_macros_auto_switch_back);
        readInt("gui_armorsw_kit", gui_armorswitcher_kit1);
        readInt("gui_armorsw_kit1", gui_armorswitcher_kit1);
        readInt("gui_armorsw_kit2", gui_armorswitcher_kit2);
        readInt("gui_armorsw_bind", gui_armorswitcher_bind);
        readFloat("gui_armorsw_delay", gui_armorswitcher_delay);



        readFloat("gui_refill_delay", gui_refill_delay);
        readFloat("gui_refill_silent_delay", gui_refill_silent_delay);
        readFloat("gui_refill_silent_ticks", gui_refill_silent_ticks);

        readFloat("gui_min_cps", gui_min_cps);
        readFloat("gui_max_cps", gui_max_cps);
        readFloat("gui_inv_cps", gui_inv_cps);
        readBool("ac_break_blocks", gui_ac_break_blocks);
        readBool("ac_inv_en", features::combat::auto_click::inventory_enabled);
        readBool("ac_weap_only", features::combat::auto_click::weapons_only);
        readBool("ac_target_only", features::combat::auto_click::target_only);
        readBool("ac_prevent_unrefill", features::combat::auto_click::prevent_unrefill);
        readBool("ac_random", features::combat::auto_click::randomization);
        readFloat("ac_drop", features::combat::auto_click::drop_chance);
        readFloat("ac_spike", features::combat::auto_click::spike_chance);
        readInt("ac_click_method", gui_ac_click_method);
        readFloat("bard_delay", features::misc::bard_helper::switch_delay);
        readBool("bard_return", features::misc::bard_helper::return_last_slot);
        readBool("bard_resistance", features::misc::bard_helper::resistance);
        readBool("bard_strength", features::misc::bard_helper::strength);
        readBool("bard_regeneration", features::misc::bard_helper::regeneration);
        readBool("bard_speed", features::misc::bard_helper::speed);
        readBool("bard_jump", features::misc::bard_helper::jump);
        readInt("friends_nearby_bind", features::friends::nearby_bind);
        readFloat("friends_nearby_distance", features::friends::nearby_distance);
        readBool("nir_potions", features::movement::no_item_release::potions);
        readBool("nir_sword", features::movement::no_item_release::sword);
        readBool("nir_bow", features::movement::no_item_release::bow);
        readBool("nir_food", gui_noitemrelease_food);
        readBool("en_snaptap", features::movement::snap_tap::enabled);
        readBool("en_instantstop", features::movement::instant_stop::enabled);
        readBool("is_only_on_ground", features::movement::instant_stop::only_on_ground);
        readBool("is_stop_on_sneak", features::movement::instant_stop::stop_on_sneak);
        readFloat("is_stop_strength", features::movement::instant_stop::stop_strength);
        readBool("ts_enabled", features::movement::timer_speed::enabled);
        readFloat("ts_speed", features::movement::timer_speed::speed);
        readBool("ts_damage", features::movement::timer_speed::require_damage);
        readBool("ts_weapon", features::movement::timer_speed::only_weapon);
        readBool("ts_moving", features::movement::timer_speed::moving);
        readBool("bh_enabled", features::movement::bunnyhop::enabled);
        readBool("bh_liquid", features::movement::bunnyhop::liquid_check);
        readBool("bh_moving", features::movement::bunnyhop::only_moving);
        readFloat("bh_delay", features::movement::bunnyhop::jump_delay);
        readFloat("bh_height", features::movement::bunnyhop::jump_height);
        readFloat("bh_power", features::movement::bunnyhop::power);
        readFloat("bh_multiplier", features::movement::bunnyhop::speed_multiplier);
        readFloat("bh_slowdown", features::movement::bunnyhop::slowdown_factor);
        readFloat("bh_friction", features::movement::bunnyhop::friction);
        readFloat("bh_threshold", features::movement::bunnyhop::direction_threshold);
        readBool("ac_sound_enabled", features::combat::auto_click::click_sound);
        readInt("ac_sound", features::combat::auto_click::sound);
        readFloat("ac_volume", features::combat::auto_click::click_volume);
        readBool("rc_enabled", features::misc::right_clicker::enabled);
        readFloat("rc_min_cps", features::misc::right_clicker::min_cps);
        readFloat("rc_max_cps", features::misc::right_clicker::max_cps);
        readBool("rc_only_click", features::misc::right_clicker::only_click);
        readBool("rc_ignore_obsidian", features::misc::right_clicker::ignore_obsidian);
        readBool("rc_only_blocks", features::misc::right_clicker::only_blocks);
        readBool("ba_enabled", features::misc::bridge_assist::enabled);
        readFloat("ba_offset", features::misc::bridge_assist::edge_offset);
        readFloat("ba_delay", features::misc::bridge_assist::unsneak_delay);
        readInt("ba_select_blocks", features::misc::bridge_assist::select_blocks);
        readBool("ba_randomize", features::misc::bridge_assist::randomize);
        readBool("ba_sneak_jump", features::misc::bridge_assist::sneak_on_jump);
        readBool("ba_avoid_double", features::misc::bridge_assist::avoid_double_sneaking);
        readBool("ba_require_sneak", features::misc::bridge_assist::require_sneak);
        readBool("ba_holding_blocks", features::misc::bridge_assist::holding_blocks);
        readBool("ba_looking_down", features::misc::bridge_assist::looking_down);
        readBool("ba_not_forward", features::misc::bridge_assist::not_forward);
        readBool("gui_aa_silent", gui_aa_silent);
        readFloat("gui_aa_min_dist", gui_aa_min_dist); readFloat("gui_aa_max_dist", gui_aa_max_dist);
        readFloat("gui_aa_min_fov", gui_aa_min_fov); readFloat("gui_aa_max_fov", gui_aa_max_fov);
        readFloat("gui_aa_horizontal_speed", gui_aa_horizontal_speed); readFloat("gui_aa_vertical_speed", gui_aa_vertical_speed);
        readFloat("gui_reach_min_distance", gui_reach_min_distance); readFloat("gui_reach_max_distance", gui_reach_max_distance);
        readBool("gui_reach_hitbox_enabled", gui_reach_hitbox_enabled); readFloat("gui_reach_hitbox_size", gui_reach_hitbox_size);
        readFloat("gui_reach_chance", gui_reach_chance);
        readBool("gui_reach_ground_only", gui_reach_ground_only);
        readBool("gui_reach_weapon_only", gui_reach_weapon_only);
        readBool("gui_reach_liquid_check", gui_reach_liquid_check);
        readBool("gui_reach_combo_mode", gui_reach_combo_mode);
        readBool("gui_reach_hit_through_walls", gui_reach_hit_through_walls);
        readInt("gui_velo_mode", gui_velo_mode);
        readFloat("gui_velo_horizontal", gui_velo_horizontal);
        readFloat("gui_velo_vertical", gui_velo_vertical);
        readFloat("gui_velo_chance", gui_velo_chance);
        readFloat("gui_velo_delay", gui_velo_delay);
        readBool("gui_velo_air_only", gui_velo_air_only);
        readBool("gui_velo_moving_only", gui_velo_moving_only);
        readBool("gui_velo_weapon_only", gui_velo_weapon_only);
        readBool("gui_velo_push_back", gui_velo_push_back);
        readBool("gui_velo_clicking_only", gui_velo_clicking_only);
        
        readFloat("gui_tracers_thickness", gui_tracers_thickness);
        readBool("gui_tracers_draw_distance", gui_tracers_draw_distance);
        readBool("gui_tracers_draw_hurt_time", gui_tracers_draw_hurt_time);
        readBool("gui_tracers_draw_invisible", gui_tracers_draw_invisible);
        readFloat("gui_blink_timer_limit", gui_blink_timer_limit);
        readBool("gui_blink_show_path", gui_blink_show_path);
        readBool("gui_blink_show_timer", gui_blink_show_timer);
        readBool("gui_nametags_draw_health", gui_nametags_draw_health);
        readInt("gui_nametags_health_format", gui_nametags_health_format);
        readFloat("gui_nametags_health_segments", gui_nametags_health_segments);
        readBool("gui_nametags_show_name", gui_nametags_show_name);
        readBool("gui_nametags_show_own", gui_nametags_show_own);
        readBool("gui_nametags_hide_vanilla", gui_nametags_hide_vanilla);
        readBool("gui_nametags_show_equipment", gui_nametags_show_equipment);
        readBool("gui_nametags_show_enchantments", gui_nametags_show_enchantments);
        readInt("hitmarkers_mode", features::visual::hit_markers::mode);
        readFloat("hitmarkers_size", features::visual::hit_markers::size);
        readFloat("hitmarkers_width", features::visual::hit_markers::line_width);
        readFloat("hitmarkers_duration", features::visual::hit_markers::duration);
        readBool("hitmarkers_fade", features::visual::hit_markers::fade_out);
        readBool("hitmarkers_scale_anim", features::visual::hit_markers::scale_animation);
        readFloat("hitmarkers_scale_amount", features::visual::hit_markers::scale_amount);
        readBool("hitmarkers_outline", features::visual::hit_markers::outline);
        readFloat("hitmarkers_outline_width", features::visual::hit_markers::outline_width);
        for (int i = 0; i < 4; ++i) { readFloat("hitmarkers_color" + std::to_string(i), features::visual::hit_markers::color[i]); readFloat("hitmarkers_outline_color" + std::to_string(i), features::visual::hit_markers::outline_color[i]); }
        readBool("gui_nametags_draw_distance", gui_nametags_draw_distance);
        readBool("gui_nametags_draw_hurt_time", gui_nametags_draw_hurt_time);
        readBool("gui_nametags_draw_invisible", gui_nametags_draw_invisible);
        readBool("gui_nametags_background", gui_nametags_background);        readBool("gui_nametags_use_fake_name", gui_nametags_use_fake_name);
        readFloat("gui_nametags_scale", gui_nametags_scale);
        gui_nametags_scale = ImClamp(gui_nametags_scale, 0.85f, 4.00f);
        readBool("gui_nametags_auto_scale", gui_nametags_distance_scaling); // Legacy configs.
        readBool("gui_nametags_distance_scaling", gui_nametags_distance_scaling);

        readInt("gui_aa_mode", features::combat::aim_assist::mode);
        features::combat::aim_assist::mode = ImClamp(features::combat::aim_assist::mode, 0, 1);
        readInt("gui_aa_aim_mode", features::combat::aim_assist::aim_mode);
        readInt("gui_aa_priority", features::combat::aim_assist::priority);
        readInt("gui_aa_target_mode", features::combat::aim_assist::target_mode);
        readBool("gui_aa_weapons_only", features::combat::aim_assist::weapons_only);
        readBool("gui_aa_break_blocks", features::combat::aim_assist::break_blocks);
        readBool("gui_aa_clicking_only", features::combat::aim_assist::clicking_only);
        readBool("gui_aa_through_walls", features::combat::aim_assist::through_walls);
        readBool("gui_aa_ignore_invisible", features::combat::aim_assist::ignore_invisible);
        readBool("friends_enabled", gui_friends_enabled);
        readInt("friends_add_bind", gui_friends_add_bind);
        // Colors (RGBA)
        readFloat("tracers_c0", gui_tracers_color_4[0]); readFloat("tracers_c1", gui_tracers_color_4[1]); readFloat("tracers_c2", gui_tracers_color_4[2]); readFloat("tracers_c3", gui_tracers_color_4[3]);
        readFloat("nametags_c0", gui_nametags_color[0]); readFloat("nametags_c1", gui_nametags_color[1]); readFloat("nametags_c2", gui_nametags_color[2]); readFloat("nametags_c3", gui_nametags_color[3]);
        readHexString("nametags_fake_name", gui_nametags_fake_name, sizeof(gui_nametags_fake_name));

        for (auto& module : modules) {
            if (module.enabledPtr) readBool(ConfigModuleKey(module.name), *module.enabledPtr);
            for (auto& setting : module.settings) {
                const std::string key = ConfigSettingKey(module.name, setting.name);
                switch (setting.type) {
                case Setting::TOGGLE:
                    if (setting.boolPtr) readBool(key, *setting.boolPtr);
                    break;
                case Setting::SLIDER:
                    if (setting.floatPtr) readFloat(key, *setting.floatPtr);
                    break;
                case Setting::DROPDOWN:
                    if (setting.intPtr) readInt(key, *setting.intPtr);
                    break;
                case Setting::COLOR:
                    if (setting.colorPtr) {
                        for (int i = 0; i < 3; ++i)
                            readFloat(key + "_" + std::to_string(i), setting.colorPtr[i]);
                    }
                    break;
                case Setting::COLOR4:
                    if (setting.colorPtr) {
                        for (int i = 0; i < 4; ++i)
                            readFloat(key + "_" + std::to_string(i), setting.colorPtr[i]);
                    }
                    break;
                default:
                    break;
                }
            }
        }

        // keybinds
        for (auto& m : modules) {
            std::string key = "bind_";
            key += m.name;
            for (char& c : key) if (c == ' ') c = '_';
            readInt(key, m.keybind);
        }
        // friends list
        if (line.find("\"friends_list\"") != std::string::npos) {
            size_t q1 = line.find(": \"");
            size_t q2 = line.rfind("\"");
            if (q1 != std::string::npos && q2 > q1 + 3) {
                std::string tlist = line.substr(q1 + 3, q2 - q1 - 3);
                std::vector<std::string> loadedFriendNames;
                std::vector<std::string> loadedFriendIds;
                std::string tok;
                for (char ch : tlist) {
                    if (ch == '|') {
                        if (!tok.empty()) {
                            const size_t separator = tok.rfind('@');
                            loadedFriendNames.push_back(separator == std::string::npos ? tok : tok.substr(0, separator));
                            loadedFriendIds.push_back(separator == std::string::npos ? "" : tok.substr(separator + 1));
                        }
                        tok.clear();
                    }
                    else tok += ch;
                }
                if (!tok.empty()) {
                    const size_t separator = tok.rfind('@');
                    loadedFriendNames.push_back(separator == std::string::npos ? tok : tok.substr(0, separator));
                    loadedFriendIds.push_back(separator == std::string::npos ? "" : tok.substr(separator + 1));
                }
                features::friends::replace_all(loadedFriendNames, loadedFriendIds);
                for (size_t i = 0; i < loadedFriendNames.size(); ++i) {
                    if (i >= loadedFriendIds.size() || loadedFriendIds[i].empty())
                        features::friends::request_uuid(loadedFriendNames[i]);
                    else
                        features::friends::verify_uuid(loadedFriendIds[i]);
                }
            }
        }
    }
    for (auto& m : modules) {
        g_KeybindMap[m.name] = m.keybind;
        
        if (m.name == "Blink")         features::latency::blink::bind = m.keybind;
        if (m.name == "Refill")        features::combat::refill::bind = m.keybind;
        if (m.name == "ArmorSwitcher") gui_armorswitcher_bind = m.keybind;
    }
    // gui_col_* is the value edited by the current UI picker. Keep ac_* as a
    // backward-compatible fallback for older configs that did not store it.
    if (loadedGuiColor) {
        for (int i = 0; i < 3; ++i) g_AccentColor[i] = gui_guicolor_custom[i];
    } else if (loadedAccentColor) {
        for (int i = 0; i < 3; ++i) gui_guicolor_custom[i] = g_AccentColor[i];
    }
    g_AccentColor[3] = 1.0f;
    gui_armorswitcher_kit1 = std::clamp(gui_armorswitcher_kit1, 0, 4);
    gui_armorswitcher_kit2 = std::clamp(gui_armorswitcher_kit2, 0, 4);
    g_GuiScaleIndex = ImClamp(g_GuiScaleIndex, 0, 3);
    static const float savedGuiScales[] = { 1.0f, 1.10f, 1.20f, 1.30f };
    g_GuiScaleTarget = savedGuiScales[g_GuiScaleIndex];
    c::accent = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.0f);
    c::accent_gradient = ImVec4(
        g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.0f);
    c::text::text_active = c::accent;

    ++g_ConfigLoadRevision;
}

static void SaveConfigTypes(); // forward declaration

void DeleteConfig(std::string name) {
    if (g_ActiveConfig == name)
        g_ActiveConfig.clear();
    g_InMemoryConfigs.erase(name);
    g_ConfigTypeMap.erase(name);
    g_ConfigDateMap.erase(name);
    Registry_DeleteConfig(name);
    RefreshConfigs();
    // Persist updated type map
    SaveConfigTypes();
}

// ============================================================
// CONFIG TYPE METADATA PERSISTENCE
// ============================================================
static void SaveConfigTypes() {
    std::string data;
    for (auto& kv : g_ConfigTypeMap) {
        if (!data.empty()) data += "|";
        std::string dt = g_ConfigDateMap[kv.first];
        if (dt.empty()) dt = "Unknown";
        data += kv.first + ":" + std::to_string(kv.second) + ":" + dt;
    }
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, CFG_REG_KEY, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "__cfg_types__", 0, REG_BINARY,
            (const BYTE*)data.c_str(), (DWORD)data.size());
        
        RegDeleteValueA(hKey, "__autoload__");
            
        RegCloseKey(hKey);
    }
}

static void LoadConfigTypes() {
    HKEY hKey;
    const std::string legacyRegistryKey = LegacyConfigRegistryKey();
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CFG_REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS &&
        RegOpenKeyExA(HKEY_CURRENT_USER, legacyRegistryKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    std::vector<BYTE> buf(8192);
    DWORD dataSize = (DWORD)buf.size(), type = 0;
    if (RegQueryValueExA(hKey, "__cfg_types__", nullptr, &type, buf.data(), &dataSize) == ERROR_SUCCESS && type == REG_BINARY) {
        std::string raw((char*)buf.data(), dataSize);
        std::string token;
        for (size_t i = 0; i <= raw.size(); i++) {
            if (i == raw.size() || raw[i] == '|') {
                size_t sep1 = token.find(':');
                if (sep1 != std::string::npos) {
                    size_t sep2 = token.find(':', sep1 + 1);
                    std::string cfgName = token.substr(0, sep1);
                    int cfgType = atoi(token.substr(sep1 + 1, sep2 != std::string::npos ? sep2 - sep1 - 1 : std::string::npos).c_str());
                    if (cfgType >= 0 && cfgType <= 2) {
                        g_ConfigTypeMap[cfgName] = cfgType;
                        if (sep2 != std::string::npos) {
                            g_ConfigDateMap[cfgName] = token.substr(sep2 + 1);
                        } else {
                            g_ConfigDateMap[cfgName] = "Unknown";
                        }
                    }
                }
                token.clear();
            } else {
                token += raw[i];
            }
        }
    }
    
    RegCloseKey(hKey);
}
