#pragma once

namespace OCPOLPUP {
    extern uint16_t occupation_policy_value;

    extern MOX2::action_func_t saved_autofunction;

    /**
     * Prompts for the current colony occupation policy and stores the selected value.
     */
    void __cdecl Occupation_Policy_Popup(::game::s_world_state* world_state);
} // namespace OCPOLPUP