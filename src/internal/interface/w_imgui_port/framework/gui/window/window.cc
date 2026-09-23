#include "../../../includes.hh"
#include <d3dcompiler.h>
extern bool g_NumericEditActive;
#include <GL/gl.h>
#define ICON_FA_SEARCH "\xEF\x80\x82"

namespace framework
{
	static constexpr float k_sidebar_width = 66.f;
	static constexpr float k_header_height = 45.f;
	static constexpr float k_content_bottom_padding = 10.f;

	c_window::c_window(std::string title, math::c_vector_2d pos, math::c_vector_2d size) : m_title(title), m_pos(pos), m_size(size)
	{ 
	}

	
	void c_window::paint()
	{
		extern void UpdatePlayerHeadTextureOnRenderThread();
		extern GLuint g_PlayerHeadTexture;
		UpdatePlayerHeadTextureOnRenderThread();
		float t = animations::m_window_opacity.val();
		float eased = t * t * (3.f - 2.f * t);

		float origin_y = this->m_pos.y;
		float origin_x = this->m_pos.x + this->m_size.x * 0.5f;

		// middle layer doesn't pass ignore_clipping so it stays on m_draw_list
		ImDrawList* dl = g_render->draw_list();
		int vtx_start = dl->VtxBuffer.Size;

		g_render->rect_shadow(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y,
			hue::c_color(0, 0, 0).modulate(this->m_window_opacity.val() * 0.48f), 24.f, 12.f);
		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_background.modulate(this->m_window_opacity.val()), 15.f);
		g_render->rect_filled(this->m_pos.x + k_sidebar_width - 1.f, this->m_pos.y,
			this->m_size.x - k_sidebar_width + 1.f, k_header_height,
			g_style->m_child_background.modulate(this->m_window_opacity.val()), 15.f,
			engine::draw_flags_::draw_flags_round_corners_top_right);
		g_render->rect_filled(this->m_pos.x, this->m_pos.y, k_sidebar_width, this->m_size.y,
			g_style->m_child_background.modulate(this->m_window_opacity.val()), 15.f,
			engine::draw_flags_::draw_flags_round_corners_left);
		// Content background: the left edge rises and turns smoothly to the right.
		// Build the corner explicitly so later ImGui corner-mask changes cannot
		// flatten or reverse this inner junction.
		const float content_x = this->m_pos.x + k_sidebar_width;
		const float content_y = this->m_pos.y + k_header_height;
		const float content_right = this->m_pos.x + this->m_size.x;
		const float content_bottom = this->m_pos.y + this->m_size.y;
		constexpr float content_radius = 12.f;
		constexpr float bezier = 0.55228475f;
		// Contrast layer revealed by the curved cut-out.
		g_render->rect_filled(content_x, content_y, content_radius, content_radius,
			g_style->m_child_background.modulate(this->m_window_opacity.val()));
		dl->PathClear();
		dl->PathLineTo({ content_x + content_radius, content_y });
		dl->PathLineTo({ content_right, content_y });
		// Match the window's existing bottom-right radius without changing the approved top-left curve.
		constexpr float outer_radius = 15.f;
		dl->PathLineTo({ content_right, content_bottom - outer_radius });
		dl->PathBezierCubicCurveTo(
			{ content_right, content_bottom - outer_radius * (1.f - bezier) },
			{ content_right - outer_radius * (1.f - bezier), content_bottom },
			{ content_right - outer_radius, content_bottom });
		dl->PathLineTo({ content_x, content_bottom });
		dl->PathLineTo({ content_x, content_y + content_radius });
		dl->PathBezierCubicCurveTo(
			{ content_x, content_y + content_radius * (1.f - bezier) },
			{ content_x + content_radius * (1.f - bezier), content_y },
			{ content_x + content_radius, content_y });
		dl->PathFillConvex(g_style->m_window_background.modulate(this->m_window_opacity.val()).transform());

		g_render->push_clip(this->m_pos.x, this->m_pos.y, k_sidebar_width, k_header_height);
		g_render->rect_shadow(this->m_pos.x + 19.f, this->m_pos.y + 20.f, 20.f, 2.f,
			g_style->m_accent.modulate(this->m_window_opacity.limit(0.15).val()), 34.f, 0.f);
		g_render->restore_clip();


		this->m_window_opacity.restore();

