// URing manual in English (A4) — same layout as make_manual.swift.
// Manual de URing en PDF (A4): capturas reales del plugin + texto. Solo usa frameworks de macOS.
// Uso: make_manual <carpeta de capturas look_*.png> <Montserrat-Bold.ttf> <salida.pdf>
import AppKit
import CoreText
import Foundation

let args = CommandLine.arguments
guard args.count >= 4 else { print("uso: make_manual <capturas> <fuente.ttf> <salida.pdf>"); exit(1) }
let shots = URL(fileURLWithPath: args[1])
CTFontManagerRegisterFontsForURL(URL(fileURLWithPath: args[2]) as CFURL, .process, nil)
let outURL = URL(fileURLWithPath: args[3])

let W: CGFloat = 595, H: CGFloat = 842, M: CGFloat = 40
let bg = NSColor(calibratedRed: 0.055, green: 0.06, blue: 0.067, alpha: 1)
let text = NSColor(calibratedWhite: 0.92, alpha: 1)
let dim = NSColor(calibratedRed: 0.64, green: 0.66, blue: 0.69, alpha: 1)
let orange = NSColor(calibratedRed: 1.0, green: 0.416, blue: 0.169, alpha: 1)
let cyan = NSColor(calibratedRed: 0.247, green: 0.847, blue: 1.0, alpha: 1)
let green = NSColor(calibratedRed: 0.482, green: 0.89, blue: 0.647, alpha: 1)
func mont(_ s: CGFloat) -> NSFont { NSFont(name: "Montserrat-Bold", size: s) ?? NSFont.boldSystemFont(ofSize: s) }
func body(_ s: CGFloat) -> NSFont { NSFont(name: "HelveticaNeue", size: s) ?? NSFont.systemFont(ofSize: s) }
func bodyB(_ s: CGFloat) -> NSFont { NSFont(name: "HelveticaNeue-Bold", size: s) ?? NSFont.boldSystemFont(ofSize: s) }

var mediaBox = CGRect(x: 0, y: 0, width: W, height: H)
guard let ctx = CGContext(outURL as CFURL, mediaBox: &mediaBox, nil) else { print("no pude crear el PDF"); exit(1) }
var pageNo = 0

// Texto con estilo simple: líneas que empiezan con "## " = subtítulo, "- " = viñeta, "**x**" = negrita al inicio
func styled(_ src: String, size: CGFloat = 9.6) -> NSAttributedString {
    let out = NSMutableAttributedString()
    for raw in src.components(separatedBy: "\n") {
        let para = NSMutableParagraphStyle()
        para.paragraphSpacing = 3.5
        para.lineSpacing = 1.2
        var line = raw
        var attrs: [NSAttributedString.Key: Any] = [.font: body(size), .foregroundColor: text]
        if line.hasPrefix("## ") {
            line = String(line.dropFirst(3)).uppercased()
            attrs = [.font: mont(size + 0.4), .foregroundColor: orange, .kern: 0.6]
            para.paragraphSpacingBefore = 7
        } else if line.hasPrefix("- ") {
            line = "•  " + line.dropFirst(2)
            para.headIndent = 11
            para.firstLineHeadIndent = 0
        }
        attrs[.paragraphStyle] = para
        let piece = NSMutableAttributedString(string: line + "\n", attributes: attrs)
        // **negrita**
        let ns = piece.string as NSString
        var searchFrom = 0
        while true {
            let a = ns.range(of: "**", options: [], range: NSRange(location: searchFrom, length: ns.length - searchFrom))
            if a.location == NSNotFound { break }
            let b = ns.range(of: "**", options: [], range: NSRange(location: a.location + 2, length: ns.length - a.location - 2))
            if b.location == NSNotFound { break }
            piece.addAttributes([.font: bodyB(size), .foregroundColor: NSColor.white], range: NSRange(location: a.location, length: b.location - a.location))
            searchFrom = b.location + 2
        }
        // quitar las marcas **
        while let r = piece.string.range(of: "**") { piece.deleteCharacters(in: NSRange(r, in: piece.string)) }
        out.append(piece)
    }
    return out
}

