// Arma un video vertical para redes: cuadros PNG (slideNN.png) con su duración + un audio.
// Uso: make_promo <carpeta con slides.txt "nombre|segundos"> <audio.wav> <salida.mp4>
import AVFoundation
import CoreGraphics
import ImageIO
import Foundation

let args = CommandLine.arguments
guard args.count >= 4 else { print("uso: make_promo <dir> <audio.wav> <out.mp4>"); exit(1) }
let dir = URL(fileURLWithPath: args[1])
let audioURL = URL(fileURLWithPath: args[2])
let outURL = URL(fileURLWithPath: args[3])

var slides: [(String, Double)] = []
for line in (try! String(contentsOf: dir.appendingPathComponent("slides.txt"))).components(separatedBy: .newlines) {
    let p = line.split(separator: "|").map(String.init)
    if p.count == 2, let d = Double(p[1]) { slides.append((p[0], d)) }
}
func image(_ name: String) -> CGImage {
    let src = CGImageSourceCreateWithURL(dir.appendingPathComponent(name + ".png") as CFURL, nil)!
    return CGImageSourceCreateImageAtIndex(src, 0, nil)!
}
let first = image(slides[0].0)
let w = first.width, h = first.height

// ---- video mudo
let silentURL = dir.appendingPathComponent("video_mudo.mp4")
try? FileManager.default.removeItem(at: silentURL)
let writer = try! AVAssetWriter(outputURL: silentURL, fileType: .mp4)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264, AVVideoWidthKey: w, AVVideoHeightKey: h,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: 8_000_000]])
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32ARGB, kCVPixelBufferWidthKey as String: w, kCVPixelBufferHeightKey as String: h])
writer.add(input)
writer.startWriting()
writer.startSession(atSourceTime: .zero)
func pixelBuffer(_ img: CGImage) -> CVPixelBuffer {
    var pb: CVPixelBuffer?
    CVPixelBufferCreate(nil, w, h, kCVPixelFormatType_32ARGB, nil, &pb)
    CVPixelBufferLockBaseAddress(pb!, [])
    let ctx = CGContext(data: CVPixelBufferGetBaseAddress(pb!), width: w, height: h, bitsPerComponent: 8,
                        bytesPerRow: CVPixelBufferGetBytesPerRow(pb!), space: CGColorSpaceCreateDeviceRGB(),
                        bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue)!
    ctx.draw(img, in: CGRect(x: 0, y: 0, width: w, height: h))
    CVPixelBufferUnlockBaseAddress(pb!, [])
    return pb!
}
let fps = 30.0
var frame = 0, t = 0.0
for (name, dur) in slides {
    let pb = pixelBuffer(image(name))
    t += dur
    while Double(frame) / fps < t {
        while !input.isReadyForMoreMediaData { usleep(1000) }
        adaptor.append(pb, withPresentationTime: CMTime(value: CMTimeValue(frame), timescale: CMTimeScale(fps)))
        frame += 1
    }
}
input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()

// ---- video + audio (recortado al largo del video, con fundido al final)
let total = t
let comp = AVMutableComposition()
let v = AVURLAsset(url: silentURL)
let vt = comp.addMutableTrack(withMediaType: .video, preferredTrackID: kCMPersistentTrackID_Invalid)!
try! vt.insertTimeRange(CMTimeRange(start: .zero, duration: v.duration), of: v.tracks(withMediaType: .video)[0], at: .zero)
let a = AVURLAsset(url: audioURL)
let at = comp.addMutableTrack(withMediaType: .audio, preferredTrackID: kCMPersistentTrackID_Invalid)!
let alen = min(CMTimeGetSeconds(a.duration), total)
try! at.insertTimeRange(CMTimeRange(start: .zero, duration: CMTime(seconds: alen, preferredTimescale: 600)),
                        of: a.tracks(withMediaType: .audio)[0], at: .zero)
let params = AVMutableAudioMixInputParameters(track: at)
params.setVolumeRamp(fromStartVolume: 1.0, toEndVolume: 0.0,
                     timeRange: CMTimeRange(start: CMTime(seconds: max(0, alen - 1.2), preferredTimescale: 600), duration: CMTime(seconds: 1.2, preferredTimescale: 600)))
let mix = AVMutableAudioMix(); mix.inputParameters = [params]
try? FileManager.default.removeItem(at: outURL)
let export = AVAssetExportSession(asset: comp, presetName: AVAssetExportPresetHighestQuality)!
export.outputURL = outURL; export.outputFileType = .mp4; export.audioMix = mix
let exported = DispatchSemaphore(value: 0)
export.exportAsynchronously { exported.signal() }
exported.wait()
try? FileManager.default.removeItem(at: silentURL)
if export.status == .completed { print("Video: \(outURL.path) (\(String(format: "%.1f", total)) s)") } else { print("Error: \(String(describing: export.error))"); exit(1) }
