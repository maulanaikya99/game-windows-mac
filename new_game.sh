#!/bin/zsh
# Buat paket game Windows siap main di Mac (Wine 11 Whisky + DXMT), satu folder di SSD.
#
#   new_game.sh "Nama Game" <file.zip|file.rar|folder-game> [relatif/ke/game.exe]
#
# Hasil: /Volumes/ikya MSI/Games/<Nama Game>/
#   Game/  Wine/Libraries/  Wine Prefix/  Logs/  Main <Nama Game>.command  <Nama Game>.app
set -e
setopt null_glob

NAME="$1"; SRC="$2"; EXE_REL="$3"
[[ -n "$NAME" && -n "$SRC" ]] || { echo "pakai: new_game.sh \"Nama Game\" <zip|rar|folder> [exe relatif]"; exit 1; }
SKILL="${0:A:h}"
SSD="/Volumes/ikya MSI"
P="$SSD/Games/$NAME"
ZIG="$SSD/application/zig/zig"
RUNTIME_URL="https://github.com/frankea/Whisky/releases/download/v3.1.1/Libraries.tar.gz"
RUNTIME_SHA="01f3a1b43b98065fe20c529c1023b61dd79a6d2ad93bba6040865f646481ccf3"
TWIN_VIDPID="2563:0555"   # adaptor stick "Twin USB Joystick" user

[[ -d "$SSD" ]] || { echo "SSD $SSD tidak terpasang"; exit 1; }
mkdir -p "$P/Game" "$P/Logs" "$P/Wine"

# 1. Game -----------------------------------------------------------------
if [[ -z "$(ls -A "$P/Game")" ]]; then
  echo "== menyiapkan file game"
  case "${SRC:l}" in
    *.zip) unzip -q "$SRC" -d "$P/Game" ;;
    *.rar|*.7z|*.tar|*.tar.*) tar -xf "$SRC" -C "$P/Game" ;;   # bsdtar macOS bisa RAR5
    *) [[ -d "$SRC" ]] || { echo "sumber tak dikenal: $SRC"; exit 1; }
       cp -cR "$SRC" "$P/Game/" 2>/dev/null || cp -R "$SRC" "$P/Game/" ;;
  esac
fi

if [[ -z "$EXE_REL" ]]; then
  # exe terbesar yang bukan installer/uninstaller/crash reporter/redist
  EXE_PATH=$(find "$P/Game" -iname '*.exe' -not -iname '*unins*' -not -iname '*setup*' -not -iname '*crash*' \
             -not -iname '*redist*' -not -iname 'vc_*' -not -iname 'dxsetup*' -not -iname '*launcher*' -print0 \
             | xargs -0 ls -S 2>/dev/null | grep -i '\.exe$' | head -1 || true)
else
  EXE_PATH="$P/Game/$EXE_REL"
fi
[[ -f "$EXE_PATH" ]] || { echo "exe tidak ketemu, beri argumen ke-3"; exit 1; }
GDIR="${EXE_PATH:h}"; EXE="${EXE_PATH:t}"
EXE_RE=$(printf '%s' "$EXE" | sed -E 's/([.*^$()+?{}|]|\[|\])/\\\1/g')
echo "== exe: ${EXE_PATH#$P/}"