func drawText(_ s: NSAttributedString, in r: CGRect) {
    let fs = CTFramesetterCreateWithAttributedString(s)
    let path = CGPath(rect: r, transform: nil)
    let frame = CTFramesetterCreateFrame(fs, CFRange(location: 0, length: 0), path, nil)
    CTFrameDraw(frame, ctx)
    let visible = CTFrameGetVisibleStringRange(frame)
    if visible.length < s.length { print("⚠️  texto cortado en la página \(pageNo): faltan \(s.length - visible.length) caracteres") }
}

func line(_ str: String, font: NSFont, color: NSColor, at p: CGPoint, kern: CGFloat = 0) {
    let a = NSAttributedString(string: str, attributes: [.font: font, .foregroundColor: color, .kern: kern])
    let l = CTLineCreateWithAttributedString(a)
    ctx.textPosition = p
    CTLineDraw(l, ctx)
}

func image(_ name: String) -> CGImage? {
    guard let src = CGImageSourceCreateWithURL(shots.appendingPathComponent(name) as CFURL, nil) else { return nil }
    return CGImageSourceCreateImageAtIndex(src, 0, nil)
}

// Imagen con borde y sombra; devuelve la altura usada
@discardableResult
func drawShot(_ name: String, top: CGFloat, width: CGFloat = W - 2 * M, crop: CGRect? = nil) -> CGFloat {
    guard var img = image(name) else { return 0 }
    if let c = crop, let cc = img.cropping(to: c) { img = cc }
    let h = width * CGFloat(img.height) / CGFloat(img.width)
    let r = CGRect(x: (W - width) / 2, y: H - top - h, width: width, height: h)
    ctx.saveGState()
    ctx.setShadow(offset: CGSize(width: 0, height: -3), blur: 10, color: NSColor.black.withAlphaComponent(0.7).cgColor)
    ctx.setFillColor(NSColor.black.cgColor); ctx.fill(r)
    ctx.restoreGState()
    ctx.draw(img, in: r)
    ctx.setStrokeColor(NSColor(calibratedWhite: 1, alpha: 0.12).cgColor); ctx.setLineWidth(0.6); ctx.stroke(r)
    return h
}

func beginPage(_ title: String, _ kicker: String, colour: NSColor = orange) {
    pageNo += 1
    ctx.beginPDFPage(nil)
    ctx.setFillColor(bg.cgColor); ctx.fill(mediaBox)
    ctx.setFillColor(colour.cgColor); ctx.fill(CGRect(x: M, y: H - 58, width: 4, height: 24))
    line(title.uppercased(), font: mont(19), color: .white, at: CGPoint(x: M + 13, y: H - 52), kern: 1.2)
    line(kicker.uppercased(), font: mont(7.5), color: dim, at: CGPoint(x: M + 14, y: H - 70), kern: 1.4)
    // pie
    ctx.setFillColor(NSColor(calibratedWhite: 1, alpha: 0.08).cgColor); ctx.fill(CGRect(x: M, y: 34, width: W - 2 * M, height: 0.6))
    line("URING 1.5  ·  MONSTRI AUDIO", font: mont(6.5), color: dim, at: CGPoint(x: M, y: 22), kern: 1.4)
    line("\(pageNo)", font: mont(7), color: dim, at: CGPoint(x: W - M - 8, y: 22))
}
func endPage() { ctx.endPDFPage() }

// Dos columnas de texto debajo de la imagen
func columns(_ left: String, _ right: String, top: CGFloat, size: CGFloat = 9.6) {
    let colW = (W - 2 * M - 18) / 2, h = H - top - 46
    drawText(styled(left, size: size), in: CGRect(x: M, y: 46, width: colW, height: h))
    drawText(styled(right, size: size), in: CGRect(x: M + colW + 18, y: 46, width: colW, height: h))
}

