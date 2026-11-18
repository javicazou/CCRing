// Probador mínimo de CLAP (sin dependencias): carga el .clap, crea el plugin, toca un acorde y mide la salida.
// Compilar: clang++ -std=c++17 -I<clap/include> ClapProbe.cpp -o clap_probe
// Uso: clap_probe <CCRing.clap>
#include <clap/clap.h>
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static const void* hostGetExtension (const clap_host*, const char*) { return nullptr; }
static void hostNoop (const clap_host*) {}

struct Events { std::vector<clap_event_note> notes; };
static uint32_t inSize (const clap_input_events* l) { return (uint32_t) ((Events*) l->ctx)->notes.size(); }
static const clap_event_header* inGet (const clap_input_events* l, uint32_t i) { return &((Events*) l->ctx)->notes[i].header; }
static bool outPush (const clap_output_events*, const clap_event_header*) { return true; }

static clap_event_note note (uint16_t type, int16_t key, uint32_t time)
{
    clap_event_note n {};
    n.header.size = sizeof (n); n.header.time = time; n.header.space_id = CLAP_CORE_EVENT_SPACE_ID; n.header.type = type;
    n.note_id = -1; n.port_index = 0; n.channel = 0; n.key = key; n.velocity = 0.8;
    return n;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::puts ("uso: clap_probe <plugin.clap>"); return 2; }
    std::string bin = std::string (argv[1]) + "/Contents/MacOS/";
    const char* name = std::strrchr (argv[1], '/');
    std::string base = name ? name + 1 : argv[1];
    base = base.substr (0, base.rfind (".clap"));
    bin += base;
    void* lib = dlopen (bin.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (! lib) { std::printf ("FAIL dlopen: %s\n", dlerror()); return 1; }
    auto* entry = (const clap_plugin_entry_t*) dlsym (lib, "clap_entry");
    if (! entry || ! entry->init (argv[1])) { std::puts ("FAIL clap_entry"); return 1; }
    auto* factory = (const clap_plugin_factory_t*) entry->get_factory (CLAP_PLUGIN_FACTORY_ID);
    if (! factory || factory->get_plugin_count (factory) < 1) { std::puts ("FAIL factory"); return 1; }
    const auto* desc = factory->get_plugin_descriptor (factory, 0);
    std::printf ("plugin: %s  id: %s  vendor: %s  version: %s  clap %u.%u.%u\n", desc->name, desc->id, desc->vendor, desc->version,
                 desc->clap_version.major, desc->clap_version.minor, desc->clap_version.revision);
    std::printf ("features:"); for (auto f = desc->features; f && *f; ++f) std::printf (" %s", *f); std::puts ("");

    clap_host host {};
    host.clap_version = CLAP_VERSION; host.name = "ClapProbe"; host.vendor = "CCR"; host.version = "1.0";
    host.get_extension = hostGetExtension; host.request_restart = hostNoop; host.request_process = hostNoop; host.request_callback = hostNoop;
    const clap_plugin* p = factory->create_plugin (factory, &host, desc->id);
    if (! p || ! p->init (p)) { std::puts ("FAIL create/init"); return 1; }
    const double sr = 48000.0; const uint32_t bs = 512;
    if (! p->activate (p, sr, 1, bs) || ! p->start_processing (p)) { std::puts ("FAIL activate"); return 1; }

    std::vector<float> L (bs), R (bs); float* ch[2] { L.data(), R.data() };
    clap_audio_buffer out {}; out.data32 = ch; out.channel_count = 2;
    Events ev; clap_input_events in { &ev, inSize, inGet }; clap_output_events outEv { nullptr, outPush };
    clap_process proc {}; proc.frames_count = bs; proc.audio_outputs = &out; proc.audio_outputs_count = 1;
    proc.in_events = &in; proc.out_events = &outEv; proc.steady_time = 0;
    float peak = 0.0f; bool finite = true; double energy = 0.0;
    const int blocks = (int) (3.0 * sr / bs);
    for (int b = 0; b < blocks; ++b)
    {
        ev.notes.clear();
        if (b == 0) for (int k : { 48, 55, 63 }) ev.notes.push_back (note (CLAP_EVENT_NOTE_ON, (int16_t) k, 0));
        if (b == blocks / 2) for (int k : { 48, 55, 63 }) ev.notes.push_back (note (CLAP_EVENT_NOTE_OFF, (int16_t) k, 0));
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        if (p->process (p, &proc) == CLAP_PROCESS_ERROR) { std::puts ("FAIL process"); return 1; }
        proc.steady_time += bs;
        for (uint32_t i = 0; i < bs; ++i)
        {
            if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false;
            peak = std::max (peak, std::max (std::fabs (L[i]), std::fabs (R[i])));
            energy += L[i] * L[i];
        }
    }
    p->stop_processing (p); p->deactivate (p); p->destroy (p); entry->deinit();
    const double rms = std::sqrt (energy / (blocks * (double) bs));
    std::printf ("audio: peak %.1f dB, rms %.1f dB, finite %s\n", 20 * std::log10 (std::max (1e-9f, peak)), 20 * std::log10 (std::max (1e-9, rms)), finite ? "yes" : "NO");
    const bool ok = finite && peak > 1e-4f && peak < 1.01f;
    std::puts (ok ? "CLAP PROBE: OK" : "CLAP PROBE: FAILED");
    return ok ? 0 : 1;
}
