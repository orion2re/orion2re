#include "pch.h"

namespace init {
    namespace {
        constexpr size_t kIoBufferSize = 0x2000;
    }

    s_animation_header* io_buffer;

    void __cdecl Create_IO_Buffer() {
        io_buffer = (s_animation_header*)malloc(kIoBufferSize);
        memset(io_buffer, 0, kIoBufferSize);
    }

    int8_t __cdecl Init_Drivers(uint32_t unused, char* font_name) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: Init_Drivers start font='%s'", font_name != nullptr ? font_name : "<null>");

        init::Create_IO_Buffer();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: io buffer created");

        video::Init_Video_Drivers();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: video drivers initialized");

        remap::Sort_Palette_List();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: palette list sorted");

        sound::Init_Sound();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: sound initialized");

        if (*font_name == '\0') {
            sysfont::Load_System_Font();
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: system font loaded");
        } else {
            fonts::Load_Font_File(font_name);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: font file loaded '%s'", font_name);
        }

        file_ani::Reset_File_Animation_Buffer();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: file animation buffer reset");

        // Clear both video pages to black (0)
        video::Set_Page_Off();
        graphics::Fill(0, 0, 639, 479, 0);

        platform::Toggle_Pages_Stub();

        video::Set_Page_Off();
        graphics::Fill(0, 0, 639, 479, 0);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "init: Init_Drivers complete");
        return 1;
    }
} // namespace init

namespace allocate {
    constexpr uint32_t Global_Data_Space() {
        return 0x2800 + MAX_SHIPS - ORIGINAL_MAX_SHIPS;
    }

    void Allocate_Data_Space() {
        MOX::global_screen_seg = Allocate_Space(Global_Screen_Space());
        MOX::techname_seg = Allocate_Space(30000);
        MOX::special_names_seg = Allocate_Space(1000);

        MOX::planet = (s_planet_data*)Allocate_Space(MAX_PLANETS * sizeof(s_planet_data));
        struct_::Clear_Structure(MOX::planet, MAX_PLANETS * sizeof(s_planet_data));

        MOX::player = (s_player*)Allocate_Space(MAX_PLAYERS * sizeof(s_player));
        struct_::Clear_Structure(MOX::player, MAX_PLAYERS * sizeof(s_player));

        MOX::colony = (s_colony*)Allocate_Space(MAX_COLONIES * sizeof(s_colony));
        struct_::Clear_Structure(MOX::colony, MAX_COLONIES * sizeof(s_colony));

        MOX::nebula_screen_seg = Allocate_Space(12 * 0x10000);

        // Global cache allocation
        MOX::global_cache_seg = buffer::Allocate_Buffer_Space(0x174508);
        MOX::global_cache_size = 0x174508;

        // DOS-space allocations for low-level segments
        MOX::global_data_seg = Allocate_Dos_Space(Global_Data_Space());

        MOX::nebula = (s_nebula*)Allocate_Dos_Space(MAX_NEBULAS * sizeof(s_nebula));
        struct_::Clear_Structure(MOX::nebula, MAX_NEBULAS * sizeof(s_nebula));

        MOX::star = (s_star_data*)Allocate_Dos_Space(MAX_STARS * sizeof(s_star_data));
        struct_::Clear_Structure(MOX::star, MAX_STARS * sizeof(s_star_data));

        MOX::ship = (s_ship_data*)Allocate_Dos_Space(MAX_SHIPS * sizeof(s_ship_data));
        struct_::Clear_Structure(MOX::ship, MAX_SHIPS * sizeof(s_ship_data));

        MOX::leaders = (s_leader_data*)Allocate_Dos_Space(MAX_LEADERS * sizeof(s_leader_data));
        struct_::Clear_Structure(MOX::leaders, MAX_LEADERS * sizeof(s_leader_data));
    }
} // namespace allocate