# 2. Runtime Wine (klon APFS dari paket lain kalau ada, kalau tidak unduh) --
if [[ ! -x "$P/Wine/Libraries/Wine/bin/wine" ]]; then
  SRC_RT=$(ls -d "$SSD"/Games/*/Wine/Libraries(N) | grep -v "^$P/" | head -1 || true)
  if [[ -n "$SRC_RT" && -f "$SRC_RT/WhiskyWineVersion.plist" ]]; then
    echo "== klon runtime dari $SRC_RT"
    cp -cR "$SRC_RT" "$P/Wine/"
  else
    echo "== unduh runtime Whisky v3.1.1"
    curl -sSL -o "$P/Wine/Libraries.tar.gz" "$RUNTIME_URL"
    echo "$RUNTIME_SHA  $P/Wine/Libraries.tar.gz" | shasum -a 256 -c
    tar -xzf "$P/Wine/Libraries.tar.gz" -C "$P/Wine" && rm "$P/Wine/Libraries.tar.gz"
  fi
fi
W="$P/Wine/Libraries/Wine/bin"
export WINEPREFIX="$P/Wine Prefix" WINEDEBUG=-all

# 3. Prefix + DXMT + registry ----------------------------------------------
if [[ ! -d "$WINEPREFIX/drive_c/windows" ]]; then
  echo "== membuat prefix"
  WINEDLLOVERRIDES="mscoree,mshtml=" "$W/wineboot" -i > "$P/Logs/wineboot.log" 2>&1
  "$W/wineserver" -w
fi
for f in d3d11 dxgi d3d10core winemetal; do
  cp -f "$P/Wine/Libraries/DXMT/x64/$f.dll" "$WINEPREFIX/drive_c/windows/system32/$f.dll"
done
# stick Twin: jangan lewat SDL (menggabungkan 2 port), pakai hidraw -> COL01/COL02
"$W/wine" reg add 'HKLM\System\CurrentControlSet\Services\winebus' /v "Enable SDL" /t REG_DWORD /d 0 /f >/dev/null
"$W/wine" reg add 'HKLM\System\CurrentControlSet\Services\winebus' /v EnableHidraw /t REG_MULTI_SZ /d "$TWIN_VIDPID" /f >/dev/null
# kursor bebas pindah ke layar ke-2
for v in GrabPointer GrabFullscreen; do
  "$W/wine" reg add 'HKCU\Software\Wine\X11 Driver' /v $v /t REG_SZ /d N /f >/dev/null
done
"$W/wineserver" -w

# 4. XInput Twin (DLL native di folder exe, sesuai nama yang diimpor game) --
XI_OVR=""
XI_DLLS=(${(f)"$(strings -a "$EXE_PATH" | grep -ioE '^xinput(1_[1-4]|9_1_0)\.dll$' | tr 'A-Z' 'a-z' | sort -u)"}); XI_DLLS=(${XI_DLLS:#})
if (( ${#XI_DLLS} )) && [[ -x "$ZIG" ]]; then
  mkdir -p "$P/Wine/twin-xinput"
  cp -f "$SKILL/files/"{twin_xinput.c,xtest.c,hidlist.c,xinput1_x.def,xinput9_1_0.def} "$P/Wine/twin-xinput/"
  # .def = ordinal resmi; game (mis. UE5) mengimpor xinput PER ORDINAL -> tanpa .def crash
  ( cd "$P/Wine/twin-xinput" &&
    "$ZIG" cc -target x86_64-windows-gnu -O2 -shared -o twin_xinput.dll twin_xinput.c xinput1_x.def -lsetupapi -lhid &&
    "$ZIG" cc -target x86_64-windows-gnu -O2 -shared -o twin_xinput9.dll twin_xinput.c xinput9_1_0.def -lsetupapi -lhid &&
    "$ZIG" cc -target x86_64-windows-gnu -O2 -o xtest.exe xtest.c &&
    "$ZIG" cc -target x86_64-windows-gnu -O2 -o hidlist.exe hidlist.c -lsetupapi -lhid )
  for d in $XI_DLLS; do
    if [[ $d == xinput9_1_0.dll ]]; then cp -f "$P/Wine/twin-xinput/twin_xinput9.dll" "$GDIR/$d"
    else cp -f "$P/Wine/twin-xinput/twin_xinput.dll" "$GDIR/$d"; fi
    XI_OVR+=";${d%.dll}=n,b"
  done
  echo "== XInput Twin dipasang: $XI_DLLS"
else
  echo "== game tidak mengimpor xinput (atau zig tak ada) -> lewati DLL Twin"
fi
# Unreal Engine (.../<Proyek>/Binaries/Win64/x.exe): paksa D3D11 + jangan kurung mouse
UE_ARGS=""
if [[ "$GDIR" == */Binaries/Win64 && -d "${GDIR:h:h:h}/Engine" ]]; then
  UE_PROJ="${GDIR:h:h:t}"; UE_ARGS='${=ARGS:--dx11} '
  UE_CFG="$WINEPREFIX/drive_c/users/$USER/AppData/Local/$UE_PROJ/Saved/Config/Windows"
  mkdir -p "$UE_CFG"
  [[ -f "$UE_CFG/Input.ini" ]] || printf '[/Script/Engine.InputSettings]\nDefaultViewportMouseLockMode=DoNotLock\nDefaultViewportMouseCaptureMode=CaptureDuringMouseDown\n' > "$UE_CFG/Input.ini"
  echo "== Unreal Engine ($UE_PROJ): -dx11 bawaan (ARGS=-dx12 utk ganti), Input.ini DoNotLock"
else
  grep -qi 'd3d9\.dll' =(strings -a "$EXE_PATH") && echo "!! game memakai D3D9: DXMT/DXVK Whisky tak punya d3d9 -> pakai RENDERER=wined3d"
fi

