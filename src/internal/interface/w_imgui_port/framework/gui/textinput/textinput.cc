#include "../../../includes.hh"

namespace framework
{
    static constexpr float k_fade_dur = 0.15f;
    static constexpr float k_offset_px = 3.0f;

    static std::string to_upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return toupper(c); });
        return s;
    }

    c_text_input::c_text_input(std::string label, std::string* var, bool hide_label)
        : m_var(var)
    {
        m_label = std::move(label);
        m_placeholder = m_label;
        m_hide_label = hide_label;
        m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 30 };
        m_type = element_type::text_input;
        m_focus_priority = focus_priority::interactive;
        m_parent_width = m_child_size;
    }

    void c_text_input::draw()
    {
        animations::m_textinput_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
        animations::m_textinput_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_textinput_value", m_visible && g_ctx->top_focus() == this && g_ctx->m_open, 0.5f);
        animations::m_textinput_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_textinput_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

        float target_opacity = 0.2f;
        if (animations::m_textinput_value.val() > 0.f)
            target_opacity = 0.2f + (0.6f * animations::m_textinput_value.val());
        else if (animations::m_textinput_hover.val() > 0.f)
            target_opacity = 0.2f + (0.2f * animations::m_textinput_hover.val());

        static std::unordered_map<std::string, float> smooth_opacity_cache;
        float& smooth_opacity = smooth_opacity_cache[m_parent + m_label + "#smooth_opacity"];
        smooth_opacity += (target_opacity - smooth_opacity) * 0.3f;

        const float final_opacity = animations::m_textinput_opacity.val() * smooth_opacity;
        auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);


        g_render->use_layer(m_layer, [&]()
            {
                if (!m_hide_label)
                    g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));

                const auto box_pos = m_pos + position;

                g_render->rect_shadow((m_pos + position).x, (m_pos + position).y, m_child_size, 25.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3).val()), 8.f, 3.f);
                animations::m_window_opacity.restore();
                g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, 25.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 3.f);

                const float text_width = g_font->f_childs.measure(*m_var).x;
                const float text_scroll = std::max(0.f, text_width - (m_child_size - 16.f));
                g_render->push_clip(box_pos.x + 4.f, box_pos.y, m_child_size - 8.f, 25.f);

                if (g_ctx->is_focused(this))
                {
                    const float base_y = this->m_pos.y + g_font->f_childs.measure(m_label).y + 8.f;
                    if (m_var->size() > 128)
                    {
                        const std::string tail = m_var->substr(m_var->size() - 128);
                        const float tail_width = g_font->f_childs.measure(tail).x;
                        const float tail_scroll = std::max(0.f, tail_width - (m_child_size - 16.f));
                        if (m_char_anims.size() != tail.size())
                            m_char_anims.assign(tail.size(), 1.f);
                        if (m_var->size() > m_last_value_size && !m_char_anims.empty())
                            m_char_anims.back() = 0.f;

                        float cursor_x = this->m_pos.x + 8.f - tail_scroll;
                        const float dt = ImGui::GetIO().DeltaTime;
                        for (size_t i = 0; i < tail.size(); ++i)
                        {
                            m_char_anims[i] += (1.f - m_char_anims[i]) * dt * 12.f;
                            const float t = std::clamp(m_char_anims[i], 0.f, 1.f);
                            const float eased = t * t * (3.f - 2.f * t);
                            const char glyph[2] = { tail[i], '\0' };
                            const auto glyph_size = g_font->f_childs.measure(glyph);
                            const float center_x = cursor_x + glyph_size.x * 0.5f;
                            const float center_y = base_y + glyph_size.y * 0.5f;
                            const int first_vertex = g_render->draw_list()->VtxBuffer.Size;
                            g_font->f_childs.text(cursor_x, base_y, glyph,
                                g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                            animations::m_window_opacity.restore();
                            const int last_vertex = g_render->draw_list()->VtxBuffer.Size;
                            ImDrawVert* vertices = g_render->draw_list()->VtxBuffer.Data;
                            for (int vertex = first_vertex; vertex < last_vertex; ++vertex)
                            {
                                vertices[vertex].pos.x = center_x + (vertices[vertex].pos.x - center_x) * eased;
                                vertices[vertex].pos.y = center_y + (vertices[vertex].pos.y - center_y) * eased;
                                const int alpha = (int)(((vertices[vertex].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
                                vertices[vertex].col = (vertices[vertex].col & ~IM_COL32_A_MASK) |
                                    (alpha << IM_COL32_A_SHIFT);
                            }
                            cursor_x += glyph_size.x;
                        }
                        m_char_removing.assign(tail.size(), false);
                        m_ghost_chars.clear();
                    }
                    else
                    {
                    while (m_char_anims.size() < this->m_var->size())
                    {
                        m_char_anims.push_back(0.f);
                        m_char_removing.push_back(false);
                    }

                    float dt = ImGui::GetIO().DeltaTime;
                    float speed = 12.f;

                    float cursor_x = this->m_pos.x + 8.f - text_scroll;
                    for (int i = 0; i < (int)m_ghost_chars.size(); i++)
                    {
                        auto& ghost = m_ghost_chars[i];
                        ghost.m_anim += (0.f - ghost.m_anim) * dt * speed;

                        if (ghost.m_anim < 0.01f) {
                            m_ghost_chars.erase(m_ghost_chars.begin() + i);
                            i--;
                            continue;
                        }

                        float t = ghost.m_anim;
                        float eased = t * t * (3.f - 2.f * t);

                        char buf[2] = { ghost.m_glyph, '\0' };
                        auto char_size = g_font->f_childs.measure(buf);
                        float char_center_x = ghost.m_x + char_size.x * 0.5f;
                        float char_center_y = base_y + char_size.y * 0.5f;

                        int vtx_start = g_render->draw_list()->VtxBuffer.Size;

                        g_font->f_childs.text(ghost.m_x, base_y, buf,
                            g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                        animations::m_window_opacity.restore();

                        int vtx_end = g_render->draw_list()->VtxBuffer.Size;

                        ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
                        for (int v = vtx_start; v < vtx_end; v++)
                        {
                            verts[v].pos.x = char_center_x + (verts[v].pos.x - char_center_x) * eased;
                            verts[v].pos.y = char_center_y + (verts[v].pos.y - char_center_y) * eased;

                            int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
                            verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
                        }
                    }

                    for (int i = 0; i < (int)m_char_anims.size(); i++)
                    {
                        bool is_active = i < (int)this->m_var->size();

                        float target = is_active ? 1.f : 0.f;
                        m_char_anims[i] += (target - m_char_anims[i]) * dt * speed;

                        if (!is_active && m_char_anims[i] < 0.01f)
                        {
                            m_char_anims.erase(m_char_anims.begin() + i);
                            m_char_removing.erase(m_char_removing.begin() + i);
                            i--;
                            continue;
                        }

                        float t = m_char_anims[i];
                        float eased = t * t * (3.f - 2.f * t);

                        char buf[2] = { is_active ? (*this->m_var)[i] : ' ', '\0' };
                        auto char_size = g_font->f_childs.measure(buf);
                        float char_center_x = cursor_x + char_size.x * 0.5f;
                        float char_center_y = base_y + char_size.y * 0.5f;

                        int vtx_start = g_render->draw_list()->VtxBuffer.Size;

                        g_font->f_childs.text(cursor_x, base_y, buf,
                            g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                        animations::m_window_opacity.restore();

                        int vtx_end = g_render->draw_list()->VtxBuffer.Size;

                        ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
                        for (int v = vtx_start; v < vtx_end; v++)
                        {
                            verts[v].pos.x = char_center_x + (verts[v].pos.x - char_center_x) * eased;
                            verts[v].pos.y = char_center_y + (verts[v].pos.y - char_center_y) * eased;

                            int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
                            verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
                        }

                        cursor_x += char_size.x;
                    }

                    if (this->m_var->empty())
                    {
                        constexpr float cycle_duration = 2.1f;
                        const float cycle = std::fmod(
                            static_cast<float>(ImGui::GetTime()), cycle_duration);
                        const auto smooth_fade = [](float value, float begin, float end) {
                            const float t = std::clamp((value - begin) / (end - begin), 0.f, 1.f);
                            return t * t * (3.f - 2.f * t);
                        };

                        float dot_opacity[3] = { 1.f, 1.f, 1.f };
                        dot_opacity[2] = 1.f - smooth_fade(cycle, 0.35f, 0.55f);
                        dot_opacity[1] = 1.f - smooth_fade(cycle, 0.85f, 1.05f);

                        const float restore = smooth_fade(cycle, 1.65f, 1.95f);
                        dot_opacity[1] = std::max(dot_opacity[1], restore);
                        dot_opacity[2] = std::max(dot_opacity[2], restore);

                        const float dot_width = g_font->f_childs.measure(".").x;
                        for (int dot = 0; dot < 3; ++dot)
                        {
                            g_font->f_childs.text(cursor_x + dot_width * static_cast<float>(dot),
                                base_y, ".", g_style->m_text.modulate(
                                    animations::m_window_opacity.limit(0.5f * dot_opacity[dot]).val()));
                            animations::m_window_opacity.restore();
                        }
                    }
                    }
                    m_last_value_size = m_var->size();
                }
                else
                {
                    const std::string& display_text = this->m_var->empty() ? m_placeholder : *this->m_var;
                    g_font->f_childs.text(this->m_pos.x + 8.f - text_scroll, this->m_pos.y + g_font->f_childs.measure(m_label).y + 8.f, display_text, g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                }
                g_render->restore_clip();
            });

       
    }

    void c_text_input::input()
    {
        auto position = m_hide_label
            ? math::c_vector_2d(0, 0)
            : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);

        math::c_rect bounding = math::c_rect(
            m_pos + math::c_vector_2d(0, position.y),
            math::c_vector_2d(m_child_size, 20.f));

        if (!g_ctx->can_interact(this, m_focus_priority))
            return;


        if (g_input->mouse_in_region(bounding.pos(), bounding.size())) {
            g_ctx->m_hovered = this;
        } else if (g_ctx->m_hovered == this) {
            g_ctx->m_hovered = nullptr;
        }

        if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
        {
            g_ctx->push_focus(this, m_focus_priority);
            g_ctx->m_click_consumed = true;
        }

        if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered != this) {
            g_ctx->pop_focus(this);
        }

        if (!g_ctx->is_focused(this))
            return;

        const float now = (float)ImGui::GetTime();

        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            g_ctx->pop_focus(this);
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true)) {
            if (!this->m_var->empty()) {
                int last = (int)this->m_var->size() - 1;

                    if (last < (int)m_char_anims.size()) {
                    ghost_char_t ghost{};
                    ghost.m_glyph = (*this->m_var)[last];
                    ghost.m_anim = m_char_anims[last]; // inherit current anim value

                    const float text_width = g_font->f_childs.measure(*m_var).x;
                    const float text_scroll = std::max(0.f, text_width - (m_child_size - 16.f));
                    float gx = this->m_pos.x + 8.f - text_scroll;
                    for (int c = 0; c < last; c++) {
                        char tmp[2] = { (*this->m_var)[c], '\0' };
                        gx += g_font->f_childs.measure(tmp).x;
                    }
                    ghost.m_x = gx;

                    m_ghost_chars.push_back(ghost);
                    m_char_anims.erase(m_char_anims.begin() + last);
                    m_char_removing.erase(m_char_removing.begin() + last);
                }

                this->m_var->erase(last, 1);
            }
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
            if (const char* clipboard = ImGui::GetClipboardText(); clipboard != nullptr) {
                constexpr size_t max_input_length = 65535;
                const size_t available = max_input_length - std::min(max_input_length, m_var->size());
                m_var->append(clipboard, strnlen(clipboard, available));
            }
        }

        for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
            unsigned int c = (unsigned int)io.InputQueueCharacters[n];
            if (!io.KeyCtrl && m_var->size() < 65535 && c != 0 && c >= 32 && c <= 255) {
                this->m_var->push_back((char)c);
            }
        }
    }
}
