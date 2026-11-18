// Grabador de la narración del tutorial: muestra el texto de cada pantalla, graba tu voz con
// Enter / Enter, la podés escuchar o repetir, y guarda slideNN.aiff (sin silencios en las
// puntas y con el nivel parejo). Uso: grabar_voz <carpeta del tutorial> [desde la pantalla N]
import AVFoundation
import Foundation

let dir = URL(fileURLWithPath: CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : ".")
let startAt = CommandLine.arguments.count > 2 ? max(1, Int(CommandLine.arguments[2]) ?? 1) : 1
let index = (try? String(contentsOf: dir.appendingPathComponent("slides.txt"))) ?? ""
let names = index.components(separatedBy: .newlines).compactMap { $0.split(separator: "|").first.map(String.init) }.filter { !$0.isEmpty }
guard !names.isEmpty else { print("No encontré slides.txt en \(dir.path)"); exit(1) }

func say(_ s: String) { print(s); fflush(stdout) }
func ask() -> String { (readLine() ?? "").trimmingCharacters(in: .whitespaces).lowercased() }

// Permiso de micrófono
let perm = DispatchSemaphore(value: 0)
var granted = false
AVCaptureDevice.requestAccess(for: .audio) { ok in granted = ok; perm.signal() }
perm.wait()
guard granted else {
    say("\n✗ macOS no dio permiso para el micrófono.")
    say("  Ajustes del Sistema → Privacidad y seguridad → Micrófono → activá la app de este terminal, y volvé a correr el grabador.")
    exit(1)
}

// Recorta silencios de las puntas (umbral -42 dB, deja 0,15 s) y normaliza el pico a -1 dB
func clean(_ raw: URL, _ out: URL) -> Double {
    guard let f = try? AVAudioFile(forReading: raw),
          let buf = AVAudioPCMBuffer(pcmFormat: f.processingFormat, frameCapacity: AVAudioFrameCount(f.length)) else { return 0 }
    try? f.read(into: buf)
    let n = Int(buf.frameLength), sr = f.processingFormat.sampleRate
    let x = buf.floatChannelData![0]
    let thr: Float = pow(10, -42 / 20)
    var a = 0, b = n - 1
    while a < n && abs(x[a]) < thr { a += 1 }
    while b > a && abs(x[b]) < thr { b -= 1 }
    let pad = Int(0.15 * sr)
    a = max(0, a - pad); b = min(n - 1, b + pad)
    var peak: Float = 1e-6
    for i in a...max(a, b) { peak = max(peak, abs(x[i])) }
    let g = pow(10, -1 / 20) / peak
    let fmt = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: sr, channels: 1, interleaved: false)!
    let o = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: AVAudioFrameCount(b - a + 1))!
    o.frameLength = AVAudioFrameCount(b - a + 1)
    for i in 0..<Int(o.frameLength) { o.floatChannelData![0][i] = x[a + i] * g }
    try? FileManager.default.removeItem(at: out)
    let settings: [String: Any] = [AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: sr, AVNumberOfChannelsKey: 1,
                                   AVLinearPCMBitDepthKey: 16, AVLinearPCMIsFloatKey: false, AVLinearPCMIsBigEndianKey: true]
    if let w = try? AVAudioFile(forWriting: out, settings: settings, commonFormat: .pcmFormatFloat32, interleaved: false) { try? w.write(from: o) }
    return Double(o.frameLength) / sr
}

say("\n🎙  GRABADOR DEL TUTORIAL DE URING  —  \(names.count) pantallas\n")
say("Leé tranquilo y con ganas, como si se lo explicaras a un amigo. Si te trabás, repetís esa pantalla.")
say("Enter = empezar a grabar  ·  Enter = terminar  ·  después: Enter = siguiente, r = repetir, e = escuchar\n")

var i = startAt - 1
while i < names.count {
    let name = names[i]
    let text = (try? String(contentsOf: dir.appendingPathComponent(name + ".txt"))) ?? ""
    let title = ((try? String(contentsOf: dir.appendingPathComponent(name + ".title"))) ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
    say("────────────────────────────────────────────────────────────")
    say("PANTALLA \(i + 1) de \(names.count)  ·  \(title)\n")
    say(text.trimmingCharacters(in: .whitespacesAndNewlines))
    say("\n[Enter] para grabar")
    _ = ask()
    let raw = dir.appendingPathComponent(name + "_raw.caf")
    let settings: [String: Any] = [AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: 44100.0, AVNumberOfChannelsKey: 1,
                                   AVLinearPCMBitDepthKey: 16, AVLinearPCMIsFloatKey: false]
    guard let rec = try? AVAudioRecorder(url: raw, settings: settings), rec.record() else { say("✗ No pude abrir el micrófono"); exit(1) }
    say("● GRABANDO…  [Enter] para terminar")
    _ = ask()
    rec.stop()
    let secs = clean(raw, dir.appendingPathComponent(name + ".aiff"))
    // la toma original queda como respaldo (slideNN_raw.caf)
    if secs < 0.5 { say("✗ No se escuchó nada (¿micrófono?). Repetimos.\n"); continue }
    say(String(format: "✓ Listo: %.1f s.  [Enter] siguiente · [r] repetir · [e] escuchar", secs))
    var answer = ask()
    while answer == "e" {
        if let p = try? AVAudioPlayer(contentsOf: dir.appendingPathComponent(name + ".aiff")) {
            p.play(); while p.isPlaying { usleep(50_000) }
        }
        say("[Enter] siguiente · [r] repetir · [e] escuchar")
        answer = ask()
    }
    if answer == "r" { continue }
    i += 1
}
say("\n✓ ¡Listo! Grabaste las \(names.count) pantallas. Avisale a Claude que ya está para armar el video.\n")
