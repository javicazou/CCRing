#!/usr/bin/env bash
# =============================================================================
#  CCRing — instalador para macOS (sin firma de Apple: proyecto libre y gratuito)
#  Genera dist/CCRing-Mac.pkg (VST3 + AU + app Standalone) y un .zip
#  para compartir con el instalador, los manuales (EN/ES), la licencia y "Allow CCRing.command".
#
#  Uso:  ./package_mac.sh            Universal: Intel + Apple Silicon (usa build-universal/)
#        ./package_mac.sh --intel    solo Intel, más rápido (usa build/)
# =============================================================================
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build-universal"; ARCHS="arm64;x86_64"; ARCH_LABEL="Mac"; HOST_ARCHS="x86_64,arm64"
ARCH_TEXT="cualquier Mac (<b>Intel o Apple Silicon M1/M2/M3/M4</b>)"
if [[ "${1:-}" == "--intel" ]]; then
  BUILD_DIR="$PROJECT_DIR/build"; ARCHS="x86_64"; ARCH_LABEL="Mac-Intel"; HOST_ARCHS="x86_64"; ARCH_TEXT="Mac con procesador <b>Intel</b>"
fi
DIST_DIR="$PROJECT_DIR/dist"
NAME="CCRing"
VERSION="$(sed -n 's/^project(CCRing VERSION \([0-9.]*\).*/\1/p' "$PROJECT_DIR/CMakeLists.txt")"
PKG_ID="com.ccr.ccring"
ARTEFACTS="$BUILD_DIR/${NAME}_artefacts/Release"

log() { printf '\033[1;36m==>\033[0m %s\n' "$*"; }
ok()  { printf '\033[1;32m ✓\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m ✗ %s\033[0m\n' "$*" >&2; exit 1; }

[[ -x "$PROJECT_DIR/../.tools/venv/bin/cmake" ]] && export PATH="$PROJECT_DIR/../.tools/venv/bin:$PATH"

