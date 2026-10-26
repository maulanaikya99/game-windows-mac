# Porting game-windows-mac (Claude Code skill)

A Claude Code skill for running Windows PC games on Apple Silicon Macs (tested on M4) using
Wine 11 (Whisky Libraries) + DXMT (DirectX 11 → Metal). It builds a self-contained game folder
with a one-click `.app` launcher, a custom XInput DLL for two "Twin" gamepads, and includes
helper tools for inspecting Windows executables.

Proven on Naruto x Boruto Ultimate Ninja Storm Connections (60 FPS, local 2-player) and
Demon Slayer: Hinokami Chronicles 2 (Unreal Engine 5, 60 FPS).

## Install

```bash
git clone https://github.com/maulanaikya99/game-windows-mac ~/.claude/skills/game-windows-mac
```

Then in Claude Code run `/game-windows-mac`, or just ask "run Windows game X on my Mac".
Tell Claude where your games drive is (or export `GAME_DRIVE`) if it is not `/Volumes/ikya MSI`.

## Contents

- `SKILL.md` – the recipe and troubleshooting notes
- `new_game.sh` – creates a game package: extracts the game, sets up the Wine prefix, installs DXMT and XInput, builds the launcher
- `files/` – C sources for the launcher, the Twin XInput DLL, and an HID test tool
- `tools/` – Python scripts for analyzing exes (disassembly, import calls, icon extraction)

## Notes

- Choose where games are stored with `GAME_DRIVE` (default `/Volumes/ikya MSI`). Packages go to `$GAME_DRIVE/Games/<Name>`:
  ```bash
  GAME_DRIVE="/Volumes/MyDrive" ~/.claude/skills/game-windows-mac/new_game.sh "Game Name" ~/Downloads/game.zip
  ```
  An internal folder also works, e.g. `GAME_DRIVE="$HOME/WinGames"`.
- The Twin XInput DLL is built with [zig](https://ziglang.org) (`brew install zig`); set `ZIG=/path/to/zig` or have `zig` on PATH. Without zig that step is skipped.
- Requires macOS on Apple Silicon, Xcode command line tools (clang), and Python 3.
