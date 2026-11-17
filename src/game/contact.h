#pragma once

namespace CONTACT {
    /** Recomputes inter-player contact and range state from current colony visibility. */
    void __cdecl Compute_Contacts(::game::s_world_state* world_state);
} // namespace CONTACT