// ==================================================================== COVER
pageNo += 1
ctx.beginPDFPage(nil)
ctx.setFillColor(bg.cgColor); ctx.fill(mediaBox)
ctx.setFillColor(orange.withAlphaComponent(0.35).cgColor); ctx.fillEllipse(in: CGRect(x: M, y: H - 128, width: 34, height: 34))
ctx.setFillColor(orange.cgColor); ctx.fillEllipse(in: CGRect(x: M + 8, y: H - 120, width: 18, height: 18))
line("URING", font: mont(54), color: .white, at: CGPoint(x: M + 48, y: H - 130), kern: 12)
line("MANUAL  ·  VERSION 1.5", font: mont(11), color: orange, at: CGPoint(x: M + 2, y: H - 162), kern: 3)
line("Physical-modelling instrument + 24 synthesis models, with a sequencer built for minimal house", font: body(11.5), color: dim, at: CGPoint(x: M + 2, y: H - 182))
drawShot("look_macro.png", top: 215)
drawText(styled("""
## What is URing
URing is an instrument that **uses no samples**: every note strikes, plucks, bows or blows simulated objects — strings, wood, metal, drum skins — and sounds the way that object would. It has **three engines**: A (resonator), B (impact) and C (ENGINE C, 24 synthesis models), two Euclidean sequencers, studio effects, a granular TEXTURE page and a page for playing live.
## How to read this manual
One page per tab: **SYNTH, FX, SEQ, LIVE, ENGINE C and TEXTURE**, plus presets, motion recording and tips for minimal / rominimal. Words in CAPITALS are the names you see in the plugin. URing can also be shown in Spanish: click the URing logo and choose **ESPAÑOL** under LANGUAGE.
""", size: 10.5), in: CGRect(x: M, y: 60, width: W - 2 * M, height: 230))
line("MONSTRI AUDIO", font: mont(8), color: dim, at: CGPoint(x: M, y: 30), kern: 2)
ctx.endPDFPage()

// ==================================================================== GETTING STARTED
beginPage("Getting started", "install, load and the top bar")
let hb = drawShot("look_browser.png", top: 88)
columns("""
## Install
- **Mac:** double-click the .pkg (if macOS warns about an "unidentified developer": right-click > Open, or System Settings > Privacy & Security > Open Anyway).
- **Windows:** see README.txt inside the zip.
- In **Ableton Live**: Settings > Plug-Ins > Rescan. URing shows up under **Monstri Audio** (VST3 and Audio Units).
- Load it on a **MIDI track**. It always starts at **−20 dB**.
## Top bar (left to right)
- **Logo:** credits, shortcuts, donations and **language** (English / Español).
- **SYNTH · FX · SEQ · LIVE · ENGINE C · TEXTURE:** the six pages.
- **List icon:** opens the **preset browser** (Cmd+F).
- **< name >:** previous / next preset / menu.
""", """
- **Curved arrows:** **undo / redo** (Cmd+Z / Cmd+Shift+Z), up to 100 steps: knobs, steps, cables and presets.
- **SAVE:** stores your version as a user preset. An **asterisk *** means you changed something; the original is never overwritten.
- **REC + length:** records knob movements (see the last page).
- **Meter:** output level; turns red when it clips.
- **Display:** shows the knob you touch; at rest, the preset and the live waveform.
## Preset browser
Categories on the left, search on top, **star = favourite** (it is remembered). Click loads, the **arrow keys** audition one by one, **Enter / Esc** closes. **PREVIEW** plays a note every time you pick one.
- **165 factory presets** in 13 categories: Resonator, Impact, Hybrid, Glitch, Sequence, Drone, Acoustic, Live, Macro, **Bass**, **Pad**, **Keys** and **Perc** (kicks, hats, rims and claves for minimal).
## Computer keyboard
**A W S E D F T G Y H U J K** play notes, **Z / X** octave, **C / V** velocity. Right-click a knob: **MIDI Learn**. Double-click: default value.
""", top: 88 + hb + 14)
endPage()

