#include "../includes.hh"
#include "../../../front/back/misc/imgui/fonts/Montserrat-SemiBold.h"
#include "fonts/fa.hh"

namespace
{
    bool get_icon_font_resource(void*& data, int& size)
    {
        data = const_cast<unsigned int*>(FA_compressed_data);
        size = static_cast<int>(FA_compressed_size);
        return size > 0;
    }
}


math::c_vector_2d input::c_input_controller::get_mouse_position()
{
	return math::c_vector_2d(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
}

bool input::c_input_controller::mouse_in_region(const math::c_vector_2d& pos, const math::c_vector_2d& size)
{
	return (ImGui::GetIO().MousePos.x > pos.x && ImGui::GetIO().MousePos.y > pos.y &&
		ImGui::GetIO().MousePos.x < pos.x + size.x && ImGui::GetIO().MousePos.y < pos.y + size.y);
}

bool input::c_input_controller::clicked(mouse_buttons button)
{
	return ImGui::IsMouseClicked(ImGuiMouseButton(button));
}

bool input::c_input_controller::click_down(mouse_buttons button)
{
	return ImGui::IsMouseDown(ImGuiMouseButton(button));
}

bool input::c_input_controller::click_released(mouse_buttons button)
{
	return ImGui::IsMouseReleased(ImGuiMouseButton(button));
}

float input::c_input_controller::get_wheel_value()
{
	return ImGui::GetIO().MouseWheel;
}

/* layering system */
void c_render::begin_layers()
{

	this->m_draw_list->ChannelsSplit((int)engine::render_layer::count);
}

void c_render::set_layer(engine::render_layer layer)
{

	this->m_draw_list->ChannelsSetCurrent((int)layer);
}

void c_render::end_layers()
{

	this->m_draw_list->ChannelsMerge();
}

void c_render::rect_shadow(int x, int y, int w, int h, hue::c_color col, float thickness, float rouding)
{
	auto offset = ImVec2{ 1.0f, 1.0f };
	auto rounding = rouding;
	auto spread = thickness;

	ImU32 c_full = IM_COL32(col.r, col.g, col.b, col.a);
	ImU32 c_near = IM_COL32(col.r, col.g, col.b, (int)(col.a * 0.35f));
	ImU32 c_far = IM_COL32(col.r, col.g, col.b, (int)(col.a * 0.08f));
	ImU32 c_zero = IM_COL32(col.r, col.g, col.b, 0);
	ImVec2 uv = m_draw_list->_Data->TexUvWhitePixel;

	w -= 2;
	h -= 2;

	float sx = x + offset.x;
	float sy = y + offset.y;
	float ex = x + w + offset.x;
	float ey = y + h + offset.y;

	float r = std::min(rounding, std::min(w * 0.5f, h * 0.5f));

	float s1 = spread * 0.33f;
	float s2 = spread * 0.66f;
	float s3 = spread;

	float r1 = r + s1, r2 = r + s2, r3 = r + s3;

	auto quad = [&](ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d,
		ImU32 ca, ImU32 cb, ImU32 cc, ImU32 cd)
		{
			m_draw_list->PrimReserve(6, 4);
			ImDrawIdx idx = (ImDrawIdx)m_draw_list->_VtxCurrentIdx;
			m_draw_list->PrimWriteVtx(a, uv, ca);
			m_draw_list->PrimWriteVtx(b, uv, cb);
			m_draw_list->PrimWriteVtx(c, uv, cc);
			m_draw_list->PrimWriteVtx(d, uv, cd);
			m_draw_list->PrimWriteIdx(idx);
			m_draw_list->PrimWriteIdx(idx + 1);
			m_draw_list->PrimWriteIdx(idx + 2);
			m_draw_list->PrimWriteIdx(idx);
			m_draw_list->PrimWriteIdx(idx + 2);
			m_draw_list->PrimWriteIdx(idx + 3);
		};

	float clx = sx + r, crx = ex - r;
	float cty = sy + r, cby = ey - r;

	auto edge_quads = [&](ImVec2 i_a, ImVec2 i_b,
		ImVec2 n_a, ImVec2 n_b,
		ImVec2 f_a, ImVec2 f_b,
		ImVec2 o_a, ImVec2 o_b)
		{
			quad(i_a, i_b, n_b, n_a, c_full, c_full, c_near, c_near);
			quad(n_a, n_b, f_b, f_a, c_near, c_near, c_far, c_far);
			quad(f_a, f_b, o_b, o_a, c_far, c_far, c_zero, c_zero);
		};

	edge_quads(
		{ clx, sy }, { crx, sy },
		{ clx, sy - s1 }, { crx, sy - s1 },
		{ clx, sy - s2 }, { crx, sy - s2 },
		{ clx, sy - s3 }, { crx, sy - s3 }
	);

	edge_quads(
		{ crx, ey }, { clx, ey },
		{ crx, ey + s1 }, { clx, ey + s1 },
		{ crx, ey + s2 }, { clx, ey + s2 },
		{ crx, ey + s3 }, { clx, ey + s3 }
	);

	edge_quads(
		{ sx, cby }, { sx, cty },
		{ sx - s1, cby }, { sx - s1, cty },
		{ sx - s2, cby }, { sx - s2, cty },
		{ sx - s3, cby }, { sx - s3, cty }
	);

	edge_quads(
		{ ex, cty }, { ex, cby },
		{ ex + s1, cty }, { ex + s1, cby },
		{ ex + s2, cty }, { ex + s2, cby },
		{ ex + s3, cty }, { ex + s3, cby }
	);


	const int seg = 16;
	auto corner = [&](ImVec2 center, float a_start)
		{
			m_draw_list->PrimReserve(seg * 18, seg * 4 + 4);
			ImDrawIdx base = (ImDrawIdx)m_draw_list->_VtxCurrentIdx;

			for (int i = 0; i <= seg; i++) {
				float a = a_start + (IM_PI * 0.5f) * (float)i / seg;
				float cs = std::cos(a);
				float sn = std::sin(a);
				m_draw_list->PrimWriteVtx({ center.x + cs * r,  center.y + sn * r }, uv, c_full);
				m_draw_list->PrimWriteVtx({ center.x + cs * r1, center.y + sn * r1 }, uv, c_near);
				m_draw_list->PrimWriteVtx({ center.x + cs * r2, center.y + sn * r2 }, uv, c_far);
				m_draw_list->PrimWriteVtx({ center.x + cs * r3, center.y + sn * r3 }, uv, c_zero);
			}

			for (int i = 0; i < seg; i++) {
				ImDrawIdx v = base + i * 4;

				m_draw_list->PrimWriteIdx(v + 0); m_draw_list->PrimWriteIdx(v + 1); m_draw_list->PrimWriteIdx(v + 4);
				m_draw_list->PrimWriteIdx(v + 4); m_draw_list->PrimWriteIdx(v + 1); m_draw_list->PrimWriteIdx(v + 5);

				m_draw_list->PrimWriteIdx(v + 1); m_draw_list->PrimWriteIdx(v + 2); m_draw_list->PrimWriteIdx(v + 5);
				m_draw_list->PrimWriteIdx(v + 5); m_draw_list->PrimWriteIdx(v + 2); m_draw_list->PrimWriteIdx(v + 6);

				m_draw_list->PrimWriteIdx(v + 2); m_draw_list->PrimWriteIdx(v + 3); m_draw_list->PrimWriteIdx(v + 6);
				m_draw_list->PrimWriteIdx(v + 6); m_draw_list->PrimWriteIdx(v + 3); m_draw_list->PrimWriteIdx(v + 7);
			}
		};

	corner({ crx, cty }, -IM_PI * 0.5f);
	corner({ clx, cty }, -IM_PI);
	corner({ clx, cby }, IM_PI * 0.5f);
	corner({ crx, cby }, 0.f);
}


void c_render::rect_filled(int x, int y, int w, int h, hue::c_color col, float rounding, engine::draw_flags flags)
{
	this->m_draw_list->AddRectFilled(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
		col.transform(),
		rounding, flags
	);
}

void c_render::rect(int x, int y, int w, int h, hue::c_color col, float rounding, float thickness)
{
	this->m_draw_list->AddRect(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
		col.transform(), rounding, 0, thickness
	);
}

void c_render::image(int x, int y, int w, int h, ImTextureID texture_id, hue::c_color col, float rounding)
{
	if (rounding > 0.f)
	{
		this->m_draw_list->AddImageRounded(texture_id, ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
			col.transform(),
			rounding
		);
	}
	else
	{
		this->m_draw_list->AddImage(texture_id, ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
			col.transform()
		);
	}
}

void c_render::fade_rect_filled(int x, int y, int w, int h, hue::c_color col1, hue::c_color col2, engine::fade_direction direction, float rounding, engine::draw_flags flags)
{
	switch (direction)
	{
	case engine::vertically:
		this->rect_filled_multi_color(x,y,w,h, { col1, col1, col2, col2 }, rounding, flags);
		break;
	case engine::horizontally:
		this->rect_filled_multi_color(x, y, w, h, { col1, col2, col1, col2 }, rounding, flags);
		break;
	case engine::diagonally:
		this->rect_filled_multi_color(x, y, w, h, { col1, col2, col2, col1 }, rounding, flags);
		break;
	case engine::diagonally_reversed:
		this->rect_filled_multi_color(x, y, w, h, { col2, col1, col1, col2 }, rounding, flags);
		break;
	default:
		break;
	}
}

void c_render::enlarged_arrow(math::c_vector_2d pos, hue::c_color col, int dir, float scale)
{
	float thickness = ImMax(scale / 15.0f, 1.0f);
	scale -= thickness * 0.5f;
	pos += math::c_vector_2d(ImVec2(thickness * 0.25f, thickness * 0.25f).x, ImVec2(thickness * 0.25f, thickness * 0.25f).y);

	float third = scale / 3.0f;
	float bx = pos.x + third;
	float by = pos.y + scale - third * 0.5f;

	switch (dir) {
	case ImGuiDir_Down:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by - third));
		break;
	case ImGuiDir_Up:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by + third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by + third));
		break;
	case ImGuiDir_Left:
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by + third));
		break;
	case ImGuiDir_Right:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx - third + third / 30, by + third));
		break;
	}

	this->m_draw_list->PathStroke(col.transform(), 0, thickness);
}

