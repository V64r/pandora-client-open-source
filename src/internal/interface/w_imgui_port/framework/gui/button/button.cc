#include "../../../includes.hh"
#include <cmath>

namespace framework
{
	c_button::c_button(std::string label, std::function<void()> callback) : m_callback(std::move(callback))
	{
		m_label = std::move(label);
		m_size = math::c_vector_2d(0, 25);

		m_type = element_type::button;
		m_focus_priority = focus_priority::interactive;

			m_parent_width = m_child_size;
	}

	void c_button::draw()
	{
		animations::m_button_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_button_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_button_value", m_visible && this->m_callback_called && g_ctx->m_open, 0.25);
		animations::m_button_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_button_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		if (animations::m_button_value.val() > 0.99f)
		{
			this->m_callback_called = false;
		}
		float target_opacity = 0.2f;
		if (animations::m_button_value.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_button_value.val());
		else if (animations::m_button_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_button_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_button_opacity.val() * smooth_opacity;



		g_render->use_layer(m_layer, [&]()
			{
				float bg_opacity = animations::m_window_opacity.val() * animations::m_button_opacity.val();
				if (m_hold_to_activate) {
					const int x = static_cast<int>(m_pos.x);
					const int y = static_cast<int>(m_pos.y);
					const int width = static_cast<int>(m_child_size);
					constexpr int height = 25;
					constexpr float rounding = 6.0f;
					const float progress = std::clamp(m_hold_progress, 0.0f, 1.0f);
					const hue::c_color base = hue::c_color(40, 20, 20, 255).modulate(bg_opacity);
					const hue::c_color border = hue::c_color(80, 30, 30, 255).modulate(bg_opacity);
					const hue::c_color fill = hue::c_color(100, 30, 30, 255).modulate(bg_opacity);

					g_render->rect_shadow(x, y, width, height,
						g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3).val() * animations::m_button_opacity.val()), 8.f, rounding);
					g_render->rect_filled(x, y, width, height, base, rounding);
					if (progress > 0.0f) {
						g_render->draw_list()->PushClipRect(ImVec2(static_cast<float>(x), static_cast<float>(y)),
                            ImVec2(x + width * progress, static_cast<float>(y + height)), true);
                        g_render->rect_filled(x, y, width, height, fill, rounding);
                        g_render->draw_list()->PopClipRect();
					}
					g_render->rect(x, y, width, height, border, rounding, 1.0f);

					const std::string label = progress > 0.0f ? "Hold to unload..." : "Unload client";
					const std::string icon = ICON_FA_ARROW_RIGHT_FROM_BRACKET;
					const float icon_width = g_font->f_icons.measure(icon).x;
					const float text_width = g_font->f_childs.measure(label).x;
					const float gap = 6.0f;
					const float content_width = icon_width + gap + text_width;
					const float start_x = m_pos.x + (m_child_size - content_width) * 0.5f;
					const auto text_color = progress > 0.0f
						? hue::c_color(255, 255, 255, 255).modulate(final_opacity)
                        : hue::c_color(165, 65, 65, 255).modulate(final_opacity);
					g_font->f_icons.text(static_cast<int>(start_x), static_cast<int>(m_pos.y + 4), icon, text_color);
					g_font->f_childs.text(static_cast<int>(start_x + icon_width + gap), static_cast<int>(m_pos.y + 3), label, text_color);
					return;
				}

				g_render->rect_shadow(m_pos.x, m_pos.y, m_child_size, 25.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3).val() * animations::m_button_opacity.val()), 8.f, 3.f);
				animations::m_window_opacity.restore();
				g_render->rect_filled(m_pos.x, m_pos.y, m_child_size, 25.f, g_style->m_element_base.modulate(bg_opacity), 3.f);

				g_font->f_childs.text(
					m_pos.x + (m_child_size * 0.5) - (g_font->f_childs.measure(this->m_label).x * 0.5),
					m_pos.y + 3,
					m_label,
					g_style->m_text.modulate(final_opacity).lerp(g_style->m_accent.modulate(final_opacity), animations::m_button_value.val()));

			});

		
	}

	void c_button::input()
	{
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_child_size, m_size.y));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		// we do not do return if the focus is nullptr as we want to access this
		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this) {
			g_ctx->m_hovered = nullptr;
		}

		if (m_hold_to_activate)
		{
			const bool holding = g_ctx->m_hovered == this && g_input->click_down(input::mouse_buttons::left);
			if (holding && !m_hold_fired)
				m_hold_progress = std::min(1.f, m_hold_progress + ImGui::GetIO().DeltaTime / m_hold_seconds);
			else if (!holding && !m_hold_fired)
				m_hold_progress = std::max(0.f, m_hold_progress - ImGui::GetIO().DeltaTime * 2.f);

			if (m_hold_progress >= 1.f && !m_hold_fired)
			{
				m_hold_fired = true;
				m_callback_called = true;
				this->m_callback();
			}
			if (!g_input->click_down(input::mouse_buttons::left) && m_hold_fired)
			{
				m_hold_fired = false;
				m_hold_progress = 0.f;
			}
		}
		else if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
		{
			this->m_callback();
			m_callback_called = true;
			m_ripples.push_back({ 0.f });
		}
	}
}
