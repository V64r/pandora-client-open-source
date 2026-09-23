#pragma once

namespace framework
{
    class c_text_input : public c_base_element
    {
    public:
        c_text_input(std::string label, std::string* var, bool hide_label = false);
        void draw() override;
        void input() override;
        void set_placeholder(std::string placeholder) { m_placeholder = std::move(placeholder); }

    private:
        std::string* m_var{};
        std::string m_placeholder{};
        struct ghost_char_t {
            char  m_glyph;
            float m_anim;
            float m_x;
        };

        std::vector<ghost_char_t> m_ghost_chars;

        std::vector<float> m_char_anims;
        std::vector<bool>  m_char_removing;
        size_t m_last_value_size{};

        float blink{};
    };
}
