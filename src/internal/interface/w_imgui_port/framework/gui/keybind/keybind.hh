#pragma once

namespace framework
{
	// True while a keybind element is actively waiting to capture a key. The main
	// GUI's WndProc checks this so pressing Escape mid-capture clears the bind to
	// "None" instead of closing the whole menu.
	extern bool g_keybind_capturing;

	enum key_mode_t : int {
		// this is a bit ghetto but we can use this to define the keybind mode
		always = 0,
		hold = 1,
		toggle = 2,
	};

	// TOTAL DOSHIT
	struct key_var_t {
		int key{}, mode{ 1 };

		// yeah i guess we can make this our main input system for keybind
		bool active(bool bound_to_box = false) {
			if ((this->key <= 0 || this->key > 255) && this->mode != key_mode_t::always) {
				// the key is not set so what the fucking ever
				return true; // invalid key
			}

			if (this->mode == key_mode_t::always) {
				// this is always on
				return true;
			}
			else if (this->mode == key_mode_t::toggle) {
				return GetKeyState(key); // this should return true or false
			}
			else if (this->mode == key_mode_t::hold) {
				return GetAsyncKeyState(key); // this should return true or false
			}
			else {
				return false;
			}
		}
	};

	class c_keybind : public c_base_element
	{
	public:
		c_keybind(std::string label, key_var_t* val, bool hide_label = false);

		void draw() override;
		void input() override;
		void set_simple() { m_simple = true; }
	private:
		bool m_simple{ false };
		bool m_wait_release{ false };
		std::array<bool, 256> m_key_was_down{};
		key_var_t* m_val{};

		// this var exists only for keybind objects, no fucking way we make it a global, its just useless
		bool m_key_callback{false}; // set it to false by default

		// tracks m_key_callback transitions so we can drive g_keybind_capturing
		// from the one instance that is actually capturing.
		bool m_was_capturing{false};
	};
}
