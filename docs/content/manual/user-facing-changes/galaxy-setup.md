# Galaxy setup

Galaxy setup controls the scale and general character of the map. Galaxy size affects how much there is to explore and manage; galaxy age changes the kinds of stars and planets that are more likely to appear. Neither choice changes the difficulty setting.

The defaults are Medium size, Average age, and five players.

## Galaxy size

| Choice | Default stars | What to expect |
| --- | ---: | --- |
| Small | 20 | Short distances and early contact. |
| Medium | 36 | More room to develop before borders meet. |
| Large | 54 | Longer exploration and a broader mid-game. |
| Huge | 71 | The largest classic-style layout. |
| Maximum | 99 | The widest map and the greatest management and display workload. |

The values above are the native defaults. Maximum uses 99 stars by default, but the engine supports up to 1,024 total stars. The `map star_count` setting accepts up to 1,023 regular stars because Orion can occupy the remaining slot.

Version 2.00 offers these five size choices. Cluster is a legacy galaxy type retained for compatibility with existing saved games, but it is not available when starting a new game.

Larger maps provide more systems to explore and can increase travel time, the number of colonies and fleets to manage, game length, turn processing, and display work. Actual travel also depends on star placement, fuel range, wormholes, and the empires in the game. Use a smaller choice if a long game or a large empire would be uncomfortable to manage.

## Galaxy age

Young, Average, and Old change the weights used when stars and planets are generated. Young galaxies favor hotter stars and more mineral-rich or lifeless worlds. Old galaxies favor cooler stars and more farmable climates. Average, the default, lies between them. Age does not change the selected number of stars.

## Navigating large maps

Press `+` to zoom in and `-` to zoom out. In Maximum galaxies, each zoom-in step halves the visible width and height by default, so the next level shows one quarter of the previous map area. Set `interface maximum_galaxy_zoom_mode = classic;` to use the wider 72-star-style zoom scales instead. At the farthest zoom level of Maximum, the map draws automatic range outlines for empires known through exploration or diplomatic contact. These outlines do not reveal unknown empires.

Press Alt+V to cycle known influence from off, to checkerboard fills with borders, to borders only. The optional `interface main_map_overview_markers = true;` setting replaces star artwork with compact markers in Maximum's all-stars view; it is disabled by default. Influence and overview markers are separate aids. See [Interface and display](interface.md#main-map-navigation) for marker colors and other map controls.

Set `interface maximum_galaxy_display = classic;` to restore the classic main-map display at Maximum's farthest zoom level. Star names use their normal visibility rules and ownership colors, visited-system dots disappear, and normal star artwork, black-hole animation, and highlight ordering replace the overview presentation. Automatic influence outlines are disabled, so Alt+V cycles through off, filled influence with borders, and borders only as it does at closer zoom levels. Classic display takes precedence over `main_map_overview_markers` in that view. The default, `overview`, retains the large-map aids; classic labels may overlap on dense maps. Other galaxy sizes, closer zoom levels, map dimensions, and zoom scaling are unchanged. The setting is local and is not saved or synchronized in multiplayer.

## Scoring and saved games

Galaxy size affects the time and captured-colonist parts of the final score. Maximum has its own extended values; see [Victory and scoring](../game-mechanics/victory-and-scoring.md#score-calculation). The selected size, age, generated map, and related setup are stored with the game and restored when that save is loaded.

In a joined game, players use the galaxy setup selected by the host.