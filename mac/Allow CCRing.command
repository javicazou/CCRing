#!/bin/bash
# Allow CCRing / Permitir CCRing — only if, after installing, your DAW does not show CCRing or macOS blocks it.
# Usalo solo si, después de instalar, tu DAW no muestra CCRing o macOS lo bloquea.
# Le quita a CCRing la marca de "descargado de internet" (cuarentena) y refresca los Audio Units.
# No cambia ninguna otra cosa de tu Mac.

echo ""
echo "  CCRing — CCR"
echo "  This removes the macOS quarantine flag from CCRing so your DAW can load it."
echo "  It will ask for your Mac password (it is not shown while you type)."
echo "  Esto le quita la cuarentena de macOS a CCRing para que tu DAW lo pueda cargar."
echo "  Te va a pedir la contraseña de tu Mac (no se ve mientras la escribís)."
echo ""

FOUND=0
for p in "/Library/Audio/Plug-Ins/VST3/CCRing.vst3" \
         "/Library/Audio/Plug-Ins/Components/CCRing.component" \
         "$HOME/Library/Audio/Plug-Ins/VST3/CCRing.vst3" \
         "$HOME/Library/Audio/Plug-Ins/Components/CCRing.component" \
         "/Applications/CCR/CCRing.app"; do
  if [ -e "$p" ]; then
    sudo xattr -dr com.apple.quarantine "$p" && echo "  ✓ $p"
    FOUND=1
  fi
done

if [ "$FOUND" = "0" ]; then
  echo "  CCRing is not installed yet: install the .pkg first. / No encontré CCRing instalado: primero instalá el .pkg."
else
  killall -9 AudioComponentRegistrar >/dev/null 2>&1
  echo ""
  echo "  Done! Quit and reopen your DAW, and rescan plug-ins."
  echo "  ¡Listo! Cerrá y volvé a abrir tu DAW, y re-escaneá los plug-ins."
fi
echo ""
read -n 1 -s -r -p "  Press any key to close / Tocá cualquier tecla para cerrar."
echo ""
