#include "pch.h"

namespace OCPOLPUP {
    uint16_t occupation_policy_value;

    MOX2::action_func_t saved_autofunction;

    void __cdecl Occupation_Policy_Popup(::game::s_world_state* world_state) {
        s_colors font_colors = {{0x9f, 0xa0, 0xa1, 0xa2, 0, 0, 0, 0}};
        ::game::id_planet planet_index;
        int16_t colony_index;
        int16_t owner_index;

        planet_index = HAROLD::Planet_Index(MOX::screen_data, legacy_world::Planet_Id(COLONY::orbit_temp));
        colony_index = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_index))).colony_index;
        occupation_policy_value = (uint16_t)(uint8_t)MOX::colony[colony_index].occupation_policy;

        fonts::Set_Font_Style_Shadow_Down(1, &font_colors);
        video::Copy_Off_To_Back();
        ERICHELP::Set_Occupation_Policy_Popup_Help_List();
        MOX2::Get_Auto_Function(&saved_autofunction);
        fields::Deactivate_Auto_Function();

        planet_index = HAROLD::Planet_Index(MOX::screen_data, legacy_world::Planet_Id(COLONY::orbit_temp));
        colony_index = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_index))).colony_index;
        owner_index = MOX::colony[colony_index].owner;

        if (MOX::player[owner_index].traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_DEMOCRACY && MOX::player[owner_index].traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_FEDERATION) {
            if (GENDRAW::Confirmation_Box(ESTRINGS::E_Strings(0xB4)) != 0) {
                occupation_policy_value = 0;
            } else {
                occupation_policy_value = 3;
            }
        } else {
            occupation_policy_value = 3;
        }

        planet_index = HAROLD::Planet_Index(MOX::screen_data, legacy_world::Planet_Id(COLONY::orbit_temp));
        colony_index = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_index))).colony_index;
        MOX::colony[colony_index].occupation_policy = (char)occupation_policy_value;

        fields::Deactivate_Help_List();
        MOX2::Put_Auto_Function(&saved_autofunction);
        video::Copy_Back_To_Off();
    }
} // namespace OCPOLPUP