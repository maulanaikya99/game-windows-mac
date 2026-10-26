---
name: game-windows-mac
description: Jalankan game PC Windows (.exe, zip/rar hasil unduhan, "di Windows tinggal run as administrator") di Mac M4 user, sebagai paket satu folder di SSD "ikya MSI" lengkap dengan app sekali klik, FPS, dan 2 stick Twin. Pakai saat user bilang "main game windows X di mac", "buat seperti naruto/gow3", "game ini di folder download", atau minta perbaiki kontrol/kursor/grafis game Wine.
---

# Game Windows → Mac (Wine 11 Whisky + DXMT)

Resep yang terbukti 26 Okt 2026 pada Naruto x Boruto Storm Connections (DX11): 60 FPS, 2 stick, 2 pemain.
Untuk game PS4 (eboot.bin) BUKAN skill ini; lihat memori spartanport-gow3.

## Langkah cepat

1. Cari file game: biasanya `/Volumes/ikya MSI/Download/` (zip/rar) atau folder. Cek kelengkapan
   (`unzip -l` / `tar -tf`), jangan jalankan isinya dulu. Cek ruang SSD (`df -h`).
2. Jalankan:
   ```
   ~/.claude/skills/game-windows-mac/new_game.sh "Nama Game" "<zip|rar|folder>" ["relatif/exe"]
   ```
   Skrip ini: ekstrak ke `/Volumes/ikya MSI/Games/<Nama>/Game`, klon runtime Wine dari paket lain
   (APFS clone, 0 byte) atau unduh Whisky Libraries v3.1.1 (SHA tersemat), buat `Wine Prefix`,
   pasang DXMT, registry stick Twin + kursor bebas, kompilasi & pasang DLL XInput Twin (kalau exe
   mengimpor xinput*), buat `Main <Nama>.command` dan `<Nama>.app` (launcher C + ikon exe + codesign).
   Exe ditebak otomatis (terbesar, bukan setup/unins/crash/launcher), jadi periksa baris `== exe:`.
3. Uji: jalankan `.command` di background, tunggu ~40 dtk, cek `Logs/run-*.log`
   ("Using feature level D3D_FEATURE_LEVEL_11_0" = DXMT hidup) dan `pgrep -f 'NSUNSC\.exe'`-style.
   `screencapture` TIDAK diizinkan → minta user melihat layar.
4. Minta user tes: tekan HOME tiap stick sampai 2 lampu (mode analog), coba tombol & 2 pemain.
5. Simpan memori proyek baru + tambah ke daftar path di memori external-drive.

## Opsi peluncur
- `RENDERER=dxvk` (DXVK 1.10.3, tanpa dxgi/d3d9) atau `RENDERER=wined3d` (OpenGL, untuk D3D9/lama).
  Bawaan = DXMT (D3D10/11 → Metal). D3D12 butuh D3DMetal (GPTK Apple, belum terpasang).
- `MTL_HUD_ENABLED=0` mematikan panel FPS (bawaan nyala).
- `TWIN_XINPUT_LOG='Z:\Volumes\ikya MSI\Games\<Nama>\Logs\twin.log'` → log DLL stick (game GUI tak punya stderr).
- `WINEDEBUG=+loaddll` untuk melihat DLL yang dimuat.

## Stick "Twin USB Joystick" (2563:0555) — inti masalah & solusi
- SDL2 macOS di Wine menggabungkan 2 port jadi 1 → registry `winebus` "Enable SDL"=0 + EnableHidraw "2563:0555"
  → Wine membuat HID `...&col01` (port 1) & `...&col02` (port 2). Setelan winebus baru terbaca kalau wineserver prefix benar-benar mati.
- Wine tidak menandai pad ini sebagai XInput. `files/twin_xinput.c` = xinput DLL native yang membaca HID itu langsung:
  tombol HID 1 △→Y, 2 ○→B, 3 ✕→A, 4 □→X, 5/6 L1/R1, 7/8 L2/R2→trigger, 9 Select→Back, 10 Start, 11/12 L3/R3;
  X/Y stik kiri, Z/Rz stik kanan (Y dibalik), hat → D-pad; pindai ulang tiap 2 dtk. Ganti `button_map` kalau tertukar.
- Wine memuat xinput bawaan kalau tak ada override → wajib `xinputX=n,b` (skrip mengisinya).
- Alat: `Wine/twin-xinput/xtest.exe <dll> <detik>` (cetak state; jalankan dgn override n,b), `hidlist.exe` (daftar path HID).