// hue::c_color top_r, hue::c_color top_l, hue::c_color bottom_r, hue::c_color bottom_l
void c_render::rect_filled_multi_color(int x, int y, int w, int h, engine::c_gradient_data gradient_data, float rounding, engine::draw_flags flags)
{
	auto fix_rect_corner_flags = [](engine::draw_flags rflags)
		{
			if ((rflags & engine::draw_flags_round_corners_mask) == 0)
				rflags |= engine::draw_flags_round_corners_all;
			return rflags;
		};

	auto p_min = ImVec2(x, y);
	auto p_max = (ImVec2(x, y) + ImVec2(w, h));

	flags = fix_rect_corner_flags(flags);
	rounding = ImMin(rounding, ImFabs(p_max.x - p_min.x) * (((flags & engine::draw_flags_round_corners_top) == engine::draw_flags_round_corners_top) || ((flags & engine::draw_flags_round_corners_bottom) == engine::draw_flags_round_corners_bottom) ? 0.5f : 1.0f) - 1.0f);
	rounding = ImMin(rounding, ImFabs(p_max.y - p_min.y) * (((flags & engine::draw_flags_round_corners_left) == engine::draw_flags_round_corners_left) || ((flags & engine::draw_flags_round_corners_right) == engine::draw_flags_round_corners_right) ? 0.5f : 1.0f) - 1.0f);

	if (rounding > 0.0f)
	{
		const int size_before = this->m_draw_list->VtxBuffer.Size;
		this->m_draw_list->AddRectFilled(p_min, p_max, IM_COL32_WHITE, rounding, flags);
		const int size_after = this->m_draw_list->VtxBuffer.Size;

		for (int i = size_before; i < size_after; i++)
		{
			ImDrawVert* vert = this->m_draw_list->VtxBuffer.Data + i;

			ImVec4 upr_left = ImGui::ColorConvertU32ToFloat4(gradient_data.m_top_l.transform());
			ImVec4 bot_left = ImGui::ColorConvertU32ToFloat4(gradient_data.m_bot_l.transform());
			ImVec4 up_right = ImGui::ColorConvertU32ToFloat4(gradient_data.m_top_r.transform());
			ImVec4 bot_right = ImGui::ColorConvertU32ToFloat4(gradient_data.m_bot_r.transform());

			float X = ImClamp((vert->pos.x - p_min.x) / (p_max.x - p_min.x), 0.0f, 1.0f);

			// 4 colors - 8 deltas

			float r1 = upr_left.x + (up_right.x - upr_left.x) * X;
			float r2 = bot_left.x + (bot_right.x - bot_left.x) * X;

			float g1 = upr_left.y + (up_right.y - upr_left.y) * X;
			float g2 = bot_left.y + (bot_right.y - bot_left.y) * X;

			float b1 = upr_left.z + (up_right.z - upr_left.z) * X;
			float b2 = bot_left.z + (bot_right.z - bot_left.z) * X;

			float a1 = upr_left.w + (up_right.w - upr_left.w) * X;
			float a2 = bot_left.w + (bot_right.w - bot_left.w) * X;


			float Y = ImClamp((vert->pos.y - p_min.y) / (p_max.y - p_min.y), 0.0f, 1.0f);
			float r = r1 + (r2 - r1) * Y;
			float g = g1 + (g2 - g1) * Y;
			float b = b1 + (b2 - b1) * Y;
			float a = a1 + (a2 - a1) * Y;
			ImVec4 RGBA(r, g, b, a);

			RGBA = RGBA * ImGui::ColorConvertU32ToFloat4(vert->col);

			vert->col = ImColor(RGBA);
		}
		return;
	}

	const ImVec2 uv = this->m_draw_list->_Data->TexUvWhitePixel;
	this->m_draw_list->PrimReserve(6, 4);
	this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 1)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 2));
	this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 2)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 3));


	this->m_draw_list->PrimWriteVtx(p_min, uv, gradient_data.m_top_l.transform());
	this->m_draw_list->PrimWriteVtx(ImVec2(p_max.x, p_min.y), uv, gradient_data.m_top_r.transform());
	this->m_draw_list->PrimWriteVtx(p_max, uv, gradient_data.m_bot_r.transform());
	this->m_draw_list->PrimWriteVtx(ImVec2(p_min.x, p_max.y), uv, gradient_data.m_bot_l.transform());
}