		const float titleXOffset = this->m_pos.x + k_sidebar_width + 18.0f;
		g_font->f_default.text(titleXOffset, this->m_pos.y + 13, this->m_title, g_style->m_text.modulate(this->m_window_opacity.limit(0.6).val()));
		this->m_window_opacity.restore();

		// Tabs belong to the header and must be painted before the content clip.
		this->m_obj_tab->paint();
	
		// search input 
		{
			math::c_rect bounding = math::c_rect(this->m_pos.x - 12 + this->m_size.x - g_font->f_icons_medium.measure(ICON_FA_SEARCH).x, this->m_pos.y + +(45 * 0.5) - (g_font->f_icons_medium.measure(ICON_FA_SEARCH).y * 0.5), g_font->f_icons_medium.measure(ICON_FA_SEARCH).x, g_font->f_icons_medium.measure(ICON_FA_SEARCH).y);
			if (g_input->mouse_in_region(bounding.pos(), bounding.size()) &&
				g_input->clicked(input::mouse_buttons::left) && !g_ctx->m_click_consumed && !g_NumericEditActive)
			{
				g_search.m_should_draw = !g_search.m_should_draw;
				g_ctx->m_focus_stack.clear();
				if (!g_search.m_should_draw) g_search.cleanup();
				g_ctx->m_click_consumed = true;
			}

			g_search.m_search_anim = utils::builder::create_animation_ctx("search_anim_flow", g_search.m_should_draw && g_ctx->m_open, 0.5f);
		}

		g_font->f_icons_medium.text(this->m_pos.x - 12 + this->m_size.x - g_font->f_icons_medium.measure(ICON_FA_SEARCH).x, this->m_pos.y + +(45 * 0.5) - (g_font->f_icons_medium.measure(ICON_FA_SEARCH).y * 0.5), ICON_FA_SEARCH, 
			g_style->m_text.modulate(this->m_window_opacity.limit(0.2).val()).lerp(g_style->m_accent.modulate(this->m_window_opacity.limit(0.8).val()), g_search.m_search_anim.val()));
		this->m_window_opacity.restore();
		// Keep every module panel inside the W window while the category scrolls.
		g_render->push_clip(this->m_pos.x + k_sidebar_width, this->m_pos.y + k_header_height,
			this->m_size.x - k_sidebar_width, this->m_size.y - k_header_height);
		this->setup_objects();
		
		g_render->restore_clip();


        // Repaint the fixed header after scrolling content. Some prioritized
        // controls intentionally bypass child clipping, so this is the final occlusion layer.
        g_render->rect_filled(this->m_pos.x + k_sidebar_width - 1.f, this->m_pos.y,
            this->m_size.x - k_sidebar_width + 1.f, k_header_height,
            g_style->m_child_background.modulate(this->m_window_opacity.val()), 15.f,
            engine::draw_flags_::draw_flags_round_corners_top_right);
        g_font->f_default.text(titleXOffset, this->m_pos.y + 13, this->m_title,
            g_style->m_text.modulate(this->m_window_opacity.limit(0.6).val()));
        g_font->f_icons_medium.text(this->m_pos.x - 12 + this->m_size.x - g_font->f_icons_medium.measure(ICON_FA_SEARCH).x,
            this->m_pos.y + (45 * 0.5f) - (g_font->f_icons_medium.measure(ICON_FA_SEARCH).y * 0.5f), ICON_FA_SEARCH,
            g_style->m_text.modulate(this->m_window_opacity.limit(0.2).val()).lerp(
                g_style->m_accent.modulate(this->m_window_opacity.limit(0.8).val()), g_search.m_search_anim.val()));
        this->m_window_opacity.restore();

