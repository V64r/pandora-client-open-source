#include "../../../includes.hh"

extern bool g_ConfigInputActive;
extern bool g_NumericEditActive;

namespace
{
	enum class numeric_edit_result { active, accept, cancel };
	bool mouse_in_rect(float x, float y, float w, float h);

	numeric_edit_result draw_numeric_editor(const ImVec2& pos,
		float width, char* buffer, size_t buffer_size, bool& request_focus,
		framework::numeric_edit_animation_t& animation, double minimum, double maximum, int decimals)
	{
		constexpr float height = 18.f;
		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) return numeric_edit_result::cancel;
        auto valid_value = [&]() {
            char* end = nullptr;
            const double value = std::strtod(buffer, &end);
            return end != buffer && *end == '\0' && std::isfinite(value) && value >= minimum && value <= maximum;
        };
        if ((ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) && valid_value())
            return numeric_edit_result::accept;
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true)) {
            const size_t length = std::strlen(buffer);
            if (request_focus) buffer[0] = '\0';
            else if (length) buffer[length - 1] = '\0';
            request_focus = false;
        }
        const double magnitude = std::max(1.0, std::max(std::abs(minimum), std::abs(maximum)));
        const size_t max_length = std::min(buffer_size - 1,
            static_cast<size_t>(std::floor(std::log10(magnitude)) + 1) +
            (decimals ? decimals + 1 : 0) + (minimum < 0 ? 1 : 0));
        for (int index = 0; index < io.InputQueueCharacters.Size; ++index) {
            const unsigned int character = io.InputQueueCharacters[index];
            std::string candidate = request_focus ? "" : buffer;
            const bool digit = character >= '0' && character <= '9';
            const bool decimal = decimals > 0 && character == '.' && candidate.find('.') == std::string::npos;
            const bool negative = minimum < 0 && character == '-' && candidate.empty();
            if (!digit && !decimal && !negative) continue;
            candidate += static_cast<char>(character);
            const auto point = candidate.find('.');
            if (candidate.size() > max_length ||
                (point != std::string::npos && candidate.size() - point - 1 > static_cast<size_t>(decimals))) continue;
            const double value = std::strtod(candidate.c_str(), nullptr);
            if (!std::isfinite(value) || value > maximum || (value < 0 && value < minimum)) continue;
            std::memcpy(buffer, candidate.c_str(), candidate.size() + 1);
            request_focus = false;
        }

		const std::string current_text(buffer);
		const std::string& previous = animation.previous_text;
		if (current_text != previous) {
			size_t common = 0;
			while (common < current_text.size() && common < previous.size() &&
				current_text[common] == previous[common]) ++common;
			float removed_x = pos.x + 5.f;
			for (size_t index = 0; index < common; ++index) {
				char glyph[2] = { previous[index], '\0' };
				removed_x += ::g_font->f_childs.measure(glyph).x;
			}
			for (size_t index = common; index < previous.size(); ++index) {
				char glyph[2] = { previous[index], '\0' };
				const float inherited = index < animation.character_animations.size()
					? animation.character_animations[index] : 1.f;
				animation.ghosts.push_back({ previous[index], inherited, removed_x });
				removed_x += ::g_font->f_childs.measure(glyph).x;
			}
			animation.character_animations.resize(common);
			while (animation.character_animations.size() < current_text.size())
				animation.character_animations.push_back(0.f);
			animation.previous_text = current_text;
		}

		const float opacity = framework::animations::m_window_opacity.val();
		ImDrawList* draw = ::g_render->draw_list();
		draw->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
			IM_COL32(9, 9, 11, static_cast<int>(250.f * opacity)), 5.f);
		draw->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
			framework::g_style->m_accent.modulate(opacity).transform(), 5.f, 0, 1.f);
		draw->PushClipRect(ImVec2(pos.x + 3.f, pos.y + 1.f), ImVec2(pos.x + width - 3.f, pos.y + height - 1.f), true);
		const float dt = std::min(std::max(ImGui::GetIO().DeltaTime, 0.f), 0.05f);
		constexpr float animation_speed = 12.f;
		const float text_y = pos.y + (height - ::g_font->f_childs.measure("0").y) * 0.5f;
		for (size_t index = 0; index < animation.ghosts.size();) {
			auto& ghost = animation.ghosts[index];
			ghost.animation += (0.f - ghost.animation) * dt * animation_speed;
			if (ghost.animation < 0.01f) {
				animation.ghosts.erase(animation.ghosts.begin() + index);
				continue;
			}
			const float eased = ghost.animation * ghost.animation * (3.f - 2.f * ghost.animation);
			char glyph[2] = { ghost.glyph, '\0' };
			const auto size = ::g_font->f_childs.measure(glyph);
			const int first_vertex = draw->VtxBuffer.Size;
			::g_font->f_childs.text(ghost.x, text_y, glyph,
				framework::g_style->m_text.modulate(opacity * 0.7f));
			const int last_vertex = draw->VtxBuffer.Size;
			for (int vertex = first_vertex; vertex < last_vertex; ++vertex) {
				auto& value = draw->VtxBuffer[vertex];
				const float center_x = ghost.x + size.x * 0.5f;
				const float center_y = text_y + size.y * 0.5f;
				value.pos.x = center_x + (value.pos.x - center_x) * eased;
				value.pos.y = center_y + (value.pos.y - center_y) * eased;
				const int alpha = static_cast<int>(((value.col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
				value.col = (value.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
			}
			++index;
		}

		float cursor_x = pos.x + 5.f;
		for (size_t index = 0; index < current_text.size(); ++index) {
			animation.character_animations[index] +=
				(1.f - animation.character_animations[index]) * dt * animation_speed;
			const float progress = std::clamp(animation.character_animations[index], 0.f, 1.f);
			const float eased = progress * progress * (3.f - 2.f * progress);
			char glyph[2] = { current_text[index], '\0' };
			const auto size = ::g_font->f_childs.measure(glyph);
			const int first_vertex = draw->VtxBuffer.Size;
			::g_font->f_childs.text(cursor_x, text_y, glyph,
				framework::g_style->m_text.modulate(opacity));
			const int last_vertex = draw->VtxBuffer.Size;
			for (int vertex = first_vertex; vertex < last_vertex; ++vertex) {
				auto& value = draw->VtxBuffer[vertex];
				const float center_x = cursor_x + size.x * 0.5f;
				const float center_y = text_y + size.y * 0.5f;
				value.pos.x = center_x + (value.pos.x - center_x) * eased;
				value.pos.y = center_y + (value.pos.y - center_y) * eased;
				const int alpha = static_cast<int>(((value.col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
				value.col = (value.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
			}
			cursor_x += size.x;
		}
		if (std::fmod(ImGui::GetTime(), 1.0) < 0.55) {
			const float caret_x = std::min(cursor_x, pos.x + width - 4.f);
			draw->AddLine(ImVec2(caret_x, pos.y + 3.f), ImVec2(caret_x, pos.y + height - 3.f),
				framework::g_style->m_text.modulate(opacity).transform(), 1.f);
		}
		draw->PopClipRect();
		return numeric_edit_result::active;
	}

	bool mouse_in_rect(float x, float y, float w, float h)
	{
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		return mouse.x >= x && mouse.x <= x + w && mouse.y >= y && mouse.y <= y + h;
	}
}

namespace framework
{
	c_slider_float::c_slider_float(std::string label, float* val, float min, float max, bool hide_label, std::wstring prefix) : m_val(val), m_min(min), m_max(max), m_prefix(prefix)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;

		m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 23.f * ImGui::GetIO().FontGlobalScale };
		m_type = element_type::slider;
		m_focus_priority = focus_priority::interactive;

		// we only use this in terms of inlining elements
		m_parent_width = m_child_size;
	}

	void c_slider_float::input()
	{
		if (m_edit_target >= 0) {
			g_ConfigInputActive = g_NumericEditActive = true;
            g_ctx->m_modal_owner = this;
			return;
		}
		if (m_range_max)
		{
			const float label_h = g_font->f_childs.measure(m_label).y;
			const math::c_vector_2d track_pos = m_pos + math::c_vector_2d(0.f, label_h + 5.f);
			const math::c_rect track(track_pos, math::c_vector_2d(m_child_size, 12.f));
			const float knob_pad = 8.f * ImGui::GetIO().FontGlobalScale;
			const float usable_width = std::max(m_child_size - knob_pad * 2.f, 1.f);
			if (!g_ctx->can_interact(this, m_focus_priority)) return;
			const bool hovered = g_input->mouse_in_region(track.pos(), track.size());
			if (hovered) g_ctx->m_hovered = this; else if (g_ctx->m_hovered == this) g_ctx->m_hovered = nullptr;
			if (hovered && g_input->clicked(input::mouse_buttons::left)) {
				const float x = std::clamp(g_input->get_mouse_position().x - track_pos.x, knob_pad, m_child_size - knob_pad);
				const float lo_x = knob_pad + utils::builder::modulate_float(*m_val, m_min, m_max, 0.f, usable_width);
				const float hi_x = knob_pad + utils::builder::modulate_float(*m_range_max, m_min, m_max, 0.f, usable_width);
				m_dragging_range_max = std::abs(x - hi_x) < std::abs(x - lo_x);
				g_ctx->push_focus(this, m_focus_priority);
			}
			if (g_ctx->is_focused(this) && g_input->click_down(input::mouse_buttons::left)) {
				const float x = std::clamp(g_input->get_mouse_position().x - track_pos.x, knob_pad, m_child_size - knob_pad);
				const float value = utils::builder::modulate_float(x - knob_pad, 0.f, usable_width, m_min, m_max);
				if (m_dragging_range_max) *m_range_max = std::max(value, *m_val);
				else *m_val = std::min(value, *m_range_max);
			}
			if (g_input->click_released(input::mouse_buttons::left) && g_ctx->is_focused(this)) g_ctx->pop_focus(this);
			return;
		}
		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		math::c_rect bounding = math::c_rect(m_pos + math::c_vector_2d(0, position.y), math::c_vector_2d(m_child_size, 10.f));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;
			
		// check if hovered is nullptr and then if we are in region
		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
			g_ctx->m_hovered = this;
		else if (g_ctx->m_hovered == this)
		{
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
			g_ctx->push_focus(this, m_focus_priority);

		if (g_input->click_down(input::mouse_buttons::left)) {
			if (g_ctx->is_focused(this))
			{
				const float knob_pad = 8.f * ImGui::GetIO().FontGlobalScale;
				const float usable_width = std::max(m_child_size - knob_pad * 2.f, 1.f);
				float offset = std::clamp<float>(math::c_vector_2d(g_input->get_mouse_position() - this->m_pos).x, knob_pad, m_child_size - knob_pad);
				float target_value = utils::builder::modulate_float(offset - knob_pad, 0, usable_width, this->m_min, this->m_max);

				/* update value */
				*this->m_val = std::clamp(target_value, m_min, m_max);
			}
		}

		// release focus on mouse up
		if (g_input->click_released(input::mouse_buttons::left) && g_ctx->is_focused(this))
			g_ctx->pop_focus(this);

	}

	void c_slider_float::draw()
	{
		if (m_range_max)
		{
			const float opacity = animations::m_window_opacity.val();
			const float dt = std::min(std::max(ImGui::GetIO().DeltaTime, 0.f), 0.05f);
			const float label_h = g_font->f_childs.measure(m_label).y;
			const float y = m_pos.y + label_h + 5.f;
			const float scaled_radius = 8.f * ImGui::GetIO().FontGlobalScale;
			const float knob_pad = 8.f * ImGui::GetIO().FontGlobalScale;
			const float usable_width = std::max(m_child_size - knob_pad * 2.f, 1.f);
			const float target_lo = knob_pad + utils::builder::modulate_float(*m_val, m_min, m_max, 0.f, usable_width);
			const float target_hi = knob_pad + utils::builder::modulate_float(*m_range_max, m_min, m_max, 0.f, usable_width);
			if (m_range_display_min < 0.f) m_range_display_min = target_lo;
			if (m_range_display_max < 0.f) m_range_display_max = target_hi;
			const float position_blend = 1.f - std::exp(-18.f * dt);
			const float scale_blend = 1.f - std::exp(-20.f * dt);
			m_range_display_min += (target_lo - m_range_display_min) * position_blend;
			m_range_display_max += (target_hi - m_range_display_max) * position_blend;
			const bool focused = g_ctx->is_focused(this);
			const auto mouse = g_input->get_mouse_position();
			const bool hover_lo = std::abs(mouse.x - (m_pos.x + target_lo)) <= scaled_radius + 3.f && std::abs(mouse.y - (y + 5.f)) <= scaled_radius + 3.f;
			const bool hover_hi = std::abs(mouse.x - (m_pos.x + target_hi)) <= scaled_radius + 3.f && std::abs(mouse.y - (y + 5.f)) <= scaled_radius + 3.f;
			const float min_scale_target = focused && !m_dragging_range_max ? 1.28f : (hover_lo ? 1.13f : 1.f);
			const float max_scale_target = focused && m_dragging_range_max ? 1.28f : (hover_hi ? 1.13f : 1.f);
			m_range_min_scale += (min_scale_target - m_range_min_scale) * scale_blend;
			m_range_max_scale += (max_scale_target - m_range_max_scale) * scale_blend;
			const float lo = m_range_display_min;
			const float hi = m_range_display_max;
			const std::string lo_text = utils::builder::precision(*m_val, 1);
			const std::string hi_text = utils::builder::precision(*m_range_max, 1);
			const std::string suffix = utils::builder::wstring_to_string(m_prefix.c_str());
			const std::string info = lo_text + " - " + hi_text + (suffix.empty() ? "" : " " + suffix);
			const float info_w = g_font->f_childs.measure(info).x;
			const float info_x = std::max(m_pos.x + m_child_size - info_w,
				m_pos.x + g_font->f_childs.measure(m_label).x + 10.f);
			const float lo_w = g_font->f_childs.measure(lo_text).x;
			const float separator_w = g_font->f_childs.measure(" - ").x;
			const float hi_w = g_font->f_childs.measure(hi_text).x;
			if (m_edit_target < 0 && !g_NumericEditActive && g_ctx->can_interact(this, m_focus_priority) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				if (mouse_in_rect(info_x - 3.f, m_pos.y - 3.f, lo_w + 6.f, label_h + 6.f)) {
					_snprintf_s(m_edit_buffer, _TRUNCATE, "%.1f", *m_val);
					m_edit_target = 0;
					m_request_edit_focus = true;
					m_edit_animation = {};
					g_ConfigInputActive = g_NumericEditActive = true;
            g_ctx->m_modal_owner = this;
				} else if (mouse_in_rect(info_x + lo_w + separator_w - 3.f, m_pos.y - 3.f,
					hi_w + 6.f, label_h + 6.f)) {
					_snprintf_s(m_edit_buffer, _TRUNCATE, "%.1f", *m_range_max);
					m_edit_target = 1;
					m_request_edit_focus = true;
					m_edit_animation = {};
					g_ConfigInputActive = g_NumericEditActive = true;
            g_ctx->m_modal_owner = this;
				}
			}
			g_render->use_layer(m_layer, [&]() {
				g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(opacity * 0.85f));
				if (m_edit_target < 0)
					g_font->f_childs.text(info_x, m_pos.y - 0.5f, info, g_style->m_text.modulate(opacity * 0.8f));
				g_render->rect_shadow(m_pos.x + knob_pad, y, usable_width, 10.f, g_style->m_window_shadow.modulate(opacity * 0.3f), 8.f, 2.f);
				g_render->rect_filled(m_pos.x + knob_pad, y, usable_width, 10.f, g_style->m_element_base.modulate(opacity), 2.f);
				g_render->rect_shadow(m_pos.x + lo, y, std::max(hi - lo, 2.f), 10.f, g_style->m_accent.modulate(opacity * 0.45f), 8.f, 2.f);
				g_render->rect_filled(m_pos.x + lo, y, std::max(hi - lo, 2.f), 10.f, g_style->m_accent.modulate(opacity), 2.f);
				const float inner_radius = 5.8f * ImGui::GetIO().FontGlobalScale;
				const ImU32 knob_out = IM_COL32(235,235,240,(int)(255.f * opacity));
				const ImU32 knob_in = IM_COL32(255,255,255,(int)(255.f * opacity));
				g_render->draw_list()->AddCircleFilled(ImVec2(m_pos.x + lo, y + 5.f), scaled_radius * m_range_min_scale, knob_out, 16);
				g_render->draw_list()->AddCircleFilled(ImVec2(m_pos.x + hi, y + 5.f), scaled_radius * m_range_max_scale, knob_out, 16);
				g_render->draw_list()->AddCircleFilled(ImVec2(m_pos.x + lo, y + 5.f), inner_radius * m_range_min_scale, knob_in, 16);
				g_render->draw_list()->AddCircleFilled(ImVec2(m_pos.x + hi, y + 5.f), inner_radius * m_range_max_scale, knob_in, 16);
			});
			if (m_edit_target >= 0) {
				constexpr float editor_width = 48.f;
				const std::string fixed_text = m_edit_target == 0
					? " - " + hi_text + (suffix.empty() ? "" : " " + suffix)
					: lo_text + " - ";
				const float fixed_width = g_font->f_childs.measure(fixed_text).x;
				const float suffix_width = m_edit_target == 1 && !suffix.empty()
					? g_font->f_childs.measure(" " + suffix).x : 0.f;
				const float editor_x = m_pos.x + m_child_size - editor_width -
					(m_edit_target == 0 ? fixed_width : suffix_width);
				if (m_edit_target == 0)
					g_font->f_childs.text(editor_x + editor_width, m_pos.y - 0.5f, fixed_text,
						g_style->m_text.modulate(opacity * 0.8f));
				else {
					g_font->f_childs.text(editor_x - fixed_width, m_pos.y - 0.5f, fixed_text,
						g_style->m_text.modulate(opacity * 0.8f));
					if (!suffix.empty()) g_font->f_childs.text(editor_x + editor_width, m_pos.y - 0.5f,
						" " + suffix, g_style->m_text.modulate(opacity * 0.8f));
				}
				const auto result = draw_numeric_editor(ImVec2(editor_x, m_pos.y - 3.f),
					editor_width, m_edit_buffer, sizeof(m_edit_buffer), m_request_edit_focus,
					m_edit_animation, m_min, m_max, 1);
				if (result != numeric_edit_result::active) {
					if (result == numeric_edit_result::accept) {
						char* end = nullptr;
						const float parsed = std::strtof(m_edit_buffer, &end);
						if (end != m_edit_buffer && std::isfinite(parsed)) {
							if (m_edit_target == 0) *m_val = std::clamp(parsed, m_min, *m_range_max);
							else *m_range_max = std::clamp(parsed, *m_val, m_max);
						}
					}
					m_edit_target = -1;
					g_ConfigInputActive = g_NumericEditActive = false;
                if (g_ctx->m_modal_owner == this) g_ctx->m_modal_owner = nullptr;
				}
			}
			return;
		}
		animations::m_slider_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_slider_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_value", m_visible && (*this->m_val > this->m_min + 0.1) && g_ctx->m_open, 0.5);
		animations::m_slider_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_slider_value.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_slider_value.val());
		else if (animations::m_slider_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_slider_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_slider_opacity.val() * smooth_opacity;

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		const std::string info = utils::builder::precision(*m_val, 1) + " " + utils::builder::wstring_to_string(m_prefix.c_str());
		const float info_w = g_font->f_childs.measure(info).x;
		const float info_x = m_pos.x + m_child_size - info_w;
		if (!m_hide_label && m_edit_target < 0 && !g_NumericEditActive && g_ctx->can_interact(this, m_focus_priority) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
			mouse_in_rect(info_x - 3.f, m_pos.y - 3.f, info_w + 6.f, g_font->f_childs.measure(info).y + 6.f)) {
			_snprintf_s(m_edit_buffer, _TRUNCATE, "%.1f", *m_val);
			m_edit_target = 0;
			m_request_edit_focus = true;
			m_edit_animation = {};
			g_ConfigInputActive = g_NumericEditActive = true;
            g_ctx->m_modal_owner = this;
		}

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));

				g_render->rect_shadow((m_pos + position).x + 8.f * ImGui::GetIO().FontGlobalScale, (m_pos + position).y, std::max(m_child_size - 16.f * ImGui::GetIO().FontGlobalScale, 1.f), 6.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.val() * 0.3f), 8.f, 2.f);
				g_render->rect_filled((m_pos + position).x + 8.f * ImGui::GetIO().FontGlobalScale, (m_pos + position).y, std::max(m_child_size - 16.f * ImGui::GetIO().FontGlobalScale, 1.f), 6.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 2.f);

				const float knob_pad = 8.f * ImGui::GetIO().FontGlobalScale;
				const float usable_width = std::max(m_child_size - knob_pad * 2.f, 1.f);
				float target_width = knob_pad + utils::builder::modulate_float(*this->m_val, this->m_min, this->m_max, 0, usable_width);

				float dt = ImGui::GetIO().DeltaTime;
				float speed = 10.f;

				if (m_display_width < 0.f) m_display_width = target_width;
                m_display_width += (target_width - m_display_width) * (1.f - std::exp(-22.f * std::max(dt, 0.f)));
                if (std::abs(target_width - m_display_width) < 0.05f) m_display_width = target_width;
				if (m_display_width < 0.5f && target_width <= 0.f)
					m_display_width = 0.f;

				bool focused = g_ctx->is_focused(this);
				m_focus_anim += ((focused ? 1.f : 0.f) - m_focus_anim) * dt * speed;
				m_focus_anim = std::clamp(m_focus_anim, 0.f, 1.f);
				const auto mouse = g_input->get_mouse_position();
				const float knob_x = m_pos.x + m_display_width;
				const float knob_y = (m_pos + position).y + 3.f;
				const bool knob_hovered = std::abs(mouse.x - knob_x) <= 10.f && std::abs(mouse.y - knob_y) <= 10.f;
				m_knob_hover_anim += ((knob_hovered ? 1.f : 0.f) - m_knob_hover_anim) * dt * 14.f;
				m_knob_hover_anim = std::clamp(m_knob_hover_anim, 0.f, 1.f);
				float knob_scale = 1.f + m_knob_hover_anim * 0.13f + m_focus_anim * 0.15f;

				if (m_display_width > knob_pad + 0.5f)
				{
					auto bar_pos = m_pos + math::c_vector_2d(knob_pad, position.y);
					const float fill_width = std::max(m_display_width - knob_pad, 0.f);
					g_render->rect_shadow(bar_pos.x, bar_pos.y, fill_width, 6.f,g_style->m_accent.modulate(animations::m_window_opacity.val() * 0.4f), 8.f, 2.f);
					g_render->rect_filled(bar_pos.x, bar_pos.y, fill_width, 6.f,g_style->m_accent.modulate(animations::m_window_opacity.val()), 2.f);
					g_render->fade_rect_filled(bar_pos.x, bar_pos.y, fill_width, 6.f,hue::c_color(0, 0, 0, 0),hue::c_color(0, 0, 0, (int)(50 * animations::m_window_opacity.val())),engine::fade_direction::vertically, 2.f);
				}
				const ImVec2 knob(knob_x, knob_y);
				const int alpha = (int)(255.f * animations::m_window_opacity.val());
				g_render->draw_list()->AddCircleFilled(knob, 8.f * knob_scale, IM_COL32(235, 235, 240, alpha), 24);
				g_render->draw_list()->AddCircleFilled(knob, 5.8f * knob_scale, IM_COL32(255, 255, 255, alpha), 24);

				if (!m_hide_label && m_edit_target < 0)
					g_font->f_childs.text((m_pos + math::c_vector_2d(m_child_size - g_font->f_childs.measure(info).x, 0.5)).x, (m_pos + math::c_vector_2d(m_child_size - g_font->f_childs.measure(info).x, 0.5)).y,
						info, g_style->m_text.modulate(animations::m_window_opacity.limit(0.3f).val()));
				animations::m_window_opacity.restore();
			});
		if (m_edit_target == 0) {
			constexpr float editor_width = 52.f;
			const std::string suffix = utils::builder::wstring_to_string(m_prefix.c_str());
			const float suffix_width = suffix.empty() ? 0.f : g_font->f_childs.measure(" " + suffix).x;
			const float editor_x = m_pos.x + m_child_size - editor_width - suffix_width;
			if (!suffix.empty()) g_font->f_childs.text(editor_x + editor_width, m_pos.y - 0.5f,
				" " + suffix, g_style->m_text.modulate(animations::m_window_opacity.val() * 0.8f));
			const auto result = draw_numeric_editor(ImVec2(editor_x, m_pos.y - 3.f),
				editor_width, m_edit_buffer,
				sizeof(m_edit_buffer), m_request_edit_focus, m_edit_animation, m_min, m_max, 1);
			if (result != numeric_edit_result::active) {
				if (result == numeric_edit_result::accept) {
					char* end = nullptr;
					const float parsed = std::strtof(m_edit_buffer, &end);
					if (end != m_edit_buffer && std::isfinite(parsed)) *m_val = std::clamp(parsed, m_min, m_max);
				}
				m_edit_target = -1;
				g_ConfigInputActive = g_NumericEditActive = false;
                if (g_ctx->m_modal_owner == this) g_ctx->m_modal_owner = nullptr;
			}
		}
	}

	c_slider_int::c_slider_int(std::string label, int* val, int min, int max, bool hide_label, std::wstring prefix) : m_val(val), m_min(min), m_max(max), m_prefix(prefix)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;

		m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 23.f * ImGui::GetIO().FontGlobalScale };
		m_type = element_type::slider;

		// we only use this in terms of inlining elements
		m_parent_width = m_child_size;
	}

	void c_slider_int::input()
	{
		if (m_editing) {
			g_ConfigInputActive = g_NumericEditActive = true;
            g_ctx->m_modal_owner = this;
			return;
		}
		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		math::c_rect bounding = math::c_rect(m_pos + math::c_vector_2d(0, position.y), math::c_vector_2d(m_child_size, 10.f));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this)
		{
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
			g_ctx->push_focus(this, m_focus_priority);

		if (g_input->click_down(input::mouse_buttons::left)) {
			if (g_ctx->is_focused(this))
			{
				const float knob_pad = 8.f * ImGui::GetIO().FontGlobalScale;
				const float usable_width = std::max(m_child_size - knob_pad * 2.f, 1.f);
				float offset = std::clamp<float>(math::c_vector_2d(g_input->get_mouse_position() - this->m_pos).x, knob_pad, m_child_size - knob_pad);

				/* we are forcing the conversion to float here using (float) */
				float target_value = utils::builder::modulate_float(offset - knob_pad, 0, usable_width, (float)this->m_min, (float)this->m_max);

				// Each slider owns its value. A function-static accumulator made
				// unrelated integer sliders influence one another while dragging.
				*this->m_val = std::clamp((int)std::lround(target_value), m_min, m_max);
			}		
		}


		// release focus on mouse up
		if (g_input->click_released(input::mouse_buttons::left) && g_ctx->is_focused(this))
			g_ctx->pop_focus(this);
	}

	void c_slider_int::draw()
	{
		animations::m_slider_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_slider_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_int_value", m_visible && (*this->m_val > this->m_min) && g_ctx->m_open, 0.5);
		animations::m_slider_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_int_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_slider_value.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_slider_value.val());
		else if (animations::m_slider_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_slider_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_slider_opacity.val() * smooth_opacity;

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		const std::string info = std::to_string(*m_val) + " " + utils::builder::wstring_to_string(m_prefix.c_str());
		const float info_w = g_font->f_childs.measure(info).x;
		const float info_x = m_pos.x + m_child_size - info_w;
		if (!m_hide_label && !m_editing && !g_NumericEditActive && g_ctx->can_interact(this, m_focus_priority) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
			mouse_in_rect(info_x - 3.f, m_pos.y - 3.f, info_w + 6.f, g_font->f_childs.measure(info).y + 6.f)) {
			_snprintf_s(m_edit_buffer, _TRUNCATE, "%d", *m_val);
			m_editing = true;
			m_request_edit_focus = true;
			m_edit_animation = {};
			g_ConfigInputActive = g_NumericEditActive = true;
            g_ctx->m_modal_owner = this;
		}

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));

				g_render->rect_shadow((m_pos + position).x + 8.f * ImGui::GetIO().FontGlobalScale, (m_pos + position).y, std::max(m_child_size - 16.f * ImGui::GetIO().FontGlobalScale, 1.f), 6.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.val() * 0.3f), 8.f, 2.f);
				g_render->rect_filled((m_pos + position).x + 8.f * ImGui::GetIO().FontGlobalScale, (m_pos + position).y, std::max(m_child_size - 16.f * ImGui::GetIO().FontGlobalScale, 1.f), 6.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 2.f);

				const float knob_pad = 8.f * ImGui::GetIO().FontGlobalScale;
				const float usable_width = std::max(m_child_size - knob_pad * 2.f, 1.f);
				float target_width = knob_pad + utils::builder::modulate_float(*this->m_val, this->m_min, this->m_max, 0, usable_width);

				float dt = ImGui::GetIO().DeltaTime;
				float speed = 10.f;

				if (m_display_width < 0.f) m_display_width = target_width;
                m_display_width += (target_width - m_display_width) * (1.f - std::exp(-22.f * std::max(dt, 0.f)));
                if (std::abs(target_width - m_display_width) < 0.05f) m_display_width = target_width;
				if (m_display_width < 0.5f && target_width <= 0.f)
					m_display_width = 0.f;

				bool focused = g_ctx->is_focused(this);
				m_focus_anim += ((focused ? 1.f : 0.f) - m_focus_anim) * dt * speed;
				m_focus_anim = std::clamp(m_focus_anim, 0.f, 1.f);

				const auto mouse = g_input->get_mouse_position();
				const float knob_x = m_pos.x + m_display_width;
				const float knob_y = (m_pos + position).y + 3.f;
				const bool knob_hovered = std::abs(mouse.x - knob_x) <= 10.f && std::abs(mouse.y - knob_y) <= 10.f;
				m_knob_hover_anim += ((knob_hovered ? 1.f : 0.f) - m_knob_hover_anim) * dt * 14.f;
				m_knob_hover_anim = std::clamp(m_knob_hover_anim, 0.f, 1.f);
				float knob_scale = 1.f + m_knob_hover_anim * 0.13f + m_focus_anim * 0.15f;

				if (m_display_width > knob_pad + 0.5f)
				{
					auto bar_pos = m_pos + math::c_vector_2d(knob_pad, position.y);
					const float fill_width = std::max(m_display_width - knob_pad, 0.f);
					g_render->rect_shadow(bar_pos.x, bar_pos.y, fill_width, 6.f, g_style->m_accent.modulate(animations::m_window_opacity.val() * 0.4f), 8.f, 2.f);
					g_render->rect_filled(bar_pos.x, bar_pos.y, fill_width, 6.f, g_style->m_accent.modulate(animations::m_window_opacity.val()), 2.f);
					g_render->fade_rect_filled(bar_pos.x, bar_pos.y, fill_width, 6.f, hue::c_color(0, 0, 0, 0), hue::c_color(0, 0, 0, (int)(50 * animations::m_window_opacity.val())), engine::fade_direction::vertically, 2.f);
				}
				const ImVec2 knob(knob_x, knob_y);
				const int alpha = (int)(255.f * animations::m_window_opacity.val());
				g_render->draw_list()->AddCircleFilled(knob, 8.f * knob_scale, IM_COL32(235, 235, 240, alpha), 24);
				g_render->draw_list()->AddCircleFilled(knob, 5.8f * knob_scale, IM_COL32(255, 255, 255, alpha), 24);

				if (!m_hide_label && !m_editing)
					g_font->f_childs.text((m_pos + math::c_vector_2d(m_child_size - g_font->f_childs.measure(info).x, 0.5)).x, (m_pos + math::c_vector_2d(m_child_size - g_font->f_childs.measure(info).x, 0.5)).y,
						info, g_style->m_text.modulate(animations::m_window_opacity.limit(0.3f).val()));
				animations::m_window_opacity.restore();
			});
		if (m_editing) {
			constexpr float editor_width = 52.f;
			const std::string suffix = utils::builder::wstring_to_string(m_prefix.c_str());
			const float suffix_width = suffix.empty() ? 0.f : g_font->f_childs.measure(" " + suffix).x;
			const float editor_x = m_pos.x + m_child_size - editor_width - suffix_width;
			if (!suffix.empty()) g_font->f_childs.text(editor_x + editor_width, m_pos.y - 0.5f,
				" " + suffix, g_style->m_text.modulate(animations::m_window_opacity.val() * 0.8f));
			const auto result = draw_numeric_editor(ImVec2(editor_x, m_pos.y - 3.f),
				editor_width, m_edit_buffer,
				sizeof(m_edit_buffer), m_request_edit_focus, m_edit_animation, m_min, m_max, 0);
			if (result != numeric_edit_result::active) {
				if (result == numeric_edit_result::accept) {
					char* end = nullptr;
					const long parsed = std::strtol(m_edit_buffer, &end, 10);
					if (end != m_edit_buffer) *m_val = std::clamp(static_cast<int>(parsed), m_min, m_max);
				}
				m_editing = false;
				g_ConfigInputActive = g_NumericEditActive = false;
                if (g_ctx->m_modal_owner == this) g_ctx->m_modal_owner = nullptr;
			}
		}
	}
}
