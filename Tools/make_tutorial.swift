// Monta el video tutorial de URing: diapositivas PNG + narración AIFF + sonidos de URing de fondo.
// Uso: swift make_tutorial.swift <carpeta tutorial> <fondo1.wav> <fondo2.wav> <salida.mp4>
import AVFoundation
import CoreGraphics
import ImageIO
import Foundation

let args = CommandLine.arguments
guard args.count >= 5 else { print("uso: make_tutorial <dir> <bed1.wav> <bed2.wav> <out.mp4>"); exit(1) }
let dir = URL(fileURLWithPath: args[1])
let beds = [URL(fileURLWithPath: args[2]), URL(fileURLWithPath: args[3])]
let outURL = URL(fileURLWithPath: args[4])
let lead = 0.4 // silencio antes de la voz en cada diapositiva

struct Slide { let name: String; let music: Float; let extra: Double; var start = 0.0; var duration = 0.0 }
var slides: [Slide] = []
let index = try! String(contentsOf: dir.appendingPathComponent("slides.txt"))
for raw in index.components(separatedBy: .newlines) { // el índice puede venir con fin de línea CRLF
    let line = raw.trimmingCharacters(in: .whitespaces)
    let p = line.split(separator: "|").map { String($0) }
    guard p.count == 3, let m = Float(p[1]), let e = Double(p[2]) else { continue }
    slides.append(Slide(name: p[0], music: m, extra: e))
}

// Duraciones a partir de la narración
func seconds(_ url: URL) -> Double { CMTimeGetSeconds(AVURLAsset(url: url).duration) }
var t = 0.0
for i in slides.indices {
    let voice = seconds(dir.appendingPathComponent(slides[i].name + ".aiff"))
    slides[i].start = t
    slides[i].duration = lead + voice + slides[i].extra
    t += slides[i].duration
}
let total = t
print(String(format: "Duración total: %.1f s (%d diapositivas)", total, slides.count))

// ---- 1. Video mudo con las diapositivas
let silentURL = dir.appendingPathComponent("video_mudo.mp4")
try? FileManager.default.removeItem(at: silentURL)
let writer = try! AVAssetWriter(outputURL: silentURL, fileType: .mp4)
let w = 1920, h = 1080
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264, AVVideoWidthKey: w, AVVideoHeightKey: h,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: 6_000_000]])
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32ARGB, kCVPixelBufferWidthKey as String: w, kCVPixelBufferHeightKey as String: h])
writer.add(input)
writer.startWriting()
writer.startSession(atSourceTime: .zero)

func pixelBuffer(_ image: CGImage) -> CVPixelBuffer {
    var pb: CVPixelBuffer?
    CVPixelBufferCreate(nil, w, h, kCVPixelFormatType_32ARGB, nil, &pb)
    CVPixelBufferLockBaseAddress(pb!, [])
    let ctx = CGContext(data: CVPixelBufferGetBaseAddress(pb!), width: w, height: h, bitsPerComponent: 8,
                        bytesPerRow: CVPixelBufferGetBytesPerRow(pb!), space: CGColorSpaceCreateDeviceRGB(),
                        bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue)!
    ctx.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
    CVPixelBufferUnlockBaseAddress(pb!, [])
    return pb!
}
let fps = 10.0
var frame = 0
for s in slides {
    let src = CGImageSourceCreateWithURL(dir.appendingPathComponent(s.name + ".png") as CFURL, nil)!
    let pb = pixelBuffer(CGImageSourceCreateImageAtIndex(src, 0, nil)!)
    while Double(frame) / fps < s.start + s.duration {
        while !input.isReadyForMoreMediaData { usleep(1000) }
        adaptor.append(pb, withPresentationTime: CMTime(value: CMTimeValue(frame), timescale: CMTimeScale(fps)))
        frame += 1
    }
}
input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()

// ---- 2. Composición: video + voz + fondo de URing
let comp = AVMutableComposition()
let vAsset = AVURLAsset(url: silentURL)
let vTrack = comp.addMutableTrack(withMediaType: .video, preferredTrackID: kCMPersistentTrackID_Invalid)!
try! vTrack.insertTimeRange(CMTimeRange(start: .zero, duration: vAsset.duration), of: vAsset.tracks(withMediaType: .video)[0], at: .zero)

let voiceTrack = comp.addMutableTrack(withMediaType: .audio, preferredTrackID: kCMPersistentTrackID_Invalid)!
for s in slides {
    let a = AVURLAsset(url: dir.appendingPathComponent(s.name + ".aiff"))
    try! voiceTrack.insertTimeRange(CMTimeRange(start: .zero, duration: a.duration), of: a.tracks(withMediaType: .audio)[0],
                                    at: CMTime(seconds: s.start + lead, preferredTimescale: 600))
}

let bedTrack = comp.addMutableTrack(withMediaType: .audio, preferredTrackID: kCMPersistentTrackID_Invalid)!
var at = 0.0, k = 0
while at < total {
    let a = AVURLAsset(url: beds[k % beds.count])
    let len = min(CMTimeGetSeconds(a.duration), total - at)
    try! bedTrack.insertTimeRange(CMTimeRange(start: .zero, duration: CMTime(seconds: len, preferredTimescale: 600)),
                                  of: a.tracks(withMediaType: .audio)[0], at: CMTime(seconds: at, preferredTimescale: 600))
    at += len; k += 1
}
// Volumen del fondo por diapositiva (rampas cortas entre niveles; al final se apaga)
let bedParams = AVMutableAudioMixInputParameters(track: bedTrack)
var prev: Float = 0.0
for s in slides {
    let ramp = CMTimeRange(start: CMTime(seconds: s.start, preferredTimescale: 600), duration: CMTime(seconds: 0.6, preferredTimescale: 600))
    bedParams.setVolumeRamp(fromStartVolume: prev, toEndVolume: s.music, timeRange: ramp)
    prev = s.music
}
bedParams.setVolumeRamp(fromStartVolume: prev, toEndVolume: 0.0,
                        timeRange: CMTimeRange(start: CMTime(seconds: total - 2.5, preferredTimescale: 600), duration: CMTime(seconds: 2.5, preferredTimescale: 600)))
let mix = AVMutableAudioMix()
mix.inputParameters = [bedParams]

try? FileManager.default.removeItem(at: outURL)
let export = AVAssetExportSession(asset: comp, presetName: AVAssetExportPresetHighestQuality)!
export.outputURL = outURL
export.outputFileType = .mp4
export.audioMix = mix
let exported = DispatchSemaphore(value: 0)
export.exportAsynchronously { exported.signal() }
exported.wait()
if export.status == .completed { print("Video: \(outURL.path)") } else { print("Error: \(String(describing: export.error))"); exit(1) }
try? FileManager.default.removeItem(at: silentURL)
