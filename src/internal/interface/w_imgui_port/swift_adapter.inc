#include <deque>
#include "render/fonts/fa6.hh"

// We map our internal names to the correct FA6 names if needed, but since fa6.hh has them:
#define ICON_FA_SWORDS "\xef\x9c\x9d"
#define ICON_FA_RABBIT_RUNNING ICON_FA_PERSON_RUNNING
// We will use the proper FA6 names for the rest.

extern void TriggerNotification(const char* title, const char* body, const char* tag);

namespace swift_w_port {
static constexpr float k_base_window_width = 840.f;
static constexpr float k_base_window_height = 620.f;
struct ColorBinding { float* source{}; bool alpha{}; hue::c_color value{}; };
struct KeyBinding { Module* module{}; framework::key_var_t value{}; };
struct RawKeyBinding { int* source{}; framework::key_var_t value{}; };
struct HiddenModuleBinding { std::string name{}; bool value{}; const Module* module{}; };
static void add_extras(const std::shared_ptr<framework::c_window>& target);
static void add_friends_panel(const std::shared_ptr<framework::c_window>& target);
static void sync_extras(bool before_input);

static std::shared_ptr<framework::c_window> window;
static std::deque<ColorBinding> colors;
static std::deque<std::shared_ptr<framework::c_colorpicker>> colorpickers;
static std::deque<KeyBinding> keys;
static std::deque<RawKeyBinding> raw_keys;
static std::deque<HiddenModuleBinding> hidden_modules;
static std::shared_ptr<framework::c_listbox> friends_listbox;
static std::string fake_name_value = "";
static int selected_friend = 0;
static bool initialized = false;

bool menu_intersects_rect(float left, float top, float right, float bottom) {
    if (!::g_MenuVisible || !window) return false;
    const auto pos = window->get_pos();
    const auto size = window->get_size();
    return right >= pos.x && left <= pos.x + size.x &&
           bottom >= pos.y && top <= pos.y + size.y;
}

void invalidate_context() {
    ::g_NumericEditActive = false;
    ::g_ConfigInputActive = false;
    window.reset();
    colors.clear();
    colorpickers.clear();
    keys.clear();
    raw_keys.clear();
    hidden_modules.clear();
    friends_listbox.reset();
    selected_friend = 0;
    initialized = false;
    framework::g_search.reset_context();
    if (framework::g_ctx) {
        framework::g_ctx->m_modal_owner = nullptr;
        framework::g_ctx->m_hovered = nullptr;
        framework::g_ctx->m_focus_stack.clear();
        framework::g_ctx->m_click_consumed = false;
        framework::g_ctx->m_tabs.clear();
    }
    utils::stack.clear();
    utils::vector_stack.clear();
}
static const char* category_name(int category) {
    static const char* names[] = { "Combat", "Visuals", "Movement", "Latency", "Misc", "Settings", "Settings" };
    return category >= CAT_COMBAT && category <= CAT_CONFIGS ? names[category] : "Misc";
}

static void apply_visibility(const std::shared_ptr<framework::c_base_element>& control,
                             const Setting& setting) {
    if (!control) return;
    if (setting.visibleCondition) control->set_callback_visibility(setting.visibleCondition);
    if (setting.inlineWithPrevious) control->set_inlined();
}

static void initialize() {
    if (initialized || modules.empty()) return;
    initialized = true;

    fake_name_value = gui_nametags_fake_name;
    for (const auto& candidate : modules) {
        if (candidate.name == "Arraylist" || candidate.name == "Array List" || candidate.name == "Notifications" || candidate.name == "Watermark") continue;
        if (candidate.enabledPtr) {
            const std::string display = candidate.name == "Left Clicker" ? "Auto Clicker" : candidate.name;
            hidden_modules.push_back({ display, g_arraylist_hidden_modules.count(display) > 0, &candidate });
        }
    }
    // Hardcoded sub-modules that the user wants to hide
    hidden_modules.push_back({ "NoHitDelay", g_arraylist_hidden_modules.count("NoHitDelay") > 0, nullptr });
    hidden_modules.push_back({ "NoJumpDelay", g_arraylist_hidden_modules.count("NoJumpDelay") > 0, nullptr });

    ImGuiIO& io = ImGui::GetIO();
    window = std::make_shared<framework::c_window>("", math::c_vector_2d(
        io.DisplaySize.x * 0.5f - k_base_window_width * 0.5f,
        io.DisplaySize.y * 0.5f - k_base_window_height * 0.5f),
        math::c_vector_2d(k_base_window_width, k_base_window_height));
    window->prebuild_tabs([](framework::c_tab* tabs) {
        tabs->create_tab(ICON_FA_BULLSEYE, "Combat", {});
        tabs->create_tab(ICON_FA_EYE, "Visuals", {});
        tabs->create_tab(ICON_FA_WHEELCHAIR_MOVE, "Movement", {});
        tabs->create_tab(ICON_FA_WIFI, "Latency", {});
        tabs->create_tab(ICON_FA_BARS, "Misc", {});
        tabs->create_tab(ICON_FA_GEAR, "Settings", {});
    });
    window->finish_tab_prebuild();
    add_friends_panel(window);

    for (auto& module : modules) {
        if (module.hidden || module.category == CAT_SETTINGS || module.name == "Friends") continue;
        // Keep module cards compact enough to coexist with the fixed account footer.
        // The controls themselves use a 6 px row gap, so the estimate must match it.
        float estimated = 101.f + (float)module.settings.size() * 30.f;
        for (const Setting& setting : module.settings) {
            if (setting.type == Setting::SLIDER) estimated += 8.f;
        }
        if (module.name == "Left Clicker" || module.name == "Right Clicker") estimated += 32.f;
        const float height = std::clamp(estimated, 50.f, 500.f);
        Module* module_ptr = &module;
        window->build_child(module.name, framework::child_width::half, height,
            [module_ptr](framework::c_child* child) {
                child->set_auto_fit_height();
                child->attach_child(category_name(module_ptr->category), "", module_ptr->category);
                if (module_ptr->enabledPtr || module_ptr->name == "Refill" || module_ptr->name == "ArmorSwitcher") {
                    keys.push_back({ module_ptr, { module_ptr->keybind, framework::key_mode_t::toggle } });
                    KeyBinding* binding = &keys.back();
                    if (module_ptr->enabledPtr) {
                        auto enabled = child->add_checkbox("Enabled", module_ptr->enabledPtr);
                        if (module_ptr->name == "Refill")
                            enabled->require_to_enable([binding] {
                                return binding->value.key != 0 && features::combat::refill::bind != 0;
                            });
                        else if (module_ptr->name == "ArmorSwitcher" || module_ptr->name == "BardHelper")
                            enabled->require_to_enable([binding] { return binding->value.key != 0; });
                    }
                    auto bind = child->add_keybind("Bind", &binding->value, module_ptr->enabledPtr != nullptr);
                    if (module_ptr->enabledPtr) bind->set_inlined();
                    bind->set_simple();
                }
                if (module_ptr->name == "Left Clicker") {
                    auto cps_range = child->add_slider_float("CPS Range", &gui_min_cps, 1.0f, 25.0f);
                    cps_range->set_range_max(&gui_max_cps);
                }
                if (module_ptr->name == "Right Clicker") {
                    auto cps_range = child->add_slider_float("CPS Range", &features::misc::right_clicker::min_cps, 1.0f, 25.0f);
                    cps_range->set_range_max(&features::misc::right_clicker::max_cps);
                }
                for (const Setting& setting : module_ptr->settings) {
                    std::shared_ptr<framework::c_base_element> control;
                    switch (setting.type) {
                    case Setting::TOGGLE:
                        if (setting.boolPtr) control = child->add_checkbox(setting.name, setting.boolPtr);
                        break;
                    case Setting::SLIDER:
                        if (setting.floatPtr) {
                            auto slider = child->add_slider_float(setting.name, setting.floatPtr, setting.min, setting.max);
                            if (module_ptr->name == "Aim Assist" && setting.name == "Distance") slider->set_range_max(&gui_aa_max_dist);
                            if (module_ptr->name == "Aim Assist" && setting.name == "Field Of View") slider->set_range_max(&gui_aa_max_fov);
                            control = slider;
                        }
                        break;
                    case Setting::DROPDOWN:
                        if (setting.name == "Elements") {
                            control = child->add_multibox("Elements", false, [](framework::c_multidropdown* box) {
                                box->add_selection("Name", &gui_watermark_show_name);
                                box->add_selection("FPS", &gui_watermark_show_fps);
                                box->add_selection("Server", &gui_watermark_show_server);
                                box->add_selection("Player", &gui_watermark_show_player);
                                box->add_selection("Time", &gui_watermark_show_time);
                            });
                        } else if (setting.name == "Hide Modules") {
                            control = child->add_multibox("Hide Modules", false, [](framework::c_multidropdown* box) {
                                for (auto& item : hidden_modules) {
                                    const bool* enabled = item.module ? item.module->enabledPtr :
                                        item.name == "NoHitDelay" ? &gui_nohitdelay_enabled :
                                        item.name == "NoJumpDelay" ? &gui_nojumpdelay_enabled : nullptr;
                                    box->add_selection(item.name, &item.value, [enabled]() {
                                        return enabled && *enabled;
                                    });
                                }
                            });
                        } else if (setting.intPtr) control = child->add_dropdown(setting.name, setting.intPtr, setting.dropdownItems);
                        break;
                    case Setting::MULTIBOX:
                        control = child->add_multibox(setting.name, false, [&setting](framework::c_multidropdown* box) {
                            for (const auto& item : setting.multiboxItems) {
                                if (item.second) box->add_selection(item.first, item.second);
                            }
                        });
                        break;
                    case Setting::COLOR:
                    case Setting::COLOR4:
                        if (setting.colorPtr) {
                            const bool alpha = setting.type == Setting::COLOR4;
                            colors.push_back({ setting.colorPtr, alpha,
                                hue::c_color((int)(setting.colorPtr[0] * 255.f),
                                             (int)(setting.colorPtr[1] * 255.f),
                                             (int)(setting.colorPtr[2] * 255.f),
                                             alpha ? (int)(setting.colorPtr[3] * 255.f) : 255) });
                            auto picker = child->add_colorpicker(setting.name, &colors.back().value);
                            colorpickers.push_back(picker);
                            control = picker;
                        }
                        break;
                    case Setting::LABEL:
                        control = child->add_button(setting.name, [] {});
                        break;
                    case Setting::BUTTON:
                        control = child->add_button(setting.name, [] {});
                        break;
                    }
                    apply_visibility(control, setting);
                }
                if (module_ptr->name == "Nametags") {
                    auto fake_name = child->add_input_box("Fake Name", &fake_name_value);
                    fake_name->set_placeholder("Enter fake name");
                    fake_name->set_callback_visibility([] { return gui_nametags_use_fake_name; });
                }
            });
    }
    add_extras(window);
}

static void render() {
    static uint64_t rendered_revision = g_ConfigLoadRevision;
    if (rendered_revision != g_ConfigLoadRevision) {
        invalidate_context();
        rendered_revision = g_ConfigLoadRevision;
    }
    initialize();
    if (!window) return;
    const uint64_t config_revision_before_input = g_ConfigLoadRevision;
    for (auto& binding : colors) {
        binding.value.r = (uint8_t)std::clamp((int)(binding.source[0] * 255.f), 0, 255);
        binding.value.g = (uint8_t)std::clamp((int)(binding.source[1] * 255.f), 0, 255);
        binding.value.b = (uint8_t)std::clamp((int)(binding.source[2] * 255.f), 0, 255);
        if (binding.alpha) binding.value.a = (uint8_t)std::clamp((int)(binding.source[3] * 255.f), 0, 255);
    }

    static std::vector<int> prev_keys;
    if (prev_keys.size() != keys.size()) prev_keys.resize(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        keys[i].value.key = keys[i].module->keybind;
        prev_keys[i] = keys[i].value.key;
    }

    static std::vector<int> prev_raw_keys;
    if (prev_raw_keys.size() != raw_keys.size()) prev_raw_keys.resize(raw_keys.size());
    for (size_t i = 0; i < raw_keys.size(); ++i) {
        raw_keys[i].value.key = *raw_keys[i].source;
        prev_raw_keys[i] = raw_keys[i].value.key;
    }

    if (!framework::g_ctx || framework::g_ctx->top_focus() == nullptr)
        fake_name_value = gui_nametags_fake_name;
    sync_extras(true);
    if (friends_listbox) friends_listbox->set_items(g_FriendsList);
    for (auto& item : hidden_modules)
        item.value = g_arraylist_hidden_modules.count(item.name) > 0;

    framework::g_style->m_accent = hue::c_color(
        (int)(g_AccentColor[0] * 255.f), (int)(g_AccentColor[1] * 255.f),
        (int)(g_AccentColor[2] * 255.f));
        
    static int applied_scale_index = std::clamp(g_GuiScaleIndex, 0, 3);
    const int requested_scale_index = std::clamp(g_GuiScaleIndex, 0, 3);
    static const float rendered_scales[] = { 1.f, 1.1f, 1.2f, 1.3f };
    static bool was_visible = ::g_MenuVisible;
    const bool opening_menu = ::g_MenuVisible && !was_visible;
    if (opening_menu && requested_scale_index != applied_scale_index) {
        applied_scale_index = requested_scale_index;
        ImGui::GetIO().FontGlobalScale = rendered_scales[applied_scale_index];
        was_visible = ::g_MenuVisible;
        invalidate_context();
        return;
    }
    was_visible = ::g_MenuVisible;
    framework::g_ctx->m_open = ::g_MenuVisible;
    if (!::g_MenuVisible && framework::g_search.m_should_draw) {
        framework::g_search.m_should_draw = false;
        framework::g_search.cleanup();
    }
    const float gui_scale = rendered_scales[applied_scale_index];
    ImGui::GetIO().FontGlobalScale = gui_scale;
    window->set_size(math::c_vector_2d(k_base_window_width * gui_scale,
        k_base_window_height * gui_scale));
    static float dim_opacity = 0.f;
    const float dim_target = (g_BackgroundDim && ::g_MenuVisible) ? 1.0f : 0.0f;
    dim_opacity += (dim_target - dim_opacity) * std::clamp(ImGui::GetIO().DeltaTime * 9.f, 0.f, 1.f);
    
    if (dim_opacity > 0.01f) {
        ImDrawList* background = ImGui::GetBackgroundDrawList();
        const float screenH = ImGui::GetIO().DisplaySize.y;
        const float screenW = ImGui::GetIO().DisplaySize.x;

        float topAlpha = 225.0f * dim_opacity;    
        float bottomAlpha = 100.0f * dim_opacity; 

        background->AddRectFilledMultiColor(
            ImVec2(0.0f, 0.0f),
            ImVec2(screenW, screenH),
            IM_COL32(0, 0, 0, (int)topAlpha),
            IM_COL32(0, 0, 0, (int)topAlpha),
            IM_COL32(0, 0, 0, (int)bottomAlpha),
            IM_COL32(0, 0, 0, (int)bottomAlpha)
        );
    }
    framework::g_ctx->m_click_consumed = false;
    g_render->set_draw_list(ImGui::GetForegroundDrawList());
    g_render->begin_layers();
    ::g_ConfigInputActive = false;
    window->input();
    window->paint();
    if (framework::g_search.m_should_draw || framework::g_search.m_search_anim.val() > 0.01f)
        framework::g_search.paint_database();
    g_render->end_layers();
    const auto focus = framework::g_ctx->top_focus();
    ::g_ConfigInputActive = ::g_ConfigInputActive || (focus && (focus->m_type == framework::element_type::text_input ||
        focus->m_type == framework::element_type::listbox));

    const bool config_loaded_during_input =
        g_ConfigLoadRevision != config_revision_before_input;
    for (auto& binding : colors) {
        if (config_loaded_during_input) {
            binding.value.r = (uint8_t)std::clamp((int)(binding.source[0] * 255.f), 0, 255);
            binding.value.g = (uint8_t)std::clamp((int)(binding.source[1] * 255.f), 0, 255);
            binding.value.b = (uint8_t)std::clamp((int)(binding.source[2] * 255.f), 0, 255);
            if (binding.alpha)
                binding.value.a = (uint8_t)std::clamp((int)(binding.source[3] * 255.f), 0, 255);
        } else {
            binding.source[0] = binding.value.r / 255.f;
            binding.source[1] = binding.value.g / 255.f;
            binding.source[2] = binding.value.b / 255.f;
            if (binding.alpha) binding.source[3] = binding.value.a / 255.f;
        }
    }
    if (config_loaded_during_input) {
        for (auto& picker : colorpickers) {
            if (picker) picker->sync_from_value();
        }
    }
    if (config_loaded_during_input) return;
    gui_min_cps = std::clamp(gui_min_cps, 1.0f, 25.0f);
    for (int i = 0; i < 3; ++i) g_AccentColor[i] = gui_guicolor_custom[i];
    if (fake_name_value.size() >= sizeof(gui_nametags_fake_name))
        fake_name_value.resize(sizeof(gui_nametags_fake_name) - 1);
    strncpy_s(gui_nametags_fake_name, sizeof(gui_nametags_fake_name),
        fake_name_value.c_str(), _TRUNCATE);

    gui_max_cps = std::clamp(gui_max_cps, gui_min_cps, 25.0f);
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].value.key != prev_keys[i]) {
            keys[i].module->keybind = keys[i].value.key;
            g_KeybindMap[keys[i].module->name] = keys[i].value.key;
            if (keys[i].value.key != 0) {
                const std::string key_name = GetKeyName(keys[i].value.key);
                TriggerNotification("Keybind set to", key_name.c_str(), "KEYBIND");
                g_KeybindBlockedUntil[keys[i].module->name] = GetTickCount64() + 3500;
                keyStates[keys[i].value.key] =
                    (GetAsyncKeyState(keys[i].value.key) & 0x8000) != 0;
            }
            if (keys[i].module->name == "Refill")
                features::combat::refill::bind = keys[i].value.key;
            else if (keys[i].module->name == "Blink")
                features::latency::blink::bind = keys[i].value.key;
            else if (keys[i].module->name == "ArmorSwitcher")
                gui_armorswitcher_bind = keys[i].value.key;
        }
    }
    for (size_t i = 0; i < raw_keys.size(); ++i) {
        if (raw_keys[i].value.key != prev_raw_keys[i]) {
            *raw_keys[i].source = raw_keys[i].value.key;
        }
    }
    g_arraylist_hidden_modules.clear();
    for (const auto& item : hidden_modules) if (item.value) g_arraylist_hidden_modules.insert(item.name);
    sync_extras(false);
}
} // namespace swift_w_port
