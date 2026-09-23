#pragma once

namespace framework
{
	class c_checkbox :public c_base_element
	{
	private:
		bool* m_value{};
		std::function<bool()> m_enable_requirement{};
		std::string m_requirement_message{};
		double m_requirement_started_at{ -1.0 };
	public:
		c_checkbox(std::string label, bool* value);
		void require_to_enable(std::function<bool()> requirement, std::string message = "Need bind") {
			m_enable_requirement = std::move(requirement);
			m_requirement_message = std::move(message);
		}

		void draw() override;
		void input() override;
	};
}