# 5. Peluncur ----------------------------------------------------------------
CMD="$P/Main $NAME.command"
cat > "$CMD" <<EOF
#!/bin/zsh
# Peluncur $NAME (Wine 11 Whisky + DXMT). RENDERER=dxvk / wined3d untuk ganti penerjemah grafis.
P="$P"
W="\$P/Wine/Libraries/Wine"
export WINEPREFIX="\$P/Wine Prefix"
export PATH="\$W/bin:\$PATH"
export WINEDEBUG="\${WINEDEBUG:--all}"
export WINEESYNC=1 WINEMSYNC=1
export ROSETTA_ADVERTISE_AVX=1
SYS="\$WINEPREFIX/drive_c/windows/system32"
case "\$RENDERER" in
  dxvk)    # DXVK 1.10.3 Whisky: d3d11 + d3d10core saja (dxgi tetap bawaan Wine)
           cp -f "\$P/Wine/Libraries/DXVK/x64/"{d3d11,d3d10core}.dll "\$SYS/"
           export WINEDLLOVERRIDES="d3d11,d3d10core=n,b;dxgi=b;winemetal=b$XI_OVR" ;;
  wined3d) export WINEDLLOVERRIDES="dxgi,d3d11,d3d10core=b;winemetal=b$XI_OVR" ;;
  *)       # DXMT (bawaan): pastikan DLL DXMT yang ada di system32
           for f in d3d11 dxgi d3d10core winemetal; do cp -f "\$P/Wine/Libraries/DXMT/x64/\$f.dll" "\$SYS/\$f.dll"; done
           export WINEDLLOVERRIDES="dxgi,d3d11,d3d10core=n,b;winemetal=n,b$XI_OVR" ;;
esac
export MTL_HUD_ENABLED="\${MTL_HUD_ENABLED:-1}"   # panel FPS Metal
mkdir -p "\$P/Logs"
old=( "\$P/Logs"/run-*.log(Nom) ); (( \${#old} > 10 )) && rm -f "\${(@)old[11,-1]}"
LOG="\$P/Logs/run-\$(date +%Y%m%d-%H%M%S).log"
cd "$GDIR" || exit 1
wine "$EXE" $UE_ARGS"\$@" > "\$LOG" 2>&1
wineserver -w
EOF
chmod +x "$CMD"

# 6. App sekali klik -----------------------------------------------------------
A="$P/$NAME.app/Contents"
BUNDLE_ID="com.ikya.$(echo "$NAME" | tr -cd 'A-Za-z0-9' | tr 'A-Z' 'a-z')"
mkdir -p "$A/MacOS" "$A/Resources"
clang -O2 -arch arm64 -o "$A/MacOS/$NAME" "$SKILL/files/launcher.c"
cat > "$A/Resources/launch.zsh" <<EOF
#!/bin/zsh
P="$P"
exec >> "\$HOME/Library/Logs/$BUNDLE_ID.log" 2>&1
echo "=== \$(date)"
if [[ ! -d "\$P" ]]; then
  osascript -e 'display alert "SSD \"ikya MSI\" tidak terpasang" message "Colokkan SSD lalu buka lagi."'
  exit 1
fi
if pgrep -f '[\\\\/]$EXE_RE' >/dev/null; then echo "sudah jalan"; exit 0; fi
"\$P/Main $NAME.command"
EOF
cat > "$A/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>$NAME</string>
<key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
<key>CFBundleName</key><string>$NAME</string>
<key>CFBundleDisplayName</key><string>$NAME</string>
<key>CFBundleIconFile</key><string>AppIcon</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>1.0</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
EOF
T=$(mktemp -d)
if (cd "$T" && uv run -q --with pefile python -I "$SKILL/tools/exe_icon.py" "$EXE_PATH" "$T/icon.ico" >/dev/null 2>&1) &&
   sips -s format png "$T/icon.ico" --out "$T/icon.png" >/dev/null 2>&1; then
  mkdir -p "$T/AppIcon.iconset"
  for s in 16 32 128 256; do sips -z $s $s "$T/icon.png" --out "$T/AppIcon.iconset/icon_${s}x${s}.png" >/dev/null; done
  cp "$T/AppIcon.iconset/icon_32x32.png" "$T/AppIcon.iconset/icon_16x16@2x.png"
  cp "$T/AppIcon.iconset/icon_256x256.png" "$T/AppIcon.iconset/icon_128x128@2x.png"
  iconutil -c icns "$T/AppIcon.iconset" -o "$A/Resources/AppIcon.icns" && echo "== ikon dari exe"
else
  echo "== ikon exe tak terbaca, app tanpa ikon"
fi
rm -rf "$T"
codesign --force --sign - --identifier "$BUNDLE_ID" "$P/$NAME.app"

echo
echo "SELESAI: $P"
echo "  main : klik dua kali \"$NAME.app\" (atau \"Main $NAME.command\")"
print -r -- "  log  : $P/Logs/   (tes stick: env TWIN_XINPUT_LOG=<path Windows ke file log>)"
