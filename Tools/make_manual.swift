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

// ==================================================================== PORTADA
pageNo += 1
ctx.beginPDFPage(nil)
ctx.setFillColor(bg.cgColor); ctx.fill(mediaBox)
ctx.setFillColor(orange.withAlphaComponent(0.35).cgColor); ctx.fillEllipse(in: CGRect(x: M, y: H - 128, width: 34, height: 34))
ctx.setFillColor(orange.cgColor); ctx.fillEllipse(in: CGRect(x: M + 8, y: H - 120, width: 18, height: 18))
line("URING", font: mont(54), color: .white, at: CGPoint(x: M + 48, y: H - 130), kern: 12)
line("MANUAL  ·  VERSIÓN 1.5", font: mont(11), color: orange, at: CGPoint(x: M + 2, y: H - 162), kern: 3)
line("Instrumento de modelado físico + 24 modelos de síntesis, con secuenciador para minimal house", font: body(11.5), color: dim, at: CGPoint(x: M + 2, y: H - 182))
drawShot("look_macro.png", top: 215)
drawText(styled("""
## Qué es URing
URing es un instrumento que **no usa samples**: cada nota golpea, pulsa, frota o sopla objetos simulados — cuerdas, maderas, metales, parches — y suena como sonaría ese objeto. Tiene **tres motores**: A (resonador), B (impacto) y C (MOTOR C, 24 modelos de síntesis), dos secuenciadores euclidianos, efectos de estudio y una página para tocar en vivo.
## Cómo leer este manual
Una página por pestaña: **SYNTH, FX, SEQ, LIVE, ENGINE C y TEXTURE** (en español: SINTE, FX, SEQ, VIVO, MOTOR C y TEXTURA), más presets, grabación de movimientos y consejos para minimal / rominimal. Las palabras en MAYÚSCULAS son los nombres que ves en el plugin, que viene en inglés: si lo preferís en español, tocá el logo de URing y elegí **ESPAÑOL** en IDIOMA.
""", size: 10.5), in: CGRect(x: M, y: 60, width: W - 2 * M, height: 230))
line("MONSTRI AUDIO", font: mont(8), color: dim, at: CGPoint(x: M, y: 30), kern: 2)
ctx.endPDFPage()

// ==================================================================== EMPEZAR
beginPage("Empezar", "instalar, cargar y la barra superior")
let hb = drawShot("look_browser.png", top: 88)
columns("""
## Instalar
- **Mac:** doble clic en el .pkg (si avisa "desarrollador no identificado": clic derecho > Abrir).
- **Windows:** ver LEEME_WINDOWS.txt dentro del zip.
- En **Ableton Live**: Preferencias > Plug-ins > Re-escanear. URing aparece en **Monstri Audio** (VST3 y Audio Units).
- Cargalo en una **pista MIDI**. Arranca siempre en **−20 dB**.
## Barra superior (de izquierda a derecha)
- **Logo:** abre los créditos, los atajos, las donaciones y el **idioma** (English / Español).
- **SYNTH · FX · SEQ · LIVE · ENGINE C · TEXTURE:** las seis páginas (en español: SINTE · FX · SEQ · VIVO · MOTOR C · TEXTURA).
- **Lista:** abre el **buscador de presets** (Cmd+F).
- **< nombre >:** preset anterior / siguiente / menú.
""", """
- **Flechas curvas:** **deshacer / rehacer** (Cmd+Z / Cmd+Shift+Z), hasta 100 pasos: perillas, pasos, cables y presets.
- **SAVE:** guarda tu versión como preset de usuario. Un **asterisco *** indica que tocaste algo; el original nunca se pisa.
- **REC + largo:** graba movimientos de perillas (ver página 8).
- **Vúmetro:** nivel de salida; se pone rojo si satura.
- **Pantalla:** muestra la perilla que tocás; en reposo, el preset y la forma de onda en vivo.
## Buscador de presets
Categorías a la izquierda, búsqueda arriba, **estrella = favorito** (queda guardado). Clic carga, las **flechas** van escuchando uno por uno, **Enter / Esc** cierra. **ESCUCHAR** toca una nota cada vez que elegís uno.
- **165 presets de fábrica** en 13 categorías: Resonator, Impact, Hybrid, Glitch, Sequence, Drone, Acoustic, Live, Macro y las nuevas **Bass**, **Pad**, **Keys** y **Perc** (kicks, hats, rims y claves para minimal).
## Teclado de la compu
**A W S E D F T G Y H U J K** tocan notas, **Z / X** octava, **C / V** velocidad. Clic derecho en una perilla: **MIDI Learn**. Doble clic: valor por defecto.
""", top: 88 + hb + 14)
endPage()

