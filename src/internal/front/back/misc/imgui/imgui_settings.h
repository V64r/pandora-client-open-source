#include "imgui.h"


namespace c {

	inline ImVec4 accent = ImColor(156, 156, 213);
	inline ImVec4 accent_gradient = ImColor(156, 156, 213);

	namespace bg {

		inline ImVec4 shadow = ImColor(14, 14, 16, 0);

		inline ImVec4 background = ImColor(19, 19, 21, 255);
		inline ImVec4 background_pad = ImColor(22, 22, 24, 255);

		// Structural borders never inherit the user accent. Keep them nearly
		// black so cards are separated by depth instead of bright outlines.
		inline ImVec4 border = ImColor(21, 21, 23, 255);

		inline ImVec2 size = ImVec2(900, 600);

		inline float rounding = 12.f;

		namespace child {
			inline ImVec4 shadow = ImColor(21, 20, 27, 0);

			inline ImVec4 background = ImColor(24, 24, 25, 255);
			inline ImVec4 background_cap = ImColor(25, 25, 28, 255);
			inline ImVec4 text_name = ImColor(200, 205, 210, 255);
			inline ImVec4 border = ImColor(40, 40, 44, 255);

			inline float rounding = 6.f;
		}

		namespace child_header {
			inline ImVec4 background = ImColor(26, 26, 29, 255);
			inline ImVec4 border = ImColor(43, 43, 47, 255);

			inline float rounding = 6.f;
		}

	}

	namespace tabs {

		inline ImVec4 background_active = ImColor(18, 19, 23, 255);
		inline ImVec4 background_hov = ImColor(14, 15, 18, 255);
		inline ImVec4 background = ImColor(21, 20, 27, 0);

		inline ImVec4 border_active = ImColor(28, 29, 33, 255);
		inline ImVec4 border_inactive = ImColor(35, 36, 40, 0);

		inline float rounding = 3.f;
	}

	namespace text {

		inline ImVec4 text_selected = ImColor(21, 20, 27, 100);
		inline ImVec4 text_active = ImColor(255, 255, 255, 255);
		inline ImVec4 text_hov = ImColor(255, 255, 255, 255);
		inline ImVec4 text = ImColor(150, 150, 158, 255);

	}

	namespace scrollbar {
		inline ImVec4 border = ImColor(55, 60, 70, 200);

		inline float rounding = 30.f;
	}

	namespace checkbox {
		inline ImVec4 background_hov = ImColor(20, 21, 25, 255);
		inline ImVec4 background = ImColor(13, 14, 17, 255);
		inline ImVec4 border = ImColor(28, 29, 33, 255);

		inline ImVec4 checkmark_active = ImColor(0, 0, 0, 255);
		inline ImVec4 checkmark_inactive = ImColor(0, 0, 0, 0);

		inline float rounding = 3.f;
	}

	namespace slider {

		inline ImVec4 background_hov = ImColor(20, 21, 25, 255);
		inline ImVec4 background = ImColor(13, 14, 17, 255);
		inline ImVec4 border = ImColor(28, 29, 33, 255);
		inline ImVec4 circle = ImColor(34, 35, 42, 255);

	}

	namespace button {

		inline ImVec4 background_hov = ImColor(20, 21, 25, 255);
		inline ImVec4 background = ImColor(13, 14, 17, 255);
		inline ImVec4 border = ImColor(28, 29, 33, 255);

		inline float rounding = 3.f;
	}

	namespace input {

		inline ImVec4 background_hov = ImColor(20, 21, 25, 255);
		inline ImVec4 background = ImColor(13, 14, 17, 255);
		inline ImVec4 border = ImColor(28, 29, 33, 255);

		inline float rounding = 3.f;
	}

	namespace combo {

		inline ImVec4 shadow = ImColor(30, 31, 40, 0);


		inline ImVec4 background_hov = ImColor(20, 21, 25, 255);
		inline ImVec4 background = ImColor(11, 12, 15, 255);
		inline ImVec4 border = ImColor(28, 29, 33, 255);

		inline float rounding = 3.f;
	}

	namespace picker {

		inline ImVec4 background = ImColor(13, 14, 17, 255);
		inline ImVec4 border = ImColor(28, 29, 33, 255);

		inline float rounding = 3.f;
	}

	namespace keybind {

		inline ImVec4 background_hov = ImColor(20, 21, 25, 255);
		inline ImVec4 background = ImColor(13, 14, 17, 255);

		inline float rounding = 3.f;
	}

	namespace alpha_preview {
		inline ImVec4 alpha_bland_one = ImColor(30, 30, 30, 255);
		inline ImVec4 alpha_bland_two = ImColor(55, 55, 55, 255);

	}

	namespace separator {
		inline ImVec4 border = ImColor(28, 29, 33, 255);
	}



}
