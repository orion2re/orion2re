#include "pch.h"

namespace OCPOLPUP {
    uint16_t occupation_policy_value;

    MOX2::action_func_t saved_autofunction;

    void __cdecl Occupation_Policy_Popup(game::id_colony colony) {
        s_colors font_colors = {{0x9f, 0xa0, 0xa1, 0xa2, 0, 0, 0, 0}};
        auto& state = game::current_state;
        const auto initial = state.colonies.find(colony);
        if (initial == state.colonies.end()) return;
        const auto planet = initial->second.planet_id;
        const auto owner = initial->second.owner;
        if (owner.id >= MAX_PLAYERS) return;
        occupation_policy_value = (uint16_t)(uint8_t)initial->second.occupation_policy;

        fonts::Set_Font_Style_Shadow_Down(1, &font_colors);
        video::Copy_Off_To_Back();
        ERICHELP::Set_Occupation_Policy_Popup_Help_List();
        MOX2::Get_Auto_Function(&saved_autofunction);
        fields::Deactivate_Auto_Function();

        const auto government = state.players[owner.id].traits[TRAIT_CURRENT_GOVERNMENT];
        if (government != GOVERNMENT_DEMOCRACY && government != GOVERNMENT_FEDERATION) {
            if (GENDRAW::Confirmation_Box(ESTRINGS::E_Strings(0xB4)) != 0) {
                occupation_policy_value = 0;
            } else {
                occupation_policy_value = 3;
            }
        } else {
            occupation_policy_value = 3;
        }

        const auto live = state.colonies.find(colony);
        if (live != state.colonies.end() && live->second.planet_id == planet && live->second.owner == owner) {
            live->second.occupation_policy = (char)occupation_policy_value;
        }

        fields::Deactivate_Help_List();
        MOX2::Put_Auto_Function(&saved_autofunction);
        video::Copy_Back_To_Off();
    }
} // namespace OCPOLPUP
