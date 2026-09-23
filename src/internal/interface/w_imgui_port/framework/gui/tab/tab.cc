#include "../../../includes.hh"
extern bool g_NumericEditActive;
#include <GL/gl.h>
#include "../../../../assets/swift_ui_assets.hpp"
#include "../../../../assets/combat_bullseye.hpp"
#include "../../../render/stb/stb_image.hh"

namespace {
struct swift_embedded_image { const unsigned char* data; std::size_t size; };
static swift_embedded_image swift_tab_image(const std::string& name) {
    if (name == "Combat") return { combat_bullseye_png, sizeof(combat_bullseye_png) };
    if (name == "Visuals") return { swift_visuals_png, swift_visuals_png_size };
    if (name == "Movement") return { swift_movement_png, swift_movement_png_size };
    if (name == "Latency") return { swift_latency_png, swift_latency_png_size };
    if (name == "Misc") return { swift_misc_png, swift_misc_png_size };
    if (name == "Configs") return { swift_settings_png, swift_settings_png_size };
    return { swift_settings_png, swift_settings_png_size };
}
static GLuint swift_load_texture(const unsigned char* data, std::size_t size) {
    int width=0, height=0, channels=0;
    auto* pixels=stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
    if (!pixels || width<=0 || height<=0) { if (pixels) stbi_image_free(pixels); return 0; }
    GLint previous=0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    GLuint texture=0; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    const auto mip = reinterpret_cast<void(APIENTRY*)(GLenum)>(wglGetProcAddress("glGenerateMipmap"));
    if (reinterpret_cast<intptr_t>(mip) > 3) {
        mip(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previous)); stbi_image_free(pixels); return texture;
}
}

namespace framework
{
	c_tab::c_tab(math::c_vector_2d pos, math::c_vector_2d size, utils::anim_context_t parent) : m_pos(pos), m_size(size), parent_opcity(parent) { }

