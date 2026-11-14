#pragma once

namespace SETTINGS_SCREEN {
    // Runs the in-game settings screen, in three tabbed categories:
    // Game Internals and Gameplay list the interface and
    // synchronized_interface settings from the spec (editor picked
    // from the leaf type: booleans toggle as [x]/[ ], small enums
    // open a selection list, integers and strings take an edit
    // field), and Mods lists the two mod-adjacent settings first and
    // then every installed mod from the catalogue with an
    // enable/disable toggle. A setting change is written to
    // the user orion2re.cfg and reloaded live; settings the spec
    // marks as taking effect on the next start are flagged with "*".
    // A mod toggle dry-runs the candidate set through the staging
    // path, writes the resolved order to orion2re.cfg, and applies
    // it with a full load. Settings with no editor render as info
    // rows. The background is the generated hi-res surface
    // (settings_background), not the colony's baked bitmap.
    // Returns to the main menu when closed.
    void __cdecl Settings_Screen();

} // namespace SETTINGS_SCREEN