// ==================================================================== SYNTH
beginPage("SYNTH", "los motores A y B, filtro, envolvente, LFO y salida")
let hs = drawShot("look_synth.png", top: 88)
columns("""
## Motor A · Resonador
- **MODEL:** Modal, String, Sympathetic, Metallic, Piano, Tine Keys, Reed Keys.
- **EXCITER:** cómo se excita: Pluck (púa), Strike, Bow (arco), Blow (soplido), Hammer o **Engine C** (el motor C hace vibrar al resonador).
- **STRUCTURE:** material y armónicos. **BRIGHTNESS:** brillo. **DAMPING:** cuánto dura. **POSITION:** dónde golpeás (borde = fino, centro = redondo).
## Motor B · Impacto
- 12 modelos: Membrane, Bar, Plate, Bell, Tube, Glass, Marimba, Vibes, Mbira, Steel Pan, Hand Drum, Gong.
- **MATERIAL, DECAY, TONE, SPREAD, SWEEP** y su propio **PITCH**.
## Mezcla
- **A / B:** balance entre los motores. **A > B FEED:** la cuerda hace sonar al impacto. **VARIATION:** cada nota distinta. **WIDTH:** estéreo. **TUNE:** afinación fina. **ENGINE C (MOTOR C):** volumen del motor C (0 = apagado; si elegís un modelo en su pestaña se prende solo al 50 %).
""", """
## Filtro y envolvente
- **FILTER:** Low/Band/High Pass y Notch (12 dB), **Ladder 24** (más gordo), **Comb** (peine afinado: tubos, metales, flanger; RESO = realimentación) y **Formant** (el CUTOFF recorre las vocales A-E-I-O-U). Con **ENV AMT**, **DRIVE** y **KEYTRK**.
- **AMP ENV:** Attack, Decay, Sustain, Release.
## LFO 1 y 2 · cables
- Forma, velocidad o **SYNC** a tempo, destino y **DEPTH**.
- **Cables:** arrastrá desde la salida **OUT** de un LFO o de **ENV** a cualquier entrada (STRUC, BRIGHT, CUT…). Rueda del mouse sobre el cable = cantidad; clic derecho = invertir o quitar. Mouse encima de un jack = qué tiene conectado. El botón de **cables** (arriba, junto al vúmetro) los muestra, los hace transparentes o los oculta: siguen funcionando igual.
## Output
- **VELOCITY:** cuánto influye la fuerza de la nota. **VOLUME:** salida. **BODY:** caja de madera que resuena. **FREEZE:** congela el sonido (drone).
""", top: 88 + hs + 14)
endPage()

// ==================================================================== FX
beginPage("FX", "chorus, glitch, tape delay, spring reverb y analizador")
let hf = drawShot("look_fx.png", top: 88)
columns("""
## Cadena
El sonido pasa en este orden: **CHORUS > GLITCH > TAPE DELAY > SPRING REVERB** (los cables de colores lo muestran).
## Chorus
- **MIX, RATE, DEPTH:** ensancha y hace "respirar" a pianos eléctricos y pads.
## Glitch
- Repite pedacitos a tempo: **CHANCE** (probabilidad), **LENGTH** (1/8 a 1/64), **VARIATION**, **CRUSH** (baja resolución) y **MIX**.
""", """
## Tape Delay
- **SYNC + DIVISION** a tempo (1/8D es el clásico dub), **FEEDBACK**, **WOW** (cinta), **DRIVE**, **LOW / HIGH CUT**, **PING PONG**, **DUCK** (baja el eco mientras tocás) y **MIX**.
## Spring Reverb
- Resorte de amplificador: **PRE-DELAY, TENSION, DECAY, DAMP, TONE, DRIP** (el "boing") y **MIX**.
## Analizador
- **SCOPE:** forma de onda. **SPECTRUM:** graves a la izquierda, agudos a la derecha. Medidor estéreo con pico y aviso rojo de saturación.
""", top: 88 + hf + 14)
endPage()

