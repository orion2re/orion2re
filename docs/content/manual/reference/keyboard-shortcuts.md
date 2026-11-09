# Manual: Keyboard shortcuts

Keys are not case-sensitive unless a modifier is shown. Some screens also provide shortcuts on visible buttons.

## Any Screen

| Key | Action |
| --- | --- |
| PageUp | Increase animation speed, up to +3. |
| PageDown | Decrease animation speed, down to -3. |
| Alt+Enter | Switch between windowed and full-screen display. |

## Main Screen / Galaxy Map

| Key | Action |
| --- | --- |
| F1 | Go to the previous fleet. |
| F2 | Go to the next fleet. |
| F4 | Enter relocation mode, then select a destination to change all relocations. |
| F5 | Go to the previous colonized system. |
| F6 | Go to the next colonized system. |
| F9 | Find the distance between two stars. |
| F10 | Save to the automatic-save slot, unless the internal debugger is enabled. |
| `+` / `-` | Zoom in / out. |
| `G` | Open the Game menu. |
| `T` | End the turn. |
| `C` | Open Colonies. |
| `P` | Open Planets. |
| `F` | Open Fleets. |
| `L` | Open Leaders. |
| `R` | Open Races. |
| `I` | Open Info. |
| Alt+V | Cycle influence off, filled with borders, and borders only. |
| Alt+Q | Quit without saving. |

F10 shows or hides the [internal debugger](../advanced-tools/debugger.md) instead of saving when `interface debugger_enabled = true;` and debugger support is present in the build. The setting requires a restart. The debugger is intended for testing and experimentation rather than normal play.

### Game Options

| Key | Action |
| --- | --- |
| Alt+F1 | Toggle End of Turn Summary. |
| Alt+F2 | Toggle End of Turn Wait. |
| Alt+F3 | Toggle Enemy Moves. |
| Alt+F5 | Toggle Auto-select Ships. |
| Alt+F6 | Toggle Animations. |
| Alt+F7 | Toggle Auto-select Colony. |
| Alt+F8 | Toggle relocation lines. |
| Alt+F9 | Load the currently selected save after confirmation. This is not available in multiplayer. |
| Alt+F10 | Save to the currently selected save after confirmation. |

## Colonies Screen

| Key | Action |
| --- | --- |
| `N` | Sort colonies by name. |
| `P` | Sort colonies by population. |
| `F` | Sort colonies by food. |
| `I` | Sort colonies by industry. |
| `S` | Sort colonies by science. |
| `R` | Sort colonies by production. |
| `B` | Sort colonies by BC. |
| `C` | Open the selected colony. |
| `Q`, `0` through `9` | Apply the corresponding configured queue preset to the selected colony. |
| Shift+`Q`, Shift+`0` through Shift+`9` | Apply the corresponding queue preset to all colonies. |
| `,` / `.` | Go to the previous / next colony. |
| Space or Esc | Return to the main screen. |

Queue presets are configuration entries. Orion2 Redone does not load the old `BUILD.CFG` or `BUILD0.CFG` through `BUILD9.CFG` files.

## Fleets Screen

| Key | Action |
| --- | --- |
| `,` / `.` | Go to the previous / next fleet. |
| `A` | Select all displayed ships. |
| `S` | Scrap selected ships. |
| `L` | Open Ship Officers. |
| `R` | Relocate ships. |
| `U` / `C` | Change the displayed fleet filters. |
| `+` / `-` | Scroll the fleet list. |
| Alt+F5 | Cancel all relocations. |
| Esc | Return to the previous screen. |

## Colony Screen

| Key | Action |
| --- | --- |
| `A` | Turn on Autobuild. |
| `B` | Buy or undo the purchase of the current project. |
| `C` | Open the Build screen. |
| `,` / `.` | Go to the previous / next colony. |
| Space | Return. |
| Esc | Return to the previous screen. |

## Build Screen

| Key | Action |
| --- | --- |
| `Q`, `0` through `9` | Apply the corresponding configured queue preset. |
| `B` | Buy or undo the purchase of the current project. |
| `E` | Add or remove Repeat Build. |
| `R` | Refit a ship. |
| `,` / `.` | Go to the previous / next colony. |
| Space | Accept the queue. |
| Esc | Return without saving queue changes. |
| Alt+A, Alt+D | Run Auto Design and update all ship designs. |

## Design Dock Lists

| Key | Action |
| --- | --- |
| `-` / `+` | Scroll up / down through weapon or special-system choices. |
| Esc | Return to Ship Design without saving the list choice. |

## Tactical Combat

| Key | Action |
| --- | --- |
| `A` | Start automatic combat; click to stop it. |
| `B` | Board the selected target. |
| `C` | Center on the active ship. |
| `D` | Mark the active ship done. |
| `H` | Wait with all ships under your command when Ship Initiative permits it. |
| `O` | Open combat options. |
| `R` | Retreat. |
| `S` | Scan the selected target when scanning is available. |
| `T` | End your tactical turn. |
| `W` | Wait with the active ship. |
| `Z` | Start fast resolution against the AI when available; click to stop it. |

## Orbital Combat Selection

| Key | Action |
| --- | --- |
| `B` | Bombard. |
| `D` | Destroy with a Stellar Converter. |
| `H` | Hold. |
| `A` | Drop all available troops. |
| `I` | Invade. |
| `M` | Mind control when playing a Telepathic race. |
| Esc | Cancel. |

## Reloading Configuration

The following sequences work on the Main, Build, and Tactical Combat screens. Hold Alt while typing the letters in order.

| Key sequence | Action |
| --- | --- |
| Alt+R, Alt+E | Reload the full configuration. This is not allowed during multiplayer games. |
| Alt+R, Alt+I | Reload interface options. This is allowed during multiplayer games. |

## Hall of Fame

Alt+C immediately and permanently replaces the recorded Hall of Fame scores with the default entries. Back up your Hall of Fame data before using this shortcut if you want to preserve the current scores.

The numbered `MAIN0.LUA` through `MAIN9.LUA` script-launch shortcuts from 1.50 are not supported in Orion2 Redone.