void c_render::check_mark(int x, int y, float size, hue::c_color col)
{
	float thickness = ImMax(size / 5.0f, 1.0f);
	size -= thickness * 0.5f;

	x += thickness * 0.25f;
	y += thickness * 0.25f;

	float third = size / 3.0f;
	float bx = x + third;
	float by = y + size - third * 0.5f;

	ImVec2 p0 = { bx - third,        by - third };
	ImVec2 p1 = { bx,                by };
	ImVec2 p2 = { bx + third * 2.f,  by - third * 2.f };

	float r = thickness * 0.5f;

	ImVec2 cp0 = ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
	ImVec2 cp1 = ImVec2((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);

	this->m_draw_list->AddBezierCubic(p0, cp0, cp0, p1, col.transform(), thickness, 0);
	this->m_draw_list->AddBezierCubic(p1, cp1, cp1, p2, col.transform(), thickness, 0);

	this->m_draw_list->AddCircleFilled(p1, thickness * 0.5f, col.transform());

	this->m_draw_list->AddCircleFilled(p0, thickness * 0.5f, col.transform());
	this->m_draw_list->AddCircleFilled(p2, thickness * 0.5f, col.transform());
}

void c_render::push_clip(int x, int y, int w, int h)
{
	this->m_draw_list->PushClipRect(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), true);
}

