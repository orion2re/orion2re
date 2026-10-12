#include "pch.h"

namespace {
    char captured_paragraph[1024];

    char trait_government[] = "Government";
    char trait_growth[] = "Population Growth";
    char trait_farming[] = "Farming";
    char trait_industry[] = "Industry";
    char trait_science[] = "Science";
    char trait_money[] = "Money";
    char trait_defense[] = "Ship Defense";
    char trait_attack[] = "Ship Attack";
    char trait_ground[] = "Ground Combat";
    char trait_spying[] = "Spying";
    char trait_low_g[] = "Low-G World";
    char trait_high_g[] = "High-G World";
    char trait_aquatic[] = "Aquatic";
    char trait_subterranean[] = "Subterranean";
    char trait_large_home_world[] = "Large Home World";
    char trait_rich_home_world[] = "Rich Home World";
    char trait_artifacts[] = "Ancient Artifacts Home World";
    char trait_cybernetic[] = "Cybernetic";
    char trait_lithovore[] = "Lithovore";
    char trait_repulsive[] = "Repulsive";
    char trait_charismatic[] = "Charismatic";
    char trait_uncreative[] = "Uncreative";
    char trait_creative[] = "Creative";
    char trait_tolerant[] = "Tolerant";
    char trait_fantastic_traders[] = "Fantastic Traders";
    char trait_telepathic[] = "Telepathic";
    char trait_lucky[] = "Lucky";
    char trait_omniscient[] = "Omniscient";
    char trait_stealthy_ships[] = "Stealthy Ships";
    char trait_trans_dimensional[] = "Trans-Dimensional";
    char trait_warlord[] = "Warlord";
    char trait_poor_home_world[] = "Poor Home World";
    char government_unification[] = "Unification";
}

namespace TEST_RACESEL_DIGEST {
    void Reset_() {
        captured_paragraph[0] = '\0';

        char* trait_names[TRAIT_COUNT + 1] = {
            trait_government,
            trait_growth,
            trait_farming,
            trait_industry,
            trait_science,
            trait_money,
            trait_defense,
            trait_attack,
            trait_ground,
            trait_spying,
            trait_low_g,
            trait_high_g,
            trait_aquatic,
            trait_subterranean,
            trait_large_home_world,
            trait_rich_home_world,
            trait_artifacts,
            trait_cybernetic,
            trait_lithovore,
            trait_repulsive,
            trait_charismatic,
            trait_uncreative,
            trait_creative,
            trait_tolerant,
            trait_fantastic_traders,
            trait_telepathic,
            trait_lucky,
            trait_omniscient,
            trait_stealthy_ships,
            trait_trans_dimensional,
            trait_warlord,
            trait_poor_home_world,
        };
        memcpy(MOX::special_ability_names, trait_names, sizeof(trait_names));
        memset(MOX::government_labels, 0, sizeof(MOX::government_labels));
        MOX::government_labels[GOVERNMENT_UNIFICATION] = government_unification;
    }

    const char* Captured_Paragraph_() {
        return captured_paragraph;
    }
}

namespace MOX {
    void* global_cache_seg;
    char* government_labels[GOVERNMENT_COUNT];
    char* special_ability_names[32];
}

namespace DESIGN {
    const char s_comma_space[3] = ", ";
}

namespace buffer {
    void* __cdecl Buffer_Reload(const char*, int32_t, void*) {
        return nullptr;
    }
}

namespace animate {
    void __cdecl Draw(int16_t, int16_t, s_animation_header*) {
    }
}

namespace fonts {
    void __cdecl Set_Font_Style_Shadow_Down(int16_t, const s_colors*) {
    }

    void __cdecl Set_Outline_Color(uint16_t, uint16_t) {
    }

    void __cdecl Print_Centered(int16_t, int16_t, const char*) {
    }
}

namespace KEN {
    void __cdecl Ken_Get_Text_Message(int32_t message_id, char* buffer, int16_t buffer_size) {
        const char* message = message_id == 0x68 ? ", and " : "";
        strlcpy(buffer, message, buffer_size);
    }
}

namespace BILL {
    void __cdecl _Print_Paragraph(int16_t x, int16_t y, int16_t width,
                                  const char* text, int16_t alignment) {
        if (x == 0x32 && y == 0x187 && width == 0x127 && alignment == 0) {
            strlcpy(captured_paragraph, text, sizeof(captured_paragraph));
        }
    }
}