    c_tab::~c_tab() {
        if (m_texture_context && m_texture_context == wglGetCurrentContext()) {
            if(m_logo_texture) glDeleteTextures(1, &m_logo_texture);
            glDeleteTextures(7, m_tab_textures);
        }
    }
	void c_tab::paint()
	{
		if (g_ctx->m_tabs.empty())
		{

			// we do not need to run the tabs if there are none
			return;
		}

		constexpr float sidebar_width = 66.f;
		constexpr float item_height = 46.f;
		const float item_x = this->m_pos.x + 7.f;

        m_texture_context = wglGetCurrentContext();
        if (!m_logo_texture) m_logo_texture = swift_load_texture(swift_logo_png, swift_logo_png_size);
        for (int i = 0; i < static_cast<int>(g_ctx->m_tabs.size()) && i < 7; ++i) {
            if (!m_tab_textures[i]) {
                const auto source = swift_tab_image(g_ctx->m_tabs[i].m_name);
                m_tab_textures[i] = swift_load_texture(source.data, source.size);
            }
        }

        constexpr float logo_size = 31.f;
        const float logo_x = this->m_pos.x + (sidebar_width - logo_size) * 0.5f;
        const float logo_y = this->m_pos.y + 7.f;
        if (m_logo_texture) {
            const auto accent = g_style->m_accent.modulate(this->parent_opcity.val());
            auto* draw_list = g_render->draw_list();
            for (int layer = 3; layer >= 1; --layer) {
                const float spread = layer * 1.4f;
                draw_list->AddImage(reinterpret_cast<ImTextureID>((intptr_t)m_logo_texture),
                    ImVec2(logo_x - spread, logo_y - spread),
                    ImVec2(logo_x + logo_size + spread, logo_y + logo_size + spread),
                    ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), accent.modulate(0.08f / layer).transform());
            }
            draw_list->AddImage(reinterpret_cast<ImTextureID>((intptr_t)m_logo_texture),
                ImVec2(logo_x, logo_y), ImVec2(logo_x + logo_size, logo_y + logo_size),
                ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), accent.transform());
        }

        float item_y = this->m_pos.y + 54.f;

		for (int i = 0; i < g_ctx->m_tabs.size(); i++) {
			auto& tab = g_ctx->m_tabs[i];
			math::c_rect bounding = math::c_rect(item_x, item_y, sidebar_width - 14.f, item_height);
			const bool hovered = g_input->mouse_in_region(bounding.pos(), bounding.size());

			if (!g_NumericEditActive && hovered && g_input->clicked(input::mouse_buttons::left))
			{
				g_ctx->clear_non_modal_focus();
				g_ctx->m_active_tab = i;
				g_ctx->m_cur_tab = tab.m_name;
				g_ctx->m_click_consumed = true;
			}

			animations::m_tab_switching = utils::builder::create_animation_ctx(tab.m_name + utils::builder::get_id(i), (g_ctx->m_active_tab == i) && g_ctx->m_open, 0.5);
			const float active = animations::m_tab_switching.val();
			auto hover_anim = utils::builder::create_animation_ctx(tab.m_name + "_hover_" + utils::builder::get_id(i), hovered && g_ctx->m_open, 0.22);
			const float hover = hover_anim.val();
            const float icon_size = tab.m_name == "Combat" ? 24.f : 24.f;
            const ImVec2 icon_center(this->m_pos.x + sidebar_width * 0.5f, item_y + item_height * 0.5f);
            const ImVec2 icon_min(icon_center.x - icon_size * 0.5f, icon_center.y - icon_size * 0.5f);
            const ImVec2 icon_max(icon_center.x + icon_size * 0.5f, icon_center.y + icon_size * 0.5f);
            g_render->rect_filled(this->m_pos.x, item_y + 8.f, 3.f, item_height - 16.f,
                g_style->m_accent.modulate(this->parent_opcity.val() * active), 2.f,
                engine::draw_flags_::draw_flags_round_corners_right);
            const char* icon_glyph = ICON_FA_CIRCLE;
            if (tab.m_name == "Configs") icon_glyph = ICON_FA_FOLDER_OPEN;
            if (tab.m_name == "Combat") icon_glyph = ICON_FA_BULLSEYE;
            if (tab.m_name == "Visuals") icon_glyph = ICON_FA_EYE;
            if (tab.m_name == "Movement") icon_glyph = ICON_FA_WHEELCHAIR_MOVE;
            if (tab.m_name == "Latency") icon_glyph = ICON_FA_WIFI;
            if (tab.m_name == "Misc") icon_glyph = ICON_FA_BARS;
            if (tab.m_name == "Settings") icon_glyph = ICON_FA_GEAR;
            const auto icon_color = g_style->m_text.modulate(this->parent_opcity.limit(0.45f).val())
                .lerp(hue::c_color(255,255,255).modulate(this->parent_opcity.limit(1.f).val()), hover)
                .lerp(g_style->m_accent.modulate(this->parent_opcity.limit(1.f).val()), active);
            if (tab.m_name == "Combat" && i < 7 && m_tab_textures[i]) {
                g_render->draw_list()->AddImage(reinterpret_cast<ImTextureID>((intptr_t)m_tab_textures[i]),
                    icon_min, icon_max, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), icon_color.transform());
            } else {
            const auto icon_extent = g_font->f_icons_medium.measure(icon_glyph);
            g_font->f_icons_medium.text(static_cast<int>(icon_center.x - icon_extent.x * 0.5f),
                static_cast<int>(icon_center.y - icon_extent.y * 0.5f), icon_glyph, icon_color);
            }
            /*
            if (i < 7 && m_tab_textures[i] && tab.m_name != "Configs") {
                const auto icon_color = g_style->m_text.modulate(this->parent_opcity.limit(0.45f).val())
                    .lerp(hue::c_color(255,255,255).modulate(this->parent_opcity.limit(1.f).val()), hover)
                    .lerp(g_style->m_accent.modulate(this->parent_opcity.limit(1.f).val()), active);
                const float glow = (active * 0.34f + hover * 0.12f) * this->parent_opcity.val();
                auto* draw_list = g_render->draw_list();
                for (int layer = 3; layer >= 1; --layer) {
                    const float spread = layer * 1.35f;
                    draw_list->AddImage(reinterpret_cast<ImTextureID>((intptr_t)m_tab_textures[i]),
                        ImVec2(icon_min.x - spread, icon_min.y - spread),
                        ImVec2(icon_max.x + spread, icon_max.y + spread),
                        ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
                        g_style->m_accent.modulate(glow * (0.13f / layer)).transform());
                }
                draw_list->AddImage(reinterpret_cast<ImTextureID>((intptr_t)m_tab_textures[i]),
                    icon_min, icon_max, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), icon_color.transform());
            }
            else if (tab.m_name == "Configs") {
                const auto icon_color = g_style->m_text.modulate(this->parent_opcity.limit(0.45f).val())
                    .lerp(hue::c_color(255, 255, 255).modulate(this->parent_opcity.limit(1.f).val()), hover)
                    .lerp(g_style->m_accent.modulate(this->parent_opcity.limit(1.f).val()), active);
                g_font->f_icons_medium.text(icon_center.x, icon_center.y, ICON_FA_FOLDER_OPEN,
                    icon_color, framework::text_alignment::center);
            }
            */

			item_y += item_height + 5.f;
		}
	}

	void c_tab::update_input(math::c_vector_2d pos, math::c_vector_2d size)
	{
		this->m_pos = pos;
		this->m_size = size;
	}

	void c_tab::create_tab(std::string icon, std::string name, std::vector<std::string> subtabs)
	{
		g_ctx->m_tabs.push_back({ name, icon, subtabs });
	}
}
