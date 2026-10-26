# game-windows-mac

Skill Claude Code untuk menjalankan game PC Windows di Mac Apple Silicon (M4) memakai
Wine 11 (Whisky Libraries) + DXMT (DirectX 11 → Metal), lengkap dengan DLL XInput untuk
2 stick "Twin", launcher `.app` sekali klik, dan alat bantu analisis exe.

Terbukti pada Naruto x Boruto Storm Connections (60 FPS, 2 pemain) dan Demon Slayer HC2 (UE5).

## Pasang

```bash
git clone https://github.com/maulanaikya99/game-windows-mac ~/.claude/skills/game-windows-mac
```

Lalu di Claude Code: `/game-windows-mac` atau "main game windows X di mac".

## Isi

- `SKILL.md` – resep & pemecahan masalah
- `new_game.sh` – buat paket game satu folder (ekstrak, prefix Wine, DXMT, XInput, launcher)
- `files/` – sumber C launcher, DLL XInput Twin, alat uji HID
- `tools/` – skrip Python untuk membongkar exe (disasm, impor, ikon)

Catatan: path default mengarah ke SSD `/Volumes/ikya MSI`; sesuaikan untuk Mac lain.