### Kalau game TIDAK memanggil XInput (log twin kosong)
Banyak game (contoh Naruto) memakai cara Microsoft `IsXInputDevice`: WMI `Win32_PNPEntity` cari "IG_" di DeviceID;
hanya VID/PID itu yang lewat XInput, sisanya DirectInput (tombol kacau). Cek: string UTF-16 "IG_"/"Win32_PNPEntity" di exe.
Solusi = patch kecil exe (cadangkan dulu `cp -n x.exe x.exe.orig`, catat sha256):
1. `tools/find_import_calls.py <exe> DirectInput8Create XInputGetState` → alamat pemanggil.
2. `tools/disasm_func.py <exe> <alamat>` (pakai .pdata) / `tools/disasm_range.py <exe> a-b` → cari callback EnumDevices
   (argumen DIDEVICEINSTANCEW: guidProduct di +0x14) yang mencari VIDPID di sebuah set lalu cabang ke XInputGetState.
3. Ubah cabang "tidak ketemu" jadi lompat ke jalur XInput. Naruto: VA 0x14143a4ad `48 85 c0 0f 84` → `EB 2F`.
   JANGAN men-disable joystick di DirectInput (registry `DirectInput\Joysticks`) — enum DI justru dibutuhkan.
Jalankan alat python: `uv run -q --with pefile --with capstone --with numpy python -I tools/<x>.py ...` dari direktori scratchpad.

## Masalah lain yang sudah diketahui
- Kursor tak bisa ke layar 2 → `HKCU\Software\Wine\X11 Driver` GrabPointer=N & GrabFullscreen=N (skrip sudah set; dibaca driver Mac juga, nama kunci memang "X11 Driver").
  Kalau MASIH terkurung (game sendiri memanggil ClipCursor, mis. UE5 Demon Slayer): DLL Twin otomatis menambal
  `user32!ClipCursor` + `win32u!NtUserClipCursor` jadi return TRUE saat dimuat (matikan: env `TWIN_ALLOW_CLIP=1`).
  Game UE: tambah juga `Saved/Config/Windows/Input.ini` → `[/Script/Engine.InputSettings]` DefaultViewportMouseLockMode=DoNotLock.
  Catatan: tambalan hanya jalan kalau game memuat DLL xinput Twin (impor xinput).
- Game mengimpor xinput PER ORDINAL (UE5): DLL wajib dibangun dgn `files/xinput1_x.def` / `xinput9_1_0.def` (skrip sudah).
  Gejala kalau salah: crash ACCESS_VIOLATION di XINPUT1_3+0x17xx.
- Game UE5 (folder `Binaries/Win64/D3D12`): luncurkan dgn `-dx11` (DXMT tak bisa D3D12). Peringatan "D3D9" skrip = salah deteksi.
  Crash UE: `Wine Prefix/.../AppData/Local/<Proyek>/Saved/Crashes/*/CrashContext.runtime-xml` (CallStack + ErrorMessage).
- Installer repack ISO (Inno+FreeArc, Setup-1.bin): mount `diskutil image attach --readOnly`, jalankan
  `wine Setup.exe /SP- /VERYSILENT /SUPPRESSMSGBOXES /NOICONS /DIR="Z:\...\<Nama>\Game"` di prefix paket, lalu new_game.sh dgn folder Game.
  Installer memutar musik (BASS.dll) — bukan bug suara Mac.
  Kalau installer langsung keluar tanpa file ("Failed to proceed to next wizard page" di `/LOG=`): cek ruang kosong —
  Z: = `/` (disk internal). Buat drive di SSD: `ln -sfn "<paket>" "<paket>/Wine Prefix/dosdevices/g:"` lalu `/DIR=G:\Game`.
- Crash dini di bagian exe terlindungi (Denuvo dll., section besar `.xtls`/`.v38`) dengan `SIGSYS, rax 0x..` di log
  `WINEDEBUG=+seh` = syscall langsung dari lapisan DRM/crack. Ganti renderer tak membantu. BERHENTI: jangan bongkar/tambal
  DRM; sarankan versi resmi (Steam) + CrossOver/D3DMetal. Contoh: Black Myth Wukong repack voices38 (26 Okt 2026).
- Matikan game: `WINEPREFIX="<paket>/Wine Prefix" <paket>/Wine/Libraries/Wine/bin/wineserver -k`. JANGAN `pkill wine` (MT5 juga Wine).
- `.app` executable harus biner (bukan skrip) agar izin SSD (TCC) atas nama app; pertama kali buka → user klik Izinkan.
- Jangan klik dua kali .exe langsung (Wine bikin `~/.wine` di disk internal).
- Rilis bajakan (steam_emu dll.): jangan bantu crack/unduh; hanya kompatibilitas Mac untuk file yang sudah user punya.
- Kompiler Windows: zig di `/Volumes/ikya MSI/application/zig/zig` (`zig cc -target x86_64-windows-gnu`).
