#!/usr/bin/env bash
# =============================================================================
#  CCRing — compilación e instalación automática para macOS
#  VST3 + AU, Universal Binary (arm64 + x86_64), JUCE vía CMake FetchContent.
#
#  Uso:
#    ./install.sh              # compila e instala en ~/Library/Audio/Plug-Ins
#    ./install.sh --system     # instala en /Library/Audio/Plug-Ins (pide sudo)
#    ./install.sh --native     # solo la arquitectura de este Mac (compila más rápido)
#    ./install.sh --clean      # borra build/ antes de compilar
#    ./install.sh --no-auval   # no valida el Audio Unit con auval
# =============================================================================
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
TOOLS_DIR="$PROJECT_DIR/.tools"
[[ -d "$PROJECT_DIR/../.tools/venv" ]] && TOOLS_DIR="$PROJECT_DIR/../.tools"   # comparte herramientas con Cazotone
TARGET="CCRing"
CONFIG="Release"
AU_TYPE="aumu"; AU_SUBTYPE="Urng"; AU_MANU="Jvto"   # deben coincidir con CMakeLists.txt

SCOPE="user"
CLEAN=0
RUN_AUVAL=1
ARCHS="arm64;x86_64"

log()  { printf '\033[1;36m==>\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m ✓\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m !\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m ✗ %s\033[0m\n' "$*" >&2; exit 1; }

usage() { sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --system)   SCOPE="system" ;;
    --native)   ARCHS="$(uname -m)" ;;
    --clean)    CLEAN=1 ;;
    --no-auval) RUN_AUVAL=0 ;;
    -h|--help)  usage; exit 0 ;;
    *)          usage; die "Opción desconocida: $1" ;;
  esac
  shift
done

[[ "$(uname -s)" == "Darwin" ]] || die "Este script es solo para macOS."

# -----------------------------------------------------------------------------
# 1. Xcode Command Line Tools (compilador, SDK, git)
# -----------------------------------------------------------------------------
log "Comprobando Xcode Command Line Tools…"
if ! xcode-select -p >/dev/null 2>&1; then
  warn "No están instaladas. Abriendo el instalador de Apple…"
  xcode-select --install || true
  die "Termina la instalación en la ventana emergente y vuelve a ejecutar ./install.sh"
fi

# Caso frecuente tras actualizar macOS: la carpeta existe pero está rota (falta xcrun)
if ! xcrun --find clang++ >/dev/null 2>&1; then
  DEV_DIR="$(xcode-select -p)"
  warn "La instalación de herramientas en '$DEV_DIR' está incompleta (xcrun/clang no responden)."
  if [[ -d /Applications/Xcode.app ]]; then
    echo "    Solución: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer"
  fi
  if [[ "$DEV_DIR" == "/Library/Developer/CommandLineTools" && -t 0 ]]; then
    read -r -p "    ¿Reinstalar las Command Line Tools ahora? (borra $DEV_DIR con sudo) [s/N] " ans
    if [[ "$ans" =~ ^[sSyY]$ ]]; then
      sudo rm -rf /Library/Developer/CommandLineTools
      xcode-select --install || true
      die "Termina la instalación en la ventana emergente y vuelve a ejecutar ./install.sh"
    fi
  fi
  die "Repara las herramientas: sudo rm -rf /Library/Developer/CommandLineTools && xcode-select --install"
fi
command -v git >/dev/null 2>&1 || die "git no disponible (forma parte de las Command Line Tools)."
ok "Compilador: $(xcrun --find clang++)"

# -----------------------------------------------------------------------------
# 2. CMake y Ninja  (orden: ya instalado → Homebrew → venv de Python local)
# -----------------------------------------------------------------------------
if ! command -v brew >/dev/null 2>&1; then
  for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do
    if [[ -x "$b" ]]; then eval "$("$b" shellenv)"; break; fi
  done
fi
[[ -x "$TOOLS_DIR/venv/bin/cmake" ]] && export PATH="$TOOLS_DIR/venv/bin:$PATH"

install_tool() {
  local tool="$1"
  command -v "$tool" >/dev/null 2>&1 && return 0
  if command -v brew >/dev/null 2>&1; then
    log "Instalando $tool con Homebrew…"
    brew install "$tool" && return 0
  fi
  log "Instalando $tool en un entorno Python local ($TOOLS_DIR/venv)…"
  if [[ ! -x "$TOOLS_DIR/venv/bin/pip" ]]; then
    python3 -m venv "$TOOLS_DIR/venv" || return 1
  fi
  "$TOOLS_DIR/venv/bin/pip" install --quiet --upgrade "$tool" || return 1
  export PATH="$TOOLS_DIR/venv/bin:$PATH"
}

install_tool cmake || die "No se pudo instalar CMake. Instálalo desde https://cmake.org/download/ o con Homebrew (https://brew.sh)."

