#include "pch.h"
#include "string_overrides.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config/config_loader.h"
#include "config/config_runtime.h"
#include "config/config_writer.h"
#include "config/mod_catalogue.h"
#include "config/settings_value.h"
#include "game/fields.h"
#include "game/fonts.h"
#include "game/graphics.h"
#include "game/mox2.h"
#include "game/mouse.h"
#include "game/palette.h"
#include "game/platform.h"
#include "game/russ.h"
#include "game/animate.h"
#include "game/line.h"
#include "game/settings_background.h"
#include "game/sprite_art.h"
#include "game/settings_layout.h"
#include "game/settings_screen.h"
#include "game/timer.h"
#include "game/user_paths.h"
#include "game/vfs.h"
#include "game/video.h"

namespace SETTINGS_SCREEN {

    namespace {

        // Fields-rendered settings screen: the screen
        // renders through the game's fields system, following
        // the fields design principle (moo2mod fields doc) --
        // the background logic (config writer, mod catalogue +
        // staging/reload, metadata rows, categories, XDG
        // fallback, wheel forwarding via the field system)
        // stays, and the rendering is field types, not custom
        // drawing. Row labels are STRING_LIST fields (the
        // field system draws the text and the selection
        // highlight) in the sample's bright green; values are BUTTON fields on derived
        // sprites (the dropdown-closed button for a dropdown
        // value, the widened value-box frame for an edit value,
        // the colony buy-button toggle frames for a
        // bool/mod); the category tabs are BUTTON fields on
        // the colony sort-tab frames (active for the current
        // category, sized to the label like the colony's
        // switcher); MORE and RETURN are BUTTON fields; the
        // scroll arrows are BUTTON fields on the colony
        // scroll-arrow frames; the scrollbar is the field
        // system's SCROLL field; dropdowns open through the
        // shared Get_List_Field picker; edits use the
        // continuous-input field. The only drawing the screen
        // does itself is the text the fields system does not
        // draw (header labels, description, status) in the
        // game's own font, and the scrollbar thumb (the
        // colony's indicator bar, drawn with the game's line
        // and fill primitives). A field's sprite is chosen at
        // build time for the element's current state, so a
        // single-frame derived sprite carries the state.
        void Rebuild_Row_Lines();
        int Page_Count();
        SETTINGS_LAYOUT::s_rect Visible_Row_Rect(int offset);
        SETTINGS_LAYOUT::s_rect Visible_Value_Rect(int offset);
        std::string Visible_Row_Name(int visible);
        std::string Row_Value_Text(const config::s_settings_row& row);

        int16_t Text_Pad(int16_t screen_width);

        constexpr int ROWS_PER_PAGE = 10;

        // The standard UI text palette, as used by the council screen.
        s_colors g_normal_colors = {{150, 154, 155, 151, 152, 153, 154, 155}};

        // The header-label white: the menu palette's near-white
        // grey ramp, measured from MAINMENU.LBX entry 2 (the
        // palette installed while this screen is up): 250 is
        // (220,220,228), the brightest near-white, 249 is
        // (204,204,212) for the outline. SETTING/VALUE draw in
        // this, not in the teal g_normal_colors (whose face,
        // 154, is cyan (120,180,184)). Slot 0 is the outline,
        // the rest the face, like g_list_green_colors.
        s_colors g_header_white_colors =
            {{249, 250, 250, 250, 250, 250, 250, 250}};

        // The dark button-label colour: the menu palette's black
        // (index 0 is (0,0,0), and the settings screen does not
        // remap it -- it owns only 160-223 and 253). MORE sits on
        // the light value-box frame, like the colonies RETURN, so
        // its label is dark like the colonies RETURN's baked label,
        // not the teal g_normal_colors.
        s_colors g_button_dark_colors =
            {{0, 0, 0, 0, 0, 0, 0, 0}};

        // The row-label green, as in Tom's settings sample: the
        // setting names are bright green, while the button labels
        // and header/description stay the council grey. The
        // indices are the menu palette's green ramp, measured
        // from fonts.lbx palette 0 (MOX::combat_green's 0x81/0x89
        // are RED in this palette -- 0x81 is r=49,g=0 -- so they
        // must not be used here): 0x79 is the darker green
        // outline (g=45), 0x78 the bright pure-green face (g=63).
        // Applied to the string-list colours only
        // (list_normal_color / list_highlight_colors), so the
        // BUTTON fields keep g_normal_colors -- they capture the
        // current font colour at creation, and the string list
        // reads the globals.
        s_colors g_list_green_colors = {{0x79, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78}};

        enum e_apply_result {
            APPLY_LIVE,
            APPLY_SAVED,
            APPLY_FAILED,
        };

        // Categories, in selector order. A category button is the
        // cleanest fit for the existing field system: the screen is
        // already a scrolling list with RETURN/MORE fields, so a
        // category is one more row of hidden fields, now in the
        // bottom selector bar like the colony summary's sort buttons,
        // and each category keeps its own scroll position and
        // selection. Sections (one long list with headers) would
        // break the scrolling and edit-field geometry instead.
        constexpr int CATEGORY_INTERNALS = 0;
        constexpr int CATEGORY_GAMEPLAY = 1;
        constexpr int CATEGORY_MODS = 2;
        constexpr int CATEGORY_COUNT = 3;

        const char* CATEGORY_LABELS[CATEGORY_COUNT] = {
            "Game Internals",
            "Gameplay",
            "Mods",
        };
        // The tab labels ARE the full category names: the tabs
        // are the widened sort-tab frames (160/104/80 px), sized
        // to the labels, so the full names fit (Tom, 2026-11-13).
        // The full name is also the category's description.

        config::e_settings_category Settings_Category_For_Tab(int category) {
            switch (category) {
                case CATEGORY_GAMEPLAY:
                    return config::SETTINGS_CATEGORY_GAMEPLAY;
                case CATEGORY_MODS:
                    return config::SETTINGS_CATEGORY_MODS;
                case CATEGORY_INTERNALS:
                default:
                    return config::SETTINGS_CATEGORY_GAME_INTERNALS;
            }
        }

        struct s_settings_screen_state {
            std::vector<config::s_settings_row> rows;
            // Indices into rows for the current settings category.
            std::vector<int> visible_rows;
            // Lines each visible row takes (1, or 2 for a row whose
            // label wraps onto a second line), parallel to the
            // visible indices (settings rows, then catalogue
            // entries on the Mods tab). Rebuilt with the visible
            // rows: a wrapped row takes two slots, so its label
            // renders at full size.
            std::vector<int> row_lines;
            // Catalogue entries for the Mods category.
            std::vector<config::s_mod_catalogue_entry> mods;
            // Working enabled set, in file order: the `enable` lines
            // of the config file. A toggle previews, writes and
            // applies from this set, then refreshes it from the
            // preview's resolved order (the order written to the file).
            std::vector<std::string> enabled_ids;
            int category = CATEGORY_INTERNALS;
            // Colony-list scroll, per tab: the scrollbar's bound
            // value (colsum.cpp's slider_bar_position) and the
            // first visible row (colsum.cpp's `first`). The slider
            // is inverted against the top row, like the colony's:
            // the slider at its maximum shows the first rows, the
            // slider at 0 shows the last. Sync_Scroll derives the
            // top row from the slider, mirroring
            // COLSUM::Update_First. A fresh tab opens at the top:
            // its slider starts at its maximum on first visit
            // (slider_initialised), and keeps its position on
            // later visits.
            int16_t slider_position[CATEGORY_COUNT] = {};
            int scroll_top[CATEGORY_COUNT] = {};
            bool slider_initialised[CATEGORY_COUNT] = {};
            // Index within the current category's list (visible rows
            // for settings, catalogue entries for mods).
            int selected = 0;
            std::string status;
            std::string target_path;
            std::string seed_path;
            bool target_ready = false;
            // Visible index of the row being edited, or -1.
            int editing_row = -1;
            char edit_buffer[512] = {};
            int16_t edit_field = -1;
            int16_t scroll_field = -1;
            // The scrollbar's arrow buttons (4f): hidden fields
            // over the arrow zones at the strip's ends, stepping
            // the list one row per click, like the colony's
            // x_fields[1]/[2]. -1 when the list does not scroll.
            int16_t scroll_up_field = -1;
            int16_t scroll_down_field = -1;
            int16_t done_field = -1;
            int16_t show_all_field = -1;
            // MORE is global for the screen, not per category: the user
            // is either browsing the most-used settings or everything.
            // It resets when the screen reopens (fresh state).
            bool show_all = false;
            int16_t tab_fields[CATEGORY_COUNT];
            int16_t row_fields[ROWS_PER_PAGE];
            // The derived sprites this screen renders with
            // (sprite_art), loaded once by Ensure_Sprites into
            // MOX::screen_seg. Null when the frame is missing;
            // the element falls back to a hidden field, so the
            // screen stays usable.
            bool sprites_loaded = false;
            // The widened sort-tab frames (the colony's sort-tab
            // frames stretched by repeating a flat middle column,
            // pixel-exact colonies art): 160 px for Internals
            // (sort-population-wide), 104 px for Gameplay
            // (sort-population-mid), 80 px for Mods
            // (sort-food-wide). The colony's switcher sizes each
            // tab to its label, so the tabs read as a switcher
            // instead of three identical wide buttons.
            s_animation_header* tab_sprite_normal = nullptr;
            s_animation_header* tab_sprite_active = nullptr;
            s_animation_header* tab_sprite_mid_normal = nullptr;
            s_animation_header* tab_sprite_mid_active = nullptr;
            s_animation_header* tab_sprite_small_normal = nullptr;
            s_animation_header* tab_sprite_small_active = nullptr;
            s_animation_header* return_sprite = nullptr;
            // The blank RETURN frame (the colony RETURN frame
            // with its baked "RETURN" label blanked): the
            // dropdown/edit value buttons draw on it, so the
            // baked label cannot bleed through the value text
            // ("Rhalf", "RETURN" for the value 1). The
            // bottom-bar RETURN keeps the labelled frame.
            s_animation_header* value_box_sprite = nullptr;
            // The widened value box (the blank RETURN frame
            // stretched to 104 px by repeating a flat middle
            // column): the row dropdown/edit value buttons draw
            // on it, so the widest value, "overview" (74 px),
            // fits instead of truncating to "overvi..." (Tom,
            // 2026-11-13). MORE keeps the 85 px frame: its zone
            // is 99 px and the wide frame would overflow it.
            s_animation_header* value_box_wide_sprite = nullptr;
            // The collapsed dropdown's own button (104x26, built
            // from scratch by tools/sprites/build_dropdown_closed.py,
            // not a reuse of the value box): the value-box face and
            // border language, so it matches its sibling edit
            // buttons, with a darker triangle zone at its right end
            // holding a header-white down-triangle (7x4, right edge
            // 4 px inside the frame, vertically centred -- the
            // Dropdown_Triangle_Rect geometry). The triangle has to
            // live in the button art: the field system redraws
            // button fields after the screen's auto-function, so a
            // triangle drawn at runtime is covered by the button
            // (proven by a solid blue rect over a button showing
            // only 111 of 2704 px). White, not black: a black
            // triangle sat on the frame's dark right-edge bevel
            // and was invisible, and the widest value ("overview")
            // leaves no room for a dark triangle on the light
            // face. An edit row draws on the plain wide box: it is
            // not a dropdown.
            s_animation_header* dropdown_closed_sprite = nullptr;
            // The open dropdown window's border: two bitmap strips
            // derived from the screen frame's own border (the
            // riveted, textured, chiseled outer frame), thinner
            // (5 px) than the screen border (~8 px) -- the same
            // border family, not a code-drawn chisel (Tom,
            // 2026-11-13). dropdown-border-h (256x10) carries the
            // top strip (rows 0..4) and the bottom strip (rows
            // 5..9); dropdown-border-v (10x256) the left strip
            // (cols 0..4) and the right strip (cols 5..9).
            s_animation_header* dropdown_border_h_sprite = nullptr;
            s_animation_header* dropdown_border_v_sprite = nullptr;
            s_animation_header* toggle_off = nullptr;
            s_animation_header* toggle_on = nullptr;
            s_animation_header* arrow_up = nullptr;
            s_animation_header* arrow_down = nullptr;
            // Per visible row: the value button field (a
            // dropdown/edit value, or a bool/mod toggle), or -1
            // when the row shows no value. The row's label is
            // row_fields (a string list); row_hit_fields is the
            // hidden field over the whole row, so the row stays
            // clickable outside the label and the value button.
            int16_t value_fields[ROWS_PER_PAGE];
            int16_t row_hit_fields[ROWS_PER_PAGE];
            // The row labels and value texts, kept alive for the
            // fields (which store pointers, not copies).
            std::vector<std::string> row_labels;
            std::vector<std::string> row_value_texts;
            // The string-list selection, shared by the page's row
            // fields: the visible index of the highlighted row.
            // Synced with `selected` (the description follows the
            // highlighted row, like the game's lists). The field
            // system's string-list value is int8_t; the settings
            // list is far below 127 rows.
            int8_t list_selection = 0;
            // The open dropdown's window: set while Pick_From_List
            // runs the field system's picker, so the auto-function
            // (Draw_Settings_Screen) can draw the window behind
            // the list. Cleared when the picker returns.
            bool dropdown_open = false;
            SETTINGS_LAYOUT::s_dropdown_window dropdown_window{};
            // The picker's live selection (the caller's index
            // variable, updated by the picker as the user moves):
            // the auto-function draws the selection bar behind
            // this row. Null when no dropdown is open.
            int8_t* dropdown_selected = nullptr;
            std::vector<config::s_resolution_option> resolution_options;
        };

