// Pone una imagen como icono de un archivo o carpeta en el Finder: seticon <imagen.png> <archivo...>
import AppKit
let a = CommandLine.arguments
guard a.count >= 3, let img = NSImage(contentsOfFile: a[1]) else { print("uso: seticon <imagen> <archivo...>"); exit(1) }
for path in a.dropFirst(2) {
    print(NSWorkspace.shared.setIcon(img, forFile: path, options: []) ? "icono: \(path)" : "no pude: \(path)")
}