void c_render::restore_clip()
{
	this->m_draw_list->PopClipRect();
}

void c_render::setup()
{
	this->set_draw_list(ImGui::GetBackgroundDrawList());

	/* initialize fonts */
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig default_cfg{};
	ImFontConfig* cfg = &default_cfg;
	{
		cfg->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_NoHinting;
		cfg->FontDataOwnedByAtlas = false;

		const void* regular_data = static_cast<const void*>(montserrat_semibold_data);
		const int regular_size = static_cast<int>(montserrat_semibold_size);
		const void* bold_data = static_cast<const void*>(montserrat_semibold_data);
		const int bold_size = static_cast<int>(montserrat_semibold_size);

		g_font->f_default.create(const_cast<void*>(regular_data), regular_size, 21.f, cfg, NULL);
		g_font->f_childs.create(const_cast<void*>(regular_data), regular_size, 20.f, cfg, NULL);
		g_font->f_bold.create(const_cast<void*>(bold_data), bold_size, 20.f, cfg, NULL);
	}

	ImFontConfig icon_cfg{};
	cfg = &icon_cfg;
	{
		static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		cfg->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_ForceAutoHint;
		cfg->FontDataOwnedByAtlas = false;
        
        void* icon_font_data = nullptr;
		int icon_font_size = 0;
		if (get_icon_font_resource(icon_font_data, icon_font_size)) {
		    g_font->f_icons.create(icon_font_data, icon_font_size, 14.0f, cfg, icons_ranges, true);
		    g_font->f_icons_medium.create(icon_font_data, icon_font_size, 22.0f, cfg, icons_ranges, true);
		    g_font->f_icons_rs.create(icon_font_data, icon_font_size, 7.0f, cfg, icons_ranges, true);
		    g_font->f_oof_arrow.create(icon_font_data, icon_font_size, 70.0f, cfg, icons_ranges, true);
        }
	}

	//
	// c_font f_verdana{};
	// c_font f_smallest_pixel{};

	ImFontConfig verdana_cfg{};
	cfg = &verdana_cfg;
	{
		cfg->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_MonoHinting | ImGuiFreeTypeBuilderFlags_Monochrome;
		g_font->f_verdana.create((void*)montserrat_semibold_data, montserrat_semibold_size, 13, cfg, NULL);
	}

	ImFontConfig pixel_cfg{};
	cfg = &pixel_cfg;
	{
		g_font->f_smallest_pixel.create((void*)montserrat_semibold_data, montserrat_semibold_size, 10, cfg, NULL);
	}

	g_font->build_fonts(io.Fonts);



}

