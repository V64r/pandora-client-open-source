#pragma once

namespace framework
{
	class c_colorpicker : public c_base_element
	{
	public:
		c_colorpicker(std::string label, hue::c_color* val, bool m_hide_label = true);

		void draw() override;
		void input() override;
		void sync_from_value() { rgb_to_hsv(); }

	private:
		hue::c_color* m_val{};

		void rgb_to_hsv();
		void hsv_to_rgb();

		float m_hue{}, m_saturation{}, m_value{};
		bool m_context_open{ false };
	};
}