// ==================================================================== SEQ
beginPage("SEQ", "dos secuenciadores euclidianos y la grilla de pasos")
let hq = drawShot("look_seq.png", top: 88)
columns("""
## Secuenciador euclidiano
- **STEPS:** largo (1 a 64). **PULSES:** cuántos golpes, repartidos parejo. **ROTATE:** corre el dibujo.
- **DIVISION:** 1/32 a 1/4. **SWING**, **DIRECTION** (adelante, atrás, ping-pong, al azar), **FILL**.
- **RUN:** sigue al DAW. Arranca **apagado** (también al cargar un preset): lo prendés vos. **MIDI OUT:** manda las notas a otra pista para grabarlas.
## Notas y azar
- **ROOT, SCALE, OCTAVE, RANGE:** de dónde salen las notas. **GATE** y **PROB**.
- **RANDOM:** **DENSITY**, **AMOUNT**, **RATE**, **TIME** (adelanta o atrasa) y **LONG / SHORT**.
## Marbles
- Con **MARBLES ON** las notas salen de un generador aleatorio con memoria (de Mutable Instruments Marbles), dentro de la escala.
- **DEJA VU:** a la izquierda siempre nuevo, al **medio el bucle se repite**, a la derecha salta dentro del bucle. **LENGTH:** largo del bucle (1-16). **SPREAD:** qué tan lejos van las notas. **BIAS:** hacia graves o agudos.
""", """
## SEQ 2
Un segundo dibujo, en otra pista del mismo plugin: **SEQ 2 ON**, **DIVISION**, **OCTAVE**. **EDIT** elige qué secuenciador muestra la grilla. En la pestaña ENGINE C (MOTOR C) elegís si toca el motor B, el C o los dos.
## La grilla
- Carriles: **NOTE, OCT, VELO, GATE, PROB, RTCH** (ratchet), **COND** (1:2, 3:4…) **LOCK** (cambia el timbre de cada paso) y **MOD** (otro modelo del motor C en cada paso: bombo, hat, acorde…).
- **PATTERN A–H** + **CHAIN** para encadenar, **COPY / PASTE**.
- **RND LANE, RND RHYTHM, RND ALL, FIT SCALE, GENERATE, RESET**.
""", top: 88 + hq + 14)
endPage()

// ==================================================================== TEXTURE
beginPage("TEXTURE", "granular, plate reverb y salida", colour: green)
let ht = drawShot("look_texture.png", top: 88)
columns("""
## Texture · granular
Basado en **Clouds** de Mutable Instruments: graba lo que suena y lo vuelve a tocar en granos.
- **MODE:** Granular (nubes de granos), Stretch (estira sin cambiar la altura), Looping Delay y Spectral (congela y difumina el espectro).
- **POSITION:** de dónde toma los granos (más a la derecha = más atrás en el tiempo). **SIZE:** tamaño. **PITCH:** transposición. **DENSITY:** cantidad de granos (al medio, ninguno). **TEXTURE:** forma y filtro.
- **SPREAD, FEEDBACK, REVERB, MIX**. **FREEZE:** deja de grabar y se queda sonando lo último.
""", """
## Plate reverb
- Reverb de placa clásica para pads y drones: **PRE-DELAY, SIZE, DECAY, DAMP, MOTION** (movimiento de la cola) y **MIX**.
## Output
- **LOW / MID / HIGH:** ecualizador de 3 bandas (**MID FREQ** elige la frecuencia del medio). **DRIVE:** saturación de cinta.
- **LIMITER:** nunca deja pasar de −0,3 dB: protege tus oídos y el máster.
- **QUALITY:** **High (2x)** calcula las voces al doble de frecuencia (sin brillos metálicos falsos); **Eco** gasta menos CPU.
""", top: 88 + ht + 14)
endPage()

// ==================================================================== LIVE
beginPage("LIVE", "macros, morph, piano y el pad X/Y")
let hl = drawShot("look_live.png", top: 88)
columns("""
## Macros
- **MATERIAL:** madera ↔ metal. **MOTION:** quieto ↔ vivo. **SPACE:** seco ↔ reverb y eco. **TONE:** oscuro ↔ brillante. Cada uno mueve varias perillas a la vez.
## Morph A / B
- **STORE A** y **STORE B** guardan dos sonidos; **MORPH** pasa suave de uno al otro.
## Piano / Life
- **PEDAL:** sustain. **SYMPATHY:** cuerdas que resuenan solas. **LIFE:** pequeñas variaciones para que el sonido respire.
""", """
## Sequence
- **LOCK DEST:** qué cambia el carril LOCK. **SEQ 2 FOLLOW:** el SEQ 2 sigue al SEQ 1 en una distancia (unísono, tercera, quinta, octava). **MUTATE:** las notas van cambiando de a poco.
## Pad X/Y
- **X = MORPH** (de A a B), **Y = SPACE**. Arrastralo mientras suena; doble clic lo centra. Lo podés grabar con REC.
## Salida
- Osciloscopio, espectro y nivel, igual que en FX.
""", top: 88 + hl + 14)
endPage()

