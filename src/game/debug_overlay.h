#pragma once

#include "object_ids.h"

namespace debug_overlay {
    constexpr int16_t k_all_screens = INT16_MIN;
    extern uint8_t g_enabled;
    extern uint16_t g_font_size;
    extern uint16_t g_ui_scale_percent;

    struct s_panel_context {
        int16_t screen_id;
        int16_t previous_screen_id;
        game::id_player player_id;
        bool screen_entered;
    };

    using panel_callback = void (*)(const s_panel_context& context);

    class s_screen_panel_registration {
      public:
        s_screen_panel_registration(int16_t screen_id, panel_callback callback);
    };

    class s_global_pane_registration {
      public:
        s_global_pane_registration(const char* label, panel_callback callback);
    };

    bool Initialize(SDL_Renderer* renderer);
    void Shutdown();
    void Handle_Display_Size(int32_t width, int32_t height);

    bool Handle_Key_Event(const SDL_KeyboardEvent& event, SDL_Window* window);
    bool Handle_Text_Input(const char* text);
    bool Handle_Mouse_Position(
        int16_t game_x,
        int16_t game_y,
        float raw_x,
        float raw_y,
        int32_t raw_width,
        int32_t raw_height);
    bool Handle_Mouse_Button(
        uint8_t button,
        bool down,
        int16_t game_x,
        int16_t game_y,
        float raw_x,
        float raw_y,
        int32_t raw_width,
        int32_t raw_height);
    bool Handle_Mouse_Wheel(float x, float y);
    void Handle_Focus(bool focused);

    void Build_Game_Frame();
    void Publish_Game_Frame(uint32_t generation);
    void Render_Published_Frame(SDL_Renderer* renderer, uint32_t generation);

    bool Register_Screen_Panel(int16_t screen_id, panel_callback callback);
    bool Register_Global_Panel(panel_callback callback);
    bool Register_Global_Pane(const char* label, panel_callback callback);
    bool Visible();
} // namespace debug_overlay