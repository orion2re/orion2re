# Multiplayer

Orion2 Redone supports Network, Online, and Hotseat games.

Network games need a shared meeting point where players can see open games and connect to one another. Orion2 Redone calls this meeting point a multiplayer router. It is a small service that keeps the session list and passes game traffic between connected players. It does not run the game, provide mods, or store saved games.

For a local-network game, the host's game starts this service automatically. For an Internet game, a separate copy of the router service must be running on a computer that every player can reach. The person operating that service and the player hosting the game session can be different people.

## Choosing a mode

| Mode | Use it for | How players connect |
| --- | --- | --- |
| Network | A game on one local network | The game starts the meeting service on the host's computer, and the other players find the game automatically. |
| Online | A game over the Internet | Every player configures the same separately running meeting service and chooses the host's game. |
| Hotseat | Several players sharing one computer | No network connection or meeting service is used. |

Network and Online games use the same game rules. The difference is whether the game starts the router automatically on the local network or the players connect to a separately running router configured in `data/orion2re.cfg`.

## Changes since versions 1.31 and 1.50

The original version 1.31 multiplayer system came from the IPX and DirectPlay era and also included modem, null-modem or serial, and Hotseat paths. Version 1.50 retained that connectivity model while adding synchronization and save handling work, multiplayer configuration, and fixes for specific network-game problems.

Orion2 Redone removes the obsolete DirectPlay, IPX, modem, and serial networking paths. Network and Online games instead use a new project-owned TCP protocol and the multiplayer router. This provides one cross-platform networking system for LAN and Internet games.

A router lists only games created through that particular service; it is not a global matchmaking directory.

Orion2 Redone also checks the game build, synchronized configuration, required mod versions, and script state during multiplayer setup. When loading a game, the host sends the saved game state to the other players rather than requiring each player to select a separate local copy.

## Before starting

Every network participant should use the same Orion2 Redone build and compatible versions of the required mods. The host distributes the gameplay configuration and checks the required mod versions during setup. Local presentation settings that do not affect the game may differ.

If you use mods, agree on the enabled mods and their versions before opening the session. A mod can depend on another mod or require matching script content even when its visible settings appear identical.

## Play on a local network

To host a LAN game:

1. Open Multiplayer and choose Network.
2. Choose Start Game, enter a game name, and complete the game setup.
3. Wait for the other players to join.
4. Choose Begin when the intended players are present.

The game starts an embedded router for the host. Other computers on the same local network can find its open session automatically.

To join the game:

1. Open Multiplayer and choose Network.
2. Choose Join Game.
3. Wait for the session list to refresh, then select the host's game.
4. Select an available position and complete race selection when prompted.

If the game does not appear, confirm that both computers are on the same local network and that the host's firewall allows the game to accept local connections.

## Play over the Internet

Online games use the standalone `orion2re-router` program as their shared meeting service. One participant or another trusted operator must start it on a computer that every player can reach. The player hosting the game session does not have to operate the router, but the router must remain running for the whole session.

### Start a router

If your distribution includes `orion2re-router`, run it from a terminal or command prompt. On Windows the file may be named `orion2re-router.exe`.

```sh
orion2re-router
```

The default command listens for TCP connections on all IPv4 interfaces at port `47800`. To choose a different listen address or port, use:

```sh
orion2re-router --host 0.0.0.0 --port 47800
```

Use `--verbose` when diagnosing connection or session problems. Press Ctrl+C in the router terminal to stop it cleanly.

For Internet access, allow inbound TCP traffic to the selected port in the router computer's firewall. If that computer is behind a home or office gateway, forward the same TCP port from the gateway to the router computer. The service does not provide automatic port forwarding or NAT traversal.

The router does not encrypt connections, authenticate players, or enforce game passwords. Run it only on a computer and network where accepting these connections is appropriate, and stop it when it is no longer needed.

The router is built as a separate program and might not be included in every game distribution. Source users can build only the router on Linux with the `linux-router` CMake preset described in the project README.

### Connect the game to the router

The router operator gives every player its DNS name or IPv4 address and port. Configure the endpoint in `data/orion2re.cfg` before starting the game:

```cfg
interface router = "router.example.com:47800";
```

The port is optional and defaults to `47800`. For example, `interface router = "router.example.com";` uses the default port. Use `127.0.0.1:47800` only when the router is running on the same computer as the game. Players on other computers must configure an address that reaches the router computer.

Use the same router endpoint on every computer participating in the Online game. Restart the game after changing the setting. Network mode uses LAN discovery and ignores this setting.

To host an Online game:

1. Open Multiplayer and choose Online.
2. Choose Start Game, enter a game name, and complete the game setup.
3. Wait for the other players to join, then choose Begin.

To join an Online game:

1. Open Multiplayer and choose Online.
2. Choose Join Game.
3. Wait for the session list to refresh, select the game, and take an available position.
4. Complete race selection when prompted.

The session list contains open games on the configured router. If the list remains empty, check that the router is running, the configured endpoint and port are correct, and the router computer is reachable through its firewall and gateway.

### Optional LAN discovery for a standalone router

A standalone router does not advertise itself on a LAN by default. An operator can enable UDP discovery with:

```sh
orion2re-router --discovery
```

Discovery uses UDP port `47800` by default and is intended for a local network. It does not replace the TCP port required for game connections and is not needed when every player uses Online mode with a known endpoint.

## Host and join checks

The host controls the game setup and sends its synchronized configuration to the other players. If a participant reports an incompatible network version, configuration, mod, or script error, cancel setup and correct the mismatch rather than trying to continue with different game data.

There is no separate player-ready button. After joining, players select an available position or race as prompted. The host chooses Begin after the intended players have joined.

The session list refreshes periodically. If a game is not shown, wait for another refresh and confirm that the host has created the session and that both players selected the intended mode and router.

## Load a multiplayer save

The host chooses Load Game from Multiplayer and selects the saved game. Other players join the host's new session in the same way as for a new game. The host sends the saved game state, configuration, and script state to the participants; only the host needs to select the save file.

Keep compatible versions of the required mods installed on every computer. A missing or incompatible mod or script can prevent the save from loading. Human positions without a connected player become AI-controlled when the loaded session begins.

## End turns

Ending a turn marks that player as done and does not display the ordinary single-player confirmation dialog. The game advances after the required human players have ended their turns. The player-done lights show which participants are waiting for the next turn.

## Disconnects and recovery

If a player disconnects, that empire becomes AI-controlled and the remaining players can continue when the connection state permits it. A disconnected player cannot rejoin a game that is already running.

To bring that player back, save the game, leave the current session, and create a new multiplayer session by loading the save. Use the same game build and compatible mod versions. A position without a returning player can remain assigned to AI.

If the router itself stops or becomes unreachable, its connected games lose their network service. Restart the router, then create a new session from the latest multiplayer save.