// ==================================================================== SYNTH
beginPage("SYNTH", "engines A and B, filter, envelope, LFOs and output")
let hs = drawShot("look_synth.png", top: 88)
columns("""
## Engine A · Resonator
- **MODEL:** Modal, String, Sympathetic, Metallic, Piano, Tine Keys, Reed Keys.
- **EXCITER:** how it is played: Pluck, Strike, Bow, Blow, Hammer or **Engine C** (engine C makes the resonator ring).
- **STRUCTURE:** material and overtones. **BRIGHTNESS:** brightness. **DAMPING:** how long it rings. **POSITION:** where you hit it (edge = thin, centre = round).
## Engine B · Impact
- 12 models: Membrane, Bar, Plate, Bell, Tube, Glass, Marimba, Vibes, Mbira, Steel Pan, Hand Drum, Gong.
- **MATERIAL, DECAY, TONE, SPREAD, SWEEP** and its own **PITCH**.
## Mix
- **A / B:** balance between the engines. **A > B FEED:** the string makes the object ring. **VARIATION:** every note slightly different. **WIDTH:** stereo. **TUNE:** fine tuning. **ENGINE C:** level of engine C (0 = off; picking a model on the ENGINE C page turns it on at 50 %).
""", """
## Filter and envelope
- **FILTER:** Low/Band/High Pass and Notch (12 dB), **Ladder 24** (fatter), **Comb** (tuned comb: tubes, metals, flanger; RESO = feedback) and **Formant** (CUTOFF sweeps the vowels A-E-I-O-U). With **ENV AMT**, **DRIVE** and **KEYTRK**.
- **AMP ENV:** Attack, Decay, Sustain, Release.
## LFO 1 and 2 · cables
- Shape, rate or **SYNC** to tempo, destination and **DEPTH**.
- **Cables:** drag from the **OUT** of an LFO or **ENV** to any input (STRUC, BRIGHT, CUT…). Mouse wheel over a cable = amount; right-click = invert or remove. Hover a jack to see what is patched. The **cables** button (top bar, next to the meter) shows them, fades them or hides them: they keep working.
## Output
- **VELOCITY:** how much playing strength matters. **VOLUME:** output. **BODY:** a resonant wooden box. **FREEZE:** freezes the sound (drone).
""", top: 88 + hs + 14)
endPage()

// ==================================================================== FX
beginPage("FX", "chorus, glitch, tape delay, spring reverb and analyser")
let hf = drawShot("look_fx.png", top: 88)
columns("""
## Signal chain
Sound flows in this order: **CHORUS > GLITCH > TAPE DELAY > SPRING REVERB** (the coloured cables show it), then the TEXTURE page.
## Chorus
- **MIX, RATE, DEPTH:** widens and makes electric pianos and pads "breathe".
## Glitch
- Repeats tiny slices in time: **CHANCE** (probability), **LENGTH** (1/8 to 1/64), **VARIATION**, **CRUSH** (lower resolution) and **MIX**.
""", """
## Tape Delay
- **SYNC + DIVISION** to tempo (1/8D is the classic dub delay), **FEEDBACK**, **WOW** (tape), **DRIVE**, **LOW / HIGH CUT**, **PING PONG**, **DUCK** (lowers the echoes while you play) and **MIX**.
## Spring Reverb
- Amp-style spring: **PRE-DELAY, TENSION, DECAY, DAMP, TONE, DRIP** (the "boing") and **MIX**.
## Analyser
- **SCOPE:** waveform. **SPECTRUM:** lows on the left, highs on the right. Stereo meter with peak hold and a red clip warning.
""", top: 88 + hf + 14)
endPage()

