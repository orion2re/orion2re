#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>
#include "proggy_tiny_font.h"
#endif

namespace debug_overlay {
    uint8_t g_enabled;
    uint16_t g_font_size = 10;
    uint16_t g_ui_scale_percent = 80;

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
    namespace {
        constexpr uint16_t k_input_capacity = 256;
        constexpr uint16_t k_text_capacity = 32;
        constexpr ImTextureID k_font_texture_id = 1;
        constexpr float k_percent_scale = 0.01f;

        float Ui_Scale() {
            return (float)g_ui_scale_percent * k_percent_scale;
        }

        enum e_input_type : uint8_t {
            INPUT_KEY,
            INPUT_TEXT,
            INPUT_MOUSE_POSITION,
            INPUT_MOUSE_BUTTON,
            INPUT_MOUSE_WHEEL,
            INPUT_FOCUS,
            INPUT_DISPLAY_SIZE,
            INPUT_RESET,
        };

        struct s_panel_entry {
            int16_t screen_id;
            panel_callback callback;
        };

        struct s_global_pane_entry {
            const char* label;
            panel_callback callback;
        };

        struct s_input_event {
            e_input_type type;
            SDL_Keycode keycode;
            SDL_Scancode scancode;
            SDL_Keymod modifiers;
            int16_t game_x;
            int16_t game_y;
            float x;
            float y;
            int32_t display_width;
            int32_t display_height;
            uint8_t button;
            bool down;
            char text[k_text_capacity];
        };

        struct s_vertex {
            ImVec2 position;
            ImVec2 uv;
            SDL_FColor color;
        };

        struct s_draw_command {
            ImVec4 clip_rect;
            uint32_t vertex_offset;
            uint32_t vertex_count;
            uint32_t index_offset;
            uint32_t index_count;
        };

        struct s_draw_packet {
            std::vector<s_vertex> vertices;
            std::vector<uint32_t> indices;
            std::vector<s_draw_command> commands;
            int32_t display_width;
            int32_t display_height;
            uint32_t generation;

            void Clear() {
                vertices.clear();
                indices.clear();
                commands.clear();
                display_width = 0;
                display_height = 0;
                generation = 0;
            }
        };

        ImGuiContext* g_context;
        SDL_Texture* g_font_texture;
        SDL_Mutex* g_input_mutex;
        SDL_Mutex* g_draw_mutex;
        s_input_event g_input_queue[k_input_capacity];
        uint16_t g_input_read;
        uint16_t g_input_write;
        s_draw_packet g_pending_packet;
        s_draw_packet g_published_packet;
        s_draw_packet g_present_packet;
        uint64_t g_last_frame_ticks;
        bool g_show_metrics;
        SDL_AtomicInt g_visible;
        int16_t g_last_panel_screen = k_all_screens;

        std::vector<s_panel_entry>& Panel_Registry() {
            static std::vector<s_panel_entry> panels;
            return panels;
        }

        std::vector<s_global_pane_entry>& Global_Pane_Registry() {
            static std::vector<s_global_pane_entry> panes;
            return panes;
        }

        bool Initialized() {
            return g_context != nullptr && g_input_mutex != nullptr && g_draw_mutex != nullptr;
        }

        bool Enabled() {
            return g_enabled != 0;
        }

        void Enqueue(const s_input_event& event) {
            if (g_input_mutex == nullptr) {
                return;
            }

            SDL_LockMutex(g_input_mutex);
            if (event.type == INPUT_MOUSE_POSITION && g_input_read != g_input_write) {
                const uint16_t last_index = g_input_write == 0
                    ? (uint16_t)(k_input_capacity - 1)
                    : (uint16_t)(g_input_write - 1);
                if (g_input_queue[last_index].type == INPUT_MOUSE_POSITION) {
                    g_input_queue[last_index] = event;
                    SDL_UnlockMutex(g_input_mutex);
                    return;
                }
            }
            const uint16_t next_write = (uint16_t)((g_input_write + 1) % k_input_capacity);
            if (next_write == g_input_read) {
                g_input_read = (uint16_t)((g_input_read + 1) % k_input_capacity);
            }
            g_input_queue[g_input_write] = event;
            g_input_write = next_write;
            SDL_UnlockMutex(g_input_mutex);
        }