# 1. Compilar (Release)
JUCE_ARGS=()
[[ -d "$PROJECT_DIR/../build/_deps/juce-src/modules" ]] && JUCE_ARGS=(-DFETCHCONTENT_SOURCE_DIR_JUCE="$(cd "$PROJECT_DIR/../build/_deps/juce-src" && pwd)")
GEN=(); command -v ninja >/dev/null && GEN=(-G Ninja)
log "Compilando $NAME (Release, $ARCHS)…"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" "${GEN[@]+"${GEN[@]}"}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 "${JUCE_ARGS[@]+"${JUCE_ARGS[@]}"}" >/dev/null
cmake --build "$BUILD_DIR" --config Release --target "${NAME}_VST3" "${NAME}_AU" "${NAME}_CLAP" "${NAME}_Standalone" >/dev/null
for b in "$ARTEFACTS/VST3/$NAME.vst3/Contents/MacOS/$NAME" "$ARTEFACTS/AU/$NAME.component/Contents/MacOS/$NAME" \
         "$ARTEFACTS/CLAP/$NAME.clap/Contents/MacOS/$NAME" "$ARTEFACTS/Standalone/$NAME.app/Contents/MacOS/$NAME"; do
  for a in ${ARCHS//;/ }; do lipo -archs "$b" | grep -q "$a" || die "$b no tiene $a"; done
done
ok "Binarios: $(lipo -archs "$ARTEFACTS/AU/$NAME.component/Contents/MacOS/$NAME") (macOS 11 o superior)"

# 2. Contenido del paquete: plugins en /Library/Audio/Plug-Ins, app en /Applications/CCR
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
PLUG="$WORK/plug"; APP="$WORK/app"
mkdir -p "$PLUG/VST3" "$PLUG/Components" "$PLUG/CLAP" "$APP" "$WORK/scripts" "$WORK/resources" "$WORK/zip"
ditto "$ARTEFACTS/VST3/$NAME.vst3"      "$PLUG/VST3/$NAME.vst3"
ditto "$ARTEFACTS/AU/$NAME.component"   "$PLUG/Components/$NAME.component"
ditto "$ARTEFACTS/CLAP/$NAME.clap"      "$PLUG/CLAP/$NAME.clap"
ditto "$ARTEFACTS/Standalone/$NAME.app" "$APP/$NAME.app"
for b in "$PLUG/VST3/$NAME.vst3" "$PLUG/Components/$NAME.component" "$PLUG/CLAP/$NAME.clap" "$APP/$NAME.app"; do
  xattr -cr "$b"
  codesign --force --deep --sign - "$b" >/dev/null 2>&1   # firma local (ad-hoc): obligatoria en Apple Silicon
  xattr -cr "$b"
  codesign --verify --deep "$b" || die "Firma inválida: $b"
done
ok "Plugins y app firmados (ad-hoc)"

# Después de instalar: sacar la cuarentena y que macOS registre el Audio Unit nuevo
cat > "$WORK/scripts/postinstall" <<'EOF'
#!/bin/bash
# CCRing reemplaza a URing (mismos identificadores): se quita la versión vieja para que el DAW no vea dos
for old in "/Library/Audio/Plug-Ins/VST3/URing.vst3" "/Library/Audio/Plug-Ins/Components/URing.component" \
           "/Applications/Monstri Audio/URing.app"; do
  [ -e "$old" ] && rm -rf "$old"
done
rmdir "/Applications/Monstri Audio" 2>/dev/null || true
for p in "/Library/Audio/Plug-Ins/VST3/CCRing.vst3" "/Library/Audio/Plug-Ins/Components/CCRing.component" "/Library/Audio/Plug-Ins/CLAP/CCRing.clap" \
         "/Applications/CCR/CCRing.app"; do
  [ -e "$p" ] && xattr -dr com.apple.quarantine "$p" 2>/dev/null
done
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
EOF
chmod +x "$WORK/scripts/postinstall"

# Componentes que no se "mudan" a otra carpeta si ya existe una copia en otro lado
component() {  # $1 raíz, $2 destino, $3 id, $4 salida
  pkgbuild --analyze --root "$1" "$WORK/$3.plist" >/dev/null
  local i=0
  while /usr/libexec/PlistBuddy -c "Print :$i:RootRelativeBundlePath" "$WORK/$3.plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$WORK/$3.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$WORK/$3.plist"
    i=$((i+1))
  done
  COPYFILE_DISABLE=1 pkgbuild --root "$1" --component-plist "$WORK/$3.plist" --install-location "$2" --identifier "$3" \
           --version "$VERSION" --scripts "$WORK/scripts" "$4" >/dev/null
}
component "$PLUG" "/Library/Audio/Plug-Ins"     "$PKG_ID.plugins" "$WORK/plugins.pkg"
component "$APP"  "/Applications/CCR" "$PKG_ID.app"     "$WORK/app.pkg"

# Pantallas del instalador en español y en inglés (macOS muestra las del idioma del sistema)
ES="$WORK/resources/es.lproj"; EN="$WORK/resources/en.lproj"; mkdir -p "$ES" "$EN"
ARCH_EN="$( [[ "$ARCH_LABEL" == "Mac" ]] && echo "any Mac (<b>Intel or Apple Silicon M1/M2/M3/M4</b>)" || echo "a Mac with an <b>Intel</b> processor")"
cat > "$ES/welcome.html" <<EOF
<html><body style="font-family: -apple-system, Helvetica; font-size: 13px">
<h2>CCRing</h2>
<p>Sintetizador de modelado físico de <b>CCR</b> — <b>gratis y de código libre</b>.</p>
<p>CCRing es la nueva versión de <b>URing</b>: si tenés URing instalado, lo reemplaza (tus proyectos lo abren con CCRing).</p>
<p>Se instalará para todos los usuarios de este Mac:</p>
<ul>
<li><b>VST3</b> → /Library/Audio/Plug-Ins/VST3/CCRing.vst3</li>
<li><b>Audio Unit</b> → /Library/Audio/Plug-Ins/Components/CCRing.component</li>
<li><b>CLAP</b> → /Library/Audio/Plug-Ins/CLAP/CCRing.clap (Bitwig, Reaper…)</li>
<li><b>App Standalone</b> → Aplicaciones/CCR/CCRing.app</li>
</ul>
<p>Requisitos: $ARCH_TEXT, macOS 11 o superior.</p>
<p>Después de instalar, re-escaneá los plug-ins en tu DAW (Ableton: <i>Preferencias → Plug-ins → Re-escanear</i>).
CCRing arranca en inglés: para pasarlo a español tocá el logo → LANGUAGE → ESPAÑOL.</p>
</body></html>
EOF
cat > "$EN/welcome.html" <<EOF
<html><body style="font-family: -apple-system, Helvetica; font-size: 13px">
<h2>CCRing</h2>
<p>Physical-modelling synth by <b>CCR</b> — <b>free and open source</b>.</p>
<p>CCRing is the new version of <b>URing</b>: if URing is installed, it is replaced (your projects open with CCRing).</p>
<p>It will be installed for all users of this Mac:</p>
<ul>
<li><b>VST3</b> → /Library/Audio/Plug-Ins/VST3/CCRing.vst3</li>
<li><b>Audio Unit</b> → /Library/Audio/Plug-Ins/Components/CCRing.component</li>
<li><b>CLAP</b> → /Library/Audio/Plug-Ins/CLAP/CCRing.clap (Bitwig, Reaper…)</li>
<li><b>Standalone app</b> → Applications/CCR/CCRing.app</li>
</ul>
<p>Requirements: $ARCH_EN, macOS 11 or later.</p>
<p>After installing, rescan plug-ins in your DAW (Ableton: <i>Settings → Plug-Ins → Rescan</i>).</p>
</body></html>
EOF
cat > "$ES/license.html" <<EOF
<html><body style="font-family: -apple-system, Helvetica; font-size: 12px">
<h3>CCRing es software libre</h3>
<p>CCRing © 2026 CCR se distribuye bajo la <b>GNU Affero General Public License v3.0</b>.
Podés usarlo para hacer música (también comercial), compartirlo y modificarlo. Si distribuís una
versión modificada, tiene que seguir siendo libre bajo la misma licencia. Lo que hagas con CCRing es tuyo.</p>
<p>CCRing se entrega <b>"tal cual", sin ninguna garantía</b>. Texto completo de la licencia:
archivo LICENSE.txt (en el zip) o https://www.gnu.org/licenses/agpl-3.0.html</p>
<p><b>Créditos:</b> el motor C, TEXTURE y MARBLES usan código de Plaits, Clouds y Marbles de Mutable Instruments (Émilie Gillet, licencia MIT).
Interfaz con JUCE (AGPLv3). Tipografía Montserrat (SIL OFL). CCRing no está afiliado a Mutable Instruments.</p>
</body></html>
EOF
cat > "$EN/license.html" <<EOF
<html><body style="font-family: -apple-system, Helvetica; font-size: 12px">
<h3>CCRing is free software</h3>
<p>CCRing © 2026 CCR is distributed under the <b>GNU Affero General Public License v3.0</b>.
You can use it to make music (commercial music too), share it and modify it. If you distribute a
modified version, it must stay free under the same licence. What you make with CCRing is yours.</p>
<p>CCRing is provided <b>"as is", without any warranty</b>. Full licence text:
LICENSE.txt (in the zip) or https://www.gnu.org/licenses/agpl-3.0.html</p>
<p><b>Credits:</b> engine C, TEXTURE and MARBLES use code from Plaits, Clouds and Marbles by Mutable Instruments (Émilie Gillet, MIT licence).
Interface built with JUCE (AGPLv3). Montserrat typeface (SIL OFL). CCRing is not affiliated with Mutable Instruments.</p>
</body></html>
EOF
cat > "$ES/conclusion.html" <<EOF
<html><body style="font-family: -apple-system, Helvetica; font-size: 13px">
<h2>¡Listo!</h2>
<p>Abrí tu DAW y re-escaneá los plug-ins. También podés abrir <b>Aplicaciones → CCR → CCRing</b> para tocarlo sin DAW.</p>
<p>CCRing es gratis. Si te gusta, podés apoyarlo con una donación en <b>paypal.me/JavierCazou</b> o por Mercado Pago al alias <b>nodrama.perro</b> (o desde el plugin: logo de CCRing → <b>DONATE</b>).</p>
</body></html>
EOF
cat > "$EN/conclusion.html" <<EOF
<html><body style="font-family: -apple-system, Helvetica; font-size: 13px">
<h2>All done!</h2>
<p>Open your DAW and rescan plug-ins. You can also open <b>Applications → CCR → CCRing</b> to play it without a DAW.</p>
<p>CCRing is free. If you like it, you can support it with a donation at <b>paypal.me/JavierCazou</b> or, in Argentina, by Mercado Pago to the alias <b>nodrama.perro</b> (or from the plug-in: CCRing logo → <b>DONATE</b>).</p>
</body></html>
EOF

# 3. Instalador
mkdir -p "$DIST_DIR"
PKG="$DIST_DIR/$NAME-$ARCH_LABEL.pkg"
log "Creando ${PKG}…"
cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>CCRing</title>
  <background file="installer_logo.png" alignment="bottomleft" scaling="none"/>
  <background-darkAqua file="installer_logo.png" alignment="bottomleft" scaling="none"/>
  <welcome file="welcome.html"/>
  <license file="license.html"/>
  <conclusion file="conclusion.html"/>
  <options customize="allow" require-scripts="false" hostArchitectures="$HOST_ARCHS"/>
  <volume-check><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
  <choices-outline><line choice="plugins"/><line choice="app"/></choices-outline>
  <choice id="plugins" title="CCRing VST3 + Audio Unit + CLAP" description="Plug-ins for your DAW / Plugins para tu DAW (/Library/Audio/Plug-Ins).">
    <pkg-ref id="$PKG_ID.plugins"/>
  </choice>
  <choice id="app" title="CCRing (Standalone app)" description="Play CCRing without a DAW / Para tocar CCRing sin DAW (Applications/CCR).">
    <pkg-ref id="$PKG_ID.app"/>
  </choice>
  <pkg-ref id="$PKG_ID.plugins" version="$VERSION" onConclusion="none">plugins.pkg</pkg-ref>
  <pkg-ref id="$PKG_ID.app" version="$VERSION" onConclusion="none">app.pkg</pkg-ref>
</installer-gui-script>
EOF
cp "$PROJECT_DIR/Resources/installer_logo.png" "$WORK/resources/installer_logo.png"   # logo de CCR en la ventana del instalador
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" --package-path "$WORK" "$PKG" >/dev/null
# Icono de CCR en el instalador y el zip (Finder)
if command -v swiftc >/dev/null; then
  swiftc -O -o "$WORK/seticon" "$PROJECT_DIR/Tools/seticon.swift" 2>/dev/null && ICON_OK=1 || ICON_OK=0
else ICON_OK=0; fi
ok "Instalador: $PKG"

# 4. Zip para compartir
Z="$WORK/zip"
cp "$PKG" "$Z/"
cp "$PROJECT_DIR/LICENSE" "$Z/LICENSE.txt"
cp "$PROJECT_DIR/CREDITOS.md" "$Z/CREDITOS.txt"
cp "$PROJECT_DIR/docs/CCRing_Manual_EN.pdf" "$PROJECT_DIR/docs/CCRing_Manual_ES.pdf" "$Z/"
cp "$PROJECT_DIR/mac/Allow CCRing.command" "$Z/"
chmod +x "$Z/Allow CCRing.command"
sed -e "s|@VERSION@|$VERSION|g" -e "s|@PKG@|$(basename "$PKG")|g" \
    -e "s|@ARCH@|$( [[ "$ARCH_LABEL" == "Mac" ]] && echo "Mac Intel o Apple Silicon (M1/M2/M3/M4)" || echo "Mac Intel")|g" \
    "$PROJECT_DIR/mac/LEEME.txt" > "$Z/LEEME.txt"
sed -e "s|@VERSION@|$VERSION|g" -e "s|@PKG@|$(basename "$PKG")|g" \
    -e "s|@ARCH_EN@|$( [[ "$ARCH_LABEL" == "Mac" ]] && echo "Intel or Apple Silicon Mac (M1/M2/M3/M4)" || echo "Intel Mac")|g" \
    "$PROJECT_DIR/mac/README.txt" > "$Z/README.txt"
ZIP="$DIST_DIR/$NAME-$ARCH_LABEL.zip"
rm -f "$ZIP"
( cd "$Z" && COPYFILE_DISABLE=1 zip -q -X -r "$ZIP" . )
[[ "${ICON_OK:-0}" == 1 ]] && "$WORK/seticon" "$PROJECT_DIR/Resources/ccr_app_icon_1024.png" "$PKG" "$ZIP" >/dev/null
ok "Para compartir: $ZIP"