// ==================================================================== SEQ
beginPage("SEQ", "two Euclidean sequencers and the step grid")
let hq = drawShot("look_seq.png", top: 88)
columns("""
## Euclidean sequencer
- **STEPS:** length (1 to 64). **PULSES:** how many hits, spread evenly. **ROTATE:** shifts the pattern.
- **DIVISION:** 1/32 to 1/4. **SWING**, **DIRECTION** (forward, backward, ping-pong, random), **FILL**.
- **RUN:** follows the DAW. It starts **off** (also when you load a preset): you switch it on. **MIDI OUT:** sends the notes to another track so you can record them.
## Notes and randomness
- **ROOT, SCALE, OCTAVE, RANGE:** where the notes come from. **GATE** and **PROB**.
- **RANDOM:** **DENSITY**, **AMOUNT**, **RATE**, **TIME** (pushes or pulls) and **LONG / SHORT**.
## Marbles
- With **MARBLES ON** the notes come from a random generator with memory (from Mutable Instruments Marbles), kept inside the scale.
- **DEJA VU:** left = always new, **centre = the loop repeats**, right = jumps around inside the loop. **LENGTH:** loop length (1-16). **SPREAD:** how far the notes wander. **BIAS:** towards low or high notes.
""", """
## SEQ 2
A second pattern, on another track of the same plugin: **SEQ 2 ON**, **DIVISION**, **OCTAVE**. **EDIT** chooses which sequencer the grid shows. On the ENGINE C page you choose whether it plays engine B, C or both.
## The grid
- Lanes: **NOTE, OCT, VELO, GATE, PROB, RTCH** (ratchet), **COND** (1:2, 3:4…) **LOCK** (changes the timbre of each step) and **MOD** (a different engine C model on each step: kick, hat, chord…).
- **PATTERN A–H** + **CHAIN** to chain them, **COPY / PASTE**.
- **RND LANE, RND RHYTHM, RND ALL, FIT SCALE, GENERATE, RESET**.
""", top: 88 + hq + 14)
endPage()

// ==================================================================== TEXTURE
beginPage("TEXTURE", "granular, plate reverb and output", colour: green)
let ht = drawShot("look_texture.png", top: 88)
columns("""
## Texture · granular
Based on Mutable Instruments **Clouds**: it records what is playing and plays it back as grains.
- **MODE:** Granular (clouds of grains), Stretch (time-stretch without changing pitch), Looping Delay and Spectral (freezes and blurs the spectrum).
- **POSITION:** where grains are taken from (further right = further back in time). **SIZE:** grain size. **PITCH:** transposition. **DENSITY:** how many grains (none at the centre). **TEXTURE:** grain shape and filter.
- **SPREAD, FEEDBACK, REVERB, MIX**. **FREEZE:** stops recording and keeps playing what was last captured.
""", """
## Plate reverb
- Classic plate reverb for pads and drones: **PRE-DELAY, SIZE, DECAY, DAMP, MOTION** (tail movement) and **MIX**.
## Output
- **LOW / MID / HIGH:** 3-band EQ (**MID FREQ** sets the mid frequency). **DRIVE:** tape saturation.
- **LIMITER:** never lets the output go above −0.3 dB: protects your ears and your master.
- **QUALITY:** **High (2x)** runs the voices at double sample rate (no false metallic sparkle); **Eco** uses less CPU.
""", top: 88 + ht + 14)
endPage()

// ==================================================================== LIVE
beginPage("LIVE", "macros, morph, piano and the X/Y pad")
let hl = drawShot("look_live.png", top: 88)
columns("""
## Macros
- **MATERIAL:** wood ↔ metal. **MOTION:** still ↔ alive. **SPACE:** dry ↔ reverb and echo. **TONE:** dark ↔ bright. Each one moves several knobs at once.
## Morph A / B
- **STORE A** and **STORE B** save two sounds; **MORPH** blends smoothly from one to the other.
## Piano / Life
- **PEDAL:** sustain. **SYMPATHY:** strings that ring on their own. **LIFE:** small variations so the sound breathes.
""", """
## Sequence
- **LOCK DEST:** what the LOCK lane changes. **SEQ 2 FOLLOW:** SEQ 2 follows SEQ 1 at an interval (unison, third, fifth, octave). **MUTATE:** notes slowly change by themselves.
## X/Y pad
- **X = MORPH** (A to B), **Y = SPACE**. Drag it while it plays; double-click centres it. You can record it with REC.
## Output
- Scope, spectrum and level, like on the FX page.
""", top: 88 + hl + 14)
endPage()