        s_settings_screen_state g_state;

        // Layout (step 3 of the hi-res rewrite): every element
        // rect comes from the percentage-based layout
        // (settings_layout), computed for the current game
        // resolution. The same rects draw the placeholder frames
        // and take the clicks, so drawing and hit-testing agree.
        // The tabs sit in the bottom bar with MORE and RETURN:
        // the header is the first band, and the list takes the
        // space the old top tab bar used.
        SETTINGS_LAYOUT::s_settings_layout Layout() {
            int lengths[CATEGORY_COUNT];
            for (int c = 0; c < CATEGORY_COUNT; ++c) {
                lengths[c] = (int)strlen(CATEGORY_LABELS[c]);
            }
            return SETTINGS_LAYOUT::Compute_Layout(
                video::g_screen_res_x, video::g_screen_res_y,
                ROWS_PER_PAGE, lengths);
        }

        // Rows on the current tab. The Mods tab lists its settings
        // rows first (the two mod-adjacent settings) and then the
        // catalogue entries, so every row stays reachable.
        int Visible_Count() {
            if (g_state.category == CATEGORY_MODS) {
                return (int)g_state.visible_rows.size() + (int)g_state.mods.size();
            }
            return (int)g_state.visible_rows.size();
        }

        // True when a visible index on the Mods tab names a catalogue
        // entry rather than a settings row.
        bool Is_Mod_Row(int visible) {
            return g_state.category == CATEGORY_MODS &&
                visible >= (int)g_state.visible_rows.size();
        }

        // Catalogue index for a visible index on the Mods tab.
        int Mod_Index(int visible) {
            return visible - (int)g_state.visible_rows.size();
        }

        // Maximum first-visible-row index on the current tab:
        // the smallest first index whose settings to the end fit
        // in the page's row slots (a wrapped row takes two).
        int Max_Scroll() {
            return SETTINGS_LAYOUT::First_For_Last_Page(
                g_state.row_lines.data(), Visible_Count(),
                ROWS_PER_PAGE);
        }

        // Derive the current tab's first visible row from its
        // slider position, the kit scroll model (COLSUM's inverted
        // slider: the top row is the maximum minus the slider).
        // Clamps the slider into range (the field system also
        // clamps it, but the count can shrink under it when MORE
        // toggles or the category changes).
        void Sync_Scroll() {
            const int max_scroll = Max_Scroll();
            int16_t& slider = g_state.slider_position[g_state.category];
            // A fresh tab opens at the top: the first visit starts
            // the slider at its maximum (the first rows), and later
            // visits keep the position the user left. The bound
            // value stays inverted -- the field system's drag and
            // wheel need that -- only the starting position changes.
            if (!g_state.slider_initialised[g_state.category]) {
                slider = (int16_t)max_scroll;
                g_state.slider_initialised[g_state.category] = true;
            }
            if (slider < 0) {
                slider = 0;
            }
            if (slider > max_scroll) {
                slider = (int16_t)max_scroll;
            }
            // COLSUM's inverted slider: the top row is the
            // maximum minus the slider.
            g_state.scroll_top[g_state.category] = max_scroll - slider;
            // Focus follows the topmost item: if scrolling
            // (arrow, wheel, or drag -- all of them land here
            // through the slider) moved the selected row out
            // of view, the selection moves to the topmost
            // visible row, so the description shows the row
            // in view instead of one that scrolled away.
            if (Visible_Count() > 0) {
                const int first = g_state.scroll_top[g_state.category];
                if (g_state.selected < first ||
                    g_state.selected >= first + Page_Count()) {
                    g_state.selected = first;
                }
            }
        }

        // First visible row on the current tab (after Sync_Scroll).
        int Scroll_Top() {
            return g_state.scroll_top[g_state.category];
        }

        // Step the list by `lines` rows (positive = down, toward
        // the last rows): the slider is inverted against the top
        // row (top = max - slider), so scrolling down lowers it.
        // Clamped to the scroll range. The arrow buttons (4f)
        // step one row per click, like the colony's.
        void Scroll_Lines(int lines) {
            const int max_scroll = Max_Scroll();
            int16_t& slider = g_state.slider_position[g_state.category];
            int16_t next = (int16_t)(slider - lines);
            if (next < 0) {
                next = 0;
            }
            if (next > max_scroll) {
                next = (int16_t)max_scroll;
            }
            slider = next;
            Sync_Scroll();
        }

        // Rows index for a visible index in a settings category.
        int Visible_Row_Index(int visible) {
            return g_state.visible_rows[(size_t)visible];
        }

        std::string Join_Ids(const std::vector<std::string>& ids) {
            std::string joined;
            for (const std::string& id : ids) {
                if (!joined.empty()) {
                    joined += ", ";
                }
                joined += id;
            }
            return joined;
        }

        // The file the settings UI writes: the one config. When the user
        // has no
        // orion2re.cfg yet, the first write seeds it from the file the
        // game loaded (usually the shipped data file): creating a fresh
        // empty user file would shadow the shipped file's live settings
        // on the next reload. When there is no user config directory
        // (portable mode), the loaded file itself is the target: the loader
        // records its filesystem path on the active config at load time, so
        // the file the game loaded is the file saved to. The entry path
        // itself is a VFS key, not a file path, so it must never be used
        // as the target.
        void Resolve_Target() {
            g_state.target_ready = false;
            g_state.target_path.clear();
            g_state.seed_path.clear();
            const config::s_active_config* active = config::Get_Active_Config();
            if (active == nullptr) {
                g_state.status =
                    "Settings cannot be saved: the configuration is not initialized.";
                return;
            }
            const char* config_dir_raw =
                user_paths::Get_Directory(user_paths::USER_DIRECTORY_CONFIG);
            const std::string config_dir =
                config_dir_raw != nullptr ? config_dir_raw : "";
            if (!config_dir.empty()) {
                g_state.target_path = config_dir + "orion2re.cfg";
                if (std::filesystem::exists(g_state.target_path)) {
                    g_state.target_ready = true;
                    return;
                }
                if (!active->entry_filesystem_path.empty() &&
                    std::filesystem::is_regular_file(active->entry_filesystem_path)) {
                    g_state.seed_path = active->entry_filesystem_path;
                } else {
                    char resolved[4096] = {};
                    if (vfs::Resolve_Read_Path_No_Zip("orion2re.cfg", resolved,
                            sizeof(resolved)) != 0) {
                        g_state.seed_path = resolved;
                    } else if (!active->entry_path.empty() &&
                        vfs::Resolve_Read_Path_No_Zip(active->entry_path.c_str(),
                            resolved, sizeof(resolved)) != 0) {
                        g_state.seed_path = resolved;
                    }
                }
                g_state.target_ready = true;
                return;
            }
            // Portable mode: no user configuration directory. Save to the
            // file the game loaded.
            if (!active->entry_filesystem_path.empty() &&
                std::filesystem::is_regular_file(active->entry_filesystem_path)) {
                g_state.target_path = active->entry_filesystem_path;
                g_state.target_ready = true;
                return;
            }
            // No real file was loaded (defaults, or a zip-only entry): the
            // one config becomes a file next to the executable, seeded from
            // the loaded file when one can be resolved. Portable mode only
            // runs in a writable directory, and the VFS finds the
            // executable-directory file on the next load when no data file
            // shadows it.
            const char* base_path = SDL_GetBasePath();
            if (base_path != nullptr && base_path[0] != 0) {
                g_state.target_path = std::string(base_path) + "orion2re.cfg";
                if (!active->entry_filesystem_path.empty()) {
                    g_state.seed_path = active->entry_filesystem_path;
                } else {
                    char resolved[4096] = {};
                    if (vfs::Resolve_Read_Path_No_Zip("orion2re.cfg", resolved,
                            sizeof(resolved)) != 0) {
                        g_state.seed_path = resolved;
                    } else if (!active->entry_path.empty() &&
                        vfs::Resolve_Read_Path_No_Zip(active->entry_path.c_str(),
                            resolved, sizeof(resolved)) != 0) {
                        g_state.seed_path = resolved;
                    }
                }
                g_state.target_ready = true;
                return;
            }
            g_state.status =
                "Settings cannot be saved: no user configuration directory is available.";
        }

