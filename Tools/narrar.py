# Narra cada diapositiva del tutorial con Piper (voz es_AR Daniela) -> slideNN.aiff
#
# Uso (desde la carpeta del paquete piper, para que la ruta de espeak sea corta):
#   cd <venv>/lib/python3.x/site-packages/piper
#   <venv>/bin/python -I <ruta>/narrar.py <es_AR-daniela-high.onnx> <carpeta tutorial3es>
# La versión en inglés se narra con la voz de macOS:
#   for f in slide*.txt; do say -v Samantha -r 178 -f "$f" -o "${f%.txt}.aiff" --data-format=BEI16@22050; done
import sys, wave, pathlib, subprocess
from piper import PiperVoice
from piper.config import SynthesisConfig

model, outdir = sys.argv[1], pathlib.Path(sys.argv[2])
voice = PiperVoice.load(model, espeak_data_dir="./espeak-ng-data")   # ruta corta: espeak tiene límite de 160 caracteres
cfg = SynthesisConfig(length_scale=0.93)
for txt in sorted(outdir.glob("slide[0-9]*.txt")):
    wav = txt.with_suffix(".wav")
    with wave.open(str(wav), "wb") as w:
        voice.synthesize_wav(txt.read_text(encoding="utf-8").strip(), w, syn_config=cfg)
    aiff = txt.with_suffix(".aiff")
    subprocess.run(["afconvert", "-f", "AIFF", "-d", "BEI16", str(wav), str(aiff)], check=True)
    wav.unlink()
    print("ok", aiff.name, flush=True)
