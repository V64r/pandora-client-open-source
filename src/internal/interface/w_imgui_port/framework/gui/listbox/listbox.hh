#pragma once

namespace framework
{
	class c_listbox : public c_base_element
	{
		friend class c_child;
		friend class c_window;
	public:
		c_listbox(std::string label, int* var, std::vector<std::string> items, float height, bool hide_label = false);

		void draw() override;
		void input() override;
		void set_items(const std::vector<std::string>& items) { if (m_items == items) return; m_items = items; if (m_var) *m_var = std::clamp(*m_var, 0, std::max(0, (int)m_items.size() - 1)); }
		void set_editable(bool value) { m_editable = value; }
		void set_reorderable(bool value) { m_reorderable = value; }
        void set_markers_visible(bool value) { m_markers_visible = value; }

		std::function<void(int)> on_right_click;
		std::function<void(int, int)> on_reorder;
		std::function<void(int, const std::string&)> on_rename;
        std::function<ImTextureID(int, const std::string&)> item_texture;
	private:
		int* m_var{};
		std::vector<std::string> m_items{};
		float m_height{};

		bool m_listbox_item_hovered{false};
		float m_scroll_offset{ 0.f };
		float m_scroll_target{ 0.f };

		int m_dragging_index{ -1 };
		int m_hovering_index{ -1 };
		bool m_editable{ true };
		bool m_reorderable{ true };
		bool m_markers_visible{ true };

		// Inline editing state
		int m_editing_index{ -1 };
		std::string m_edit_buffer{};
        float m_edit_cursor_timer{ 0.f };
	};
}
