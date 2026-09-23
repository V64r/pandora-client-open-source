#pragma once

namespace framework
{
	class c_window
	{
	public:
		c_window(std::string title, math::c_vector_2d pos, math::c_vector_2d size);
		~c_window() // no point in creating this in cc file as its only a destructor
		{
		}

		// no virtual as this is the base object
		void paint();
		void setup_objects();
		void input();

		std::shared_ptr<c_child> build_child(std::string name, child_width width, float y, std::function<void(c_child* ptr)> callback);
		std::shared_ptr<c_tab> prebuild_tabs(std::function<void(c_tab* ptr)> callback);

		void finish_tab_prebuild();

		// child pading func
		math::c_vector_2d subtab_padding();
		void set_size(math::c_vector_2d size) { m_size = size; }
		math::c_vector_2d get_pos() const { return m_pos; }
		math::c_vector_2d get_size() const { return m_size; }
	private:
		std::string m_prev_tab{};
		std::string m_prev_subtab{};
		bool m_tab_switching{ false };

		// tab pointer
		std::shared_ptr<c_tab> m_obj_tab{};

		std::string m_title{};
		math::c_vector_2d m_pos{};
		math::c_vector_2d m_size{};

		// window animation
		utils::anim_context_t m_window_opacity{};
		math::c_vector_2d m_last_window_pos{};
		float m_content_scroll_offset = 0.f;
		float m_content_scroll_target = 0.f;
		float m_content_scroll_max = 0.f;

		std::string m_last_rendered_tab{};

		// childrens
		std::vector<std::shared_ptr<c_child>> m_childrens{};
	};

}
