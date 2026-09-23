#pragma once

namespace framework
{
	struct ripple_t {
		float m_anim;
	};

	class c_button : public c_base_element
	{
	public:
		c_button(std::string label, std::function<void()> callback);

		void draw() override;
		void input() override;
		void set_hold_to_activate(float seconds = 1.5f) { m_hold_to_activate = true; m_hold_seconds = std::max(0.1f, seconds); }
	private:
		std::function<void()> m_callback;

		bool m_callback_called{ false };

		bool m_hold_to_activate{ false };
		float m_hold_seconds{ 1.5f };
		float m_hold_progress{ 0.f };
		bool m_hold_fired{ false };

		std::vector<ripple_t> m_ripples;
	};
}
