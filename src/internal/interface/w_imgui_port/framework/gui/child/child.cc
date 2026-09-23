#include "../../../includes.hh"

namespace framework
{
	c_child::c_child(std::string name, child_width width, float y) : m_name(std::move(name)), m_width_type(width), m_size(math::c_vector_2d(0, y)), m_titlebar(true) {}

	void c_child::draw()
	{
		int vtx_start = g_render->draw_list()->VtxBuffer.Size;

		g_render->rect_shadow(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, hue::c_color(0, 0, 0).modulate(this->m_child_opacity.limit(0.3).val()), 10.f, 7.f);
		this->m_child_opacity.restore();

		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_child_background.modulate(this->m_child_opacity.val()), 7);
		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, 35,
			g_style->m_child_bars.modulate(this->m_child_opacity.val()), 7,
			engine::draw_flags_::draw_flags_round_corners_top);

		if (this->m_titlebar)
		{
			g_render->gradient(this->m_pos.x, this->m_pos.y + 35, this->m_size.x, 10, g_style->m_window_shadow.modulate(this->m_child_opacity.limit(0.2).val()), g_style->m_window_shadow.modulate(this->m_child_opacity.limit(0.1).val()).with_alpha(0), engine::fade_direction::horizontally);
			this->m_child_opacity.restore();
			g_font->f_default.text(this->m_pos.x + 10, this->m_pos.y + 7, this->m_name, g_style->m_text.modulate(this->m_child_opacity.limit(0.6).val()));
		}
		this->m_child_opacity.restore();

		// element handling
		float padding = 0.f;
		c_base_element* m_parent_control = nullptr;

		math::c_vector_2d content_start = this->m_pos + this->calculate_element_padding();
		float content_area_height = this->m_size.y - this->calculate_element_padding().y;

		for (auto& control : m_controls)
		{
			// Inline controls need the current panel width before their position is
			// calculated. Otherwise a keybind may reuse a stale width and float.
			control->m_child_size = this->m_size.x - 25.f;
			if (fn && visible())
				fn();

			// if control is inlined we use parent's control data to init this one
			if (control->m_inlined && m_parent_control != nullptr) {
				// we have to check if the element is a slider since it needs abit more presuring on it
				if (control->m_type == element_type::slider)
				{
					// if its slider we also have to lower out the m_child_size
					int base_scalling = (this->m_size.x - 20.f);

					// check if parent is slider
					if (m_parent_control->m_type == element_type::slider)
					{
						m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5) - 7.5;
					}

					control->m_child_size = (base_scalling * 0.5) - 7.5;

					// hide label if the parent control is a checkbox
					if (m_parent_control->m_type == element_type::checkbox && !control->m_hide_label)
					{
						// force label hide
						control->hide_label();
					}

					// check if parent control is checkbox so we align it when we have no label
					if (m_parent_control->m_type == element_type::checkbox && control->m_hide_label)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 4);
					}
					else {
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 0);
					}
				}
				else if (control->m_type == element_type::button)
				{
					// check if parent is not a button, so we dont allow it
					if (m_parent_control->m_type != element_type::button)
					{
						// do not allow inlining
						control->m_inlined = false;
						continue;
					}

					// if its slider we also have to lower out the m_child_size
					int base_scalling = (this->m_size.x - 20.f);

					m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5) - 7.5;
					control->m_child_size = (base_scalling * 0.5) - 7.5;

					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 10.f, 0);
				}
				else if (control->m_type == element_type::text_input)
				{
					// check if parent is not a button, so we dont allow it
					if (m_parent_control->m_type != element_type::text_input)
					{
						// do not allow inlining
						control->m_inlined = false;
						continue;
					}

					// if its slider we also have to lower out the m_child_size
					int base_scalling = (this->m_size.x - 20.f);

					m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5) - 7.5;
					control->m_child_size = (base_scalling * 0.5) - 7.5;

					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 10.f, 0);
				}
				else if (control->m_type == element_type::colorpicker)
				{
					// if the colorpicker is inlined disabled label
					control->hide_label();

					// if we are inlining to a checkbox we are setting parent + pos - icon
					if (m_parent_control->m_type == element_type::checkbox)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d((control->m_child_size - g_font->f_icons.measure(ICON_FA_PALETTE).x) - 2, 0);
					} // down from here is multiinlining
					else if (m_parent_control->m_type == element_type::colorpicker)
					{
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(m_parent_control->m_parent_width + 5.f, 0);
					}
					else
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(
							m_parent_control->m_child_size - g_font->f_icons.measure(ICON_FA_PALETTE).x - 2.f, 0.f);
					}
				}
				else if (control->m_type == element_type::keybind)
				{
					// if the colorpicker is inlined disabled label
					control->hide_label();

					// if we are inlining to a checkbox we are setting parent + pos - icon
					if (m_parent_control->m_type == element_type::checkbox)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d((control->m_child_size - g_font->f_icons.measure(ICON_FA_KEYBOARD).x) - 2, 0);
					} // down from here is multiinlining
					else if (m_parent_control->m_type == element_type::colorpicker)
					{
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(m_parent_control->m_parent_width + 8.f, 0);
					}
				}
				else if (control->m_type == element_type::popup)
				{
					// if the colorpicker is inlined disabled label
					control->hide_label();

					// if we are inlining to a checkbox we are setting parent + pos - icon
					if (m_parent_control->m_type == element_type::checkbox)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d((control->m_child_size - g_font->f_icons.measure(ICON_FA_SQUARE_PLUS).x) - 2, 0);
					} // down from here is multiinlining
					else if (m_parent_control->m_type == element_type::colorpicker)
					{
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(m_parent_control->m_parent_width + 8.f, 0);
					}
				}
				else {
					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 0);
				}
			}
			else {
				control->m_pos = this->m_pos + math::c_vector_2d(0, padding - m_scroll_offset) + this->calculate_element_padding();
			}

			if (!this->visible())
			{
				continue;
			}

			// callback visibility
			bool is_visible_by_cb = true;
			if (control->m_callback_visibility && control->m_visible_by_callback)
				is_visible_by_cb = control->m_visible_by_callback();
				
				
			const std::string control_animation_id = m_name + "#" +
				this->m_child_attach_data.m_subtab_name + "#" + control->m_label;
			auto visibility_anim = utils::builder::create_animation_ctx(
				control_animation_id + "#visibility", is_visible_by_cb, 0.4f);
			float vis_val = visibility_anim.val();

			if (vis_val <= 0.01f && !is_visible_by_cb)
				continue;

			float control_top = control->m_pos.y;
			float control_bottom = control->m_pos.y + control->m_size.y;
			float visible_top = content_start.y;
			float visible_bottom = this->m_pos.y + this->m_size.y - 12.f;

			bool is_visible = false;

			if (!g_search.m_should_draw)
				is_visible = !(control_bottom < visible_top || control_top > visible_bottom);
			else 
				is_visible = true;

			animations::m_window_opacity = utils::builder::create_animation_ctx(
				control_animation_id + "#opacity", is_visible && g_ctx->m_open && is_visible_by_cb, 0.5f);

			// c_base_control::base
			// set the child parent, we are going to use this in the checkbox data ( if there are problems, make sure to include, tab subtab )
			control->set_parent(this->m_name + "#" + this->m_child_attach_data.m_subtab_name);


			// set base control visibility based on where we are
			control->set_visibility(this->visible() && is_visible && is_visible_by_cb);

			// c_base_control->element
			// no point in inputting if we have no menu opened
			if (g_ctx->m_open && is_visible)
			{
				// if a modal is open, only allow the popup that owns it to run input
				// everything else behind it gets blocked
				if (g_ctx->m_modal_owner != nullptr && control.get() != g_ctx->m_modal_owner)
				{
					// skip input for background elements
				}
				else if (g_ctx->can_interact(control.get(), control->m_focus_priority))
				{
					if (!g_search.m_should_draw)
						control->input();

				}
			}

			// engine::c_layout_engine(this->m_pos + this->calculate_element_padding(), this->calculate_safe_area())
			if (control->m_type != element_type::popup && control->m_type != element_type::interactive_preview && control->m_type != element_type::dropdown && control->m_type != element_type::colorpicker && control->m_type != element_type::keybind)
				g_render->push_clip((this->m_pos + this->calculate_element_padding()).x - 5.f, (this->m_pos + this->calculate_element_padding()).y - 5.f, this->calculate_safe_area().x, this->calculate_safe_area().y);

			if (is_visible)
			{
				if (control->m_call_stacks)
					control->m_call_stacks();

				control->draw();
			}

			if (control->m_type != element_type::popup && control->m_type != element_type::interactive_preview && control->m_type != element_type::dropdown && control->m_type != element_type::colorpicker && control->m_type != element_type::keybind)
				g_render->restore_clip();

			// push y only if the control is not inlined
			if (!control->m_inlined)
				padding += (control->m_size.y + ((6.f + control->m_bottom_spacing) * ImGui::GetIO().FontGlobalScale)) * vis_val;



			// we only set this if we do inlining
			control->m_parent_control = m_parent_control;
			m_parent_control = control.get();

			// g_ctx->m_focus_took = control.get();

			// check which control is focused
			if (g_ctx->m_focus_took != nullptr)

			animations::m_window_opacity.restore();
		}

		m_content_height = padding;
		if (m_auto_fit_height) {
			m_size.y = m_content_height + calculate_element_padding().y + 5.f;
			m_scroll_offset = 0.f;
			m_scroll_target = 0.f;
			m_max_scroll = 0.f;
		}
		else {
			m_max_scroll = std::max(0.f, m_content_height - content_area_height);
		}

		// reset it
		animations::m_window_opacity.restore();

		int vtx_end = g_render->draw_list()->VtxBuffer.Size;
		float anim = this->m_child_opacity.val();
		if (vtx_end > vtx_start && anim < 1.0f)
		{
			float center_x = this->m_pos.x + this->m_size.x * 0.5f;
			float center_y = this->m_pos.y + this->m_size.y * 0.5f;
			
			// Stronger but still compact category transition.
			float scale = 0.86f + (0.14f * anim);
			
			float y_offset = 44.f * (1.f - anim);

			ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
			for (int i = vtx_start; i < vtx_end; i++)
			{
				verts[i].pos.x = center_x + (verts[i].pos.x - center_x) * scale;
				verts[i].pos.y = center_y + (verts[i].pos.y - center_y) * scale + y_offset;
		
				ImU32 col = verts[i].col;
				int a = (col >> IM_COL32_A_SHIFT) & 0xFF;
				a = (int)(a * anim);
				verts[i].col = (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
			}
		}

	}

	void c_child::input()
	{
		bool is_hovered = g_input->mouse_in_region(this->m_pos, this->m_size);
		if (is_hovered && this->visible() && m_max_scroll > 0.f)
		{
			// Check if the mouse is over a scrollable listbox inside this child.
			// If so, let the listbox handle the scroll exclusively.
			bool listbox_wants_scroll = false;
			for (const auto& ctrl : m_controls) {
				if (ctrl && ctrl->m_type == element_type::listbox) {
					auto* lb = static_cast<c_listbox*>(ctrl.get());
					float lb_visible = lb->m_height - 10.f;
					float lb_total = (g_font->f_childs.measure(lb->m_label).y + 5.f) * lb->m_items.size();
					float lb_max_scroll = std::max(0.f, lb_total - lb_visible);
					if (lb_max_scroll > 0.f) {
						auto lb_position = lb->m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(lb->m_label).y + 5);
						if (g_input->mouse_in_region(lb->m_pos + lb_position, math::c_vector_2d(lb->m_child_size, lb->m_height))) {
							listbox_wants_scroll = true;
							break;
						}
					}
				}
			}

			if (!listbox_wants_scroll) {
				float scroll_speed = 40.f;
				float scroll_delta = ImGui::GetIO().MouseWheel;

				m_scroll_target -= scroll_delta * scroll_speed;
				m_scroll_target = std::clamp(m_scroll_target, 0.f, m_max_scroll);
			}
		}

		float lerp_speed = 0.15f;
		m_scroll_offset += (m_scroll_target - m_scroll_offset) * lerp_speed;

		if (std::abs(m_scroll_target - m_scroll_offset) < 0.5f)
		{
			m_scroll_offset = m_scroll_target;
		}
	}

	void c_child::attach_child(std::string tab_name, std::string subtab_name, int tab_index)
	{
		this->m_child_attach_data.m_tab_name = tab_name;
		this->m_child_attach_data.m_subtab_name = subtab_name;
		this->m_child_attach_data.tab = tab_index;

		static bool dummy_val = false;
		auto dummy_control = std::make_shared<framework::c_checkbox>(this->m_name, &dummy_val);
		g_search.add_to_database(dummy_control, tab_name, tab_index);
	}

		bool c_child::visible()
	{
		if (g_ctx->m_cur_tab != this->m_child_attach_data.m_tab_name)
		{
			return false;
		}

		bool same_tab = g_ctx->m_cur_tab == this->m_child_attach_data.m_tab_name;
		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_subtab.empty())
		{
			return true;
		}

		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_cur_subtab != this->m_child_attach_data.m_subtab_name)
		{
			return false;
		}

		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_cur_subtab == this->m_child_attach_data.m_subtab_name)
		{
			return true;
		}

		// if none of the conditions matched return false
		return false;
	}

	child_width c_child::get_type()
	{
		return this->m_width_type;
	}

	math::c_vector_2d c_child::calculate_element_padding()
	{
		if (this->m_titlebar)
		{
			return math::c_vector_2d(12.f, 45.f);
		}
		else {
			return math::c_vector_2d(10.f, 12.f);
		}
	}

	math::c_vector_2d c_child::calculate_safe_area()
	{
		if (this->m_titlebar)
		{
			return this->m_size - math::c_vector_2d(18.f, 50.f);
		}
		else {
			return this->m_size - math::c_vector_2d(20.f, 24.f);
		}
	}

	std::shared_ptr<framework::c_checkbox> c_child::add_checkbox(std::string label, bool* val)
	{
		auto control = std::make_shared<framework::c_checkbox>(label, val);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			// just add the control

			auto copied = std::make_shared<c_checkbox>(*control);

		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_float> c_child::add_slider_float(std::string label, float* val, float min, float max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_float>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			auto copied = std::make_shared<c_slider_float>(*control);

		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_int> c_child::add_slider_int(std::string label, int* val, int min, int max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_int>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_slider_int>(*control);

		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_dropdown> c_child::add_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label)
	{
		auto control = std::make_shared<c_dropdown>(label, val, items, hide_label);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_dropdown>(*control);

		}

		// allow chaining
		return control;
	}

	std::shared_ptr<c_multidropdown> c_child::add_multibox(std::string label, bool hide_label, std::function<void(c_multidropdown* ptr)> callback)
	{
		auto control = std::make_shared<c_multidropdown>(label, hide_label);
		{
			callback(control.get());

			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_multidropdown>(*control);

		}

		return control;
	}

	std::shared_ptr<c_button> c_child::add_button(std::string label, std::function<void()> callback)
	{
		auto control = std::make_shared<c_button>(label, callback);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_button>(*control);
		}

		return control;
	}

	std::shared_ptr<c_colorpicker> c_child::add_colorpicker(std::string label, hue::c_color* val, bool hide_label)
	{
		auto control = std::make_shared<c_colorpicker>(label, val, hide_label);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_colorpicker>(*control);

			//g_search.add_to_database(control, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);
		}

		return control;
	}

	std::shared_ptr<c_keybind> c_child::add_keybind(std::string label, key_var_t* val, bool hide_label)
	{
		auto control = std::make_shared<c_keybind>(label, val, hide_label);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_keybind>(*control);
		}

		return control;
	}

	std::shared_ptr<c_text_input> c_child::add_input_box(std::string label, std::string* val, bool hide_label)
	{
		auto control = std::make_shared<c_text_input>(label, val, hide_label);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_text_input>(*control);
		}
		return control;
	}

	std::shared_ptr<c_popup> c_child::add_popup(std::string label, bool hide_label, std::function<void(c_popup* ptr)> callback)
	{
		auto control = std::make_shared<c_popup>(label, hide_label);
		{
			callback(control.get());

			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_popup>(*control);
		}

		return control;
	}

		std::shared_ptr<c_listbox> c_child::add_listbox(std::string label, int* val, std::vector<std::string> items, float height, bool hide_label)
	{
		auto control = std::make_shared<c_listbox>(label, val, items, height, hide_label);
		{
			this->m_controls.push_back(control);
		}

		return control;
	}
}
