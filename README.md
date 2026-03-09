# 100 Lives

A Geometry Dash mod that gives you multiple lives per attempt, so one mistake doesn't end your run. Includes a game mode switcher and a rescue system to get you back on track.

## Features

### Extra Lives
Instead of dying on the first hit, you get a configurable number of lives (default: 100). Each hit costs one life and triggers a brief invincibility window — the player blinks to show you're protected. When all lives are gone, the attempt ends normally.

The lives counter is shown in the top-right corner and changes color based on how many you have left:
- Green — plenty of lives
- Yellow — getting low
- Red — almost out

### Protection Mode
Choose what your lives protect you from:
- **All obstacles** — blocks, spikes, saws, everything
- **Spikes & saws only** — only hazards trigger the life system; falling into blocks kills normally

### Game Mode Cycling (M key)
Press **M** during a level to cycle through game modes: Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing. Each mode can be individually enabled or disabled in settings — only enabled modes appear in the cycle.

### Rescue System (B key)
Press **B** to teleport back to your last safe position. The mod saves your position periodically as you play. If you've touched the ground at some point, it restores that ground position; otherwise it uses the last known position (useful for ship/wave levels).

## Settings

| Setting | Description | Default |
|---|---|---|
| Number of Lives | Lives per attempt | 100 |
| Invincibility Time | Seconds of protection after each hit | 1.2s |
| Protection Mode | All obstacles or spikes/saws only | All |
| Checkpoint Interval | How often to save rescue position (seconds) | 0.5s |
| Cube / Ship / Ball / UFO / Wave / Robot / Spider / Swing | Toggle each mode in the M cycle | Cube, Ship, UFO, Wave, Swing on |
| Rescue Key | Key to teleport to last safe position | B |
| Cycle Game Mode Key | Key to switch game modes | M |

## Keybinds

Both keys can be rebound in the mod settings.

- **B** — Rescue: teleport to last safe position
- **M** — Cycle game mode

## Authors

Larry & Daniel

## Requirements

- Geometry Dash 2.2081
- [Geode](https://geode-sdk.org) 5.3.0+
