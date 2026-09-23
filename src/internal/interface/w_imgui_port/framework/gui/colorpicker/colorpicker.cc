#include "../../../includes.hh"

namespace framework
{
	c_colorpicker::c_colorpicker(std::string label, hue::c_color* val, bool hide_label) : m_val(val)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;
		m_size = { 0, (m_hide_label ? 0.f : 16.f) };

		m_type = element_type::colorpicker;
		m_focus_priority = focus_priority::persistent;

		// we only use this in terms of inlining elements
		m_parent_width = 20;

		// wtv
		this->rgb_to_hsv();
	}

	void c_colorpicker::draw()
	{
		animations::m_colorpicker_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_colorpicker_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_colorpicker_value", m_visible && g_ctx->is_focused(this) && g_ctx->m_open, 0.5);
		animations::m_colorpicker_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_colorpicker_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handling
		float target_opacity = 0.55f;
		if (animations::m_colorpicker_value.val() > 0.f)
			target_opacity = 0.55f + (0.40f * animations::m_colorpicker_value.val());
		else if (animations::m_colorpicker_hover.val() > 0.f)
			target_opacity = 0.55f + (0.25f * animations::m_colorpicker_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_colorpicker_opacity.val() * smooth_opacity;

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
				{
					// data when hide label is false its diff, if we're gonna inline elements atleast do it properly
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));
					g_font->f_icons.text(m_pos.x + (m_child_size - g_font->f_icons.measure(ICON_FA_PALLET).x) + 2, m_pos.y, ICON_FA_PALETTE, this->m_val->modulate(final_opacity));
				}
				else
				{
					// imagine if we have multiinline
					g_font->f_icons.text(m_pos.x, m_pos.y, ICON_FA_PALETTE, this->m_val->modulate(final_opacity));
				}
			});
	
		if (animations::m_colorpicker_value.val() > 0.f)
		{
			math::c_vector_2d pallete_size = m_context_open ? math::c_vector_2d(80, 50) : math::c_vector_2d(180, 220);

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_PALETTE).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			ImVec2 display_size = ImGui::GetIO().DisplaySize;
			if (pallete_pos.x + pallete_size.x > display_size.x)
				pallete_pos.x = display_size.x - pallete_size.x - 5.f;
			if (pallete_pos.y + pallete_size.y > display_size.y)
				pallete_pos.y = display_size.y - pallete_size.y - 5.f;

			float t = animations::m_colorpicker_value.val();
			float eased = t * t * (3.f - 2.f * t);

			float origin_y = pallete_pos.y;
			float origin_x = pallete_pos.x;

			ImDrawList* dl = ImGui::GetForegroundDrawList();
			int vtx_start = dl->VtxBuffer.Size;

			g_render->use_layer(engine::render_layer::prioritized, [&]()
			{
					dl->PushClipRectFullScreen();
					// draw the background of the pallete
					g_render->rect_shadow(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_shadow.modulate(animations::m_colorpicker_value.val()), 10.f, 5.f);
					g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_background.modulate(animations::m_colorpicker_value.val()), 5.f);

					if (m_context_open) {
						math::c_rect copy_btn = math::c_rect(pallete_pos + math::c_vector_2d(0, 5), math::c_vector_2d(pallete_size.x, 20));
						math::c_rect paste_btn = math::c_rect(pallete_pos + math::c_vector_2d(0, 25), math::c_vector_2d(pallete_size.x, 20));

						bool copy_hovered = g_input->mouse_in_region(copy_btn.pos(), copy_btn.size());
						if (copy_hovered) {
							g_render->rect_filled(copy_btn.x, copy_btn.y, copy_btn.w, copy_btn.h, g_style->m_element_base.modulate(animations::m_colorpicker_value.val() * 1.5f), 5.f);
						}
						g_font->f_childs.text(copy_btn.x + 8, copy_btn.y + 3, "Copy", g_style->m_text.modulate(animations::m_colorpicker_value.val()));

						bool paste_hovered = g_input->mouse_in_region(paste_btn.pos(), paste_btn.size());
						if (paste_hovered) {
							g_render->rect_filled(paste_btn.x, paste_btn.y, paste_btn.w, paste_btn.h, g_style->m_element_base.modulate(animations::m_colorpicker_value.val() * 1.5f), 5.f);
						}
						g_font->f_childs.text(paste_btn.x + 8, paste_btn.y + 3, "Paste", g_style->m_text.modulate(animations::m_colorpicker_value.val()));
					} else {
						g_render->gradient(pallete_pos.x, pallete_pos.y + 25.f, pallete_size.x, 10, g_style->m_window_shadow.modulate(animations::m_colorpicker_value.limit(0.2).val()), g_style->m_window_shadow.modulate(animations::m_colorpicker_value.limit(0.1).val()).with_alpha(0), engine::fade_direction::horizontally);

						// pallete name
						g_font->f_childs.text(pallete_pos.x + 8, pallete_pos.y + 3, m_label, g_style->m_text.modulate(animations::m_colorpicker_value.limit(0.6).val()));
						animations::m_colorpicker_value.restore();

				// get pure hsv color
				float h_normalized = m_hue / 360.f;
				int hue_r, hue_g, hue_b;

				if (h_normalized < 1.f / 6.f) {
					hue_r = 255; hue_g = (int)(255 * h_normalized * 6.f); hue_b = 0;
				}

				else if (h_normalized < 2.f / 6.f) {
					hue_r = (int)(255 * (2.f / 6.f - h_normalized) * 6.f); hue_g = 255; hue_b = 0;
				}
				else if (h_normalized < 3.f / 6.f) {
					hue_r = 0; hue_g = 255; hue_b = (int)(255 * (h_normalized - 2.f / 6.f) * 6.f);
				}
				else if (h_normalized < 4.f / 6.f) {
					hue_r = 0; hue_g = (int)(255 * (4.f / 6.f - h_normalized) * 6.f); hue_b = 255;
				}
				else if (h_normalized < 5.f / 6.f) {
					hue_r = (int)(255 * (h_normalized - 4.f / 6.f) * 6.f); hue_g = 0; hue_b = 255;
				}
				else {
					hue_r = 255; hue_g = 0; hue_b = (int)(255 * (1.f - h_normalized) * 6.f);
				}

				// hsv pos
				math::c_vector_2d hsv_pos = { pallete_pos.x + 8, pallete_pos.y + 33 };
				math::c_vector_2d hsv_size = { pallete_size.x - 16, 180 - 41 };

				g_render->rect_filled(hsv_pos.x, hsv_pos.y, hsv_size.x, hsv_size.y, hue::c_color(hue_r, hue_g, hue_b).modulate(animations::m_colorpicker_value.val()), 2.f);
				g_render->gradient(hsv_pos, hsv_size, hue::c_color(255, 255, 255, 255 * animations::m_colorpicker_value.val()), hue::c_color(255, 255, 255, 0), engine::fade_direction::vertically, 2.f, g_style->m_window_bars.modulate(animations::m_colorpicker_value.val()));
				g_render->gradient(hsv_pos, hsv_size, hue::c_color(0, 0, 0, 0), hue::c_color(0, 0, 0, 255 * animations::m_colorpicker_value.val()), engine::fade_direction::horizontally, 2.f, g_style->m_window_bars.modulate(animations::m_colorpicker_value.val()));

				// hue bar
				math::c_vector_2d hue_pos = { hsv_pos.x, hsv_pos.y + hsv_size.y + 10 };
				math::c_vector_2d hue_size = { hsv_size.x, 10 };

				int segment_count = 6;
				for (int i = 0; i < segment_count; i++) {
					float segment_width = hsv_size.x / (float)segment_count;
					int segment_x = hsv_pos.x + (int)(i * segment_width);
					int next_x = hsv_pos.x + (int)((i + 1) * segment_width);

					hue::c_color color1, color2;
					switch (i) {
					case 0: color1 = hue::c_color(255, 0, 0); color2 = hue::c_color(255, 255, 0); break;
					case 1: color1 = hue::c_color(255, 255, 0); color2 = hue::c_color(0, 255, 0); break;
					case 2: color1 = hue::c_color(0, 255, 0); color2 = hue::c_color(0, 255, 255); break;
					case 3: color1 = hue::c_color(0, 255, 255); color2 = hue::c_color(0, 0, 255); break;
					case 4: color1 = hue::c_color(0, 0, 255); color2 = hue::c_color(255, 0, 255); break;
					case 5: color1 = hue::c_color(255, 0, 255); color2 = hue::c_color(255, 0, 0); break;
					}

					g_render->gradient(math::c_vector_2d(segment_x, hue_pos.y), math::c_vector_2d(next_x - segment_x, 10.f), color1.modulate(animations::m_colorpicker_value.val()), color2.modulate(animations::m_colorpicker_value.val()),
						engine::fade_direction::vertically, (i == 0 || i == 5) ? 2 : 0, g_style->m_window_bars.modulate(animations::m_colorpicker_value.val()), ((i == 0 ? engine::draw_flags_::draw_flags_round_corners_left : 0) |
							(i == 5 ? engine::draw_flags_::draw_flags_round_corners_right : 0)));
				}

				// alpha bar
				math::c_vector_2d alpha_bar = { hue_pos.x, hue_pos.y + 20 };
				math::c_vector_2d alpha_size = { hsv_size.x, 10 };

				const float r = 3.f;
				const int total_cols = (alpha_size.x + 1) / 5;
				const int bar_h = 12;

				for (int i = 0; i < total_cols; i++) {
					for (int j = 0; j < 2; j++) {
						bool is_light = (i + j) % 2 == 0;
						auto color = hue::c_color(is_light ? 200 : 150, is_light ? 200 : 150, is_light ? 200 : 150)
							.modulate(animations::m_colorpicker_value.val());

						engine::draw_flags flags = engine::draw_flags_round_corners_none;
						float tile_r = 0.f;

						if (i == 0) {
							flags = engine::draw_flags_round_corners_left;
							tile_r = r;
						}
						else if (i == total_cols - 1) {
							flags = engine::draw_flags_round_corners_right;
							tile_r = r;
						}

						g_render->rect_filled(
							alpha_bar.x + (i * 5), alpha_bar.y + (j * 5), 5, 5,
							color, tile_r, flags
						);
					}
				}

				g_render->gradient(math::c_vector_2d(alpha_bar.x, alpha_bar.y), alpha_size, hue::c_color(this->m_val->r, this->m_val->g, this->m_val->b, 0), hue::c_color(this->m_val->r, this->m_val->g, this->m_val->b, 255).modulate(animations::m_colorpicker_value.val()),engine::fade_direction::vertically, 0);

				// cursor data - remake this shit
				{
					int cursor_x = hsv_pos.x + (int)(m_saturation * hsv_size.x);
					int cursor_y = hsv_pos.y + (int)((1.f - m_value) * hsv_size.y);
					int cursor_size = 8;

					g_render->rect(cursor_x - cursor_size / 2, cursor_y - cursor_size / 2, cursor_size, cursor_size, hue::c_color(255, 255, 255).modulate(animations::m_colorpicker_value.val()), 4);
					g_render->rect(cursor_x - cursor_size / 2 + 1, cursor_y - cursor_size / 2 + 1, cursor_size - 2, cursor_size - 2, hue::c_color(0, 0, 0).modulate(animations::m_colorpicker_value.val()), 4);
				}

				{
					int hue_cursor_x = hue_pos.x + (int)((m_hue / 360.f) * hue_size.x);
					int hue_cursor_width = 4;
					g_render->rect_filled(hue_cursor_x - hue_cursor_width / 2, hue_pos.y - (2 ), hue_cursor_width, hue_size.y + (4),
						hue::c_color(255, 255, 255).modulate(animations::m_colorpicker_value.val()), 2.f);
					g_render->rect(hue_cursor_x - hue_cursor_width / 2, hue_pos.y - (2 ), hue_cursor_width, hue_size.y + (4),
						hue::c_color(0, 0, 0).modulate(animations::m_colorpicker_value.val()), 2.f);
				}

				{
					int alpha_cursor_x = alpha_bar.x + (int)((this->m_val->a / 255.f) * alpha_size.x);
					int alpha_cursor_width = 4;
					g_render->rect_filled(alpha_cursor_x - alpha_cursor_width / 2, alpha_bar.y - (2), alpha_cursor_width, alpha_size.y + (4),
						hue::c_color(255, 255, 255).modulate(animations::m_colorpicker_value.val()), 2.f);
					g_render->rect(alpha_cursor_x - alpha_cursor_width / 2, alpha_bar.y - (2), alpha_cursor_width, alpha_size.y + (4),
						hue::c_color(0, 0, 0).modulate(animations::m_colorpicker_value.val()), 2.f);
				}

					//g_render->rect(pallete_pos.x - 1, pallete_pos.y - 1, pallete_size.x + 2, pallete_size.y + 2, g_style->m_outline.modulate(animations::m_colorpicker_value.val()), 4.f);
					} // end else context_open
				dl->PopClipRect();
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
				g_ctx->m_click_consumed = true;
			}
		}
	}

	void c_colorpicker::input()
	{
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_hide_label ? g_font->f_icons.measure(ICON_FA_PALLET).x : m_child_size, m_size.y));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this) {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			g_ctx->push_focus(this, m_focus_priority); // we opened it mfucker
			m_context_open = false;
		}

		if (g_input->clicked(input::mouse_buttons::right) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			g_ctx->push_focus(this, m_focus_priority);
			m_context_open = true;
		}

		if (g_ctx->is_focused(this))
		{
			// inside colorpicker 
			math::c_vector_2d pallete_size = m_context_open ? math::c_vector_2d(80, 50) : math::c_vector_2d(180, 230);

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_PALETTE).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			ImVec2 display_size = ImGui::GetIO().DisplaySize;
			if (pallete_pos.x + pallete_size.x > display_size.x)
				pallete_pos.x = display_size.x - pallete_size.x - 5.f;
			if (pallete_pos.y + pallete_size.y > display_size.y)
				pallete_pos.y = display_size.y - pallete_size.y - 5.f;

			if (m_context_open) {
				math::c_rect copy_btn = math::c_rect(pallete_pos + math::c_vector_2d(0, 5), math::c_vector_2d(pallete_size.x, 20));
				math::c_rect paste_btn = math::c_rect(pallete_pos + math::c_vector_2d(0, 25), math::c_vector_2d(pallete_size.x, 20));

				if (g_input->clicked(input::mouse_buttons::left)) {
					if (g_input->mouse_in_region(copy_btn.pos(), copy_btn.size())) {
						char hex_str[10];
						snprintf(hex_str, sizeof(hex_str), "#%02X%02X%02X%02X", m_val->r, m_val->g, m_val->b, m_val->a);
						ImGui::SetClipboardText(hex_str);
						g_ctx->pop_focus(this);
					} else if (g_input->mouse_in_region(paste_btn.pos(), paste_btn.size())) {
						const char* clip = ImGui::GetClipboardText();
						if (clip != nullptr && clip[0] == '#') {
							int r, g, b, a = 255;
							int parsed = sscanf(clip, "#%02X%02X%02X%02X", &r, &g, &b, &a);
							if (parsed >= 3) {
								m_val->r = r; m_val->g = g; m_val->b = b;
								if (parsed == 4) m_val->a = a;
								rgb_to_hsv();
							}
						}
						g_ctx->pop_focus(this);
					} else if (!g_input->mouse_in_region(pallete_pos, pallete_size)) {
						g_ctx->pop_focus(this);
						g_ctx->m_click_consumed = true;
					}
				}
			} else {
				// hsv pos
				math::c_vector_2d hsv_pos = { pallete_pos.x + 8, pallete_pos.y + 33 };
				math::c_vector_2d hsv_size = { pallete_size.x - 16, 180 - 41 };

				// hue bar
				math::c_vector_2d hue_pos = { hsv_pos.x, hsv_pos.y + hsv_size.y + 10 };
				math::c_vector_2d hue_size = { hsv_size.x, 10 };

				// alpha bar
				math::c_vector_2d alpha_bar = { hue_pos.x, hue_pos.y + 20 };
				math::c_vector_2d alpha_size = { hsv_size.x, 10 };

				if (g_input->click_down(input::mouse_buttons::left) &&
					g_input->mouse_in_region(hsv_pos, hsv_size)) {

					auto mouse_pos = g_input->get_mouse_position();
					m_saturation = std::clamp((mouse_pos.x - hsv_pos.x) / (float)hsv_size.x, 0.f, 1.f);
					m_value = std::clamp(1.f - (mouse_pos.y - hsv_pos.y) / (float)hsv_size.y, 0.f, 1.f);
					hsv_to_rgb();
				}

				if (g_input->click_down(input::mouse_buttons::left) &&
					g_input->mouse_in_region(hue_pos, hue_size)) {

					auto mouse_pos = g_input->get_mouse_position();
					m_hue = std::clamp((mouse_pos.x - hue_pos.x) / (float)hue_size.x, 0.f, 1.f) * 360.f;
					hsv_to_rgb();
				}

				if (g_input->click_down(input::mouse_buttons::left) &&
					g_input->mouse_in_region(alpha_bar, alpha_size)) {

					auto mouse_pos = g_input->get_mouse_position();
					this->m_val->a = (int)(std::clamp((mouse_pos.x - alpha_bar.x) / (float)alpha_size.x, 0.f, 1.f) * 255.f);
				}
			}
		}
	}

	void c_colorpicker::rgb_to_hsv()
	{
		float r = m_val->r / 255.f;
		float g = m_val->g / 255.f;
		float b = m_val->b / 255.f;

		float max_val = std::max({ r, g, b });
		float min_val = std::min({ r, g, b });
		float delta = max_val - min_val;

		m_value = max_val;
		if (max_val == 0.f) {
			m_saturation = 0.f;
		}
		else {
			m_saturation = delta / max_val;
		}

		if (delta == 0.f) {
			m_hue = 0.f;
		}
		else if (max_val == r) {
			m_hue = 60.f * fmod(((g - b) / delta), 6.f);
		}
		else if (max_val == g) {
			m_hue = 60.f * (((b - r) / delta) + 2.f);
		}
		else {
			m_hue = 60.f * (((r - g) / delta) + 4.f);
		}

		if (m_hue < 0.f) m_hue += 360.f;
	}

	void c_colorpicker::hsv_to_rgb()
	{
		float c = m_value * m_saturation;
		float x = c * (1.f - std::abs(fmod(m_hue / 60.f, 2.f) - 1.f));
		float m = m_value - c;

		float r, g, b;
		if (m_hue < 60.f) { r = c; g = x; b = 0; }
		else if (m_hue < 120.f) { r = x; g = c; b = 0; }
		else if (m_hue < 180.f) { r = 0; g = c; b = x; }
		else if (m_hue < 240.f) { r = 0; g = x; b = c; }
		else if (m_hue < 300.f) { r = x; g = 0; b = c; }
		else { r = c; g = 0; b = x; }

		this->m_val->r = (int)((r + m) * 255.f);
		this->m_val->g = (int)((g + m) * 255.f);
		this->m_val->b = (int)((b + m) * 255.f);
	}
}
