#pragma once

namespace framework
{
	struct multidropdown_data_t {
		std::string m_label{};
		bool* m_val{};
		std::function<bool()> m_visible{};
	};

	class c_multidropdown : public c_base_element {
	public:
		c_multidropdown(std::string label, bool hide_label = false);

		void draw() override;
		void input() override;

		void add_selection(std::string label, bool* val, std::function<bool()> visible = nullptr);
	private:
		std::vector< multidropdown_data_t> m_data{};
	};
}