        // The seeded user config must be a proper readable file:
        // the writer copies the target's permissions onto each
        // rewrite, so a seeded copy inherits the source's mode --
        // a write-only (or read-only) source would poison every
        // later write. Normalize to 0644 (owner read/write,
        // group/others read) regardless of what the source's
        // mode was.
        bool Normalize_User_Config_Permissions(std::string* error) {
            std::error_code ec;
            std::filesystem::permissions(g_state.target_path,
                std::filesystem::perms::owner_read |
                    std::filesystem::perms::owner_write |
                    std::filesystem::perms::group_read |
                    std::filesystem::perms::others_read,
                std::filesystem::perm_options::replace, ec);
            if (ec) {
                *error = "could not set permissions on the configuration file";
                return false;
            }
            return true;
        }

        // Seeds the user config from the settings the game is
        // running with, when no file is available to copy: every
        // settings row's live value, plus the enabled mods. Never
        // an empty file -- an empty user file shadows the live
        // settings on the next reload (the loader reads the user
        // file first), so a toggle that "worked" would silently
        // drop every other setting.
        bool Write_Live_Settings_Seed(std::string* error) {
            std::string content =
                "# Orion2 Redone configuration.\n"
                "# Seeded from the settings the game is running with;\n"
                "# edit the values in the Settings screen.\n";
            for (const config::s_settings_row& row : g_state.rows) {
                if (row.variable_id == config::CONFIG_NO_VARIABLE) {
                    continue;
                }
                config::s_typed_value value;
                if (!config::Get_Settings_Row_Value(row, &value)) {
                    continue;
                }
                content += row.path + " = " +
                    config::Format_Leaf_Value_For_Config(row.variable_id, value) +
                    ";\n";
            }
            for (const std::string& id : g_state.enabled_ids) {
                content += "enable " + id + ";\n";
            }
            std::ofstream output(g_state.target_path,
                std::ios::binary | std::ios::trunc);
            if (!output) {
                *error = "could not create the configuration file";
                return false;
            }
            output.write(content.data(), (std::streamsize)content.size());
            output.flush();
            if (!output) {
                *error = "could not write the configuration file";
                return false;
            }
            return Normalize_User_Config_Permissions(error);
        }