CMAKE_VER="$(cmake --version | head -n1 | awk '{print $3}')"
IFS=. read -r CM_MAJ CM_MIN _ <<<"$CMAKE_VER"
if (( CM_MAJ < 3 || (CM_MAJ == 3 && CM_MIN < 22) )); then
  die "Se necesita CMake >= 3.22 (tienes $CMAKE_VER). Actualízalo: brew upgrade cmake"
fi
ok "CMake $CMAKE_VER"

GENERATOR="Unix Makefiles"
if install_tool ninja; then
  GENERATOR="Ninja"
  ok "Ninja $(ninja --version)"
else
  warn "Ninja no disponible; se usará Make."
fi

# -----------------------------------------------------------------------------
# 3. Configurar (descarga JUCE con FetchContent) y compilar en Release
# -----------------------------------------------------------------------------
if [[ $CLEAN -eq 1 ]]; then
  log "Limpiando $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

# Si cambia el generador, la caché anterior no sirve
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]] && ! grep -q "CMAKE_GENERATOR:INTERNAL=$GENERATOR\$" "$BUILD_DIR/CMakeCache.txt"; then
  warn "Generador distinto al de la compilación anterior: regenerando build/"
  rm -rf "$BUILD_DIR"
fi

# Reutiliza el JUCE ya descargado por Cazotone si está al lado (evita otra descarga)
JUCE_ARGS=()
if [[ -d "$PROJECT_DIR/../build/_deps/juce-src/modules" ]]; then
  JUCE_ARGS=(-DFETCHCONTENT_SOURCE_DIR_JUCE="$(cd "$PROJECT_DIR/../build/_deps/juce-src" && pwd)")
fi

log "Configurando ($GENERATOR, $CONFIG, arquitecturas: $ARCHS)…"
log "La primera vez se descarga JUCE desde GitHub (puede tardar unos minutos)."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  "${JUCE_ARGS[@]+"${JUCE_ARGS[@]}"}"

JOBS="$(sysctl -n hw.ncpu)"
log "Compilando VST3 + AU con $JOBS hilos…"
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS" \
  --target "${TARGET}_VST3" "${TARGET}_AU"

ARTEFACTS="$BUILD_DIR/${TARGET}_artefacts/$CONFIG"
VST3_SRC="$ARTEFACTS/VST3/${TARGET}.vst3"
AU_SRC="$ARTEFACTS/AU/${TARGET}.component"
[[ -d "$VST3_SRC" ]] || die "No se encontró $VST3_SRC"
[[ -d "$AU_SRC"   ]] || die "No se encontró $AU_SRC"

BIN="$AU_SRC/Contents/MacOS/$TARGET"
[[ -f "$BIN" ]] && ok "Binario: $(lipo -archs "$BIN" 2>/dev/null || echo '?')"

# -----------------------------------------------------------------------------
# 4. Instalar
# -----------------------------------------------------------------------------
if [[ "$SCOPE" == "system" ]]; then
  DEST="/Library/Audio/Plug-Ins"
  run() { sudo "$@"; }
  log "Instalación global en $DEST (se pedirá tu contraseña)"
else
  DEST="$HOME/Library/Audio/Plug-Ins"
  run() { "$@"; }
fi

install_bundle() {
  local src="$1" dir="$2"
  local dst="$dir/$(basename "$src")"
  run mkdir -p "$dir"
  run rm -rf "$dst"
  run ditto "$src" "$dst"
  run xattr -dr com.apple.quarantine "$dst" 2>/dev/null || true
  run codesign --force --sign - "$dst" >/dev/null 2>&1 || warn "No se pudo firmar (ad-hoc) $dst"
  ok "Instalado: $dst"
}

install_bundle "$VST3_SRC" "$DEST/VST3"
install_bundle "$AU_SRC"   "$DEST/Components"

# -----------------------------------------------------------------------------
# 5. Forzar a macOS a registrar los nuevos Audio Units
# -----------------------------------------------------------------------------
log "Refrescando la caché de Audio Units…"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
rm -f "$HOME/Library/Caches/AudioUnitCache/com.apple.audiounits.cache" 2>/dev/null || true
sleep 2

if [[ $RUN_AUVAL -eq 1 ]]; then
  log "Validando el AU con auval ($AU_TYPE $AU_SUBTYPE $AU_MANU)…"
  AUVAL_LOG="$BUILD_DIR/auval.log"
  if auval -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANU" >"$AUVAL_LOG" 2>&1; then
    ok "auval: AU VALIDATION SUCCEEDED"
  else
    warn "auval no pasó la validación; detalles en $AUVAL_LOG"
  fi
fi

echo
ok "¡Listo! CCRing instalado en $DEST"
echo "   • Ableton Live: Preferencias → Plug-ins → Re-escanear (activar 'Use VST3 Plug-in System Folders')"
echo "   • Logic Pro:    Ajustes → Plug-in Manager → Restablecer y reescanear selección"
echo "   • Reaper:       Preferences → Plug-ins → VST → Re-scan"
