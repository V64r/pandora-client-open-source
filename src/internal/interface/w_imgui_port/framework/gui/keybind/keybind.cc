#include "../../../includes.hh"

namespace framework
{
	bool g_keybind_capturing = false;

	c_keybind::c_keybind(std::string label, key_var_t* val, bool hide_label) : m_val(val)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;
		m_size = { 0, 16.f };

		m_type = element_type::keybind;
		m_focus_priority = focus_priority::persistent;

		// we only use this in terms of inlining elements
		m_parent_width = 20;
	}
	
	static std::vector<std::string> modes = { "Always", "Hold", "Toggle" };
	void c_keybind::draw()
	{
		// Publish capture state from the one instance that owns it: only the
		// capturing keybind transitions true, and the same instance transitions
		// it back to false when the capture ends (key set, escape, click-out).
		if (m_key_callback != m_was_capturing)
		{
			m_was_capturing = m_key_callback;
			g_keybind_capturing = m_key_callback;
		}

		if (m_simple)
		{
			const float icon_w = g_font->f_icons.measure(ICON_FA_KEYBOARD).x;
			const float icon_x = m_inlined ? m_pos.x : (m_pos.x + m_child_size - icon_w - 2.f);
			const std::string value = m_key_callback ? "..." : (m_val->key ? g_ctx->get_key_name(m_val->key) : "None");
			if (!m_hide_label) g_font->f_childs.text(m_pos.x, m_pos.y, m_label, g_style->m_text.modulate(0.65f));
						g_font->f_icons.text(icon_x, m_pos.y, ICON_FA_KEYBOARD, g_style->m_accent.modulate(m_key_callback ? 1.f : 0.7f));
            animations::m_keybind_value = utils::builder::create_animation_ctx(
                m_parent + m_label + "#simple_bind_popup",
                m_visible && g_ctx->is_focused(this) && g_ctx->m_open, 0.42);
            if (animations::m_keybind_value.val() > 0.01f) {
                const float popup_w = 180.f, popup_h = 62.f;
                const float popup_x = std::clamp(icon_x - popup_w + icon_w + 4.f, 6.f, ImGui::GetIO().DisplaySize.x - popup_w - 6.f);
                const float t = animations::m_keybind_value.val();
                const float eased = t * t * (3.f - 2.f * t);
                const float popup_y = std::clamp(m_pos.y + 20.f, 6.f, ImGui::GetIO().DisplaySize.y - popup_h - 6.f) + (1.f - eased) * 8.f;
                g_render->use_layer(engine::render_layer::prioritized, [&]() {
                    g_render->rect_shadow(popup_x, popup_y, popup_w, popup_h, g_style->m_window_shadow.modulate(0.65f * eased), 10.f, 5.f);
                    g_render->rect_filled(popup_x, popup_y, popup_w, popup_h, g_style->m_window_background.modulate(eased), 5.f);
                    g_font->f_childs.text(popup_x + 9.f, popup_y + 5.f, m_label, g_style->m_text.modulate(0.75f * eased));
                    g_render->rect_filled(popup_x + 9.f, popup_y + 30.f, popup_w - 18.f, 24.f, g_style->m_element_base.modulate(eased), 3.f);
                    g_font->f_icons.text(popup_x + 17.f, popup_y + 34.f, ICON_FA_KEYBOARD, g_style->m_accent.modulate(eased));
                    g_font->f_childs.text(popup_x + 40.f, popup_y + 34.f, m_key_callback ? "..." : value, g_style->m_text.modulate(eased));
                }, true);
            }
			return;
		}
		animations::m_keybind_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_keybind_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_value", m_visible && g_ctx->is_focused(this) && g_ctx->m_open, 0.5);
		animations::m_keybind_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_keybind_value.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_keybind_value.val());
		else if (animations::m_keybind_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_keybind_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_keybind_opacity.val() * smooth_opacity;

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
				{
					// data when hide label is false its diff, if we're gonna inline elements atleast do it properly
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));
					g_font->f_icons.text(m_pos.x + (m_child_size - g_font->f_icons.measure(ICON_FA_KEYBOARD).x) - 1, m_pos.y, ICON_FA_KEYBOARD, g_style->m_text.modulate(final_opacity));
				}
				else
				{
					// imagine if we have multiinline
					g_font->f_icons.text(m_pos.x, m_pos.y, ICON_FA_KEYBOARD, g_style->m_text.modulate(final_opacity));
				}
			});

		if (animations::m_keybind_value.val() > 0.f)
		{
			math::c_vector_2d pallete_size = { 180, 105 };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_KEYBOARD).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			float t = animations::m_keybind_value.val();
			float eased = t * t * (3.f - 2.f * t);

			float origin_y = pallete_pos.y;
			float origin_x = pallete_pos.x;

			ImDrawList* dl = ImGui::GetForegroundDrawList();
			int vtx_start = dl->VtxBuffer.Size;

			g_render->use_layer(engine::render_layer::prioritized, [&]() {
				// draw the background of the pallete
				g_render->rect_shadow(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_shadow.modulate(animations::m_keybind_value.val()), 10.f, 5.f);
				g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_background.modulate(animations::m_keybind_value.val()), 5.f);

				g_render->gradient(pallete_pos.x, pallete_pos.y + 25.f, pallete_size.x, 10, g_style->m_window_shadow.modulate(animations::m_keybind_value.limit(0.2).val()), g_style->m_window_shadow.modulate(animations::m_keybind_value.limit(0.1).val()).with_alpha(0), engine::fade_direction::horizontally);


				//g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, 25.f, g_style->m_window_background.modulate(animations::m_keybind_value.val()), 4.f, engine::draw_flags_::draw_flags_round_corners_top);

				// pallete name
				g_font->f_childs.text(pallete_pos.x + 8, pallete_pos.y + 3, m_label, g_style->m_text.modulate(animations::m_keybind_value.limit(0.6).val()));
				animations::m_keybind_value.restore();

				// mode selection
				//g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_child_top.modulate(animations::m_keybind_value.val()), 2.f);
				//g_render->rect(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_outline.modulate(animations::m_keybind_value.val()), 2.f);

				g_render->rect_shadow(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_window_shadow.modulate(animations::m_keybind_value.limit(0.3).val()), 8.f, 3.f);
				animations::m_keybind_value.restore();
				g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_element_base.modulate(animations::m_keybind_value.val()), 3.f);

				// mode drawing
				{ // store this in {} so we dont affect next vars
					int start_x = pallete_pos.x + 15;
					for (int i = 0; i < modes.size(); i++)
					{
						auto mode = modes[i];

						math::rect_t bounding = math::rect_t(start_x, pallete_pos.y + 35, g_font->f_childs.measure(mode).x, 25.f);
						if (g_input->mouse_in_region(bounding.position(), bounding.size()) && g_input->clicked(input::mouse_buttons::left))
						{
							this->m_val->mode = i;
						}

						animations::m_keybind_selected_mode = utils::builder::create_animation_ctx(m_parent + mode + std::to_string(i) + "#m_keybind_selected_mode", this->m_val->mode == i && g_ctx->is_focused(this), 0.5);
						g_font->f_childs.text(start_x, pallete_pos.y + 38, mode, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_selected_mode.val()));

						start_x += g_font->f_childs.measure(mode).x + 10.f;
					}
				}
				
				// key selection
				// area for binding
				//g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 70, pallete_size.x - 20, 25.f, g_style->m_child_top.modulate(animations::m_keybind_value.val()), 2.f);
				//g_render->rect(pallete_pos.x + 10, pallete_pos.y + 70, pallete_size.x - 20, 25.f, g_style->m_outline.modulate(animations::m_keybind_value.val()), 2.f);

				g_render->rect_shadow(pallete_pos.x + 10, pallete_pos.y + 70, pallete_size.x - 20, 25.f, g_style->m_window_shadow.modulate(animations::m_keybind_value.limit(0.3).val()), 8.f, 3.f);
				animations::m_keybind_value.restore();
				g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 70, pallete_size.x - 20, 25.f, g_style->m_element_base.modulate(animations::m_keybind_value.val()), 3.f);

				math::c_rect binding_area = math::c_rect(pallete_pos.x + 20, pallete_pos.y + 70, pallete_size.x - 20, 25.f);

				animations::m_keybind_binding = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_binding", this->m_key_callback && g_ctx->is_focused(this), 0.5);

				g_font->f_icons.text(binding_area.x, binding_area.y + 5.f, ICON_FA_KEYBOARD, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_binding.val()));

				std::string key_name = g_ctx->get_key_name(this->m_val->key);
				g_font->f_childs.text(binding_area.x + g_font->f_icons.measure(ICON_FA_KEYBOARD).x + 8.f, binding_area.y + 4.f, key_name, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_binding.val()));

				//g_render->rect(pallete_pos.x - 1, pallete_pos.y - 1, pallete_size.x + 2, pallete_size.y + 2, g_style->m_outline.modulate(animations::m_keybind_value.val()), 4.f);
			}, true);

			int vtx_end = dl->VtxBuffer.Size;

			ImDrawVert* verts = dl->VtxBuffer.Data;
			for (int v = vtx_start; v < vtx_end; v++)
			{
				verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;
				verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

				int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
				verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
			}

			// check if we click oob
			math::c_rect bounding_data = math::c_rect(pallete_pos, pallete_size);
			if (!g_input->mouse_in_region(bounding_data.pos(), bounding_data.size()) && g_input->clicked(input::mouse_buttons::left))
			{
				g_ctx->pop_focus(this);
				this->m_key_callback = false; // we need to reset this
			}
		}
	}


	void c_keybind::input()
	{
		if (m_simple)
		{
			const float icon_w = g_font->f_icons.measure(ICON_FA_KEYBOARD).x;
			const float icon_x = m_inlined ? m_pos.x : (m_pos.x + m_child_size - icon_w - 2.f);
			const math::c_vector_2d icon_pos(icon_x - 5.f, m_pos.y - 4.f);
			const math::c_vector_2d icon_size(icon_w + 10.f, 24.f);
			const bool icon_hovered = g_input->mouse_in_region(icon_pos, icon_size);
			if (icon_hovered) g_ctx->m_hovered = this;

			if (icon_hovered && g_input->clicked(input::mouse_buttons::left)) {
				if (!g_ctx->is_focused(this)) {
					m_key_callback = false;
					g_ctx->push_focus(this, m_focus_priority);
				} else {
					m_key_callback = false;
					g_ctx->pop_focus(this);
				}
				g_ctx->m_click_consumed = true;
				return;
			}

			if (!g_ctx->is_focused(this)) return;

			const float popup_w = 180.f, popup_h = 62.f;
			const float popup_x = std::clamp(icon_x - popup_w + icon_w + 4.f, 6.f, ImGui::GetIO().DisplaySize.x - popup_w - 6.f);
			const float popup_y = std::clamp(m_pos.y + 20.f, 6.f, ImGui::GetIO().DisplaySize.y - popup_h - 6.f);
			const math::c_vector_2d popup_pos(popup_x, popup_y);
			const math::c_vector_2d popup_size(popup_w, popup_h);
			const math::c_vector_2d bind_pos(popup_x + 9.f, popup_y + 30.f);
			const math::c_vector_2d bind_size(popup_w - 18.f, 24.f);
			const bool bind_hovered = g_input->mouse_in_region(bind_pos, bind_size);

			if (!m_key_callback && bind_hovered && g_input->clicked(input::mouse_buttons::left)) {
				m_key_callback = true;
				m_wait_release = true;
				for (int i = 0; i < 256; ++i)
					m_key_was_down[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
				g_ctx->m_click_consumed = true;
				return;
			}

			if (!m_key_callback) {
				if (g_input->clicked(input::mouse_buttons::left) &&
					!g_input->mouse_in_region(popup_pos, popup_size)) {
					g_ctx->pop_focus(this);
					g_ctx->m_click_consumed = true;
				}
				return;
			}

			bool any_mouse = false;
			for (int i = 0; i < 5; ++i) any_mouse |= ImGui::GetIO().MouseDown[i];
			if (m_wait_release) {
				if (!any_mouse) m_wait_release = false;
				return;
			}

			if ((GetAsyncKeyState(VK_ESCAPE) & 1) != 0) {
				m_val->key = 0;
				m_key_callback = false;
				g_ctx->pop_focus(this);
				return;
			}

			static const int mouse_keys[5] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
			for (int i = 0; i < 5; ++i) {
				const int key = mouse_keys[i];
				const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
				if (!down) { m_key_was_down[key] = false; continue; }
				if (!m_key_was_down[key]) {
					m_val->key = key;
					m_key_callback = false;
					g_ctx->m_click_consumed = true;
					g_ctx->pop_focus(this);
					return;
				}
			}

			for (int i = 1; i < 256; ++i) {
				if (i >= VK_LBUTTON && i <= VK_XBUTTON2) continue;
				const bool down = (GetAsyncKeyState(i) & 0x8000) != 0;
				if (!down) { m_key_was_down[i] = false; continue; }
				if (!m_key_was_down[i]) {
					m_val->key = i;
					m_key_callback = false;
					g_ctx->pop_focus(this);
					return;
				}
			}
			return;
		}		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_hide_label ? g_font->f_icons.measure(ICON_FA_PALLET).x : m_child_size, m_size.y));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
		{
			g_ctx->push_focus(this, m_focus_priority); // we opened it mfucker
		}

		if (g_ctx->is_focused(this))
		{
			math::c_vector_2d pallete_size = { 180, 105 };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_KEYBOARD).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			math::c_rect binding_area = math::c_rect(pallete_pos.x + 20, pallete_pos.y + 70, pallete_size.x - 20, 25.f);

			static bool skip_frame = false;
			if (g_input->mouse_in_region(binding_area.pos(), binding_area.size()) && g_input->clicked(input::mouse_buttons::left)) {
				this->m_key_callback = true;
				skip_frame = false;
			}

			if (this->m_key_callback) {
				if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Insert, false)) {
					(this->m_val)->key = 0;
					this->m_key_callback = false;
				}
				else {
					if (skip_frame) {
						bool any_mouse = false;
						for (int i = 0; i < 5; ++i) any_mouse |= ImGui::GetIO().MouseDown[i];
						if (!any_mouse) skip_frame = false;
					}
					if (!skip_frame) {
						static const int mouse_keys[5] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
						for (int i = 0; i < 5; ++i) {
							if (ImGui::GetIO().MouseClicked[i]) { this->m_val->key = mouse_keys[i]; this->m_key_callback = false; break; }
						}
					}

					// yeah this is bugged
					for (int i = 1; i < 256; ++i) {
						if (i >= VK_LBUTTON && i <= VK_XBUTTON2) continue;
						if ((GetAsyncKeyState(i) & 1) != 0) {
							this->m_val->key = i;
							this->m_key_callback = false;
							break;
						}
					}
				}
			}

		}
	}
}