		// Large skin head with an even inset from the sidebar edges.
		constexpr float head_size = 48.f;
		constexpr float head_rounding = 8.f;
		const ImVec2 head_min(this->m_pos.x + (k_sidebar_width - head_size) * 0.5f,
			this->m_pos.y + this->m_size.y - head_size - 9.f);
		const ImVec2 head_max(head_min.x + head_size, head_min.y + head_size);
		if (g_PlayerHeadTexture) {
			dl->AddImageRounded((ImTextureID)(intptr_t)g_PlayerHeadTexture, head_min, head_max,
				ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255,
					(int)(255.f * this->m_window_opacity.val())), head_rounding);
		} else {
			g_render->rect_filled(head_min.x, head_min.y, head_size, head_size,
				g_style->m_accent.modulate(this->m_window_opacity.limit(0.22f).val()), head_rounding);
		}

		g_render->rect((int)this->m_pos.x, (int)this->m_pos.y,
			(int)this->m_size.x, (int)this->m_size.y,
			hue::c_color(0, 0, 0).modulate(this->m_window_opacity.val()), 15.f, 1.5f);
		g_search.set_pos({ this->m_pos.x + this->m_size.x - 220, this->m_pos.y + 37 });

		// this->m_preview->update(this->m_pos + math::c_vector_2d(this->m_size.x + 15, 0), math::c_vector_2d(300, this->m_size.y));
		// this->m_preview->draw();

		int vtx_end = dl->VtxBuffer.Size;

		ImDrawVert* verts = dl->VtxBuffer.Data;
		for (int v = vtx_start; v < vtx_end; v++)
		{
			verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;
			verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

			int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
			verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
		}
	}

	void c_window::setup_objects()
	{
		const float layout_width = this->m_size.x - subtab_padding().x;
		const float full_width = layout_width - 10.f;
		const float column_gap = 10.f * ImGui::GetIO().FontGlobalScale;
		const float half_width = (layout_width - 24.f) * 0.5f;
        float column_y[2] = { 0.f, 0.f };


		for (auto& child : this->m_childrens)
		{
			if (child->get_type() == child_width::half) child->m_size.x = half_width;
			else child->m_size.x = full_width;

			child->m_child_opacity = utils::builder::create_animation_ctx(
				child->m_name + "_child_opacity", child->visible() && g_ctx->m_open, 0.5f);

			if (!child->visible()) continue;

			if (child->get_type() == child_width::full)
			{
				const float y = std::max(column_y[0], column_y[1]);
				child->m_relative_pos = math::c_vector_2d(0.f, y - m_content_scroll_offset);
				column_y[0] = column_y[1] = y + child->m_size.y + column_gap;
			}
			else
			{
				// Place the next panel in the shorter column.
				const int column = column_y[0] <= column_y[1] ? 0 : 1;
				child->m_relative_pos = math::c_vector_2d(
					column * (half_width + column_gap), column_y[column] - m_content_scroll_offset);
				column_y[column] += child->m_size.y + column_gap;
			}

			child->m_pos = child->m_relative_pos + (this->m_pos + subtab_padding());
			// Opacity is visual state, not layout state. Keeping the child in the
			// layout while it fades in prevents a tab/open transition from
			// reporting zero content height and destroying its saved scroll.
			if (child->m_child_opacity.val() > 0.f) {
				child->input();
				child->draw();
			}
		}

		const float content_height = std::max(column_y[0], column_y[1]);
		const float viewport_height = this->m_size.y - subtab_padding().y - k_content_bottom_padding;
		m_content_scroll_max = std::max(0.f, content_height - viewport_height);

		// Synchronize scroll with tab changes
		static bool was_open = false;
		bool menu_just_opened = g_ctx->m_open && !was_open;
		was_open = g_ctx->m_open;

		if (m_last_rendered_tab != g_ctx->m_cur_tab) {
			// The tab input is processed before this layout pass. Save the old
			// tab explicitly before loading the new one, otherwise the old
			// position is written into the newly selected tab.
			if (!m_last_rendered_tab.empty())
				g_ctx->m_child_scrolls[m_last_rendered_tab] = m_content_scroll_target;
			m_last_rendered_tab = g_ctx->m_cur_tab;
			m_content_scroll_target = g_ctx->m_child_scrolls[g_ctx->m_cur_tab];
			m_content_scroll_offset = m_content_scroll_target;
		} else if (menu_just_opened) {
			m_content_scroll_target = g_ctx->m_child_scrolls[g_ctx->m_cur_tab];
			m_content_scroll_offset = m_content_scroll_target;
		}

		if (g_ctx->m_open)
			m_content_scroll_target = std::clamp(m_content_scroll_target, 0.f, m_content_scroll_max);
		m_content_scroll_offset += (m_content_scroll_target - m_content_scroll_offset) * 0.18f;
		if (std::abs(m_content_scroll_target - m_content_scroll_offset) < 0.35f)
			m_content_scroll_offset = m_content_scroll_target;

		// Save back scroll
		if (g_ctx->m_open)
			g_ctx->m_child_scrolls[g_ctx->m_cur_tab] = m_content_scroll_target;

		const bool has_more_below = m_content_scroll_max > 0.f &&
			m_content_scroll_offset < m_content_scroll_max - 1.f;
		const bool has_more_above = m_content_scroll_max > 0.f &&
			m_content_scroll_offset > 1.f;
		auto arrow_visibility = utils::builder::create_animation_ctx(
			"content_scroll_arrow", has_more_below && g_ctx->m_open, 0.3f);
		auto bottom_fade_visibility = utils::builder::create_animation_ctx(
			"content_bottom_fade", has_more_below && g_ctx->m_open, 0.3f);
		if (bottom_fade_visibility.val() > 0.01f) {
			const float fade_height = 24.f;
			const float fade_x = this->m_pos.x + k_sidebar_width + 10.f;
			const float fade_y = this->m_pos.y + this->m_size.y - fade_height - 2.f;
			const float fade_width = this->m_size.x - k_sidebar_width - 22.f;
			g_render->fade_rect_filled(fade_x, fade_y, fade_width, fade_height,
				hue::c_color(0, 0, 0, 0),
				hue::c_color(0, 0, 0, (int)(110.f * bottom_fade_visibility.val() * this->m_window_opacity.val())),
				engine::fade_direction::vertically, 0.f);
		}
		auto top_fade_visibility = utils::builder::create_animation_ctx(
			"content_top_fade", has_more_above && g_ctx->m_open, 0.3f);
		if (top_fade_visibility.val() > 0.01f) {
			const float fade_height = 24.f;
			const float fade_x = this->m_pos.x + k_sidebar_width + 10.f;
			const float fade_y = this->m_pos.y + k_header_height;
			const float fade_width = this->m_size.x - k_sidebar_width - 22.f;
			g_render->fade_rect_filled(fade_x, fade_y, fade_width, fade_height,
				hue::c_color(0, 0, 0, (int)(110.f * top_fade_visibility.val() * this->m_window_opacity.val())),
				hue::c_color(0, 0, 0, 0),
				engine::fade_direction::vertically, 0.f);
		}
		if (arrow_visibility.val() > 0.01f) {
			const float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 3.2f);
			const hue::c_color arrow_color = g_style->m_accent
				.lerp(hue::c_color(255, 255, 255), pulse)
				.modulate(arrow_visibility.val() * this->m_window_opacity.val());
			const auto arrow_size = g_font->f_icons_medium.measure(ICON_FA_CHEVRON_DOWN);
			const float arrow_x = this->m_pos.x + k_sidebar_width +
				(this->m_size.x - k_sidebar_width - arrow_size.x) * 0.5f;
			const float arrow_y = this->m_pos.y + this->m_size.y - arrow_size.y - 8.f;
			g_font->f_icons_medium.text(arrow_x, arrow_y, ICON_FA_CHEVRON_DOWN, arrow_color);
		}
	}

	void c_window::input()
	{
		// Swift owns the configurable menu key; W consumes its open state.

		this->m_window_opacity = utils::builder::create_animation_ctx(this->m_title, g_ctx->m_open, 0.5f);
		this->m_obj_tab->parent_opcity = this->m_window_opacity;

		animations::m_window_opacity = this->m_window_opacity;

		if (!g_ctx->m_open)
		{
			g_ctx->clear_non_modal_focus();
			return;
		}

		static math::c_vector_2d prev_mouse_pos{}, delta{};

		// Keep both brand areas draggable without extending into the first tab.
		const math::c_rect title_drag_region = math::c_rect(
			this->m_pos.x + k_sidebar_width, this->m_pos.y, 108.f, 30.f);
		const math::c_rect logo_drag_region = math::c_rect(
			this->m_pos.x, this->m_pos.y, k_sidebar_width, 50.f);
		const bool mouse_over_drag_region =
			g_input->mouse_in_region(title_drag_region.pos(), title_drag_region.size()) ||
			g_input->mouse_in_region(logo_drag_region.pos(), logo_drag_region.size());

		// update this shit everyframe
		delta = prev_mouse_pos - g_input->get_mouse_position();

		// check g_ctx->m_dragging state and update it if we are in the corr region
		if (!g_ctx->m_dragging &&
			mouse_over_drag_region &&
			g_input->clicked(input::mouse_buttons::left))
		{
			g_ctx->m_dragging = true;
			g_ctx->m_click_consumed = true;
		}
		else if (g_ctx->m_dragging && g_input->click_down(input::mouse_buttons::left))
		{
			this->m_pos -= delta;
		}
		else if (g_ctx->m_dragging && !g_input->click_down(input::mouse_buttons::left))
		{
			g_ctx->m_dragging = false;
		}

		prev_mouse_pos = g_input->get_mouse_position();

		const math::c_vector_2d content_pos = this->m_pos + subtab_padding();
		const math::c_vector_2d content_size = { this->m_size.x - subtab_padding().x,
			this->m_size.y - subtab_padding().y - k_content_bottom_padding };
		bool mouse_over_child = false;
		for (const auto& child : m_childrens) {
			if (!child || !child->visible()) continue;
			if (g_input->mouse_in_region(child->m_pos, child->m_size)) {
				if (child->m_max_scroll > 0.f) {
					mouse_over_child = true;
					break;
				}
				// Also check for scrollable listboxes inside this child
				for (const auto& ctrl : child->m_controls) {
					if (ctrl && ctrl->m_type == element_type::listbox) {
						auto* lb = static_cast<c_listbox*>(ctrl.get());
						float lb_visible = lb->m_height - 10.f;
						float lb_total = (g_font->f_childs.measure(lb->m_label).y + 5.f) * lb->m_items.size();
						float lb_max_scroll = std::max(0.f, lb_total - lb_visible);
						if (lb_max_scroll > 0.f) {
							auto lb_position = lb->m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(lb->m_label).y + 5);
							if (g_input->mouse_in_region(lb->m_pos + lb_position, math::c_vector_2d(lb->m_child_size, lb->m_height))) {
								mouse_over_child = true;
								break;
							}
						}
					}
				}
				if (mouse_over_child) break;
			}
		}
		if (!mouse_over_child && m_content_scroll_max > 0.f && g_input->mouse_in_region(content_pos, content_size)) {
			const float wheel = ImGui::GetIO().MouseWheel;
			if (wheel != 0.f && g_ctx->m_modal_owner == nullptr) {
				g_ctx->clear_non_modal_focus();
				m_content_scroll_target = std::clamp(m_content_scroll_target - wheel * 70.f, 0.f, m_content_scroll_max);
			}
		}

		// update tab position when window moves
		this->m_obj_tab->update_input(this->m_pos, this->m_size);
	}

	std::shared_ptr<c_child> c_window::build_child(std::string name, child_width width, float y, std::function<void(c_child* ptr)> callback)
	{
		auto child = std::make_shared<c_child>(name, width, y);
		{
			if (!child)
			{
				return nullptr;
			}

			// attach the pos to the window's, remains to get updated in window's input but we must have atleat framed to it the moment of creation
			child->m_pos = child->m_relative_pos + this->m_pos + subtab_padding();

			// we do not call draw and input here as this only gets called once, well we just do the child caching
			this->m_childrens.push_back(child);

			callback(child.get());

		}
		return child;
	}

	std::shared_ptr<c_tab> c_window::prebuild_tabs(std::function<void(c_tab* ptr)> callback)
	{
		auto obj = std::make_shared<c_tab>(this->m_pos, this->m_size, this->m_window_opacity);
		{

			// pass the pointer
			callback(obj.get());

			// pass the pointer
			this->m_obj_tab = obj;
		}
		return obj;
	}

	void c_window::finish_tab_prebuild()
	{
		if (!g_ctx->m_tabs.empty())
		{
			if (g_ctx->m_active_tab < 0 || g_ctx->m_active_tab >= (int)g_ctx->m_tabs.size())
				g_ctx->m_active_tab = 0;

			g_ctx->m_cur_tab = g_ctx->m_tabs[g_ctx->m_active_tab].m_name;

			if (g_ctx->m_tabs[g_ctx->m_active_tab].m_subtab.empty())
			{
				g_ctx->m_tabs[g_ctx->m_active_tab].m_cur_subtab = "";
			}
		}
	}

	math::c_vector_2d c_window::subtab_padding()
	{
		return math::c_vector_2d(k_sidebar_width + 12.f, 60.f);
	}

	}