// ==================================================================== MACRO
beginPage("Engine C", "24 synthesis models", colour: green)
let hm = drawShot("look_macro.png", top: 88)
columns("""
## The 24 models
- **Row 1:** VA + filter, Phase Distortion, 6-Op FM I, II and III, Wave Terrain.
- **Row 2:** String Machine, Chiptune, Virtual Analog, Waveshaper, FM, Grain / Formant.
- **Row 3:** Harmonic, Wavetable, Chords, Speech, Swarm, Noise.
- **Row 4:** Particles, Inharmonic String, Modal, Kick, Snare, Hi-Hat.
## Knobs
- **HARMONICS, TIMBRE, MORPH:** their job depends on the model (in Chords, HARMONICS picks the chord; in Kick, the tuning of the hit).
- **FM, TIMBRE MOD, MORPH MOD:** how much they follow the internal envelope.
- **PITCH:** ±24 semitones. **OUTPUT:** Main, Aux or stereo.
""", """
## Low Pass Gate
- **LPG** on = every note is a hit with **DECAY** and **COLOUR**. Off = sustained sound (for pads and drones).
## Routing
- **A/B – C:** mixes engines A/B with C (the same control as ENGINE C in SYNTH > MIX).
- **C > A FEED:** engine C excites resonator A. A kick or a noise "striking" a string. With EXCITER = **Engine C** the resonator is driven by engine C alone.
- **C > B FEED:** the same for the engine B object (bar, plate, gong).
- **SEQ 2 PLAYS:** the second sequencer plays engine B, C, or both.
## Presets
29 presets in **Macro** plus 12 new ones in 1.5: **Model Hopper**, **Plaits Kit Walk**, **FM Struck String**, **Gong Driven**…
""", top: 88 + hm + 14)
endPage()

// ==================================================================== RECORDING + TIPS
beginPage("Recording and tips", "REC, presets, Ableton and minimal")
let hr = drawShot("look_synth.png", top: 88, crop: CGRect(x: 420, y: 0, width: 820, height: 62))
columns("""
## REC: recording movements
- While the song plays, press **REC**: it waits for step 1, records **one loop** (the SEQ length, or 1–8 bars from the menu next to it) and switches itself off.
- While it records, move knobs or the X/Y pad. Recorded knobs turn **light blue**.
- Right-click REC: list of what was recorded and clear. It is saved with your Ableton project.
## Presets
- Factory presets never change. **SAVE** creates a user preset (in Music > URing > Presets) shown in the **USER** category of the browser.
- Exporting or freezing in Ableton always gives **exactly the same audio**.
## In Ableton
- One instance per sound; with **MIDI OUT** you can record the sequencer notes on another track.
- Automate **MORPH** and **SPACE** with the X/Y pad for long changes.
""", """
## Tips for minimal / rominimal
- Start with **Rubber Blip**, **Click Bass** or **Wood Tick** and lower **PULSES** until there is air.
- Use different **STEPS** in SEQ 1 and SEQ 2 (16 and 7, 16 and 12): the patterns drift apart and never repeat the same way.
- **PROB** at 70 % and a little **MUTATE** make the loop live on its own.
- Use **LOCK** on engine C TIMBRE so every hit has a different colour.
- **Plaits Kit Walk** builds a whole kit with the **MOD** lane: kick, hats and snare from one sequencer.
- **Kick Into Modal** and **Modal Into Strings** show C > A FEED: a synthetic hit making a real object ring.
- Leave the volume at −20 dB and raise it on the Ableton mixer, so delay and reverb never clip.
## Free and open source
- URing is **free software** (GNU AGPL-3.0 licence): use it in your music, share it, study and improve the code.
- If it helps you, a **donation** keeps it growing: **paypal.me/JavierCazou** or Mercado Pago (Argentina) to the alias **nodrama.perro**, or click the URing logo (top left) and use **DONATE**.
## Credits
Engine C, TEXTURE and MARBLES use the open-source code of **Plaits**, **Clouds** and **Marbles** by Mutable Instruments (Émilie Gillet), MIT licence. Typeface **Montserrat** (SIL OFL).
""", top: 88 + hr + 16)
endPage()

ctx.closePDF()
print("Manual: \(outURL.path) (\(pageNo) pages)")
