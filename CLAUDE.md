# Dodge

Design doc for Dodge, a 2D arcade survival game in C++. Agreed 2026-11-03.
Status: building. Follow `docs/superpowers/plans/2026-11-03-dodge-mvp.md` task by task.

## Git workflow

- One branch per layer, as mapped in the plan's Branches table. Work only on the branch that owns the task.
- Commit after every completed plan step that changes files. Push that branch to `origin` after each commit.
- Merge a layer branch into `main` only when all its tasks pass. Merge by squash and rebase: squash the
  branch into one commit on top of `main`, force-push the branch with lease, then fast-forward `main`.
- Commit message format (Conventional Commits):

  ```
  <type>(<scope>): <imperative summary, lowercase, no period, max 72 chars>

  <body: what changed and why, wrapped at 72 chars>

  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  ```

  Types: `feat`, `fix`, `test`, `refactor`, `build`, `docs`, `chore`. Scopes: `shared`, `server`, `client`, `build`, `docs`.

Items marked **(Claude's call)** were filled in by Claude, not decided by the user. Change them freely.

## Intent

Players dodge projectiles and stay alive as long as possible. The game supports solo play,
LAN parties, and private online parties of up to 4 players. The online version is meant to be
deployed for real, so the architecture must not need a rewrite to go from LAN to internet.

## Goals

- Solo mode: countdown, survive, see results, return to menu.
- LAN multiplayer: host a game, others on the same network see it in a list and join.
- Online multiplayer: private parties joined by party code, hosted on a dedicated server (VPS).
- Difficulty rises over time by increasing hazard spawn rate.

## Non-goals (MVP)

- Public online game lists or matchmaking. Online games are private, code only.
- Saved stats, accounts, leaderboards, high scores. Stats exist only for the current match.
- Player abilities. Players only move.
- Art beyond basic 2D shapes.
- Multiple hazard types, hazard speed scaling.
- Player-to-player collision.

## Constraints

- C++ with CMake. Dependencies fetched with `FetchContent`.
- Server must build and run on Linux (VPS) as well as Windows. No Windows-only APIs in `shared/` or `server/`.
- Server is authoritative. Clients send input only and are never trusted (trust boundary: validate everything the server receives).
- Windows Firewall will prompt the first time the game listens on a port.

## Approaches considered

- **A. raylib + raygui + ENet — chosen.** raylib for window, drawing, input. raygui for buttons and textbox.
  ENet for UDP with reliable and unreliable channels, and its socket API covers LAN broadcast.
  Fewest moving parts. Tradeoff: raygui looks plain.
- **B. SFML 3 + TGUI.** One library for graphics and sockets, but SFML networking is raw TCP/UDP,
  so reliability over UDP would be hand-written. Rejected: more networking code.
- **C. SDL3 + Dear ImGui + Valve GameNetworkingSockets.** Most powerful, but OpenSSL/protobuf make
  the build heavy and ImGui menus look like debug tools. Rejected: overkill.

## Architecture

Dedicated server program from day 1. One server codebase serves every mode.

- `shared/`: game simulation, protocol messages, constants. No graphics dependency.
- `server/`: headless server exe (links `shared/` + ENet only). Runs on the VPS for online play.
- `client/`: rendering, menus, input (links `shared/`, raylib, raygui, ENet).

Modes:

- **Online:** clients connect to the VPS server.
- **LAN:** the creator's client runs the same server code on a background thread and broadcasts the game on the LAN.
- **Solo:** the client runs the server code on a background thread with one player. **(Claude's call)** One code path for all modes.

Clients and server must check a protocol version on connect and reject mismatches.

## Menu flow

1. Main menu: **Play**.
2. Play shows **Solo** and **Multiplayer**.
3. Solo: on-screen countdown, then the match starts. No name asked.
4. Multiplayer: name textbox first, then **LAN** or **Online**.
5. LAN or Online then shows **Create** or **Join**.
   - LAN Create: start local server, broadcast, go to lobby.
   - LAN Join: list of LAN games found by broadcast (show player count, e.g. 2/4). Select one to join.
   - Online Create: server generates a party code, go to lobby. Code is shown in the lobby.
   - Online Join: textbox for the party code.
6. Every menu screen has a Back button. **(Claude's call)**

## Player names

- Multiplayer only. Solo has no name.
- **(Claude's call)** 1 to 12 characters: letters, digits, space, `_`, `-`. Trim surrounding spaces. Reject empty.
- Duplicates allowed; color tells players apart.
- Server re-validates names.

## Party codes (online)

- Stored only in server memory. Freed when the party closes.
- Must be unique among live parties. On collision, generate a new one. Codes may repeat over time.
- **(Claude's call)** 5 characters, case-insensitive, alphabet without look-alikes (no `0 O 1 I L`).

## Lobby

- Up to 4 players. A 5th join is rejected as full. Joining a party whose match has started is rejected.
- Live player list. Each row shows the player's colored circle and name. Updates as players join and leave.
- Each player has a distinct color.
- Each player toggles **Ready**.
- When at least 2 players are in the party and all are Ready, a 3-second countdown starts, then the match auto-starts.
- The countdown cancels if anyone un-readies, a new player joins, or the party drops below 2 players.
- Online: no player has a special creator role. The party survives the creator leaving; it closes when empty.
- LAN: the creator's game is the server. If the creator leaves or disconnects, the party closes and everyone returns to the menu.
- **(Claude's call)** Leaving the lobby is allowed in all modes. The no-leave rule below applies to matches only.

## Match rules

- Server owns hazard spawning, movement, collisions, and eliminations.
- A player is eliminated instantly by touching a hazard or by their center leaving the arena.
- Eliminated players become spectators and keep watching.
- Online: players can leave at any time (alive or spectating) and return to the menu. A leaving alive player counts as eliminated.
- LAN: no Leave button during a match, for any player. A player who crashes or disconnects counts as eliminated.
- LAN: if the creator disconnects, the match and party end and everyone returns to the menu.
- The match ends when every player is eliminated.
- **(Claude's call)** Solo has no pause and no Leave button during the match (arcade style).

## Results screen

- Shown to everyone when the match ends, solo included.
- One row per player: placement (1st to 4th) and time survived. Placement is by elimination order, last out is 1st.
- A short countdown on screen (**(Claude's call)** 5 seconds), then everyone returns to the menu and the party closes.
- Stats are never saved.

## Gameplay

- Arena: fixed 1280x720 logical units, scaled to the window, so every player sees the same field.
- Players: circles. 8-way movement with WASD and arrow keys, constant speed, diagonal normalized. No bounds clamp.
- Hazards: circles. Spawn at a random point along one of the 4 screen edges and move straight across,
  perpendicular to that edge, at a fixed speed. Removed once fully off-screen.
- Difficulty: hazard spawn rate rises over time, with a cap. Hazard speed stays fixed.
- Collision: circle-vs-circle distance check, done on the server.

## Left to the builder

- Exact sizes, speeds, spawn-rate curve and cap. Tune by playtesting.
- Tick rate and snapshot rate. **(Claude's call)** Suggest 60 Hz simulation, 20 to 30 Hz snapshots.
- Whether hazards are sent as spawn events (clients simulate the straight line) or in every snapshot.
- Client prediction and interpolation for online movement.
- Ports, LAN discovery message format, how clients get the VPS address (config file is fine).
- Disconnect timeouts.
- Testing: run several client instances on one PC for party flows.
