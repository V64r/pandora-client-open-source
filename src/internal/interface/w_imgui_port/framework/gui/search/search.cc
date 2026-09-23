#include "../../../includes.hh"
extern bool g_NumericEditActive;

std::string to_upper(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return toupper(c); });
	return s;
}

static std::unordered_map<size_t, float> m_result_anims;

static size_t search_result_id(const framework::search_database_t& entry)
{
	const std::string id = entry.tab_name + "\x1f" +
		(entry.m_copied_controls ? entry.m_copied_controls->m_label : std::string{}) +
		"\x1f" + std::to_string(reinterpret_cast<uintptr_t>(entry.m_copied_controls.get()));
	return std::hash<std::string>{}(id);
}

void framework::c_search::paint_database()
{
    if (g_NumericEditActive) return;
	auto pos = this->m_pos;
	auto size = math::c_vector_2d(200, 80);

	// search
	auto pos_search = pos + math::c_vector_2d(10, 45);
	auto pos_size = math::c_vector_2d(size.x - 20, 25);

	auto opacity = g_search.m_search_anim;

	// input
	{
		math::c_rect bounding = math::c_rect(pos_search, pos_size);

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()) && g_input->clicked(input::mouse_buttons::left)) {
			this->m_focused = true;
		}
		else if (!g_input->mouse_in_region(bounding.pos(), bounding.size()) && g_input->clicked(input::mouse_buttons::left)) {
			this->m_focused = false;			
		}

		this->m_search_focusing = utils::builder::create_animation_ctx("search_focusing", this->m_focused, 0.5f);

		if (this->m_focused)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
				ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
				ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
				this->m_focused = false;
			}
			else {
				if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true) && !this->m_search_text.empty())
					this->m_search_text.pop_back();

				const ImVector<ImWchar>& characters = ImGui::GetIO().InputQueueCharacters;
				for (int i = 0; i < characters.Size && this->m_search_text.length() < 32; ++i) {
					const ImWchar character = characters[i];
					if (character >= 32 && character <= 255)
						this->m_search_text.push_back(static_cast<char>(character));
				}
			}
		}
	}

	if (!this->m_search_text.empty())
	{
		std::string search = this->m_search_text;
		std::transform(search.begin(), search.end(), search.begin(), ::tolower);

		float dt = ImGui::GetIO().DeltaTime;
		float speed = 10.f;

		for (auto& entry : this->m_database)
		{
			if (!entry.m_copied_controls) continue;
			std::string label = entry.m_copied_controls->m_label;
			std::transform(label.begin(), label.end(), label.begin(), ::tolower);

			size_t hash = search_result_id(entry);
			bool   is_match = search.size() >= 2 && label.find(search) != std::string::npos;

			float& anim = m_result_anims[hash];
			float  target = is_match ? 1.f : 0.f;
			anim += (target - anim) * dt * speed;
			
			if (anim > 0.01f) {
				float eased = anim * anim * (3.f - 2.f * anim);
				size += math::c_vector_2d(0, 30.f * eased);
			}
		}
	}
	else
	{
		float dt = ImGui::GetIO().DeltaTime;
		float speed = 10.f;
		for (auto& [hash, anim] : m_result_anims) {
			anim += (0.f - anim) * dt * speed;
			if (anim > 0.01f) {
				float eased = anim * anim * (3.f - 2.f * anim);
				size += math::c_vector_2d(0, 30.f * eased);
			}
		}
	}
	float t = opacity.val();
	float eased = t * t * (3.f - 2.f * t);

	float origin_y = pos.y;
	float origin_x = pos.x;

	// middle layer doesn't pass ignore_clipping so it stays on m_draw_list
	ImDrawList* dl = g_render->draw_list();
	int vtx_start = dl->VtxBuffer.Size;

	g_render->rect_shadow(pos.x, pos.y, size.x, size.y, g_style->m_window_shadow.modulate(opacity.limit(0.5).val()), 10.f, 10.f);
	opacity.restore();
	g_render->rect_filled(pos.x, pos.y, size.x, size.y, g_style->m_window_background.modulate(opacity.val()), 10.f);
	g_render->rect_filled(pos.x, pos.y, size.x, 35, g_style->m_window_bars.modulate(opacity.val()), 10.f, engine::draw_flags_::draw_flags_round_corners_top);
	g_render->gradient(pos.x, pos.y + 35, size.x, 10, g_style->m_window_shadow.modulate(opacity.limit(0.2).val()), g_style->m_window_shadow.modulate(opacity.limit(0.1).val()).with_alpha(0), engine::fade_direction::horizontally);
	opacity.restore();
	g_font->f_default.text(pos.x + 10, pos.y + 8, "Search results", g_style->m_text.modulate(opacity.limit(0.5).val())/*, engine::modifiers::font_flags::drop_shadow*/);
	opacity.restore();

	g_render->rect_shadow(pos_search.x, pos_search.y, pos_size.x, pos_size.y, g_style->m_window_shadow.modulate(opacity.limit(0.5).val()).lerp(g_style->m_accent.modulate(opacity.limit(0.5).val()), this->m_search_focusing.val()), 10.f, 5.f);
	opacity.restore();
	g_render->rect_filled(pos_search.x, pos_search.y, pos_size.x, pos_size.y, g_style->m_element_base.modulate(opacity.val()), 5.f);
	dl->PushClipRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + size.x, pos.y + size.y), true);

	if (!this->m_search_text.empty())
	{
		float result_y = 0.f;
		for (auto& entry : this->m_database)
		{
			if (!entry.m_copied_controls) continue;

			size_t hash = search_result_id(entry);
			float  t = m_result_anims[hash];

			if (t <= 0.01f)
				continue;

			float eased = t * t * (3.f - 2.f * t);

			const math::c_vector_2d row_pos = pos + math::c_vector_2d(10.f, 80.f + result_y);
			const math::c_vector_2d row_size = math::c_vector_2d(size.x - 20.f, 24.f);
			const bool hovered = g_input->mouse_in_region(row_pos, row_size);
			const float row_alpha = eased * opacity.val();
			const float row_tone = hovered ? 0.16f : 0.10f;
			const ImU32 row_color = ImGui::GetColorU32(ImVec4(
				row_tone, row_tone, row_tone, 0.84f * row_alpha));
			g_render->draw_list()->AddRectFilled(
				ImVec2(row_pos.x, row_pos.y),
				ImVec2(row_pos.x + row_size.x, row_pos.y + row_size.y),
				row_color, 3.f);

			const std::string result_label = entry.tab_name + "  >  " + entry.m_copied_controls->m_label;
			dl->PushClipRect(ImVec2(row_pos.x + 4.f, row_pos.y),
				ImVec2(row_pos.x + row_size.x - 4.f, row_pos.y + row_size.y), true);
			g_font->f_childs.text(row_pos.x + 7.f, row_pos.y + 5.f, result_label,
				g_style->m_text.modulate(row_alpha));
			dl->PopClipRect();

			if (hovered && g_input->clicked(input::mouse_buttons::left)) {
				if (entry.tab_index >= 0 && entry.tab_index < static_cast<int>(g_ctx->m_tabs.size())) {
					g_ctx->m_active_tab = entry.tab_index;
					g_ctx->m_cur_tab = entry.tab_name;
				}
				this->m_should_draw = false;
				this->m_focused = false;
				this->cleanup();
				break;
			}

			float contribution = 30.f * eased;
			result_y += contribution;
		}
	}
	dl->PushClipRect(ImVec2(pos_search.x + 5.f, pos_search.y + 2.f),
		ImVec2(pos_search.x + pos_size.x - 5.f, pos_search.y + pos_size.y - 2.f), true);
	if (this->m_focused)
	{
		while (m_char_anims.size() < this->m_search_text.size())
		{
			m_char_anims.push_back(0.f);
			m_char_removing.push_back(false);
		}

		float dt = ImGui::GetIO().DeltaTime;
		float speed = 12.f;

		const float input_left = pos_search.x + 8.f;
		const float input_right = pos_search.x + pos_size.x - 8.f;
		float cursor_x = input_left;
		const float input_text_width = g_font->f_childs.measure(this->m_search_text).x;
		const float input_width = input_right - input_left - 5.f;
		if (input_text_width > input_width)
			cursor_x -= input_text_width - input_width;
		float base_y = pos_search.y + 3.f;

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
				g_style->m_text.modulate(opacity.limit(0.5f).val()));
			opacity.restore();

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
			bool is_active = i < (int)this->m_search_text.size();

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

			char buf[2] = { is_active ? this->m_search_text[i] : ' ', '\0' };
			auto char_size = g_font->f_childs.measure(buf);
			float char_center_x = cursor_x + char_size.x * 0.5f;
			float char_center_y = base_y + char_size.y * 0.5f;

			int vtx_start = g_render->draw_list()->VtxBuffer.Size;

			g_font->f_childs.text(cursor_x, base_y, buf,
				g_style->m_text.modulate(opacity.limit(0.5f).val()));
			opacity.restore();

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

		std::string to_show{};
		if (this->m_search_focusing.val() >= 0.01f) {
			const uint64_t now_ms = GetTickCount64();
			if (now_ms >= (uint64_t)blink)
				blink = (float)(now_ms + 800);

			if (now_ms > (uint64_t)(blink - 400)) {
				const auto cursor_col = g_style->m_text.modulate(this->m_search_focusing.limit(0.5).val()).lerp(g_style->m_accent.modulate(this->m_search_focusing.limit(0.5).val()), this->m_search_focusing.val());
				g_font->f_childs.text(cursor_x, pos_search.y + 3, "|", cursor_col);
			}
		}
	}
	else
	{
		const std::string display_text = this->m_search_text.empty() && !this->m_focused ? "Search..." : this->m_search_text;
		const float input_left = pos_search.x + 8.f;
		const float input_width = pos_size.x - 21.f;
		const float display_width = g_font->f_childs.measure(display_text).x;
		const float display_x = input_left - (std::max)(0.f, display_width - input_width);
		g_font->f_childs.text(display_x, pos_search.y + 3, display_text, g_style->m_text.modulate(opacity.limit(0.5f).val()));
	}
	dl->PopClipRect();
	dl->PopClipRect();

	int vtx_end = dl->VtxBuffer.Size;

	ImDrawVert* verts = dl->VtxBuffer.Data;
	for (int v = vtx_start; v < vtx_end; v++)
	{
		verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;
		verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

		int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
		verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
	}

	m_backup_size = size;
}

void framework::c_search::add_to_database(std::shared_ptr<c_base_element> control, std::string tab_name, int tab_index)
{
	this->m_database.push_back({ control, 0, tab_name, tab_index });
}

void framework::c_search::cleanup()
{
	this->m_search_text.clear();
	this->m_focused = false;
}

void framework::c_search::reset_context()
{
	this->cleanup();
	this->m_should_draw = false;
	this->m_database.clear();
	this->m_ghost_chars.clear();
	this->m_char_anims.clear();
	this->m_char_removing.clear();
	this->m_backup_size = {};
	this->m_pos = {};
	this->blink = 0.f;
	m_result_anims.clear();
}
