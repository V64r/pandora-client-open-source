#include "../../../includes.hh"
extern bool g_ConfigInputActive;

namespace framework
{
	c_listbox::c_listbox(std::string label, int* var, std::vector<std::string> items, float height, bool hide_label) : m_var(var), m_items(std::move(items)), m_height(height)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;

		m_height = height;

		m_size = math::c_vector_2d(m_child_size, height + g_font->f_childs.measure(m_label).y + 5 + 8.f);
		m_type = framework::element_type::listbox;
		m_focus_priority = focus_priority::interactive;

		// we wont inline so there is no need for m_parent_width
	}

	void c_listbox::draw()
	{
		animations::m_listbox_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_listbox_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_listbox_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		const float height = m_height;

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_listbox_hover.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_listbox_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_listbox_opacity.val() * smooth_opacity;

		if (!m_hide_label)
			g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);

		g_render->rect_shadow((m_pos + position).x, (m_pos + position).y, m_child_size, height, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3).val()), 8.f, 3.f);
		animations::m_window_opacity.restore();
		g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, height, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 3.f);

		// scrolling logic
		float visible_height = height - 10;
		float total_height = (g_font->f_childs.measure(m_label).y + 5) * this->m_items.size();
		float max_scroll = std::max(0.f, total_height - visible_height);

		bool listbox_hovered = g_input->mouse_in_region(m_pos + position, math::c_vector_2d(m_child_size, height));
		float scroll_delta = ImGui::GetIO().MouseWheel;

		if (listbox_hovered && m_visible && g_ctx->m_open && max_scroll > 0.f)
		{
			float scroll_speed = 40.f;
			float scroll_delta_inner = ImGui::GetIO().MouseWheel;
			m_scroll_target -= scroll_delta_inner * scroll_speed;
			m_scroll_target = std::clamp(m_scroll_target, 0.f, max_scroll);
		}

		float lerp_speed_scroll = 0.15f;
		m_scroll_offset += (m_scroll_target - m_scroll_offset) * lerp_speed_scroll;
		if (std::abs(m_scroll_target - m_scroll_offset) < 0.5f)
		{
			m_scroll_offset = m_scroll_target;
		}

		animations::m_listbox_scroll = utils::builder::create_animation_ctx(
			m_parent + m_label + "#m_listbox_scroll", true, 0.25);

		float dt = utils::g_anim_base.delta_time(1.f);

		// lerp toward target
		animations::m_listbox_scroll.m_value += (m_scroll_offset - animations::m_listbox_scroll.m_value) * std::clamp(dt, 0.f, 1.f);
		animations::m_listbox_scroll.animate(animations::m_listbox_scroll.m_value, 0.f, max_scroll, true);

		// read back the clamped value
		float m_scroll_offset = utils::stack[animations::m_listbox_scroll.m_id];

		if (m_editing_index >= 0) g_ConfigInputActive = true;
		// draw items
		g_render->push_clip((m_pos + position).x, (m_pos + position).y, m_child_size, height);

		int start_y = (m_pos + position).y + 5;

		// Cancel editing if list changed underneath us
		if (m_editing_index >= (int)this->m_items.size()) {
			m_editing_index = -1;
		}

		for (int i = 0; i < this->m_items.size(); i++)
		{
			auto item = this->m_items[i];

			// item y
			float item_y = start_y + ((g_font->f_childs.measure(m_label).y + 5) * i) - m_scroll_offset;
			if (item_y + (g_font->f_childs.measure(m_label).y + 5) < (m_pos + position).y ||
				item_y > (m_pos + position).y + height)
				continue;

			animations::m_listbox_value = utils::builder::create_animation_ctx(m_parent + item + std::to_string(i) + "#m_listbox_value", m_visible && this->m_var && *this->m_var == i && g_ctx->m_open, 0.5);

			// setup hovering data
			m_listbox_item_hovered = g_input->mouse_in_region(math::c_vector_2d((m_pos + position).x + 6, item_y), math::c_vector_2d(m_child_size - 12, g_font->f_childs.measure(m_label).y));
			animations::m_listbox_item_hover = utils::builder::create_animation_ctx(m_parent + item + std::to_string(i) + "#m_listbox_item_hover", m_visible && this->m_listbox_item_hovered && g_ctx->m_open, 0.5);
			animations::m_listbox_item_drag = utils::builder::create_animation_ctx(m_parent + item + std::to_string(i) + "#m_listbox_item_drag", m_visible && m_dragging_index == i && g_ctx->m_open, 0.5);

			float drag_anim = animations::m_listbox_item_drag.val();
			float rendered_y = item_y;
			if (drag_anim > 0.01f) {
				float mouse_y = ImGui::GetIO().MousePos.y - 5.f;
				rendered_y = item_y + (mouse_y - item_y) * drag_anim;
				rendered_y = std::clamp(rendered_y, (float)(m_pos + position).y + 2.f,
					(float)(m_pos + position).y + height - g_font->f_childs.measure(m_label).y - 2.f);
			}

			// Handle inline editing mode
			if (m_editing_index == i)
			{
				// Process keyboard input
				auto& io = ImGui::GetIO();
				m_edit_cursor_timer += io.DeltaTime;

				// Handle character input
				for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
					ImWchar c = io.InputQueueCharacters[n];
					if (c >= 32 && c < 127) {
						m_edit_buffer += (char)c;
						m_edit_cursor_timer = 0.f;
					}
				}

				// Backspace
				if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !m_edit_buffer.empty()) {
					m_edit_buffer.pop_back();
					m_edit_cursor_timer = 0.f;
				}

				// Enter -> confirm rename
				if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
					if (!m_edit_buffer.empty() && m_edit_buffer != item && on_rename) {
						on_rename(i, m_edit_buffer);
					}
					m_editing_index = -1;
				}

				// Escape -> cancel
				if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
					m_editing_index = -1;
				}

				// Click outside -> cancel
				if (g_input->clicked(input::mouse_buttons::left) && !m_listbox_item_hovered) {
					m_editing_index = -1;
				}

				// Draw the editing text with blinking cursor
				g_render->rect(m_pos.x + 6, rendered_y + 3, 11, 11, hue::c_color().modulate(0.3), 50.f);
				g_render->rect_filled(m_pos.x + 6, rendered_y + 3, 11, 11, g_style->m_accent.modulate(1.f), 50.f);

				bool show_cursor = fmodf(m_edit_cursor_timer, 1.0f) < 0.5f;
				std::string display_text = m_edit_buffer + (show_cursor ? "|" : "");
				g_font->f_childs.text((m_pos + position).x + 22, rendered_y, display_text,
					g_style->m_accent.modulate(animations::m_window_opacity.limit(1.f).val()));
			}
			else
			{
				// Normal (non-editing) input
				if (this->m_listbox_item_hovered)
				{
					m_hovering_index = i;
					if (g_input->clicked(input::mouse_buttons::left))
					{
						if (this->m_var) *this->m_var = i;
					}
					if (m_editable && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && on_rename)
					{
						// Enter inline edit mode
						m_editing_index = i;
						m_edit_buffer = item;
						m_edit_cursor_timer = 0.f;
					}
					if (g_input->clicked(input::mouse_buttons::right))
					{
						if (this->m_var) *this->m_var = i;
						if (on_right_click) on_right_click(i);
					}
					if (m_reorderable && on_reorder && g_input->click_down(input::mouse_buttons::left) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.f) && m_dragging_index == -1)
					{
						m_dragging_index = i;
					}
				}

				ImTextureID texture = item_texture ? item_texture(i, item) : nullptr;
				if (texture)
					g_render->image(m_pos.x + 5, rendered_y, 17, 17, texture, hue::c_color(255,255,255,255), 3.f);
				else if (m_markers_visible) {
					g_render->rect(m_pos.x + 6, rendered_y + 3, 11, 11, hue::c_color().modulate(0.3), 50.f);
					g_render->rect_filled(m_pos.x + 6, rendered_y + 3, 11, 11, g_style->m_accent.modulate(std::max(animations::m_listbox_value.val(), drag_anim)), 50.f);
				}

				const float text_offset = m_markers_visible ? 27.f : 5.f;
				g_font->f_childs.text((m_pos + position).x + text_offset + (6 * animations::m_listbox_item_hover.val()) + (12 * drag_anim), rendered_y, item,
					g_style->m_text.modulate(animations::m_window_opacity.limit(0.3f).val()).lerp(g_style->m_accent.modulate(animations::m_window_opacity.limit(1.f).val()), std::max(animations::m_listbox_value.val(), drag_anim)));
			}
		}

		if (!g_input->click_down(input::mouse_buttons::left))
		{
			if (m_dragging_index != -1 && m_hovering_index != -1 && m_dragging_index != m_hovering_index && on_reorder)
			{
				on_reorder(m_dragging_index, m_hovering_index);
			}
			m_dragging_index = -1;
		}

		g_render->restore_clip();
	}

	void c_listbox::input()
	{
		math::c_rect bounding = math::c_rect(m_pos, m_size);

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		float scroll_delta = ImGui::GetIO().MouseWheel;
		if (g_input->mouse_in_region(bounding.pos(), bounding.size()) && scroll_delta != 0.f)
		{
			g_ctx->m_hovered = this;
		}

		if (g_ctx->m_hovered == this)
		{
			float scroll_delta = ImGui::GetIO().MouseWheel;
		}

		if (g_ctx->m_hovered == this && g_input->clicked(input::mouse_buttons::left))
		{
			// *this->m_value = !*this->m_value;
		}
	}
}
