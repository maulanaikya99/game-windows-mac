---
name: game-windows-mac
description: Run Windows PC games (.exe, downloaded zip/rar, "on Windows you just run as administrator") on the user's M4 Mac as a single-folder package on the "ikya MSI" SSD, with a one-click app, FPS overlay, and two "Twin" gamepads. Use when the user says "main game windows X di mac" / "play Windows game X on Mac", "buat seperti naruto/gow3" / "make it like naruto/gow3", "game ini di folder download" / "this game is in the download folder", or asks to fix controls/cursor/graphics of a Wine game.
---

# Windows Game → Mac (Wine 11 Whisky + DXMT)

Recipe proven on 26 Oct 2026 with Naruto x Boruto Storm Connections (DX11): 60 FPS, 2 gamepads, 2 players.
PS4 games (eboot.bin) are NOT for this skill; see the spartanport-gow3 memory.

## Quick steps

1. Find the game files: usually `/Volumes/ikya MSI/Download/` (zip/rar) or a folder. Check completeness
   (`unzip -l` / `tar -tf`) and do not run anything inside yet. Check SSD free space (`df -h`).
2. Run:
   ```
   ~/.claude/skills/game-windows-mac/new_game.sh "Game Name" "<zip|rar|folder>" ["relative/exe"]
   ```
   The script: extracts to `/Volumes/ikya MSI/Games/<Name>/Game`, clones the Wine runtime from another package
   (APFS clone, 0 bytes) or downloads Whisky Libraries v3.1.1 (pinned SHA), creates the `Wine Prefix`,
   installs DXMT, sets registry for the Twin gamepads + free cursor, compiles & installs the Twin XInput DLL (if the exe
   imports xinput*), and creates `Main <Name>.command` and `<Name>.app` (C launcher + exe icon + codesign).
   The exe is guessed automatically (largest, not setup/unins/crash/launcher), so check the `== exe:` line.
   Other Macs/drives: set `GAME_DRIVE="/Volumes/<Drive>"` (default `/Volumes/ikya MSI`) and optionally `ZIG=/path/to/zig`
   (falls back to `zig` on PATH; without zig the Twin XInput DLL is skipped).
3. Test: run the `.command` in the background, wait ~40 s, check `Logs/run-*.log`
   ("Using feature level D3D_FEATURE_LEVEL_11_0" = DXMT is active) and `pgrep -f 'NSUNSC\.exe'`-style.
   `screencapture` is NOT allowed → ask the user to look at the screen.
4. Ask the user to test: press HOME on each gamepad until 2 LEDs light (analog mode), try the buttons & 2 players.
5. Save a new project memory + add it to the path list in the external-drive memory.

## Launcher options
- `RENDERER=dxvk` (DXVK 1.10.3, without dxgi/d3d9) or `RENDERER=wined3d` (OpenGL, for D3D9/older games).
  Default = DXMT (D3D10/11 → Metal). D3D12 needs D3DMetal (Apple GPTK, not installed yet).
- `MTL_HUD_ENABLED=0` turns off the FPS panel (on by default).
- `TWIN_XINPUT_LOG='Z:\Volumes\ikya MSI\Games\<Name>\Logs\twin.log'` → gamepad DLL log (GUI games have no stderr).
- `WINEDEBUG=+loaddll` to see which DLLs are loaded.

## "Twin USB Joystick" gamepad (2563:0555) — core problem & fix
- SDL2 on macOS in Wine merges the 2 ports into 1 → registry `winebus` "Enable SDL"=0 + EnableHidraw "2563:0555"
  → Wine creates HID `...&col01` (port 1) & `...&col02` (port 2). New winebus settings are only read once the prefix's wineserver is fully stopped.
- Wine does not mark this pad as XInput. `files/twin_xinput.c` = a native xinput DLL that reads that HID directly:
  HID button 1 △→Y, 2 ○→B, 3 ✕→A, 4 □→X, 5/6 L1/R1, 7/8 L2/R2→triggers, 9 Select→Back, 10 Start, 11/12 L3/R3;
  X/Y left stick, Z/Rz right stick (Y inverted), hat → D-pad; rescans every 2 s. Change `button_map` if buttons are swapped.
- Wine loads its built-in xinput unless overridden → `xinputX=n,b` is required (the script sets it).
- Tools: `Wine/twin-xinput/xtest.exe <dll> <seconds>` (prints state; run with the n,b override), `hidlist.exe` (lists HID paths).