bool c_font::create(void* data, int font_size, float size, const ImFontConfig* font_template, const ImWchar* glyph, bool compressed)
{
	ImGuiIO& io = ImGui::GetIO();

	// font handler
	// check if the font has been initialized
	this->m_handle = compressed
		? io.Fonts->AddFontFromMemoryCompressedTTF(data, font_size, size, font_template, glyph)
		: io.Fonts->AddFontFromMemoryTTF(data, font_size, size, font_template, glyph);
	if (this->m_handle == nullptr) {
		return false;
	}

	this->m_size = this->measure("A");

	// finish
	return true;
}

math::c_vector_2d c_font::measure(const std::string& text)
{
	if (this->m_handle == nullptr)
		return math::c_vector_2d();

	auto wraper = this->m_handle->CalcTextSizeA(this->m_handle->FontSize, FLT_MAX, -1.0f, text.c_str());
	return math::c_vector_2d(wraper.x, wraper.y);
}

void c_font::string(int x, int y, std::string text, hue::c_color color)
{
	g_render->draw_list()->AddText(ImVec2(static_cast<float>(x), static_cast<float>(y)), color.transform(), text.c_str());
}

void c_render::gradient(math::c_vector_2d pos, math::c_vector_2d size, hue::c_color color, hue::c_color color2, engine::fade_direction flags, int rounding, hue::c_color backround_helper, ImDrawFlags draw_flags)
{
	auto start = pos.transform();
	auto end = (pos + size).transform();

	if (rounding > 0)
	{
		this->m_draw_list->AddRectFilledMultiColorRounded(
			start, end,
			backround_helper.transform(),
			flags == engine::fade_direction::vertically ? color.transform() : color.transform(),
			flags == engine::fade_direction::vertically ? color2.transform() : color.transform(),
			flags == engine::fade_direction::vertically ? color2.transform() : color2.transform(),
			flags == engine::fade_direction::vertically ? color.transform() : color2.transform(),
			rounding, draw_flags
		);
	}
	else
	{
		this->m_draw_list->AddRectFilledMultiColor(
			start, end,
			flags == engine::fade_direction::vertically ? color.transform() : color.transform(),
			flags == engine::fade_direction::vertically ? color2.transform() : color.transform(),
			flags == engine::fade_direction::vertically ? color2.transform() : color2.transform(),
			flags == engine::fade_direction::vertically ? color.transform() : color2.transform()
		);
	}
}

void c_render::gradient(int x, int y, int w, int h, hue::c_color color, hue::c_color color2, engine::fade_direction flags, int rounding, hue::c_color backround_helper, ImDrawFlags draw_flags)
{
	if (flags == engine::fade_direction::vertically) {
		if (rounding != 0) {
			this->m_draw_list->AddRectFilledMultiColorRounded(ImVec2(x, y), ImVec2(x + w, y + h),
				backround_helper.transform(), color.transform(), color2.transform(), color2.transform(), color.transform(), rounding, draw_flags);
		}
		else {
			this->m_draw_list->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h),
				color.transform(), color2.transform(), color2.transform(), color.transform());
		}
	}
	else if (flags == engine::fade_direction::horizontally) {
		if (rounding != 0) {
			this->m_draw_list->AddRectFilledMultiColorRounded(ImVec2(x, y), ImVec2(x + w, y + h),
				backround_helper.transform(), color.transform(), color.transform(), color2.transform(), color2.transform(), rounding, draw_flags);
		}
		else {
			this->m_draw_list->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h),
				color.transform(), color.transform(), color2.transform(), color2.transform());
		}
	}
}
