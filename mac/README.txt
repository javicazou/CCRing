URing @VERSION@ — Monstri Audio
Free and open-source physical-modelling synth.
For: @ARCH_EN@, macOS 11 or later.

INSTALL
1. Double-click @PKG@ and follow the steps (it asks for your Mac password).
2. If macOS says it "cannot verify the developer":
     URing is not signed by Apple because it is a free project; this is normal.
     - macOS 13 or newer: System Settings → Privacy & Security →
       scroll down → "Open Anyway".
     - macOS 11/12: right-click (or Control-click) the .pkg → Open → Open.
3. Open your DAW and rescan plug-ins:
     - Ableton Live: Settings → Plug-Ins → enable "Use VST3 Plug-in System
       Folders" and "Use Audio Units" → Rescan.
     - Logic Pro: Settings → Plug-in Manager → Reset & Rescan Selection.
     - Reaper: Preferences → Plug-ins → VST → Re-scan.
   URing appears under "Monstri Audio".
4. Without a DAW: Applications → Monstri Audio → URing.

THE PLUG-IN DOES NOT SHOW UP OR MACOS BLOCKS IT?
Double-click "Allow URing.command" (if macOS blocks that too: right-click → Open).
It removes the macOS quarantine flag from URing. Then quit and reopen your DAW.

LANGUAGE
URing starts in English. For Spanish: click the URing logo (top left) →
LANGUAGE → ESPAÑOL. Your choice is remembered.

WHAT IS IN THIS FOLDER
- @PKG@        the installer
- URing_Manual_EN.pdf     manual in English (9 pages, with screenshots)
- URing_Manual_ES.pdf     manual in Spanish
- Allow URing.command     only if the plug-in does not show up
- LICENSE.txt             GNU AGPL-3.0 licence
- CREDITOS.txt            third-party credits
- LEEME.txt               these instructions in Spanish

UNINSTALL
Delete /Library/Audio/Plug-Ins/VST3/URing.vst3,
/Library/Audio/Plug-Ins/Components/URing.component
and Applications/Monstri Audio/URing.app

FREE AND OPEN SOURCE
URing is free software (GNU AGPL-3.0): use it in your music (commercial music too),
share it and improve it. If it helps you, you can support it with a donation:
https://paypal.me/JavierCazou
(or from the plug-in: click the URing logo, top left → DONATE).

CREDITS
Engine C, the TEXTURE page and MARBLES mode use the code of Plaits, Clouds and
Marbles by Mutable Instruments (Émilie Gillet), MIT licence. Interface built with
JUCE (AGPLv3). Montserrat typeface (SIL OFL).
URing is not affiliated with Mutable Instruments.
