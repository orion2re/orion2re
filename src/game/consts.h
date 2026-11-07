#pragma once

#include "version.h"

#define BITMAP(bit_count) (((bit_count) + 7) / 8)

#define MAX_PLAYERS 8
#define MAX_STARS 1024
#define MAX_SHIPS 10000
#define MAX_PLANETS (MAX_STARS * 5)
#define MAX_COLONIES MAX_PLANETS
#define MAX_LEADERS 67
#define MAX_NEBULAS 57
#define MAX_REPORTS 50
#define MAX_MESSAGES 100
#define MAX_FREIGHTED_SETTLERS 1000
#define MAX_COLONY_POPULATION 42

#define ORIGINAL_MAX_STARS 72
#define ORIGINAL_MAX_SHIPS 500
#define ORIGINAL_MAX_PLANETS 360
#define ORIGINAL_MAX_COLONIES 250
#define ORIGINAL_MAX_NEBULAS 4
#define ORIGINAL_MAX_FREIGHTED_SETTLERS 25
#define SHIP_LOCATION_STEP 10000
#define SHIP_LOCATION_MOVING_OFFSET SHIP_LOCATION_STEP
#define SHIP_LOCATION_WORMHOLE_OFFSET (SHIP_LOCATION_STEP * 2)
#define SHIP_LOCATION_LIMIT (SHIP_LOCATION_STEP * 3)
#define SHIP_ICON_COUNT (MAX_SHIPS + MAX_STARS)
#define SHIP_LIST_CAPACITY (MAX_SHIPS + 1)
#define SHIP_NODE_UI_TAG_BASE 10000

static_assert(MAX_STARS < SHIP_LOCATION_STEP, "encoded ship locations must reserve one destination for Antares");
static_assert(SHIP_LOCATION_LIMIT - 1 <= INT16_MAX, "encoded ship locations do not fit in int16_t");
static_assert(MAX_SHIPS <= SHIP_NODE_UI_TAG_BASE, "encoded UI ship nodes overlap real ship-node indices");
static_assert(SHIP_NODE_UI_TAG_BASE + MAX_SHIPS - 1 <= INT16_MAX, "encoded UI ship nodes do not fit in int16_t");

#define ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE 100
#define ORIGINAL_MAX_COMBAT_FLYERS 300
#define MAX_COMBAT_SHIPS_PER_SIDE 500
#define MAX_COMBAT_SHIPS (MAX_COMBAT_SHIPS_PER_SIDE * 2 + 10)
#define MAX_COMBAT_FLYERS_PER_SIDE 2500
#define MAX_COMBAT_FLYERS (MAX_COMBAT_FLYERS_PER_SIDE * 2)
#define COMBAT_SHIP_NONE UINT16_MAX
#define ORIGINAL_COMBAT_GRID_WIDTH 81
#define ORIGINAL_COMBAT_GRID_HEIGHT 68
#define MAX_COMBAT_GRID_WIDTH ORIGINAL_COMBAT_GRID_WIDTH
#define MAX_COMBAT_GRID_HEIGHT (ORIGINAL_COMBAT_GRID_HEIGHT * MAX_COMBAT_SHIPS_PER_SIDE / ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE)
#define COMBAT_GRID_VIEW_WIDTH 32
#define COMBAT_GRID_VIEW_HEIGHT 18
#define COMBAT_GRID_VIEW_MAX_X (COMBAT_GRID_VIEW_WIDTH - 1)
#define COMBAT_GRID_VIEW_MAX_Y (COMBAT_GRID_VIEW_HEIGHT - 1)

// Mouse entries comprise the background, viewport cells, flyers, and bottom panel.
#define COMBAT_MOUSE_FLYER_BASE (1 + COMBAT_GRID_VIEW_WIDTH * COMBAT_GRID_VIEW_HEIGHT)
#define COMBAT_MOUSE_PANEL_INDEX (COMBAT_MOUSE_FLYER_BASE + MAX_COMBAT_FLYERS)
#define COMBAT_MOUSE_COUNT (COMBAT_MOUSE_PANEL_INDEX + 1)

static_assert(MAX_COMBAT_SHIPS < 2000, "combat ship indices overlap the legal-move grid marker");
static_assert(MAX_COMBAT_FLYERS <= INT16_MAX, "combat flyer indices do not fit in int16_t");
static_assert(MAX_COMBAT_GRID_HEIGHT * 20 <= INT16_MAX, "combat pixel coordinates do not fit in int16_t");

inline constexpr const char* GAME_VERSION = orion2re::ENGINE_VERSION;
inline constexpr const char* GAME_VERSION_LABEL = orion2re::ENGINE_VERSION_LABEL;
inline constexpr char GAME_BUILD_DATE[] = "Aug 15 2026";
inline constexpr char GAME_SETTINGS_FILENAME[] = "ORION2RE.SET";

//#define FULLSCREEN