        ImGuiKey Map_Key(SDL_Keycode keycode, SDL_Scancode scancode) {
            if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
                return (ImGuiKey)((int32_t)ImGuiKey_A + (int32_t)scancode - (int32_t)SDL_SCANCODE_A);
            }
            if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
                return (ImGuiKey)((int32_t)ImGuiKey_1 + (int32_t)scancode - (int32_t)SDL_SCANCODE_1);
            }
            if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
                return (ImGuiKey)((int32_t)ImGuiKey_F1 + (int32_t)scancode - (int32_t)SDL_SCANCODE_F1);
            }
            if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_9) {
                return (ImGuiKey)((int32_t)ImGuiKey_Keypad1 + (int32_t)scancode - (int32_t)SDL_SCANCODE_KP_1);
            }

            switch (scancode) {
                case SDL_SCANCODE_0: return ImGuiKey_0;
                case SDL_SCANCODE_KP_0: return ImGuiKey_Keypad0;
                case SDL_SCANCODE_KP_PERIOD: return ImGuiKey_KeypadDecimal;
                case SDL_SCANCODE_KP_DIVIDE: return ImGuiKey_KeypadDivide;
                case SDL_SCANCODE_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
                case SDL_SCANCODE_KP_MINUS: return ImGuiKey_KeypadSubtract;
                case SDL_SCANCODE_KP_PLUS: return ImGuiKey_KeypadAdd;
                case SDL_SCANCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
                case SDL_SCANCODE_KP_EQUALS: return ImGuiKey_KeypadEqual;
                case SDL_SCANCODE_GRAVE: return ImGuiKey_GraveAccent;
                case SDL_SCANCODE_MINUS: return ImGuiKey_Minus;
                case SDL_SCANCODE_EQUALS: return ImGuiKey_Equal;
                case SDL_SCANCODE_LEFTBRACKET: return ImGuiKey_LeftBracket;
                case SDL_SCANCODE_RIGHTBRACKET: return ImGuiKey_RightBracket;
                case SDL_SCANCODE_BACKSLASH: return ImGuiKey_Backslash;
                case SDL_SCANCODE_SEMICOLON: return ImGuiKey_Semicolon;
                case SDL_SCANCODE_APOSTROPHE: return ImGuiKey_Apostrophe;
                case SDL_SCANCODE_COMMA: return ImGuiKey_Comma;
                case SDL_SCANCODE_PERIOD: return ImGuiKey_Period;
                case SDL_SCANCODE_SLASH: return ImGuiKey_Slash;
                default: break;
            }

            switch (keycode) {
                case SDLK_TAB: return ImGuiKey_Tab;
                case SDLK_LEFT: return ImGuiKey_LeftArrow;
                case SDLK_RIGHT: return ImGuiKey_RightArrow;
                case SDLK_UP: return ImGuiKey_UpArrow;
                case SDLK_DOWN: return ImGuiKey_DownArrow;
                case SDLK_PAGEUP: return ImGuiKey_PageUp;
                case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
                case SDLK_HOME: return ImGuiKey_Home;
                case SDLK_END: return ImGuiKey_End;
                case SDLK_INSERT: return ImGuiKey_Insert;
                case SDLK_DELETE: return ImGuiKey_Delete;
                case SDLK_BACKSPACE: return ImGuiKey_Backspace;
                case SDLK_SPACE: return ImGuiKey_Space;
                case SDLK_RETURN: return ImGuiKey_Enter;
                case SDLK_ESCAPE: return ImGuiKey_Escape;
                case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
                case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
                case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
                case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
                case SDLK_PAUSE: return ImGuiKey_Pause;
                case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
                case SDLK_LSHIFT: return ImGuiKey_LeftShift;
                case SDLK_LALT: return ImGuiKey_LeftAlt;
                case SDLK_LGUI: return ImGuiKey_LeftSuper;
                case SDLK_RCTRL: return ImGuiKey_RightCtrl;
                case SDLK_RSHIFT: return ImGuiKey_RightShift;
                case SDLK_RALT: return ImGuiKey_RightAlt;
                case SDLK_RGUI: return ImGuiKey_RightSuper;
                case SDLK_APPLICATION: return ImGuiKey_Menu;
                default: return ImGuiKey_None;
            }
        }

        void Drain_Input() {
            s_input_event events[k_input_capacity];
            uint16_t event_count = 0;

            SDL_LockMutex(g_input_mutex);
            while (g_input_read != g_input_write && event_count < k_input_capacity) {
                events[event_count++] = g_input_queue[g_input_read];
                g_input_read = (uint16_t)((g_input_read + 1) % k_input_capacity);
            }
            SDL_UnlockMutex(g_input_mutex);

            ImGuiIO& io = ImGui::GetIO();
            for (uint16_t i = 0; i < event_count; ++i) {
                const s_input_event& event = events[i];
                switch (event.type) {
                    case INPUT_KEY: {
                        io.AddKeyEvent(ImGuiMod_Ctrl, (event.modifiers & SDL_KMOD_CTRL) != 0);
                        io.AddKeyEvent(ImGuiMod_Shift, (event.modifiers & SDL_KMOD_SHIFT) != 0);
                        io.AddKeyEvent(ImGuiMod_Alt, (event.modifiers & SDL_KMOD_ALT) != 0);
                        io.AddKeyEvent(ImGuiMod_Super, (event.modifiers & SDL_KMOD_GUI) != 0);
                        const ImGuiKey key = Map_Key(event.keycode, event.scancode);
                        if (key != ImGuiKey_None) {
                            io.AddKeyEvent(key, event.down);
                        }
                        break;
                    }
                    case INPUT_TEXT:
                        io.AddInputCharactersUTF8(event.text);
                        break;
                    case INPUT_MOUSE_POSITION:
                        platform::g_raw_mouse_x = event.x;
                        platform::g_raw_mouse_y = event.y;
                        platform::g_raw_screen_res_x = event.display_width;
                        platform::g_raw_screen_res_y = event.display_height;
                        io.AddMousePosEvent(event.x / Ui_Scale(), event.y / Ui_Scale());
                        break;
                    case INPUT_MOUSE_BUTTON:
                        platform::g_raw_mouse_x = event.x;
                        platform::g_raw_mouse_y = event.y;
                        platform::g_raw_screen_res_x = event.display_width;
                        platform::g_raw_screen_res_y = event.display_height;
                        io.AddMousePosEvent(event.x / Ui_Scale(), event.y / Ui_Scale());
                        io.AddMouseButtonEvent(event.button, event.down);
                        break;
                    case INPUT_MOUSE_WHEEL:
                        io.AddMouseWheelEvent(event.x, event.y);
                        break;
                    case INPUT_FOCUS:
                        io.AddFocusEvent(event.down);
                        break;
                    case INPUT_DISPLAY_SIZE:
                        platform::g_raw_screen_res_x = event.display_width;
                        platform::g_raw_screen_res_y = event.display_height;
                        break;
                    case INPUT_RESET:
                        io.ClearInputKeys();
                        io.ClearInputMouse();
                        break;
                }
            }
        }

        void Draw_Default_Panel(const s_panel_context& context) {
            const float width = std::min(300.0f, ImGui::GetIO().DisplaySize.x - 16.0f);
            const float height = std::min(464.0f, ImGui::GetIO().DisplaySize.y - 16.0f);
            ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowContentSize(ImVec2(580.0f, 0.0f));
            if (ImGui::Begin(
                    "Debug Overview", nullptr,
                    ImGuiWindowFlags_HorizontalScrollbar)) {
                if (ImGui::BeginTabBar("global_debug_panes")) {
                    if (ImGui::BeginTabItem("Status")) {
                        ImGui::Text("Screen: %d", (int)context.screen_id);
                        ImGui::Text("Player: %d", (int)context.player_id);
                        if (ImGui::Button("Force full redraw")) {
                            video::Set_Refresh_Full();
                        }
                        ImGui::SameLine();
                        ImGui::Checkbox("ImGui metrics", &g_show_metrics);
                        ImGui::Separator();
                        ImGui::TextUnformatted("F10 toggles this overlay.");
                        ImGui::EndTabItem();
                    }
                    for (const s_global_pane_entry& pane : Global_Pane_Registry()) {
                        if (ImGui::BeginTabItem(pane.label)) {
                            pane.callback(context);
                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
            if (g_show_metrics) {
                ImGui::ShowMetricsWindow(&g_show_metrics);
            }
        }

        SDL_FColor Convert_Color(ImU32 color) {
            constexpr float scale = 1.0f / 255.0f;
            return {
                (float)((color >> IM_COL32_R_SHIFT) & 0xFF) * scale,
                (float)((color >> IM_COL32_G_SHIFT) & 0xFF) * scale,
                (float)((color >> IM_COL32_B_SHIFT) & 0xFF) * scale,
                (float)((color >> IM_COL32_A_SHIFT) & 0xFF) * scale,
            };
        }

        void Copy_Draw_Data(const ImDrawData& draw_data) {
            g_pending_packet.Clear();
            g_pending_packet.display_width =
                (int32_t)lroundf(draw_data.DisplaySize.x * draw_data.FramebufferScale.x);
            g_pending_packet.display_height =
                (int32_t)lroundf(draw_data.DisplaySize.y * draw_data.FramebufferScale.y);

            for (int32_t list_index = 0; list_index < draw_data.CmdListsCount; ++list_index) {
                const ImDrawList& draw_list = *draw_data.CmdLists[list_index];
                const uint32_t base_vertex = (uint32_t)g_pending_packet.vertices.size();
                const uint32_t base_index = (uint32_t)g_pending_packet.indices.size();

                g_pending_packet.vertices.reserve(g_pending_packet.vertices.size() + draw_list.VtxBuffer.Size);
                for (const ImDrawVert& source : draw_list.VtxBuffer) {
                    g_pending_packet.vertices.push_back({
                        ImVec2(
                            (source.pos.x - draw_data.DisplayPos.x) * draw_data.FramebufferScale.x,
                            (source.pos.y - draw_data.DisplayPos.y) * draw_data.FramebufferScale.y),
                        source.uv,
                        Convert_Color(source.col),
                    });
                }

                g_pending_packet.indices.reserve(g_pending_packet.indices.size() + draw_list.IdxBuffer.Size);
                for (ImDrawIdx source : draw_list.IdxBuffer) {
                    g_pending_packet.indices.push_back((uint32_t)source);
                }

                for (const ImDrawCmd& source : draw_list.CmdBuffer) {
                    if (source.UserCallback != nullptr || source.GetTexID() != k_font_texture_id) {
                        continue;
                    }

                    g_pending_packet.commands.push_back({
                        ImVec4(
                            (source.ClipRect.x - draw_data.DisplayPos.x) * draw_data.FramebufferScale.x,
                            (source.ClipRect.y - draw_data.DisplayPos.y) * draw_data.FramebufferScale.y,
                            (source.ClipRect.z - draw_data.DisplayPos.x) * draw_data.FramebufferScale.x,
                            (source.ClipRect.w - draw_data.DisplayPos.y) * draw_data.FramebufferScale.y),
                        base_vertex + source.VtxOffset,
                        (uint32_t)draw_list.VtxBuffer.Size - source.VtxOffset,
                        base_index + source.IdxOffset,
                        source.ElemCount,
                    });
                }
            }
        }
    } // namespace

    bool Initialize(SDL_Renderer* renderer) {
        if (Initialized()) {
            return true;
        }
        if (renderer == nullptr) {
            return false;
        }

        g_input_mutex = SDL_CreateMutex();
        g_draw_mutex = SDL_CreateMutex();
        if (g_input_mutex == nullptr || g_draw_mutex == nullptr) {
            Shutdown();
            return false;
        }

        IMGUI_CHECKVERSION();
        g_context = ImGui::CreateContext();
        if (g_context == nullptr) {
            Shutdown();
            return false;
        }

        ImGui::SetCurrentContext(g_context);
        ImGuiIO& io = ImGui::GetIO();
        io.BackendPlatformName = "orion2re_game_thread";
        io.BackendRendererName = "orion2re_sdl_thread";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.MouseDrawCursor = false;
        io.DisplaySize = ImVec2(
            (float)video::g_screen_res_x / Ui_Scale(),
            (float)video::g_screen_res_y / Ui_Scale());
        io.DisplayFramebufferScale = ImVec2(Ui_Scale(), Ui_Scale());
        ImFontConfig font_config = {};
        font_config.FontDataOwnedByAtlas = false;
        font_config.GlyphOffset.y = 1.0f;
        font_config.RasterizerDensity = std::max(Ui_Scale(), 1.0f);
        io.Fonts->AddFontFromMemoryTTF(
            (void*)k_proggy_tiny_font_data,
            k_proggy_tiny_font_size,
            (float)g_font_size,
            &font_config);

        unsigned char* pixels = nullptr;
        int32_t width = 0;
        int32_t height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        g_font_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC,
            width,
            height);
        if (g_font_texture == nullptr ||
            !SDL_UpdateTexture(g_font_texture, nullptr, pixels, width * 4) ||
            !SDL_SetTextureBlendMode(g_font_texture, SDL_BLENDMODE_BLEND)) {
            Shutdown();
            return false;
        }
        (void)SDL_SetTextureScaleMode(g_font_texture, SDL_SCALEMODE_LINEAR);
        io.Fonts->SetTexID(k_font_texture_id);

        g_input_read = 0;
        g_input_write = 0;
        g_last_frame_ticks = 0;
        SDL_SetAtomicInt(&g_visible, 0);
        return true;
    }

    void Shutdown() {
        if (g_font_texture != nullptr) {
            SDL_DestroyTexture(g_font_texture);
            g_font_texture = nullptr;
        }
        if (g_context != nullptr) {
            ImGui::SetCurrentContext(g_context);
            ImGui::DestroyContext(g_context);
            g_context = nullptr;
        }
        if (g_input_mutex != nullptr) {
            SDL_DestroyMutex(g_input_mutex);
            g_input_mutex = nullptr;
        }
        if (g_draw_mutex != nullptr) {
            SDL_DestroyMutex(g_draw_mutex);
            g_draw_mutex = nullptr;
        }
        g_pending_packet.Clear();
        g_published_packet.Clear();
        g_present_packet.Clear();
        SDL_SetAtomicInt(&g_visible, 0);
    }

    void Handle_Display_Size(int32_t width, int32_t height) {
        if (!Initialized() || !Enabled() || width <= 0 || height <= 0) {
            return;
        }

        s_input_event input = {};
        input.type = INPUT_DISPLAY_SIZE;
        input.display_width = width;
        input.display_height = height;
        Enqueue(input);
    }

    bool Handle_Key_Event(const SDL_KeyboardEvent& event, SDL_Window* window) {
        if (!Initialized() || !Enabled()) {
            return false;
        }

        if (event.down && !event.repeat && event.key == SDLK_F10) {
            const bool visible = SDL_GetAtomicInt(&g_visible) == 0;
            if (!visible) {
                s_input_event reset = {};
                reset.type = INPUT_RESET;
                Enqueue(reset);
            }
            SDL_LockMutex(g_draw_mutex);
            g_published_packet.Clear();
            g_present_packet.Clear();
            SDL_UnlockMutex(g_draw_mutex);
            SDL_SetAtomicInt(&g_visible, visible ? 1 : 0);
            if (window != nullptr) {
                if (visible) {
                    (void)SDL_StartTextInput(window);
                } else {
                    (void)SDL_StopTextInput(window);
                }
            }
            return true;
        }

        if (!Visible()) {
            return false;
        }

        s_input_event input = {};
        input.type = INPUT_KEY;
        input.keycode = event.key;
        input.scancode = event.scancode;
        input.modifiers = event.mod;
        input.down = event.down;
        Enqueue(input);
        return true;
    }

    bool Handle_Text_Input(const char* text) {
        if (!Initialized() || !Enabled() || !Visible() || text == nullptr) {
            return false;
        }

        s_input_event input = {};
        input.type = INPUT_TEXT;
        strlcpy(input.text, text, sizeof(input.text));
        Enqueue(input);
        return true;
    }

    bool Handle_Mouse_Position(
        int16_t game_x,
        int16_t game_y,
        float raw_x,
        float raw_y,
        int32_t raw_width,
        int32_t raw_height) {
        if (!Initialized() || !Enabled() || !Visible()) {
            return false;
        }

        s_input_event input = {};
        input.type = INPUT_MOUSE_POSITION;
        input.game_x = game_x;
        input.game_y = game_y;
        input.x = raw_x;
        input.y = raw_y;
        input.display_width = raw_width;
        input.display_height = raw_height;
        Enqueue(input);
        return true;
    }

    bool Handle_Mouse_Button(
        uint8_t button,
        bool down,
        int16_t game_x,
        int16_t game_y,
        float raw_x,
        float raw_y,
        int32_t raw_width,
        int32_t raw_height) {
        if (!Initialized() || !Enabled() || !Visible()) {
            return false;
        }

        int32_t imgui_button = -1;
        if (button == SDL_BUTTON_LEFT) {
            imgui_button = ImGuiMouseButton_Left;
        } else if (button == SDL_BUTTON_RIGHT) {
            imgui_button = ImGuiMouseButton_Right;
        } else if (button == SDL_BUTTON_MIDDLE) {
            imgui_button = ImGuiMouseButton_Middle;
        }
        if (imgui_button < 0) {
            return true;
        }

        s_input_event input = {};
        input.type = INPUT_MOUSE_BUTTON;
        input.button = (uint8_t)imgui_button;
        input.down = down;
        input.game_x = game_x;
        input.game_y = game_y;
        input.x = raw_x;
        input.y = raw_y;
        input.display_width = raw_width;
        input.display_height = raw_height;
        Enqueue(input);
        return true;
    }

    bool Handle_Mouse_Wheel(float x, float y) {
        if (!Initialized() || !Enabled() || !Visible()) {
            return false;
        }

        s_input_event input = {};
        input.type = INPUT_MOUSE_WHEEL;
        input.x = x;
        input.y = y;
        Enqueue(input);
        return true;
    }

    void Handle_Focus(bool focused) {
        if (!Initialized() || !Enabled()) {
            return;
        }

        s_input_event input = {};
        input.type = INPUT_FOCUS;
        input.down = focused;
        Enqueue(input);
    }

    void Build_Game_Frame() {
        if (!Initialized() || !Enabled()) {
            SDL_SetAtomicInt(&g_visible, 0);
            return;
        }

        ImGui::SetCurrentContext(g_context);
        Drain_Input();
        if (!Visible()) {
            g_pending_packet.Clear();
            g_last_panel_screen = k_all_screens;
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        const float ui_scale = Ui_Scale();
        io.DisplaySize = ImVec2(
            (float)(platform::g_raw_screen_res_x > 0
                    ? platform::g_raw_screen_res_x
                    : video::g_screen_res_x) /
                ui_scale,
            (float)(platform::g_raw_screen_res_y > 0
                    ? platform::g_raw_screen_res_y
                    : video::g_screen_res_y) /
                ui_scale);
        const uint64_t now = SDL_GetTicksNS();
        io.DeltaTime = g_last_frame_ticks == 0
            ? 1.0f / 60.0f
            : std::max((float)(now - g_last_frame_ticks) / 1000000000.0f, 0.000001f);
        g_last_frame_ticks = now;

        ImGui::NewFrame();
        const s_panel_context panel_context = {
            MOX::current_screen,
            MOX::previous_screen,
            MOX::PLAYER_NUM,
            MOX::current_screen != g_last_panel_screen,
        };
        Draw_Default_Panel(panel_context);
        for (const s_panel_entry& panel : Panel_Registry()) {
            if (panel.screen_id == k_all_screens || panel.screen_id == panel_context.screen_id) {
                panel.callback(panel_context);
            }
        }
        g_last_panel_screen = MOX::current_screen;
        ImGui::Render();

        if (Visible()) {
            Copy_Draw_Data(*ImGui::GetDrawData());
        } else {
            g_pending_packet.Clear();
        }
    }

    void Publish_Game_Frame(uint32_t generation) {
        if (!Initialized() || !Enabled()) {
            return;
        }

        SDL_LockMutex(g_draw_mutex);
        g_pending_packet.generation = generation;
        g_published_packet = g_pending_packet;
        SDL_UnlockMutex(g_draw_mutex);
    }

    void Render_Published_Frame(SDL_Renderer* renderer, uint32_t generation) {
        if (!Initialized() || !Enabled() || renderer == nullptr || g_font_texture == nullptr || !Visible()) {
            return;
        }

        SDL_LockMutex(g_draw_mutex);
        if (g_published_packet.generation == generation &&
            g_published_packet.generation != g_present_packet.generation) {
            g_present_packet = g_published_packet;
        }
        SDL_UnlockMutex(g_draw_mutex);

        if (g_present_packet.display_width <= 0 || g_present_packet.display_height <= 0) {
            return;
        }

        const bool previous_clip_enabled = SDL_RenderClipEnabled(renderer);
        SDL_Rect previous_clip = {};
        (void)SDL_GetRenderClipRect(renderer, &previous_clip);
        int32_t previous_logical_width = 0;
        int32_t previous_logical_height = 0;
        SDL_RendererLogicalPresentation previous_logical_mode = SDL_LOGICAL_PRESENTATION_DISABLED;
        if (!SDL_GetRenderLogicalPresentation(
                renderer,
                &previous_logical_width,
                &previous_logical_height,
                &previous_logical_mode) ||
            !SDL_SetRenderLogicalPresentation(
                renderer,
                g_present_packet.display_width,
                g_present_packet.display_height,
                SDL_LOGICAL_PRESENTATION_STRETCH)) {
            return;
        }

        for (const s_draw_command& command : g_present_packet.commands) {
            const int32_t x1 = std::max((int32_t)floorf(command.clip_rect.x), 0);
            const int32_t y1 = std::max((int32_t)floorf(command.clip_rect.y), 0);
            const int32_t x2 = std::min((int32_t)ceilf(command.clip_rect.z), g_present_packet.display_width);
            const int32_t y2 = std::min((int32_t)ceilf(command.clip_rect.w), g_present_packet.display_height);
            if (x2 <= x1 || y2 <= y1 || command.index_count == 0 || command.vertex_count == 0) {
                continue;
            }

            const SDL_Rect clip = {x1, y1, x2 - x1, y2 - y1};
            (void)SDL_SetRenderClipRect(renderer, &clip);
            const s_vertex* vertices = g_present_packet.vertices.data() + command.vertex_offset;
            const uint32_t* indices = g_present_packet.indices.data() + command.index_offset;
            (void)SDL_RenderGeometryRaw(
                renderer,
                g_font_texture,
                &vertices->position.x,
                (int32_t)sizeof(s_vertex),
                &vertices->color,
                (int32_t)sizeof(s_vertex),
                &vertices->uv.x,
                (int32_t)sizeof(s_vertex),
                (int32_t)command.vertex_count,
                indices,
                (int32_t)command.index_count,
                (int32_t)sizeof(*indices));
        }
        (void)SDL_SetRenderClipRect(renderer, nullptr);
        (void)SDL_SetRenderLogicalPresentation(
            renderer,
            previous_logical_width,
            previous_logical_height,
            previous_logical_mode);
        (void)SDL_SetRenderClipRect(renderer, previous_clip_enabled ? &previous_clip : nullptr);
    }

    bool Register_Screen_Panel(int16_t screen_id, panel_callback callback) {
        if (callback == nullptr) {
            return false;
        }
        Panel_Registry().push_back({screen_id, callback});
        return true;
    }

    bool Register_Global_Panel(panel_callback callback) {
        return Register_Screen_Panel(k_all_screens, callback);
    }

    bool Register_Global_Pane(const char* label, panel_callback callback) {
        if (label == nullptr || label[0] == '\0' || callback == nullptr) {
            return false;
        }
        std::vector<s_global_pane_entry>& panes = Global_Pane_Registry();
        panes.push_back({label, callback});
        std::sort(
            panes.begin(), panes.end(),
            [](const s_global_pane_entry& left, const s_global_pane_entry& right) {
                return strcmp(left.label, right.label) < 0;
            });
        return true;
    }

    s_screen_panel_registration::s_screen_panel_registration(
        int16_t screen_id,
        panel_callback callback) {
        (void)Register_Screen_Panel(screen_id, callback);
    }

    s_global_pane_registration::s_global_pane_registration(
        const char* label,
        panel_callback callback) {
        (void)Register_Global_Pane(label, callback);
    }

    bool Visible() {
        return Enabled() && SDL_GetAtomicInt(&g_visible) != 0;
    }
#else
    bool Initialize(SDL_Renderer* renderer) {
        (void)renderer;
        return true;
    }

    void Shutdown() {
    }

    void Handle_Display_Size(int32_t width, int32_t height) {
        (void)width;
        (void)height;
    }

    bool Handle_Key_Event(const SDL_KeyboardEvent& event, SDL_Window* window) {
        (void)event;
        (void)window;
        return false;
    }

    bool Handle_Text_Input(const char* text) {
        (void)text;
        return false;
    }

    bool Handle_Mouse_Position(
        int16_t game_x,
        int16_t game_y,
        float raw_x,
        float raw_y,
        int32_t raw_width,
        int32_t raw_height) {
        (void)game_x;
        (void)game_y;
        (void)raw_x;
        (void)raw_y;
        (void)raw_width;
        (void)raw_height;
        return false;
    }

    bool Handle_Mouse_Button(
        uint8_t button,
        bool down,
        int16_t game_x,
        int16_t game_y,
        float raw_x,
        float raw_y,
        int32_t raw_width,
        int32_t raw_height) {
        (void)button;
        (void)down;
        (void)game_x;
        (void)game_y;
        (void)raw_x;
        (void)raw_y;
        (void)raw_width;
        (void)raw_height;
        return false;
    }

    bool Handle_Mouse_Wheel(float x, float y) {
        (void)x;
        (void)y;
        return false;
    }

    void Handle_Focus(bool focused) {
        (void)focused;
    }

    void Build_Game_Frame() {
    }

    void Publish_Game_Frame(uint32_t generation) {
        (void)generation;
    }

    void Render_Published_Frame(SDL_Renderer* renderer, uint32_t generation) {
        (void)renderer;
        (void)generation;
    }

    bool Register_Screen_Panel(int16_t screen_id, panel_callback callback) {
        (void)screen_id;
        (void)callback;
        return false;
    }

    bool Register_Global_Panel(panel_callback callback) {
        (void)callback;
        return false;
    }

    bool Register_Global_Pane(const char* label, panel_callback callback) {
        (void)label;
        (void)callback;
        return false;
    }

    s_screen_panel_registration::s_screen_panel_registration(
        int16_t screen_id,
        panel_callback callback) {
        (void)screen_id;
        (void)callback;
    }

    s_global_pane_registration::s_global_pane_registration(
        const char* label,
        panel_callback callback) {
        (void)label;
        (void)callback;
    }

    bool Visible() {
        return false;
    }
#endif
} // namespace debug_overlay