        bool Ensure_User_Config(std::string* error) {
            if (std::filesystem::exists(g_state.target_path)) {
                return true;
            }
            std::error_code ec;
            const std::filesystem::path parent =
                std::filesystem::path(g_state.target_path).parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent, ec);
                if (ec) {
                    *error = "could not create the configuration directory";
                    return false;
                }
            }
            if (!g_state.seed_path.empty()) {
                std::filesystem::copy_file(g_state.seed_path, g_state.target_path, ec);
                if (ec) {
                    *error = "could not copy the loaded configuration file";
                    return false;
                }
                return Normalize_User_Config_Permissions(error);
            }
            // No file to copy from: seed from the live settings,
            // never an empty file (it would shadow the live
            // settings on the next reload).
            return Write_Live_Settings_Seed(error);
        }

        // The working enabled set: the `enable` lines of the file the
        // screen writes, falling back to the file the game loaded when
        // the target does not exist yet (first session, nothing seeded),
        // and to the active package order when neither file can be read.
        void Read_Enabled_Set() {
            g_state.enabled_ids.clear();
            config::s_cfg_diagnostic diagnostic;
            if (!g_state.target_path.empty() &&
                config::Read_Enabled_Mods(g_state.target_path,
                    &g_state.enabled_ids, &diagnostic)) {
                return;
            }
            const config::s_active_config* active = config::Get_Active_Config();
            if (active != nullptr && !active->entry_filesystem_path.empty() &&
                config::Read_Enabled_Mods(active->entry_filesystem_path,
                    &g_state.enabled_ids, &diagnostic)) {
                return;
            }
            if (active != nullptr) {
                g_state.enabled_ids = active->package_order;
            }
        }

        void Rebuild_Visible_Rows() {
            g_state.visible_rows.clear();
            const config::e_settings_category wanted =
                Settings_Category_For_Tab(g_state.category);
            for (size_t index = 0; index < g_state.rows.size(); ++index) {
                if (config::Settings_Category_For_Path(g_state.rows[index].path) !=
                    wanted) {
                    continue;
                }
                // Secondary settings stay hidden until MORE. The
                // catalogue on the Mods tab is not a setting row, so
                // it is never filtered.
                if (!g_state.show_all &&
                    !config::Settings_Is_Primary_For_Path(g_state.rows[index].path)) {
                    continue;
                }
                g_state.visible_rows.push_back((int)index);
            }
            Rebuild_Row_Lines();
        }

        void Refresh_Mods() {
            g_state.mods = config::Build_Mod_Catalogue(g_state.enabled_ids);
            if (g_state.selected >= Visible_Count()) {
                g_state.selected = Visible_Count() - 1;
            }
            if (g_state.selected < 0) {
                g_state.selected = 0;
            }
            Rebuild_Row_Lines();
        }

        e_apply_result Apply_Row(const config::s_settings_row& row,
            const std::string& config_text, const std::string& display_text) {
            if (!g_state.target_ready) {
                g_state.status = "Settings cannot be saved: no configuration file target.";
                return APPLY_FAILED;
            }
            std::string error;
            if (!Ensure_User_Config(&error)) {
                g_state.status = "Could not save '" + row.title + "': " + error + ".";
                return APPLY_FAILED;
            }
            config::s_cfg_diagnostic diagnostic;
            if (!config::Write_Setting(g_state.target_path,
                    config::Split_Setting_Path(row.path), config_text, &diagnostic)) {
                g_state.status = "Could not save '" + row.title + "': " +
                    diagnostic.message;
                return APPLY_FAILED;
            }
            // The screen runs from the main menu with no game loaded, so
            // every settings group reloads. Reload also follows the launch
            // config while no game is loaded, so the change survives
            // returning to the menu after a game.
            if (!config::Reload_Active_Config_Groups(false, &diagnostic)) {
                g_state.status = "Saved to orion2re.cfg, but the live reload failed (" +
                    diagnostic.message +
                    "). The new value takes effect after a restart.";
                return APPLY_SAVED;
            }
            if (row.restart_required) {
                g_state.status = "'" + row.title +
                    "' saved; restart the game for it to take effect.";
            } else {
                g_state.status = "'" + row.title + "' set to " + display_text + ".";
            }
            return APPLY_LIVE;
        }

        bool Is_Resolution_Row(const config::s_settings_row& row) {
            return row.path == config::kResolutionRowPath;
        }

        // The Resolution row writes both window settings together,
        // then reloads once, mirroring Apply_Row's save/reload/status
        // shape. The width is written first; a failed height write
        // after a saved width reports the partial save.
        e_apply_result Apply_Resolution(int width, int height,
            const std::string& display_text) {
            if (!g_state.target_ready) {
                g_state.status = "Settings cannot be saved: no configuration file target.";
                return APPLY_FAILED;
            }
            std::string error;
            if (!Ensure_User_Config(&error)) {
                g_state.status = "Could not save 'Resolution': " + error + ".";
                return APPLY_FAILED;
            }
            config::s_cfg_diagnostic diagnostic;
            if (!config::Write_Setting(g_state.target_path,
                    config::Split_Setting_Path("interface.window_width"),
                    std::to_string(width), &diagnostic)) {
                g_state.status = "Could not save 'Resolution': " + diagnostic.message;
                return APPLY_FAILED;
            }
            if (!config::Write_Setting(g_state.target_path,
                    config::Split_Setting_Path("interface.window_height"),
                    std::to_string(height), &diagnostic)) {
                g_state.status = "Saved the width, but could not save the height (" +
                    diagnostic.message + ").";
                return APPLY_SAVED;
            }
            if (!config::Reload_Active_Config_Groups(false, &diagnostic)) {
                g_state.status = "Saved to orion2re.cfg, but the live reload failed (" +
                    diagnostic.message +
                    "). The new value takes effect after a restart.";
                return APPLY_SAVED;
            }
            g_state.status = "'Resolution' set to " + display_text +
                "; restart the game for it to take effect.";
            return APPLY_LIVE;
        }

        // Rebuilds the category, row and navigation fields for the
        // current page. Only call when no edit field is open: the edit
        // field is added separately by Open_Edit and removed by
        // Commit/Cancel_Edit.
        // The derived sprites this screen renders with
        // (sprite_art): the colony sort-tab frames (blanked
        // labels) for the category tabs and MORE, the colony
        // RETURN frame for RETURN, the dropdown-closed button
        // for dropdown values and the blank RETURN frame
        // (value-box) for edit values, the colony buy-button
        // toggle frames for bool values and mods, and the
        // colony scroll arrows. Loaded once
        // into MOX::screen_seg; a missing frame leaves its
        // element on a hidden field (the screen stays usable),
        // never a null sprite.
        void Ensure_Sprites() {
            if (g_state.sprites_loaded) {
                return;
            }
            g_state.sprites_loaded = true;
            g_state.tab_sprite_normal = sprite_art::Load_Sprite_Frame(
                "sort-population-wide", "normal", MOX::screen_seg);
            g_state.tab_sprite_active = sprite_art::Load_Sprite_Frame(
                "sort-population-wide", "active", MOX::screen_seg);
            g_state.tab_sprite_mid_normal =
                sprite_art::Load_Sprite_Frame(
                    "sort-population-mid", "normal", MOX::screen_seg);
            g_state.tab_sprite_mid_active =
                sprite_art::Load_Sprite_Frame(
                    "sort-population-mid", "active", MOX::screen_seg);
            g_state.tab_sprite_small_normal =
                sprite_art::Load_Sprite_Frame(
                    "sort-food-wide", "normal", MOX::screen_seg);
            g_state.tab_sprite_small_active =
                sprite_art::Load_Sprite_Frame(
                    "sort-food-wide", "active", MOX::screen_seg);
            g_state.return_sprite = sprite_art::Load_Sprite_Frame(
                "return", "normal", MOX::screen_seg);
            g_state.value_box_sprite = sprite_art::Load_Sprite_Frame(
                "value-box", "normal", MOX::screen_seg);
            g_state.value_box_wide_sprite =
                sprite_art::Load_Sprite_Frame(
                    "value-box-wide", "normal", MOX::screen_seg);
            g_state.dropdown_closed_sprite =
                sprite_art::Load_Sprite_Frame(
                    "dropdown-closed", "normal", MOX::screen_seg);
            g_state.dropdown_border_h_sprite =
                sprite_art::Load_Sprite_Frame(
                    "dropdown-border-h", "normal", MOX::screen_seg);
            g_state.dropdown_border_v_sprite =
                sprite_art::Load_Sprite_Frame(
                    "dropdown-border-v", "normal", MOX::screen_seg);
            sprite_art::Buy_Button_Frames toggle =
                sprite_art::Load_Buy_Button_Frames(MOX::screen_seg);
            g_state.toggle_off = toggle.off;
            g_state.toggle_on = toggle.on;
            sprite_art::Scroll_Arrow_Frames arrows =
                sprite_art::Load_Scroll_Arrow_Frames(MOX::screen_seg);
            g_state.arrow_up = arrows.up;
            g_state.arrow_down = arrows.down;
        }

        // Truncates `text` to fit `max_width` in the current
        // font, with a trailing "..." when it does not fit.
        std::string Truncate_To_Width(const std::string& text,
            int16_t max_width) {
            if (max_width <= 0) {
                return std::string();
            }
            if (fonts::Get_String_Width(text.c_str()) <= max_width) {
                return text;
            }
            const int16_t ellipsis =
                fonts::Get_String_Width("...");
            std::string out;
            for (size_t i = 0; i < text.size(); ++i) {
                std::string next = out + text[i];
                if (fonts::Get_String_Width(next.c_str()) + ellipsis >
                    max_width) {
                    break;
                }
                out = next;
            }
            return out + "...";
        }

        void Rebuild_Fields() {
            // Fields-rendered screen: every interactive element
            // is a visible field type, drawn by
            // Draw_Visible_Fields -- the screen draws no button
            // frames, toggles, or list rows itself. The row's
            // label is a STRING_LIST field (the field system
            // draws the text and the selection highlight); the
            // value is a BUTTON field on a derived sprite (the
            // dropdown-closed button for a dropdown value, the
            // widened value-box frame for an edit value, the
            // buy-button toggle frames for a bool/mod); the tabs
            // are BUTTON fields on the sort-tab frames (active
            // for the current category); MORE and RETURN are
            // BUTTON fields; the scroll arrows are BUTTON fields
            // on the scroll-arrow frames; the scrollbar is the
            // field system's SCROLL field, as before. A field's
            // sprite is chosen at build time for the element's
            // current state, so a single-frame sprite carries
            // the state. Each row also keeps a hidden field over
            // its whole rect, so the row stays clickable outside
            // the label and the value button.
            Ensure_Sprites();
            fields::Clear_Fields();
            fonts::Set_Font_Style(4, &g_normal_colors);
            // The string-list colours: the row labels are the
            // sample's bright green (g_list_green_colors), both
            // unselected (list_normal_color) and selected
            // (list_highlight_colors, so the selected label stays
            // green on the blue selection bar). The gradient
            // colours (list_special_colors) still come from the
            // current special colour, so the selection bar itself
            // is unchanged. Get_List_Field sets these globals the
            // same way before it builds its rows. The BUTTON
            // fields keep g_normal_colors: they capture the
            // current font colour at creation, below.
            {
                s_colors* cur_special =
                    fonts::Get_Current_Special_Color();
                for (int i = 0; i < 8; i++) {
                    fields::list_normal_color.colors[i] =
                        g_list_green_colors.colors[i];
                    fields::list_special_colors.colors[i] =
                        cur_special->colors[i];
                    fields::list_highlight_colors.colors[i] =
                        g_list_green_colors.colors[i];
                }
            }
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            const int16_t pad = Text_Pad(layout.width);
            const int16_t font_height = fonts::Get_Font_Height();
            // A button field's font colour is a pointer to the
            // shared current-font-colour global (Add_Button_Field
            // captures Get_Current_Font_Color()), so its text
            // renders in whatever the previously drawn field left
            // there -- the selected row's string list leaves the
            // green highlight colour, and the selected row's value
            // button would render green. Pin a button's colour to
            // the council grey after creating it, so the button
            // labels stay grey whatever the string list did.
            auto pin_button_color = [](int16_t field_id) {
                if (field_id >= 0) {
                    fields::fields[field_id].font_color =
                        &g_normal_colors;
                }
            };
            auto pin_button_dark_color = [](int16_t field_id) {
                if (field_id >= 0) {
                    fields::fields[field_id].font_color =
                        &g_button_dark_colors;
                }
            };
            Sync_Scroll();
            // The selection follows the selected row, clamped
            // into the visible range.
            g_state.list_selection = (int8_t)std::clamp(
                g_state.selected, 0,
                std::max(0, Visible_Count() - 1));
            // Category tabs in the bottom bar: one button per
            // category on its widened sort-tab frame, the active
            // tab on its "active" frame. The label is the full
            // category name, centred by the field system. The
            // frame is sized to the label, like the colony's
            // switcher: Internals gets the 160 px
            // sort-population-wide frame, Gameplay the 104 px
            // sort-population-mid frame, Mods the 80 px
            // sort-food-wide frame. A missing frame falls back
            // to the wide one.
            for (int category = 0; category < CATEGORY_COUNT; ++category) {
                const SETTINGS_LAYOUT::s_rect tab = layout.tabs[category];
                s_animation_header* pic_normal = nullptr;
                s_animation_header* pic_active = nullptr;
                switch (category) {
                    case CATEGORY_GAMEPLAY:
                        pic_normal = g_state.tab_sprite_mid_normal;
                        pic_active = g_state.tab_sprite_mid_active;
                        break;
                    case CATEGORY_MODS:
                        pic_normal = g_state.tab_sprite_small_normal;
                        pic_active = g_state.tab_sprite_small_active;
                        break;
                    case CATEGORY_INTERNALS:
                    default:
                        pic_normal = g_state.tab_sprite_normal;
                        pic_active = g_state.tab_sprite_active;
                        break;
                }
                if (pic_normal == nullptr || pic_active == nullptr) {
                    pic_normal = g_state.tab_sprite_normal;
                    pic_active = g_state.tab_sprite_active;
                }
                s_animation_header* pic =
                    category == g_state.category ? pic_active : pic_normal;
                if (pic != nullptr) {
                    const int16_t w = animate::Get_Width(pic);
                    const int16_t h = animate::Get_Height(pic);
                    // The tab frames sit 4 px below the centred
                    // position: centred, they overlap the bottom
                    // strip's chiseled top edge (y 439-442); the
                    // colony's bar elements sit below that edge,
                    // on the outer frame's bottom.
                    g_state.tab_fields[category] =
                        fields::Add_Button_Field(
                            (int16_t)(tab.x + (tab.w - w) / 2),
                            (int16_t)(tab.y + (tab.h - h) / 2 + 4),
                            CATEGORY_LABELS[category], pic, "", 0);
                    pin_button_color(g_state.tab_fields[category]);
                } else {
                    g_state.tab_fields[category] =
                        fields::Add_Hidden_Field(tab.x, tab.y,
                            (int16_t)(tab.x + tab.w - 1),
                            (int16_t)(tab.y + tab.h - 1), "", 0);
                }
            }
            const int first = Scroll_Top();
            const int count = Page_Count();
            g_state.row_labels.assign((size_t)count, std::string());
            g_state.row_value_texts.assign((size_t)count, std::string());
            for (int index = 0; index < count; ++index) {
                const int visible = first + index;
                const SETTINGS_LAYOUT::s_rect row =
                    Visible_Row_Rect(index);
                const SETTINGS_LAYOUT::s_rect name_rect =
                    SETTINGS_LAYOUT::Row_Name_Rect_For_Row(layout, row);
                const SETTINGS_LAYOUT::s_rect value_rect =
                    Visible_Value_Rect(index);
                // The label: a string-list field, its text
                // centred vertically in the row. Truncated to
                // the name zone (one line; the fields system
                // does not wrap).
                g_state.row_labels[(size_t)index] =
                    Truncate_To_Width(Visible_Row_Name(visible),
                        (int16_t)(name_rect.w - 2 * pad));
                // The label's ink centres on the row's plate (the
                // row minus its bottom divider), not on the full
                // row slot -- the plate is what the eye reads as
                // the row. Falls back to the font cell when the
                // label has no ink.
                const SETTINGS_LAYOUT::s_rect plate =
                    SETTINGS_LAYOUT::Row_Plate_Rect_For_Row(layout, row);
                int16_t label_ink_top = -1;
                int16_t label_ink_bottom = -1;
                fonts::Get_String_Ink_Rows(
                    g_state.row_labels[(size_t)index].c_str(),
                    &label_ink_top, &label_ink_bottom);
                int16_t label_y = plate.y;
                if (label_ink_top >= 0 &&
                    label_ink_bottom >= label_ink_top) {
                    const int16_t ink_h = (int16_t)(
                        label_ink_bottom - label_ink_top + 1);
                    label_y = (int16_t)(plate.y +
                        (plate.h - ink_h) / 2 - label_ink_top);
                } else {
                    label_y = (int16_t)(plate.y +
                        (plate.h - font_height) / 2);
                }
                g_state.row_fields[index] =
                    fields::Add_String_List_Field(
                        (int16_t)(name_rect.x + pad), label_y,
                        (int16_t)(name_rect.w - 2 * pad),
                        g_state.row_labels[(size_t)index].c_str(),
                        1, &g_state.list_selection, (int16_t)visible,
                        17, nullptr, 0, nullptr, 0, "", nullptr);
                // The whole row stays clickable, outside the
                // label and the value button.
                g_state.row_hit_fields[index] =
                    fields::Add_Hidden_Field(row.x, row.y,
                        (int16_t)(row.x + row.w - 1),
                        (int16_t)(row.y + row.h - 1), "", 0);
                // The value: a button on a derived sprite.
                g_state.value_fields[index] = -1;
                s_animation_header* value_pic = nullptr;
                std::string value_text;
                bool show_value = false;
                bool is_dropdown = false;
                if (Is_Mod_Row(visible)) {
                    const config::s_mod_catalogue_entry& entry =
                        g_state.mods[(size_t)Mod_Index(visible)];
                    value_pic = (entry.enabled && !entry.broken)
                        ? g_state.toggle_on : g_state.toggle_off;
                    show_value = true;
                } else {
                    const config::s_settings_row& row_data =
                        g_state.rows[(size_t)Visible_Row_Index(visible)];
                    if (row_data.widget ==
                        config::SETTINGS_WIDGET_TOGGLE) {
                        config::s_typed_value value;
                        if (config::Get_Settings_Row_Value(row_data,
                            &value)) {
                            value_pic = value.bool_value
                                ? g_state.toggle_on : g_state.toggle_off;
                            show_value = true;
                        }
                    } else if (row_data.widget ==
                            config::SETTINGS_WIDGET_DROPDOWN ||
                        row_data.widget ==
                            config::SETTINGS_WIDGET_EDIT) {
                        // A dropdown draws on its own button
                        // (dropdown-closed, with the triangle zone
                        // designed in); an edit draws on the plain
                        // wide box. If the dropdown frame failed to
                        // load, fall back to the plain box (no
                        // triangle, still usable).
                        is_dropdown = (row_data.widget ==
                            config::SETTINGS_WIDGET_DROPDOWN);
                        value_pic = is_dropdown &&
                                g_state.dropdown_closed_sprite !=
                                    nullptr
                            ? g_state.dropdown_closed_sprite
                            : g_state.value_box_wide_sprite;
                        value_text = Row_Value_Text(row_data);
                        show_value = true;
                    }
                }
                if (show_value && value_pic != nullptr) {
                    const int16_t w = animate::Get_Width(value_pic);
                    const int16_t h = animate::Get_Height(value_pic);
                    const char* value_help = "";
                    if (!value_text.empty()) {
                        // A dropdown's value text must clear its
                        // triangle zone (the dropdown button's
                        // darker right-end panel holding the
                        // triangle), so it truncates narrower than
                        // an edit's: the centred text's right edge
                        // must clear the zone's left edge.
                        const int16_t max_w = is_dropdown
                            ? (int16_t)std::min<int32_t>(w - 2 * pad,
                                SETTINGS_LAYOUT::
                                    Dropdown_Value_Text_Width(w))
                            : (int16_t)(w - 2 * pad);
                        g_state.row_value_texts[(size_t)index] =
                            Truncate_To_Width(value_text, max_w);
                        value_help =
                            g_state.row_value_texts[(size_t)index].c_str();
                    }
                    g_state.value_fields[index] =
                        fields::Add_Button_Field(
                            (int16_t)(value_rect.x +
                                (value_rect.w - w) / 2),
                            (int16_t)(value_rect.y +
                                (value_rect.h - h) / 2),
                            value_help, value_pic, "", 0);
                    pin_button_color(g_state.value_fields[index]);
                } else if (show_value) {
                    // No sprite: the value zone stays clickable
                    // through a hidden field, like the row.
                    g_state.value_fields[index] =
                        fields::Add_Hidden_Field(value_rect.x,
                            value_rect.y,
                            (int16_t)(value_rect.x + value_rect.w - 1),
                            (int16_t)(value_rect.y + value_rect.h - 1),
                            "", 0);
                }
            }
            for (int index = count; index < ROWS_PER_PAGE; ++index) {
                g_state.row_fields[index] = -1;
                g_state.row_hit_fields[index] = -1;
                g_state.value_fields[index] = -1;
            }
            // Scrollbar drag field, bound to this tab's slider
            // position, like COLSUM's Add_Scroll_Field. Only when
            // the list actually scrolls.
            const int total = Visible_Count();
            if (Max_Scroll() > 0) {
                g_state.scroll_field = fields::Add_Scroll_Field(
                    layout.scrollbar_track.x, layout.scrollbar_track.y, 0,
                    (int16_t)total, 0, (int16_t)Max_Scroll(),
                    layout.scrollbar_track.w, layout.scrollbar_track.h,
                    &g_state.slider_position[g_state.category], "", 0);
            } else {
                g_state.scroll_field = -1;
            }
            // The scroll arrows are always there (the colony
            // shows its arrows even when there is nothing to
            // scroll); clicking one when the list does not scroll
            // is a clamped no-op.
            auto add_arrow = [&](const SETTINGS_LAYOUT::s_rect& zone,
                s_animation_header* pic) {
                if (pic == nullptr) {
                    return fields::Add_Hidden_Field(zone.x, zone.y,
                        (int16_t)(zone.x + zone.w - 1),
                        (int16_t)(zone.y + zone.h - 1), "", 0);
                }
                const int16_t w = animate::Get_Width(pic);
                const int16_t h = animate::Get_Height(pic);
                const int16_t field_id = fields::Add_Button_Field(
                    (int16_t)(zone.x + (zone.w - w) / 2),
                    (int16_t)(zone.y + (zone.h - h) / 2),
                    "", pic, "", 0);
                pin_button_color(field_id);
                return field_id;
            };
            g_state.scroll_up_field =
                add_arrow(layout.scrollbar_arrow_up, g_state.arrow_up);
            g_state.scroll_down_field =
                add_arrow(layout.scrollbar_arrow_down, g_state.arrow_down);
            // MORE and RETURN sit side by side in the bottom
            // bar at the right end.
            auto add_bar_button = [&](const SETTINGS_LAYOUT::s_rect& zone,
                const char* label, s_animation_header* pic) {
                if (pic == nullptr) {
                    return fields::Add_Hidden_Field(zone.x, zone.y,
                        (int16_t)(zone.x + zone.w - 1),
                        (int16_t)(zone.y + zone.h - 1), "", 0);
                }
                const int16_t w = animate::Get_Width(pic);
                const int16_t h = animate::Get_Height(pic);
                // The bar buttons sit 4 px below the centred
                // position: centred, they overlap the bottom
                // strip's chiseled top edge (y 439-442); the
                // colony's RETURN sits below that edge, its
                // text centred at y 457 and its bottom on the
                // outer frame (measured from the colony screen).
                const int16_t field_id = fields::Add_Button_Field(
                    (int16_t)(zone.x + (zone.w - w) / 2),
                    (int16_t)(zone.y + (zone.h - h) / 2 + 4),
                    label, pic, "", 0);
                pin_button_color(field_id);
                return field_id;
            };
            g_state.show_all_field = add_bar_button(layout.show_more,
                "MORE", g_state.value_box_sprite);
            pin_button_dark_color(g_state.show_all_field);
            // RETURN draws no label text: the RETURN frame's baked
            // dark "RETURN" is the label (like the colonies button),
            // and a drawn teal label would cover it.
            g_state.done_field = add_bar_button(layout.done,
                "", g_state.return_sprite);
        }

        // The field system's modal list picker for a dropdown:
        // `records` holds one 64-byte record per option (the
        // label as a C string at the record start), `selected`
        // the initially selected option. Sizes and places the
        // list under the row (or above it when there is no room
        // below), clamped inside the content rect. Returns the
        // chosen index, or -1. The picker clears the screen's
        // fields, so the fields are rebuilt after it returns.
        int16_t Pick_From_List(const std::string& title,
            const std::vector<std::array<char, 64>>& records,
            int8_t* selected, int visible) {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            const SETTINGS_LAYOUT::s_rect value_rect =
                Visible_Value_Rect(visible - Scroll_Top());
            const SETTINGS_LAYOUT::s_rect row_rect =
                Visible_Row_Rect(visible - Scroll_Top());
            fonts::Set_Font_Style(4, &g_normal_colors);
            const int16_t pad = Text_Pad(layout.width);
            int16_t list_w =
                (int16_t)fonts::Get_String_Width(title.c_str());
            for (const std::array<char, 64>& record : records) {
                list_w = (int16_t)std::max<int32_t>(list_w,
                    fonts::Get_String_Width(record.data()));
            }
            list_w = (int16_t)(list_w + 2 * pad);
            const int16_t content_right =
                SETTINGS_LAYOUT::Rect_Right(layout.content);
            const int16_t content_bottom =
                SETTINGS_LAYOUT::Rect_Bottom(layout.content);
            // The list sits under the value; when it would run
            // past the content's right edge it shifts left so
            // its right edge meets the content edge (right
            // aligned), keeping its full width -- shrinking it
            // would clip the title and the longest option. Only
            // when the list is wider than the content itself
            // does it fall back to the content's full width.
            int16_t list_x = value_rect.x;
            if (list_x + list_w > content_right) {
                list_x = (int16_t)(content_right - list_w);
            }
            if (list_x < layout.content.x) {
                list_x = layout.content.x;
                list_w = (int16_t)(content_right - list_x);
            }
            const int16_t step = (int16_t)(
                fonts::Get_Font_Height() +
                fonts::Get_Font_Vertical_Spacing());
            const int16_t list_h =
                (int16_t)((records.size() + 1) * step);
            int16_t list_y = SETTINGS_LAYOUT::Rect_Bottom(row_rect);
            if (list_y + list_h > content_bottom) {
                list_y = (int16_t)(row_rect.y - list_h);
            }
            if (list_y < layout.content.y) {
                list_y = layout.content.y;
            }
            // The open dropdown's window, for the auto-function
            // to draw behind the list while the picker runs.
            g_state.dropdown_open = true;
            g_state.dropdown_window =
                SETTINGS_LAYOUT::Dropdown_Window(layout.content,
                    list_x, list_y, list_w, list_h, step,
                    (int16_t)(records.size() + 1));
            g_state.dropdown_selected = selected;
            // Get_List_Field reads records until one starts with
            // NUL, so the list must be NUL-terminated: pass a copy
            // with an empty terminator record, or the picker reads
            // past the vector and shows phantom rows (garbage from
            // the heap). The window above is sized from the real
            // record count, not the terminator.
            std::vector<std::array<char, 64>> terminated = records;
            terminated.push_back({});
            // The picker's selected-row text draws in the font's
            // highlight colour; the default highlight is dark
            // enough to vanish on the window, so pin it to the
            // row-label green (readable on the blue selection
            // bar, like the main screen's selected row).
            fonts::Set_Font_Highlight_Colors(&g_list_green_colors);
            const int16_t choice = fields::Get_List_Field(list_x,
                list_y, list_w, title.c_str(),
                (const char*)terminated.data(), 64, selected,
                nullptr, 17, nullptr, 0, nullptr, 0, nullptr);
            g_state.dropdown_open = false;
            g_state.dropdown_selected = nullptr;
            Rebuild_Fields();
            return choice;
        }

        void Toggle_Show_All() {
            g_state.show_all = !g_state.show_all;
            g_state.selected = 0;
            Rebuild_Visible_Rows();
            // The row set changed, so the scroll resets to the top:
            // the slider at its maximum shows the first rows.
            g_state.slider_position[g_state.category] = (int16_t)Max_Scroll();
            g_state.slider_initialised[g_state.category] = true;
            g_state.scroll_top[g_state.category] = 0;
            g_state.status = g_state.show_all
                ? "Showing all settings, including advanced and ordering options."
                : "Showing the most-used settings. Use MORE for the rest.";
            Rebuild_Fields();
        }

        void Switch_Category(int category) {
            if (category == g_state.category) {
                return;
            }
            if (g_state.editing_row >= 0) {
                g_state.editing_row = -1;
                g_state.edit_field = -1;
            }
            g_state.category = category;
            Rebuild_Visible_Rows();
            if (category == CATEGORY_MODS) {
                Read_Enabled_Set();
                Refresh_Mods();
                g_state.status =
                    "Mod changes apply immediately. Click a mod to enable or disable it.";
            } else {
                g_state.status = "Click a setting to change it.";
            }
            // Sync after the Mods catalogue loads: the catalogue
            // changes the visible count, so the first-visit
            // initialisation must see the final count -- otherwise
            // the Mods tab initialises against the two settings
            // rows alone (max scroll 0) and opens at the bottom
            // once the catalogue lands.
            Sync_Scroll();
            // The category keeps its own scroll position (per-category slider
            // and top row); the selection moves to the first visible
            // row so the highlight stays on screen.
            g_state.selected = g_state.scroll_top[category];
            Rebuild_Fields();
        }

        void Cancel_Edit() {
            g_state.editing_row = -1;
            g_state.edit_field = -1;
            Rebuild_Fields();
        }

        bool Commit_Edit() {
            const int row_index = Visible_Row_Index(g_state.editing_row);
            const config::s_settings_row& row = g_state.rows[row_index];
            config::s_typed_value value;
            std::string config_text;
            std::string error;
            if (!config::Parse_Leaf_Edit_Text(row.variable_id, g_state.edit_buffer,
                    &value, &config_text, &error)) {
                // Keep the edit open so the value can be fixed.
                g_state.status = error;
                return false;
            }
            const std::string display = config::Format_Leaf_Value(row.variable_id, value);
            Apply_Row(row, config_text, display);
            g_state.editing_row = -1;
            g_state.edit_field = -1;
            Rebuild_Fields();
            return true;
        }

        void Open_Edit(int visible) {
            const int row_index = Visible_Row_Index(visible);
            const config::s_settings_row& row = g_state.rows[row_index];
            config::s_typed_value value;
            if (!config::Get_Settings_Row_Value(row, &value)) {
                g_state.status = "Could not read the current value of '" + row.title + "'.";
                return;
            }
            std::string text = config::Format_Leaf_Value(row.variable_id, value);
            if (text.size() >= sizeof(g_state.edit_buffer)) {
                text.resize(sizeof(g_state.edit_buffer) - 1);
            }
            strlcpy(g_state.edit_buffer, text.c_str(), sizeof(g_state.edit_buffer));
            const config::s_type_descriptor* type =
                config::Get_Variable_Type(row.variable_id);
            int16_t max_length = 20;
            if (type != nullptr && type->value_type == config::CONFIG_VALUE_STRING) {
                max_length = type->has_max_length
                    ? (int16_t)std::min<size_t>(type->max_length,
                        sizeof(g_state.edit_buffer) - 1)
                    : 64;
            }
            g_state.editing_row = visible;
            fonts::Set_Font_Style(4, &g_normal_colors);
            // The edit field is the row's value-control rect: the
            // same rect the placeholder draws, so the click that
            // opened the edit and the typing land in the same place.
            const SETTINGS_LAYOUT::s_rect value_rect =
                Visible_Value_Rect(visible - Scroll_Top());
            g_state.edit_field = fields::Add_Continuous_String_Input_Field(
                value_rect.x, value_rect.y, value_rect.w, value_rect.h,
                g_state.edit_buffer,
                max_length, 0, 0, 0, 0);
            // The row's full-width hidden field always wins Scan_Field
            // over the edit field added after it, so a click can never
            // focus the edit field through the field system: every
            // click in the value area cancels and reopens the edit
            // instead, and typing does nothing. Activate it directly,
            // mirroring the field system's own click activation for a
            // continuous input field (fields.cpp,
            // Interpret_Mouse_Input): the buffer becomes the working
            // text, the first Backspace clears it, and Enter copies it
            // back to the buffer for Commit_Edit.
            strlcpy(fields::continuous_string, g_state.edit_buffer,
                sizeof(fields::continuous_string));
            fields::input_field_active = 1;
            fields::active_input_field_first = 1;
            fields::cursor_position = 0;
            fields::cursor_on = 0;
            fields::active_input_field_number = g_state.edit_field;
        }

        // The popup's glyph-cell height: the same size as the
        // closed value text, so the open list reads as the same
        // control opened up.

        void Open_Dropdown(int visible) {
            const int row_index = Visible_Row_Index(visible);
            const config::s_settings_row& row = g_state.rows[row_index];
            const config::s_type_descriptor* type =
                config::Get_Variable_Type(row.variable_id);
            if (type == nullptr || type->values == nullptr || type->value_count == 0) {
                g_state.status = "'" + row.title + "' has no list of values.";
                return;
            }
            config::s_typed_value value;
            int8_t current_index = 0;
            if (config::Get_Settings_Row_Value(row, &value)) {
                for (size_t index = 0; index < type->value_count; ++index) {
                    if (type->values[index].value == value.integer_value) {
                        current_index = (int8_t)index;
                    }
                }
            }
            // Fixed-size records for the field system's list
            // picker: one 64-byte record per option, the label
            // as a C string at the record start.
            std::vector<std::array<char, 64>> records(
                type->value_count);
            for (size_t index = 0; index < type->value_count; ++index) {
                strlcpy(records[index].data(),
                    type->values[index].name, 64);
            }
            const int16_t choice = Pick_From_List(row.title, records,
                &current_index, visible);
            if (choice < 0) {
                return;
            }
            config::s_typed_value chosen;
            chosen.type = type->value_type;
            chosen.integer_value = type->values[choice].value;
            Apply_Row(row,
                config::Format_Leaf_Value_For_Config(row.variable_id, chosen),
                type->values[choice].name);
        }

        // The Resolution dropdown opens as the field system's
        // own modal list (Get_List_Field): the values as rows,
        // the field system drawing the selection highlight.
        void Open_Resolution_Popup(int visible) {
            int current_width = 0;
            int current_height = 0;
            if (!config::Get_Current_Resolution(&current_width, &current_height)) {
                g_state.status = "Could not read the current window size.";
                return;
            }
            g_state.resolution_options =
                config::Resolution_Options(current_width, current_height);
            int8_t selected = 0;
            std::vector<std::array<char, 64>> records(
                g_state.resolution_options.size());
            for (size_t index = 0;
                index < g_state.resolution_options.size(); ++index) {
                if (g_state.resolution_options[index].width == current_width &&
                    g_state.resolution_options[index].height == current_height) {
                    selected = (int8_t)index;
                }
                strlcpy(records[index].data(),
                    g_state.resolution_options[index].label.c_str(), 64);
            }
            const config::s_settings_row& row =
                g_state.rows[(size_t)Visible_Row_Index(visible)];
            const int16_t choice = Pick_From_List(row.title, records,
                &selected, visible);
            if (choice < 0) {
                return;
            }
            const config::s_resolution_option& option =
                g_state.resolution_options[(size_t)choice];
            Apply_Resolution(option.width, option.height, option.label);
        }

        void Handle_Row_Click(int visible) {
            g_state.selected = visible;
            const int row_index = Visible_Row_Index(visible);
            const config::s_settings_row& row = g_state.rows[row_index];
            switch (row.widget) {
                case config::SETTINGS_WIDGET_TOGGLE: {
                    config::s_typed_value value;
                    if (!config::Get_Settings_Row_Value(row, &value)) {
                        g_state.status = "Could not read the current value of '" +
                            row.title + "'.";
                        return;
                    }
                    const std::string next = value.bool_value ? "false" : "true";
                    Apply_Row(row, next, next);
                    return;
                }
                case config::SETTINGS_WIDGET_DROPDOWN:
                    if (Is_Resolution_Row(row)) {
                        Open_Resolution_Popup(visible);
                    } else {
                        Open_Dropdown(visible);
                    }
                    return;
                case config::SETTINGS_WIDGET_EDIT:
                    Open_Edit(visible);
                    return;
                case config::SETTINGS_WIDGET_INFO:
                default:
                    g_state.status = row.info;
                    return;
            }
        }

        // A mod toggle: dry-run the candidate set through the staging
        // path, then write the resolved order to the config file and
        // apply it with a full load. The file is written before the
        // apply, so a failed apply still leaves the picked set as the
        // set the next start loads. Nothing is written when the
        // preview fails.
        // `mod_index` is the catalogue index, not the visible row:
        // the Mods tab lists its settings rows first.
        void Handle_Mod_Click(int mod_index) {
            g_state.selected = (int)g_state.visible_rows.size() + mod_index;
            // Copy the id out: Refresh_Mods() below replaces the
            // catalogue vector, so a reference into it would dangle.
            const config::s_mod_catalogue_entry entry = g_state.mods[(size_t)mod_index];
            if (entry.broken) {
                g_state.status = "'" + entry.id + "' is broken: " + entry.reason;
                return;
            }
            if (!g_state.target_ready) {
                g_state.status = "Mods cannot be saved: no configuration file target.";
                return;
            }
            const std::vector<std::string> previous_enabled = g_state.enabled_ids;
            const auto found = std::find(previous_enabled.begin(),
                previous_enabled.end(), entry.id);
            const bool enabling = found == previous_enabled.end();
            std::vector<std::string> candidate;
            if (enabling) {
                // Newly enabled mods append after the order already in
                // the file; the resolver orders the written set.
                candidate = previous_enabled;
                candidate.push_back(entry.id);
            } else {
                // A dependency chain is a unit: disabling a mod also
                // disables its dependents and any dependency nothing
                // remaining needs (see Prune_Disabled_Mod_Set), so no
                // partial chain is left behind.
                candidate = config::Prune_Disabled_Mod_Set(g_state.mods,
                    previous_enabled, entry.id);
            }
            config::s_cfg_diagnostic diagnostic;
            config::s_mod_set_preview preview;
            if (!config::Preview_Enabled_Mods(candidate, &preview, &diagnostic)) {
                g_state.status = "Cannot " + std::string(enabling ? "enable '" : "disable '") +
                    entry.id + "': " + diagnostic.message;
                return;
            }
            std::string error;
            if (!Ensure_User_Config(&error)) {
                g_state.status = "Could not save mods: " + error + ".";
                return;
            }
            if (!config::Write_Enabled_Mods(g_state.target_path,
                    preview.package_order, &diagnostic)) {
                g_state.status = "Could not save mods: " + diagnostic.message;
                return;
            }
            g_state.enabled_ids = preview.package_order;
            Refresh_Mods();
            if (!config::Apply_Enabled_Mods(candidate, &diagnostic)) {
                g_state.status = "Saved to orion2re.cfg, but the live apply failed (" +
                    diagnostic.message +
                    "). The new set takes effect after a restart.";
                return;
            }
            const std::string active = preview.package_order.empty()
                ? "no mods"
                : Join_Ids(preview.package_order);
            std::string extra;
            if (enabling) {
                // Dependencies the resolver pulled in alongside the mod.
                std::vector<std::string> pulled;
                for (const std::string& id : preview.package_order) {
                    if (std::find(candidate.begin(), candidate.end(), id) ==
                        candidate.end()) {
                        pulled.push_back(id);
                    }
                }
                if (!pulled.empty()) {
                    extra = ". Also enabled: " + Join_Ids(pulled);
                }
            } else {
                // Chain members the disable took with it.
                std::vector<std::string> removed;
                for (const std::string& id : previous_enabled) {
                    if (id != entry.id &&
                        std::find(candidate.begin(), candidate.end(), id) ==
                            candidate.end()) {
                        removed.push_back(id);
                    }
                }
                if (!removed.empty()) {
                    extra = ". Also disabled: " + Join_Ids(removed);
                }
            }
            g_state.status = "'" + entry.id + "' " +
                (enabling ? "enabled" : "disabled") + extra +
                ". Active mods: " + active + ".";
        }

        // Horizontal text padding inside a rect, scaled with the screen.
        int16_t Text_Pad(int16_t screen_width) {
            return (int16_t)std::max<int32_t>(4, screen_width / 100);
        }

        // The value text of a settings row, for the rows that show
        // one (dropdowns and edits; info rows show none, toggles
        // show the switch in step 4e). Empty when the value cannot
        // be read.
        std::string Row_Value_Text(const config::s_settings_row& row) {
            if (Is_Resolution_Row(row)) {
                int width = 0;
                int height = 0;
                if (!config::Get_Current_Resolution(&width, &height)) {
                    return std::string();
                }
                return config::Format_Resolution(width, height);
            }
            config::s_typed_value value;
            if (!config::Get_Settings_Row_Value(row, &value)) {
                return std::string();
            }
            return config::Format_Leaf_Value(row.variable_id, value);
        }

        // The name shown for a visible row: a setting's title, or
        // a mod's name (its id when the manifest names nothing).
        std::string Visible_Row_Name(int visible) {
            if (Is_Mod_Row(visible)) {
                const config::s_mod_catalogue_entry& entry =
                    g_state.mods[(size_t)Mod_Index(visible)];
                return entry.name.empty() ? entry.id : entry.name;
            }
            return g_state.rows[(size_t)Visible_Row_Index(visible)].title;
        }

        // The description shown for a visible row in the info box:
        // a setting's metadata description, or a mod's description
        // (its id when the manifest names nothing). `restart_required`
        // is set for a settings row whose change takes effect only
        // after a restart; mods never need one.
        std::string Visible_Row_Description(int visible, bool* restart_required) {
            *restart_required = false;
            if (Is_Mod_Row(visible)) {
                const config::s_mod_catalogue_entry& entry =
                    g_state.mods[(size_t)Mod_Index(visible)];
                return entry.description.empty() ? entry.id : entry.description;
            }
            const config::s_settings_row& row =
                g_state.rows[(size_t)Visible_Row_Index(visible)];
            *restart_required = row.restart_required;
            return row.description;
        }

        // Lines each visible row takes (1, or 2 for a row whose
        // label wraps onto a second line). Rebuilt with the
        // visible rows: the wrap is measured at the full row
        // cell, the same measure the draw uses, so the cache and
        // the draw always agree.
        // Lines each visible row takes: 1. The fields system
        // draws one line per row (STRING_LIST has no wrap), so
        // a long label is truncated to the name zone, never
        // wrapped onto a second slot.
        void Rebuild_Row_Lines() {
            g_state.row_lines.assign((size_t)Visible_Count(), 1);
        }

        // Cached lines for a visible index (1 when unknown).
        int Row_Lines(int visible) {
            if (visible < 0 ||
                visible >= (int)g_state.row_lines.size()) {
                return 1;
            }
            return g_state.row_lines[(size_t)visible];
        }

        // Settings visible on the current page: the page is
        // ROWS_PER_PAGE slots, and a wrapped row takes two.
        int Page_Count() {
            return SETTINGS_LAYOUT::Settings_On_Page(
                g_state.row_lines.data(), Visible_Count(),
                Scroll_Top(), ROWS_PER_PAGE);
        }

        // First slot of the setting at visible offset `offset`
        // on the current page: the slots the earlier settings
        // on the page take.
        int Visible_Slot(int offset) {
            const int first = Scroll_Top();
            int slot = 0;
            for (int i = 0; i < offset; ++i) {
                slot += Row_Lines(first + i);
            }
            return slot;
        }

        // The row rect for the setting at visible offset
        // `offset` on the current page: its slots merged, so a
        // wrapped label's row is two slots tall.
        SETTINGS_LAYOUT::s_rect Visible_Row_Rect(int offset) {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            const int first = Scroll_Top();
            return SETTINGS_LAYOUT::Row_Rect_For_Span(layout,
                Visible_Slot(offset), Row_Lines(first + offset));
        }

        // The value-control rect for the setting at visible
        // offset `offset`: vertically centred in its (possibly
        // two-slot) row.
        SETTINGS_LAYOUT::s_rect Visible_Value_Rect(int offset) {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            return SETTINGS_LAYOUT::Row_Value_Rect_For_Row(layout,
                Visible_Row_Rect(offset));
        }

        // Total lines in the list, first visible line, and lines
        // on the current page (for the scrollbar thumb).
        int Total_Lines() {
            return SETTINGS_LAYOUT::Lines_Before(
                g_state.row_lines.data(), Visible_Count(),
                Visible_Count());
        }

        int First_Line() {
            return SETTINGS_LAYOUT::Lines_Before(
                g_state.row_lines.data(), Visible_Count(),
                Scroll_Top());
        }

        int Visible_Lines() {
            return SETTINGS_LAYOUT::Lines_On_Page(
                g_state.row_lines.data(), Visible_Count(),
                Scroll_Top(), ROWS_PER_PAGE);
        }

        // The open dropdown popup (a generic dropdown, or
        // Resolution): the list over the screen, in the shape of
        // the fleet-selection popup -- a small window, the values
        // as rows, a blue bar on the selected one. The draw was
        // lost in the step-1 strip (the pixel-era
        // Draw_Resolution_Popup went with ui_kit) and never
        // restored, so the Resolution popup opened invisible;
        // this restores it in hi-res, for both dropdowns.


        // The text the fields system does not draw: the header
        // labels, the selected row's description and the status
        // line, and the scrollbar thumb. All in the game's own
        // font (fonts::Print), like the game's screens draw
        // their text -- no HD-font rendering, no custom
        // primitives. The button frames, toggles, list rows and
        // the edit text come from Draw_Visible_Fields.
        void Draw_Centred_Text(const char* text,
            const SETTINGS_LAYOUT::s_rect& rect) {
            const int16_t w = fonts::Get_String_Width(text);
            // Centre the visible glyphs, not the font cell: a
            // glyph's cell is taller than its ink (capitals sit in
            // the top rows with empty rows below), so centring by
            // Get_Font_Height() puts the text high in its plate.
            // Falls back to the cell when the string has no ink.
            int16_t ink_top = -1;
            int16_t ink_bottom = -1;
            fonts::Get_String_Ink_Rows(text, &ink_top, &ink_bottom);
            int16_t y = rect.y;
            if (ink_top >= 0 && ink_bottom >= ink_top) {
                const int16_t ink_h =
                    (int16_t)(ink_bottom - ink_top + 1);
                y = (int16_t)(rect.y + (rect.h - ink_h) / 2 - ink_top);
            } else {
                const int16_t h = fonts::Get_Font_Height();
                y = (int16_t)(rect.y + (rect.h - h) / 2);
            }
            fonts::Print(
                (int16_t)(rect.x + (rect.w - w) / 2), y, text);
        }

        void Draw_Description() {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            const int16_t pad = Text_Pad(layout.width);
            const SETTINGS_LAYOUT::s_rect band = layout.description;
            const int16_t x = (int16_t)(band.x + pad);
            const int16_t max_w = (int16_t)(band.w - 2 * pad);
            const int16_t line_h =
                (int16_t)(fonts::Get_Font_Height() + 2);
            bool restart_required = false;
            std::string description;
            if (Visible_Count() > 0 && g_state.selected >= 0 &&
                g_state.selected < Visible_Count()) {
                description = Visible_Row_Description(
                    g_state.selected, &restart_required);
            }
            // Word-wrap the description to the band width.
            std::vector<std::string> wrapped;
            {
                std::string current;
                size_t pos = 0;
                while (pos <= description.size()) {
                    size_t end = description.find(' ', pos);
                    if (end == std::string::npos) {
                        end = description.size();
                    }
                    std::string word =
                        description.substr(pos, end - pos);
                    std::string next =
                        current.empty() ? word : current + " " + word;
                    if (!current.empty() &&
                        fonts::Get_String_Width(next.c_str()) > max_w) {
                        wrapped.push_back(current);
                        current = word;
                    } else {
                        current = next;
                    }
                    if (end == description.size()) {
                        break;
                    }
                    pos = end + 1;
                }
                if (!current.empty()) {
                    wrapped.push_back(current);
                }
            }
            // Lines the description may take: the band, less the
            // status line and the restart note (when one applies).
            const int restart_lh = restart_required ? line_h : 0;
            const int max_lines = std::max(1,
                (band.h - line_h - restart_lh) / line_h);
            int16_t y = band.y;
            for (size_t i = 0; i < wrapped.size() &&
                (int)i < max_lines; ++i) {
                fonts::Print(x, y, wrapped[i].c_str());
                y = (int16_t)(y + line_h);
            }
            if (restart_required) {
                fonts::Print(x, y, "Requires restart.");
                y = (int16_t)(y + line_h);
            }
            fonts::Print(x, (int16_t)(band.y + band.h - line_h),
                Truncate_To_Width(g_state.status, max_w).c_str());
        }

        void Draw_Scroll_Thumb() {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            const SETTINGS_LAYOUT::s_rect thumb =
                SETTINGS_LAYOUT::Scrollbar_Thumb_Rect(layout,
                    Total_Lines(), First_Line(), Visible_Lines());
            if (thumb.w <= 0 || thumb.h <= 0) {
                return;
            }
            // The colony's indicator bar (colsum.cpp
            // Draw_Bar_Indicator): a solid face with shaded edges
            // -- light top/left, dark right/bottom -- and black
            // corner dots at the track's ends. The colony indices
            // are colony-palette indices (its face 229 is vivid
            // blue (28,28,164) there); this screen runs on the
            // menu palette, where 229 is near-black, so the
            // indices below are the menu palette's nearest
            // matches, keeping the colony's lightness steps
            // (light edge 99, face 79, dark edge 36): face 138
            // renders (52,48,136), light edge 134 renders
            // (88,80,128), dark edge 103 renders (24,32,52).
            graphics::Fill(thumb.x, thumb.y,
                (int16_t)(thumb.x + thumb.w - 1),
                (int16_t)(thumb.y + thumb.h - 1), 138);
            line::Line(thumb.x, thumb.y,
                (int16_t)(thumb.x + thumb.w - 1), thumb.y, 134);
            line::Line(thumb.x, thumb.y, thumb.x,
                (int16_t)(thumb.y + thumb.h - 1), 134);
            line::Line((int16_t)(thumb.x + thumb.w - 1), thumb.y,
                (int16_t)(thumb.x + thumb.w - 1),
                (int16_t)(thumb.y + thumb.h - 1), 103);
            line::Line(thumb.x, (int16_t)(thumb.y + thumb.h - 1),
                (int16_t)(thumb.x + thumb.w - 1),
                (int16_t)(thumb.y + thumb.h - 1), 103);
            // Corner dots at the track's ends, at the thumb's
            // left/right edges (the colony's Dot(621/626, 40/311,
            // 80)); the colony dot colour is black, so the dots
            // are black here too (invisible on the black track,
            // as in the colony).
            const SETTINGS_LAYOUT::s_rect track = layout.scrollbar_track;
            graphics::Dot(thumb.x, track.y, 0);
            graphics::Dot((int16_t)(thumb.x + thumb.w - 1), track.y, 0);
            graphics::Dot(thumb.x, (int16_t)(track.y + track.h - 1), 0);
            graphics::Dot((int16_t)(thumb.x + thumb.w - 1),
                (int16_t)(track.y + track.h - 1), 0);
        }

        void Draw_Dropdown_Window() {
            if (!g_state.dropdown_open) {
                return;
            }
            const SETTINGS_LAYOUT::s_dropdown_window& w =
                g_state.dropdown_window;
            const int16_t x0 = w.window.x;
            const int16_t y0 = w.window.y;
            const int16_t x1 =
                (int16_t)(w.window.x + w.window.w - 1);
            const int16_t y1 =
                (int16_t)(w.window.y + w.window.h - 1);
            // Opaque fill: the rows behind the list must not
            // show through.
            graphics::Fill(x0, y0, x1, y1, 0);
            // Bitmap border: the screen frame's own border,
            // thinner (5 px). The horizontal strips tile across
            // the top/bottom between the vertical strips (which
            // own the corners and span the full height); the
            // vertical strips tile down the left/right. The
            // strips are 256 px long, so a normal window needs
            // one tile per edge. When the border sprites fail to
            // load (an old base.zip), fall back to the thin
            // chiseled border, so the window still reads as a
            // window.
            const int16_t b = SETTINGS_LAYOUT::kDropdown_Border;
            const s_animation_header* h_border =
                g_state.dropdown_border_h_sprite;
            const s_animation_header* v_border =
                g_state.dropdown_border_v_sprite;
            if (h_border == nullptr || v_border == nullptr) {
                line::Line(x0, y0, x1, y0, 134);
                line::Line(x0, y0, x0, y1, 134);
                line::Line(x1, y0, x1, y1, 103);
                line::Line(x0, y1, x1, y1, 103);
                return;
            }
            // Top: rows 0..b-1 of border-h, tiled across
            // x0+b..x1-b. Bottom: rows b..2b-1, same span.
            for (int16_t x = (int16_t)(x0 + b); x <= (int16_t)(x1 - b);) {
                const int16_t w = (int16_t)std::min<int16_t>(
                    h_border->width, (int16_t)(x1 - b - x + 1));
                sprite_art::Draw_Sprite_Frame_Region(
                    h_border, 0, 0, w, b, x, y0);
                sprite_art::Draw_Sprite_Frame_Region(
                    h_border, 0, b, w, b, x, (int16_t)(y1 - b + 1));
                x = (int16_t)(x + w);
            }
            // Left: cols 0..b-1 of border-v, tiled down y0..y1.
            // Right: cols b..2b-1, same span.
            for (int16_t y = y0; y <= y1;) {
                const int16_t hh = (int16_t)std::min<int16_t>(
                    v_border->height, (int16_t)(y1 - y + 1));
                sprite_art::Draw_Sprite_Frame_Region(
                    v_border, 0, 0, b, hh, x0, y);
                sprite_art::Draw_Sprite_Frame_Region(
                    v_border, b, 0, b, hh, (int16_t)(x1 - b + 1), y);
                y = (int16_t)(y + hh);
            }
        }

        // The picker's selection bar: Gradient_Fill mode 17
        // (the field system's selection gradient) is not
        // implemented in this port -- it draws nothing -- so the
        // bar is drawn here, behind the list items and inside
        // the separators, in the thumb's dusty blue (menu
        // palette 138). The selected index is the picker's live
        // variable, so the bar follows arrow-key / mouse moves.
        void Draw_Dropdown_Selection() {
            if (!g_state.dropdown_open ||
                g_state.dropdown_selected == nullptr) {
                return;
            }
            const SETTINGS_LAYOUT::s_dropdown_window& w =
                g_state.dropdown_window;
            const int16_t sel = (int16_t)*g_state.dropdown_selected;
            if (sel < 0 || sel >= w.rows - 1) {
                return;
            }
            // Item sel is row sel+1 (row 0 is the title): it sits
            // between separator sel+1 and separator sel+2.
            const int16_t y0 =
                SETTINGS_LAYOUT::Dropdown_Separator_Y(w, (int16_t)(sel + 1));
            const int16_t y1 = (sel + 2 < w.rows)
                ? (int16_t)(SETTINGS_LAYOUT::Dropdown_Separator_Y(w,
                      (int16_t)(sel + 2)) - 1)
                : (int16_t)(w.window.y + w.window.h - 1 -
                      SETTINGS_LAYOUT::kDropdown_Border);
            // Inside the bitmap border (kDropdown_Border), not the
            // old 1 px chisel.
            graphics::Fill(
                (int16_t)(w.window.x + SETTINGS_LAYOUT::kDropdown_Border),
                y0,
                (int16_t)(w.window.x + w.window.w - 1 -
                    SETTINGS_LAYOUT::kDropdown_Border),
                y1, 138);
        }

        void Draw_Dropdown_Separators() {
            if (!g_state.dropdown_open) {
                return;
            }
            const SETTINGS_LAYOUT::s_dropdown_window& w =
                g_state.dropdown_window;
            const int16_t x0 = w.window.x;
            const int16_t x1 =
                (int16_t)(w.window.x + w.window.w - 1);
            // One separator between every two rows: a 2 px
            // chiseled groove (light over dark) on the row
            // boundary, inside the bitmap border. Drawn after the
            // fields, so the separators stay visible over the
            // selection highlight, framing the selected row.
            const int16_t b = SETTINGS_LAYOUT::kDropdown_Border;
            for (int k = 1; k < w.rows; ++k) {
                const int16_t sy =
                    SETTINGS_LAYOUT::Dropdown_Separator_Y(w, k);
                line::Line((int16_t)(x0 + b), (int16_t)(sy - 1),
                    (int16_t)(x1 - b), (int16_t)(sy - 1), 134);
                line::Line((int16_t)(x0 + b), sy,
                    (int16_t)(x1 - b), sy, 103);
            }
        }

        void Draw_Text_Elements() {
            const SETTINGS_LAYOUT::s_settings_layout layout = Layout();
            // The header labels are the colonies header font
            // (style 3: bold, chunky, compact -- the colonies
            // NAME/FARMERS plates are baked in that font, and
            // style 4 is taller and more elegant). The white
            // colour stays: the header is white on the dark
            // plates, like the colonies.
            fonts::Set_Font_Style(3, &g_header_white_colors);
            Draw_Centred_Text("SETTING", layout.header_setting);
            Draw_Centred_Text("VALUE", layout.header_value);
            fonts::Set_Font_Style(4, &g_normal_colors);
            // While a dropdown is open the description and the
            // scroll thumb stay hidden: both are drawn after the
            // window, so a list overlapping the description band
            // (or the scrollbar) would have their text painted
            // over it.
            if (!g_state.dropdown_open) {
                Draw_Description();
                Draw_Scroll_Thumb();
            }
            Draw_Dropdown_Separators();
        }

        void Draw_Settings_Screen() {
            // Sync the scroll position from the slider at draw
            // time, like COLSUM::Draw_Bar_Indicator calls
            // Update_First: the field system moves the slider
            // on a wheel/drag, and the top row derives from it
            // here, so the list follows the slider without
            // waiting for a rebuild.
            Sync_Scroll();
            video::Set_Page_Off();
            SETTINGS_BACKGROUND::Draw_Background();
            Draw_Dropdown_Window();
            Draw_Dropdown_Selection();
            fields::Draw_Visible_Fields();
            Draw_Text_Elements();
        }

    } // namespace

    void __cdecl Settings_Screen() {
        g_state = s_settings_screen_state{};
        g_state.rows = config::Build_Settings_Rows();
        if (g_state.rows.empty()) {
            // Nothing to show (the schema did not load); return to the
            // menu rather than drawing an empty screen.
            MOX::current_screen = SCREEN_MAIN_MENU;
            return;
        }
        g_state.status = "Click a setting to change it.";
        Resolve_Target();
        Read_Enabled_Set();
        Rebuild_Visible_Rows();

        video::Set_Refresh_Full();
        SETTINGS_BACKGROUND::Install_Background();
        fields::Clear_Fields();
        RUSS::Mox_Update();
        fonts::Set_Font_Style(4, &g_normal_colors);
        fonts::Set_Font_LF(4);
        Rebuild_Fields();
        fields::Assign_Auto_Function(Draw_Settings_Screen, 2);

        bool screen_exit = false;
        while (!screen_exit) {
            timer::Mark_Time();
            const int16_t input = fields::Get_Input();
            // The string-list highlight follows the hover (the
            // field system sets list_selection in Scan_Input);
            // the description follows the highlighted row, like
            // the game's lists. Not while editing: the edit
            // stays on its row.
            if (g_state.editing_row < 0 &&
                g_state.list_selection >= 0 &&
                g_state.list_selection < Visible_Count()) {
                g_state.selected = g_state.list_selection;
            }
            if (input == -1) {
                // Esc: cancel a pending edit, otherwise close the screen.
                if (g_state.editing_row >= 0) {
                    Cancel_Edit();
                } else {
                    screen_exit = true;
                }
            } else if (input != 0) {
                // Anything other than the edit field or RETURN cancels a
                // pending edit (RETURN commits it instead). The cancel
                // rebuilds the field set, but the row and navigation
                // field ids are deterministic — the edit field is always
                // added after them, so the input id still matches below.
                if (g_state.editing_row >= 0 && input != g_state.edit_field &&
                    input != g_state.done_field) {
                    Cancel_Edit();
                }
                if (input == g_state.done_field) {
                    if (g_state.editing_row >= 0) {
                        if (Commit_Edit()) {
                            screen_exit = true;
                        }
                    } else {
                        screen_exit = true;
                    }
                } else if (input == g_state.show_all_field) {
                    Toggle_Show_All();
                } else if (input == g_state.edit_field) {
                    // Enter commits (input_field_active is cleared by the
                    // field system on Enter); a click only focuses the
                    // field and keeps editing.
                    if (fields::input_field_active == 0) {
                        Commit_Edit();
                    }
                } else if (input == g_state.scroll_field) {
                    // The wheel or a scrollbar drag moved the list:
                    // like clicking another row, it cancels a pending
                    // edit, whose field would no longer line up with
                    // its row. The scroll itself already happened.
                    if (g_state.editing_row >= 0) {
                        Cancel_Edit();
                    }
                } else if (input == g_state.scroll_up_field) {
                    // The up arrow steps the list one row toward
                    // the first rows (a pending edit was already
                    // cancelled above).
                    Scroll_Lines(-1);
                } else if (input == g_state.scroll_down_field) {
                    // The down arrow steps the list one row toward
                    // the last rows.
                    Scroll_Lines(1);
                } else {
                    bool handled = false;
                    for (int category = 0; category < CATEGORY_COUNT; ++category) {
                        if (input == g_state.tab_fields[category]) {
                            Switch_Category(category);
                            handled = true;
                            break;
                        }
                    }
                    if (!handled) {
                        const int first = Scroll_Top();
                        const int count = Page_Count();
                        for (int index = 0; index < count; ++index) {
                            if (input == g_state.row_fields[index] ||
                                input == g_state.row_hit_fields[index] ||
                                input == g_state.value_fields[index]) {
                                const int visible = first + index;
                                if (Is_Mod_Row(visible)) {
                                    Handle_Mod_Click(Mod_Index(visible));
                                } else {
                                    Handle_Row_Click(visible);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            if (!screen_exit) {
                Draw_Settings_Screen();
                video::Copy_Off_To_Back();
                RUSS::Mox_Sync_Update();
            }
            timer::Release_Time(3);
        }

        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        SETTINGS_BACKGROUND::Remove_Background();
        MOX::current_screen = SCREEN_MAIN_MENU;
        MOX::FULL_DRAW_SCREEN_FLAG = 0;
    }

} // namespace SETTINGS_SCREEN