### If the game does NOT call XInput (twin log empty)
Many games (e.g. Naruto) use Microsoft's `IsXInputDevice` approach: WMI `Win32_PNPEntity` looking for "IG_" in the DeviceID;
only those VID/PIDs go through XInput, the rest go through DirectInput (scrambled buttons). Check: UTF-16 strings "IG_"/"Win32_PNPEntity" in the exe.
Fix = a small exe patch (back up first `cp -n x.exe x.exe.orig`, record the sha256):
1. `tools/find_import_calls.py <exe> DirectInput8Create XInputGetState` → caller addresses.
2. `tools/disasm_func.py <exe> <address>` (uses .pdata) / `tools/disasm_range.py <exe> a-b` → find the EnumDevices callback
   (DIDEVICEINSTANCEW argument: guidProduct at +0x14) that looks up the VIDPID in a set and then branches to XInputGetState.
3. Change the "not found" branch into a jump to the XInput path. Naruto: VA 0x14143a4ad `48 85 c0 0f 84` → `EB 2F`.
   Do NOT disable joysticks in DirectInput (registry `DirectInput\Joysticks`) — the DI enumeration is actually needed.
Run the Python tools with: `uv run -q --with pefile --with capstone --with numpy python -I tools/<x>.py ...` from the scratchpad directory.

## Other known issues
- Cursor can't move to the 2nd screen → `HKCU\Software\Wine\X11 Driver` GrabPointer=N & GrabFullscreen=N (the script sets this; the Mac driver reads it too, the key really is named "X11 Driver").
  If it is STILL confined (the game itself calls ClipCursor, e.g. UE5 Demon Slayer): the Twin DLL automatically patches
  `user32!ClipCursor` + `win32u!NtUserClipCursor` to return TRUE when loaded (disable: env `TWIN_ALLOW_CLIP=1`).
  UE games: also add `Saved/Config/Windows/Input.ini` → `[/Script/Engine.InputSettings]` DefaultViewportMouseLockMode=DoNotLock.
  Note: the patch only works if the game loads the Twin xinput DLL (imports xinput).
- Games that import xinput BY ORDINAL (UE5): the DLL must be built with `files/xinput1_x.def` / `xinput9_1_0.def` (the script does this).
  Symptom if wrong: ACCESS_VIOLATION crash in XINPUT1_3+0x17xx.
- UE5 games (folder `Binaries/Win64/D3D12`): launch with `-dx11` (DXMT can't do D3D12). The script's "D3D9" warning = false detection.
  UE crashes: `Wine Prefix/.../AppData/Local/<Project>/Saved/Crashes/*/CrashContext.runtime-xml` (CallStack + ErrorMessage).
- Repack ISO installers (Inno+FreeArc, Setup-1.bin): mount with `diskutil image attach --readOnly`, run
  `wine Setup.exe /SP- /VERYSILENT /SUPPRESSMSGBOXES /NOICONS /DIR="Z:\...\<Name>\Game"` in the package prefix, then new_game.sh with the Game folder.
  The installer plays music (BASS.dll) — not a Mac audio bug.
  If the installer exits immediately without files ("Failed to proceed to next wizard page" in `/LOG=`): check free space —
  Z: = `/` (internal disk). Make a drive on the SSD: `ln -sfn "<package>" "<package>/Wine Prefix/dosdevices/g:"` then `/DIR=G:\Game`.
- Early crash inside a protected exe section (Denuvo etc., large `.xtls`/`.v38` sections) with `SIGSYS, rax 0x..` in the
  `WINEDEBUG=+seh` log = direct syscalls from the DRM/crack layer. Switching renderer doesn't help. STOP: do not reverse/patch
  DRM; suggest the official version (Steam) + CrossOver/D3DMetal. Example: Black Myth Wukong repack voices38 (26 Oct 2026).
- Stop a game: `WINEPREFIX="<package>/Wine Prefix" <package>/Wine/Libraries/Wine/bin/wineserver -k`. Do NOT `pkill wine` (MT5 also runs on Wine).
- The `.app` executable must be a binary (not a script) so SSD permission (TCC) is granted to the app; on first launch → user clicks Allow.
- Don't double-click the .exe directly (Wine creates `~/.wine` on the internal disk).
- Pirated releases (steam_emu etc.): don't help crack/download; only Mac compatibility for files the user already has.
- Windows compiler: zig at `/Volumes/ikya MSI/application/zig/zig` (`zig cc -target x86_64-windows-gnu`).
