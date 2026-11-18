CCRing @VERSION@ — CCR
Free and open-source physical-modelling synth.
For: @ARCH_EN@, macOS 11 or later.

INSTALL
1. Double-click @PKG@ and follow the steps (it asks for your Mac password).
2. If macOS says it "cannot verify the developer":
     CCRing is not signed by Apple because it is a free project; this is normal.
     - macOS 13 or newer: System Settings → Privacy & Security →
       scroll down → "Open Anyway".
     - macOS 11/12: right-click (or Control-click) the .pkg → Open → Open.
3. Open your DAW and rescan plug-ins:
     - Ableton Live: Settings → Plug-Ins → enable "Use VST3 Plug-in System
       Folders" and "Use Audio Units" → Rescan.
     - Logic Pro: Settings → Plug-in Manager → Reset & Rescan Selection.
     - Reaper: Preferences → Plug-ins → VST → Re-scan.
   CCRing appears under "CCR".
4. Without a DAW: Applications → CCR → CCRing.

THE PLUG-IN DOES NOT SHOW UP OR MACOS BLOCKS IT?
Double-click "Allow CCRing.command" (if macOS blocks that too: right-click → Open).
It removes the macOS quarantine flag from CCRing. Then quit and reopen your DAW.

LANGUAGE
CCRing starts in English. For Spanish: click the CCRing logo (top left) →
LANGUAGE → ESPAÑOL. Your choice is remembered.

WHAT IS IN THIS FOLDER
- @PKG@        the installer
- CCRing_Manual_EN.pdf     manual in English (9 pages, with screenshots)
- CCRing_Manual_ES.pdf     manual in Spanish
- Allow CCRing.command     only if the plug-in does not show up
- LICENSE.txt             GNU AGPL-3.0 licence
- CREDITOS.txt            third-party credits
- LEEME.txt               these instructions in Spanish

UNINSTALL
Delete /Library/Audio/Plug-Ins/VST3/CCRing.vst3,
/Library/Audio/Plug-Ins/Components/CCRing.component
and Applications/CCR/CCRing.app

FREE AND OPEN SOURCE
CCRing is free software (GNU AGPL-3.0): use it in your music (commercial music too),
share it and improve it. If it helps you, you can support it with a donation:
https://paypal.me/JavierCazou  ·  Mercado Pago (Argentina): alias nodrama.perro
(or from the plug-in: click the CCRing logo, top left → DONATE).

CREDITS
Engine C, the TEXTURE page and MARBLES mode use the code of Plaits, Clouds and
Marbles by Mutable Instruments (Émilie Gillet), MIT licence. Interface built with
JUCE (AGPLv3). Montserrat typeface (SIL OFL).
CCRing is not affiliated with Mutable Instruments.
