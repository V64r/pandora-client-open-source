#pragma once

namespace framework
{
	struct numeric_edit_animation_t {
		struct ghost_char_t {
			char glyph{};
			float animation{};
			float x{};
		};
		std::vector<float> character_animations;
		std::vector<ghost_char_t> ghosts;
		std::string previous_text;
	};

	class c_slider_float : public c_base_element {
	public:
		c_slider_float(std::string label, float* val, float min, float max, bool hide_label = false, std::wstring prefix = L"");

		void input() override;
		void draw() override;
		void set_range_max(float* value) { m_range_max = value; }
	private:
		float* m_val{};
		float* m_range_max{};
		bool m_dragging_range_max{};
		float m_range_display_min = -1.f;
		float m_range_display_max = -1.f;
		float m_range_min_scale = 1.f;
		float m_range_max_scale = 1.f;
		float m_min{}, m_max{};

		float m_focus_anim = 0.f;
		float m_knob_hover_anim = 0.f;
		float m_display_width = -1.f;
		int m_edit_target = -1; // -1 none, 0 primary value, 1 range maximum
		bool m_request_edit_focus = false;
		char m_edit_buffer[32]{};
		numeric_edit_animation_t m_edit_animation{};

		std::wstring m_prefix{};
	};

	class c_slider_int : public c_base_element {
	public:
		c_slider_int(std::string label, int* val, int min, int max, bool hide_label = false, std::wstring prefix = L"");

		void input() override;
		void draw() override;
	private:
		int* m_val{};
		int m_min{}, m_max{};

		float m_focus_anim = 0.f;
		float m_knob_hover_anim = 0.f;
		float m_display_width = -1.f;
		bool m_editing = false;
		bool m_request_edit_focus = false;
		char m_edit_buffer[32]{};
		numeric_edit_animation_t m_edit_animation{};

		std::wstring m_prefix{};
	};
}