// ==================================================================== MACRO
beginPage("Motor C · Engine C", "24 modelos de síntesis", colour: green)
let hm = drawShot("look_macro.png", top: 88)
columns("""
## Los 24 modelos
- **Fila 1:** VA + filtro, Phase Distortion, 6-Op FM I, II y III, Wave Terrain.
- **Fila 2:** String Machine, Chiptune, Virtual Analog, Waveshaper, FM, Grain / Formant.
- **Fila 3:** Harmonic, Wavetable, Chords, Speech, Swarm, Noise.
- **Fila 4:** Particles, Inharmonic String, Modal, Kick, Snare, Hi-Hat.
## Perillas
- **HARMONICS, TIMBRE, MORPH:** cambian según el modelo (en Chords, HARMONICS elige el acorde; en Kick, la afinación del golpe).
- **FM, TIMBRE MOD, MORPH MOD:** cuánto se mueven con la envolvente interna.
- **PITCH:** ±24 semitonos. **OUTPUT:** Main, Aux o estéreo.
""", """
## Low Pass Gate
- **LPG** encendido = cada nota es un golpe con **DECAY** y **COLOUR**. Apagado = sonido sostenido (para pads y drones).
## Ruteo
- **A/B – C:** mezcla los motores A/B con el C (es la misma perilla que ENGINE C en SYNTH > MIX).
- **C > A FEED:** el motor C excita al resonador A. Un bombo o un ruido "golpeando" una cuerda. Con EXCITER = **Engine C** el resonador suena solo con el motor C.
- **C > B FEED:** lo mismo con el objeto del motor B (barra, placa, gong).
- **SEQ 2 PLAYS:** el segundo secuenciador toca el motor B, el C, o los dos.
## Presets
29 presets en **Macro** y 12 nuevos de la 1.5: **Model Hopper**, **Plaits Kit Walk**, **FM Struck String**, **Gong Driven**…
""", top: 88 + hm + 14)
endPage()

// ==================================================================== GRABAR + CONSEJOS
beginPage("Grabar y consejos", "REC, presets, Ableton y minimal")
let hr = drawShot("look_synth.png", top: 88, crop: CGRect(x: 420, y: 0, width: 820, height: 62))
columns("""
## REC: grabar movimientos
- Con la canción sonando, tocá **REC**: espera al paso 1, graba **una vuelta** (largo del SEQ, o 1–8 compases con el menú de al lado) y se apaga solo.
- Mientras graba, mové perillas o el pad X/Y. Lo grabado queda en **celeste** sobre cada perilla.
- Clic derecho en REC: lista de lo grabado y borrar. Se guarda con el proyecto de Ableton.
## Presets
- Los de fábrica nunca cambian. **SAVE** crea uno de usuario (en Música > URing > Presets) y aparece en la categoría **USER** del buscador.
- Exportar o congelar en Ableton da **siempre el mismo audio**.
## En Ableton
- Una instancia por sonido; con **MIDI OUT** grabás las notas del secuenciador en otra pista.
- Automatizá **MORPH** y **SPACE** con el pad X/Y para los cambios largos.
""", """
## Consejos para minimal / rominimal
- Empezá por **Rubber Blip**, **Click Bass** o **Wood Tick** y bajá **PULSES** hasta que quede aire.
- Poné **STEPS** distintos en SEQ 1 y SEQ 2 (16 y 7, 16 y 12): el dibujo se desfasa y nunca se repite igual.
- **PROB** al 70 % y **MUTATE** bajo hacen que el loop viva solo.
- Usá **LOCK** en TIMBRE del motor C para que cada golpe tenga otro color.
- **Plaits Kit Walk** arma una batería entera con el carril **MOD**: bombo, hats y snare en un solo secuenciador.
- **Kick Into Modal** y **Modal Into Strings** muestran el C > A FEED: un golpe sintético haciendo sonar un objeto real.
- Dejá el volumen en −20 dB y subí en el mixer de Ableton: así el delay y la reverb no saturan.
## Libre y gratis
- URing es **software libre** (licencia GNU AGPL-3.0): podés usarlo en tu música, compartirlo y estudiar o mejorar el código.
- Si te sirve, una **donación** ayuda a seguir mejorándolo: **paypal.me/JavierCazou** o Mercado Pago al alias **nodrama.perro**, o tocá el logo de URing (arriba a la izquierda) y usá **DONATE**.
## Créditos
El motor C, TEXTURE y MARBLES usan el código abierto de **Plaits**, **Clouds** y **Marbles** de Mutable Instruments (Émilie Gillet), licencia MIT. Tipografía **Montserrat** (SIL OFL). Voz del tutorial: Piper es_AR Daniela (OpenSLR 61, CC BY-SA 4.0).
""", top: 88 + hr + 16)
endPage()

ctx.closePDF()
print("Manual: \(outURL.path) (\(pageNo) páginas)")
