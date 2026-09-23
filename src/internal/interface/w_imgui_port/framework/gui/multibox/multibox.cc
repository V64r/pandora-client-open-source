#include "../../../includes.hh"

namespace framework
{
	c_multidropdown::c_multidropdown(std::string label, bool hide_label)
	{
		m_label = std::move(label);
		m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 30 };

		m_type = element_type::dropdown;
		m_focus_priority = focus_priority::persistent;

		// we only use this in terms of inlining elements
		m_parent_width = m_child_size;
	}


	void c_multidropdown::draw()
	{
		animations::m_dropdown_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		// base::m_dropdown_value = builder::create_animation_ctx(m_parent + m_label + "#m_mdropdown_value", m_visible && (this->m_items[*this->m_val] != "None") || m_focused == this && g_ctx->m_open, 0.5);
		animations::m_dropdown_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_mdropdown_hover", m_visible && g_ctx->m_hovered == this, 0.5);
		animations::m_dropdown_open = utils::builder::create_animation_ctx(m_parent + m_label + "#m_mdropdown_open", m_visible && g_ctx->is_focused(this), 0.5);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_dropdown_open.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_dropdown_open.val());
		else if (animations::m_dropdown_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_dropdown_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_ndropdown_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_dropdown_opacity.val() * smooth_opacity;


		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));

				g_render->rect_shadow((m_pos + position).x, (m_pos + position).y, m_child_size, 25.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3).val()), 8.f, 3.f);
				animations::m_window_opacity.restore();
				g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, 25.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 3.f);

				// value selected

				/* selection preview */
				std::string preview_selection{};
				std::vector<std::string> selected_labels;
				for (const auto& data : this->m_data) {
					if (data.m_visible && !data.m_visible()) continue;
					if (data.m_val && *data.m_val) selected_labels.push_back(data.m_label);
				}

				const float preview_width = (std::max)(0.f, m_child_size - 32.f);
                for (const auto& label : selected_labels) {
                    if (!preview_selection.empty()) preview_selection += ", ";
                    preview_selection += label;
                }
				if (preview_selection.empty()) preview_selection = "-";

				const auto text_pos = m_pos + position + math::c_vector_2d(6, 3);
				g_render->push_clip(text_pos.x, (m_pos + position).y,
					preview_width, 25.f);
				ImDrawList* preview_draw = g_render->draw_list();
                const int first_vertex = preview_draw->VtxBuffer.Size;
                g_font->f_childs.text(text_pos.x, text_pos.y, preview_selection,
                    g_style->m_text.modulate(animations::m_window_opacity.limit(0.3f).val()));
                if (g_font->f_childs.measure(preview_selection).x > preview_width) {
                    const float right = text_pos.x + preview_width;
                    for (int v = first_vertex; v < preview_draw->VtxBuffer.Size; ++v) {
                        auto& vertex = preview_draw->VtxBuffer[v];
                        const float fade = std::clamp((right - vertex.pos.x) / 30.f, 0.f, 1.f);
                        const ImU32 alpha = static_cast<ImU32>(((vertex.col >> IM_COL32_A_SHIFT) & 255) * fade);
                        vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
                    }
                }
				g_render->restore_clip();

				engine::rotation_data_t child_angle = {};
				child_angle.set_draw_list(g_render->draw_list());
				child_angle.rotation_start();
				g_render->enlarged_arrow(math::c_vector_2d(this->m_pos.x + m_child_size - 20, (m_pos + position + math::c_vector_2d(5, 2)).y - (animations::m_dropdown_open.val() > 0.f ? 1 * animations::m_dropdown_open.val() : 1)),
					g_style->m_text.modulate(animations::m_window_opacity.limit(0.2f).val()).lerp(g_style->m_accent.modulate(animations::m_window_opacity.limit(1.0f).val()), animations::m_dropdown_open.val()), ImGuiDir_::ImGuiDir_Right, 15.f);
				child_angle.rotation_end(IM_PI * animations::m_dropdown_open.val(), child_angle.rotation_center());

			});

		
		if (animations::m_dropdown_open.val() > 0.f)
		{
			auto position2 = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 35);
			int visible_count = 0;
			for (const auto& d : this->m_data) if (!d.m_visible || d.m_visible()) visible_count++;
			auto calculated_size = math::c_vector_2d(m_child_size, (visible_count * g_font->f_childs.measure(m_label).y + 1) + 6 /* 3 top / 3 bottom */);

			float t = animations::m_dropdown_open.val();
			float eased = t * t * (3.f - 2.f * t);

			// origin is the top edge of the dropdown
			float origin_y = (m_pos + position2).y;
			float origin_x = (m_pos + position2).x + calculated_size.x * 0.5f; // center x so it doesnt skew sideways


			auto target_list = ImGui::GetForegroundDrawList();
			int vtx_start = target_list->VtxBuffer.Size;

			g_render->use_layer(engine::render_layer::prioritized, [&]()
				{
					g_render->draw_list()->PushClipRectFullScreen();
					g_render->rect_shadow((m_pos + position2).x, (m_pos + position2).y, calculated_size.x, calculated_size.y, g_style->m_window_shadow.modulate(animations::m_dropdown_open.limit(0.3).val()), 8.f, 3.f);
					animations::m_dropdown_open.restore();
					g_render->rect_filled((m_pos + position2).x, (m_pos + position2).y, calculated_size.x, calculated_size.y, g_style->m_element_base.modulate(animations::m_dropdown_open.val()), 3.f);

					int y = m_pos.y + (position2.y + 3);
					for (int i = 0; i < this->m_data.size(); i++)
					{
						auto dropdown = this->m_data[i];
						if (dropdown.m_visible && !dropdown.m_visible()) continue;

						auto bounding_switch = math::c_rect(m_pos.x + position2.x + 5, (float)y, calculated_size.x, g_font->f_childs.measure(m_label).y);
						if (g_input->mouse_in_region(bounding_switch.pos(), bounding_switch.size()) && g_input->clicked(input::mouse_buttons::left))
						{
							//*this->val = i;
							*dropdown.m_val = !*dropdown.m_val;
							g_ctx->m_click_consumed = true;
						}

						animations::m_dropdown_switch = utils::builder::create_animation_ctx(dropdown.m_label + std::to_string(i), (*dropdown.m_val) && g_ctx->is_focused(this), 0.5);
						animations::m_dropdown_option_hover = utils::builder::create_animation_ctx(dropdown.m_label + std::to_string(i) + "anim", g_input->mouse_in_region(bounding_switch.pos(), bounding_switch.size()) && g_ctx->is_focused(this), 0.5);


						g_font->f_childs.text(m_pos.x + position2.x + 5 + (5 * animations::m_dropdown_option_hover.val()), (float)y, dropdown.m_label,
							g_style->m_text.modulate(animations::m_dropdown_open.limit(0.3f).val()).lerp(g_style->m_accent.modulate(animations::m_dropdown_open.limit(1.f).val()), animations::m_dropdown_switch.val()));

						animations::m_dropdown_open.restore();

						y += g_font->f_childs.measure(m_label).y;
					}
					g_render->draw_list()->PopClipRect();

				}, true /* we have to override push_clip */);

			int vtx_end = target_list->VtxBuffer.Size;

			// post process — scale Y from top edge, fade alpha
			ImDrawVert* verts = target_list->VtxBuffer.Data;
			for (int v = vtx_start; v < vtx_end; v++)
			{
				// grow from top down
				verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

				// keep x centered so shadow doesnt skew
				verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;

				// fade
				int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
				verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
			}

			// check if we click oob
			math::c_rect bounding_data = math::c_rect(m_pos.x + position2.x, m_pos.y + position2.y, calculated_size.x, calculated_size.y);

			if (!g_input->mouse_in_region(bounding_data.pos(), bounding_data.size()) && g_input->clicked(input::mouse_buttons::left))
			{
				g_ctx->pop_focus(this);
			}
		}

		animations::m_window_opacity.restore();
	}

	void c_multidropdown::input()
	{
		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		math::c_rect bounding = math::c_rect(m_pos + math::c_vector_2d(0, position.y), math::c_vector_2d(m_child_size, 20.f));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			g_ctx->push_focus(this, m_focus_priority); // we opened it mfucker
			g_ctx->m_click_consumed = true;
		}

		if (g_ctx->is_focused(this)) {
			// calculate popup rect
			std::vector<std::string> visible_items;
			for (size_t i = 0; i < m_data.size(); i++) {
				if (!m_data[i].m_visible || m_data[i].m_visible()) {
					visible_items.push_back(m_data[i].m_label);
				}
			}
			float h = 20.f * visible_items.size();
			math::c_rect popup_rect = math::c_rect(m_pos.x + position.x, m_pos.y + position.y + 20.f, m_child_size, h);
			if (g_input->mouse_in_region(popup_rect.pos(), popup_rect.size())) {
				g_ctx->m_click_consumed = true;
			}
		}
	}

	void c_multidropdown::add_selection(std::string label, bool* val, std::function<bool()> visible)
	{
		this->m_data.push_back({ label, val, visible });
	}
}
