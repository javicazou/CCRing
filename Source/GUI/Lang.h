#pragma once

// Idioma de la interfaz: inglés (por defecto) o español. La elección se guarda para todas las
// instancias en ~/Library/Application Support/Monstri Audio/URing/Settings.xml (Windows: AppData).
// Los textos se escriben en inglés en el código y tr() los traduce cuando el idioma es español.
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <atomic>
#include <cstring>

namespace mm::lang
{
enum class Lang { English = 0, Spanish };

inline juce::File settingsFile()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");   // en Mac la carpeta base es ~/Library
   #endif
    return base.getChildFile ("Monstri Audio").getChildFile ("URing").getChildFile ("Settings.xml");
}

// Cambia cada vez que se elige otro idioma: las ventanas abiertas lo miran en su timer y se retraducen
inline std::atomic<int>& version()
{
    static std::atomic<int> v { 0 };
    return v;
}

inline Lang& currentRef()
{
    static Lang l = []
    {
        if (auto xml = juce::XmlDocument::parse (settingsFile()))
            return xml->getStringAttribute ("language") == "es" ? Lang::Spanish : Lang::English;
        return Lang::English;
    }();
    return l;
}
inline Lang current() { return currentRef(); }
inline bool spanish() { return current() == Lang::Spanish; }

inline void setLanguage (Lang l)
{
    currentRef() = l;
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
    if (xml == nullptr) xml = std::make_unique<juce::XmlElement> ("URING_SETTINGS");
    xml->setAttribute ("language", l == Lang::Spanish ? "es" : "en");
    xml->writeTo (f);
    ++version();
}

// Otras preferencias de la interfaz en el mismo Settings.xml (valen para todas las instancias)
inline int getSetting (const char* key, int fallback)
{
    if (auto xml = juce::XmlDocument::parse (settingsFile()))
        return xml->getIntAttribute (key, fallback);
    return fallback;
}
inline void setSetting (const char* key, int value)
{
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
    if (xml == nullptr) xml = std::make_unique<juce::XmlElement> ("URING_SETTINGS");
    xml->setAttribute (key, value);
    xml->writeTo (f);
}

// Inglés → español (textos exactos, tal como aparecen en el código)
inline const std::unordered_map<juce::String, juce::String>& table()
{
    static const std::unordered_map<juce::String, juce::String> t = []
    {
        std::unordered_map<juce::String, juce::String> m;
        auto add = [&m] (const char* en, const char* es) { m[juce::String::fromUTF8 (en)] = juce::String::fromUTF8 (es); };
        // ---- barra superior y pestañas
        add ("SYNTH", "SINTE"); add ("FX", "FX"); add ("SEQ", "SEQ"); add ("LIVE", "VIVO"); add ("MACRO", "MACRO");
        add ("TEXTURE", "TEXTURA"); add ("SAVE", "GUARDAR"); add ("ENGINE C", "MOTOR C");
        add ("ENGINE C ON  (A/B - C 50%)", "MOTOR C PRENDIDO  (A/B - C 50%)");
        add ("Engine C: 24 synthesis models. Raise ENGINE C (SYNTH > MIX) or pick a model to hear it",
             "Motor C: 24 modelos de síntesis. Subí MOTOR C (SINTE > MEZCLA) o elegí un modelo para escucharlo");
        add ("Undo (Cmd+Z)", "Deshacer (Cmd+Z)"); add ("Redo (Cmd+Shift+Z)", "Rehacer (Cmd+Shift+Z)");
        add ("Preset browser: search, categories, favorites", "Buscador de presets: búsqueda, categorías, favoritos");
        add ("About URing", "Acerca de URing");
        add ("REC: while the song plays, every knob you move is recorded in a loop (LEN). Right-click: recorded knobs / clear",
             "REC: mientras suena la canción, cada perilla que movés se graba en un bucle (LEN). Clic derecho: perillas grabadas / borrar");
        add ("Automation loop: SEQ = one full cycle of SEQ 1 (8, 16, 32, 64 steps...), or 1-8 bars. REC records exactly one loop",
             "Bucle de automatización: SEQ = una vuelta completa del SEQ 1 (8, 16, 32, 64 pasos...), o 1-8 compases. REC graba exactamente una vuelta");
        add ("Save the current sound as a user preset. Factory presets never change; tweaks (*) are lost unless you save",
             "Guarda el sonido actual como preset de usuario. Los de fábrica nunca cambian; los cambios (*) se pierden si no guardás");
        add ("1 BAR", "1 COMPÁS"); add ("2 BARS", "2 COMPASES"); add ("4 BARS", "4 COMPASES"); add ("8 BARS", "8 COMPASES");
        // ---- pantalla (LCD)
        add ("PHYSICAL MODELS READY", "MODELOS FÍSICOS LISTOS");
        add ("AUTOMATION REC", "GRABAR AUTOMATIZACIÓN"); add ("ARMED - STARTS ON STEP 1", "ARMADO - EMPIEZA EN EL PASO 1");
        add ("OFF", "APAGADO"); add ("ON", "ON"); add ("USER PRESET", "PRESET DE USUARIO");
        add ("EDITING", "EDITANDO"); add ("SEQ 1  >  ENGINE A", "SEQ 1  >  MOTOR A"); add ("SEQ 2  >  ENGINE B", "SEQ 2  >  MOTOR B");
        add ("PRESET SAVED", "PRESET GUARDADO"); add ("PRESET", "PRESET"); add ("COULD NOT SAVE", "NO SE PUDO GUARDAR");
        add ("MIDI MAPPED", "MIDI ASIGNADO");
        add ("RECORDING ONE LOOP - MOVE KNOBS", "GRABANDO UNA VUELTA - MOVÉ PERILLAS");
        add ("AUTOMATION", "AUTOMATIZACIÓN"); add ("LOOP RECORDED - REC OFF", "VUELTA GRABADA - REC APAGADO");
        add ("AUTOMATION CLEARED", "AUTOMATIZACIÓN BORRADA"); add ("PATTERN CLEARED", "PATRÓN BORRADO"); add ("ALL CLEARED", "TODO BORRADO");
        add ("MOVE A CONTROLLER", "MOVÉ UN CONTROL"); add ("MIDI CLEARED", "MIDI BORRADO"); add ("ALL MAPPINGS CLEARED", "ASIGNACIONES BORRADAS");
        add ("KEYBOARD OCTAVE", "OCTAVA DEL TECLADO"); add ("KEYBOARD VELOCITY", "VELOCIDAD DEL TECLADO");
        add ("PATCH", "CABLE"); add ("CABLE UNPLUGGED", "CABLE DESCONECTADO"); add ("REMOVED", "QUITADO");
        add ("REDO", "REHACER"); add ("UNDO", "DESHACER"); add ("CHANGE RESTORED", "CAMBIO RECUPERADO"); add ("LAST CHANGE UNDONE", "ÚLTIMO CAMBIO DESHECHO");
        add ("COPIED", "COPIADO"); add ("PASTED", "PEGADO"); add ("NOTHING TO PASTE", "NADA PARA PEGAR");
        add ("STARTS NEXT CYCLE", "EMPIEZA EN LA PRÓXIMA VUELTA"); add ("ACTIVE", "ACTIVO");
        add ("PURE EUCLID RHYTHM", "RITMO EUCLIDIANO PURO"); add ("SURPRISE!", "¡SORPRESA!");
        add ("EDITED", "EDITADO"); add ("EUCLID", "EUCLIDIANO"); add ("NO LOCK", "SIN LOCK");
        add ("ALL LANES + RHYTHM", "TODOS LOS CARRILES + RITMO"); add ("RHYTHM", "RITMO");
        // ---- menús y diálogos
        add ("Save preset", "Guardar preset"); add ("Name for your preset:", "Nombre de tu preset:"); add ("My Sound", "Mi sonido");
        add ("Save", "Guardar"); add ("Cancel", "Cancelar");
        add ("Nothing recorded: press REC and move knobs while it plays", "No hay nada grabado: tocá REC y mové perillas mientras suena");
        add ("RECORDED  (click = clear)", "GRABADO  (clic = borrar)"); add ("Clear this pattern", "Borrar este patrón"); add ("Clear all patterns", "Borrar todos los patrones");
        add ("MIDI Learn", "Aprender MIDI"); add ("Clear MIDI", "Borrar MIDI"); add ("Clear all MIDI mappings", "Borrar todas las asignaciones MIDI");
        add ("Clear recorded automation", "Borrar la automatización grabada"); add ("Remove cable", "Quitar cable");
        add ("Click to type a number", "Clic para escribir un número"); add ("Surprise pattern", "Patrón sorpresa");
        // ---- módulos (títulos)
        add ("ENGINE A  /  RESONATOR", "MOTOR A  /  RESONADOR"); add ("ENGINE B  /  IMPACT", "MOTOR B  /  IMPACTO");
        add ("MIX", "MEZCLA"); add ("OUTPUT", "SALIDA"); add ("FILTER", "FILTRO"); add ("AMP ENV", "ENVOLVENTE");
        add ("CHORUS", "CHORUS"); add ("GLITCH", "GLITCH"); add ("TAPE DELAY", "DELAY DE CINTA"); add ("SPRING REVERB", "REVERB DE RESORTE");
        add ("EUCLIDEAN SEQUENCER", "SECUENCIADOR EUCLIDIANO"); add ("NOTES", "NOTAS"); add ("RANDOM", "AZAR");
        add ("SEQ 2  /  ENGINE B", "SEQ 2  /  MOTOR B"); add ("MACROS", "MACROS"); add ("MORPH  A / B", "MORPH  A / B");
        add ("PIANO  /  LIFE", "PIANO  /  VIDA"); add ("SEQUENCE", "SECUENCIA");
        add ("ENGINE C  /  MACRO  -  24 MODELS", "MOTOR C  /  MACRO  -  24 MODELOS"); add ("LOW PASS GATE", "LOW PASS GATE"); add ("ROUTING", "RUTEO");
        add ("TEXTURE  /  GRANULAR", "TEXTURA  /  GRANULAR"); add ("PLATE REVERB", "REVERB DE PLACA");
        add ("OUTPUT  /  ANALYZER", "SALIDA  /  ANALIZADOR"); add ("PERFORM  /  X-Y", "TOCAR  /  X-Y");
        add ("SCOPE", "OSCILOSCOPIO"); add ("SPECTRUM", "ESPECTRO"); add ("PRESETS", "PRESETS");
        // ---- perillas y botones
        add ("MODEL", "MODELO"); add ("EXCITER", "EXCITADOR"); add ("STRUCTURE", "ESTRUCTURA"); add ("BRIGHTNESS", "BRILLO");
        add ("DAMPING", "AMORTIGUACIÓN"); add ("POSITION", "POSICIÓN"); add ("PITCH", "AFINACIÓN"); add ("A > B FEED", "A > B");
        add ("TUNE", "AFINAR"); add ("VARIATION", "VARIACIÓN"); add ("WIDTH", "ANCHO"); add ("MATERIAL", "MATERIAL");
        add ("DECAY", "CAÍDA"); add ("TONE", "TONO"); add ("SPREAD", "DISPERSIÓN"); add ("SWEEP", "BARRIDO");
        add ("VELOCITY", "VELOCIDAD"); add ("VOLUME", "VOLUMEN"); add ("BODY", "CUERPO"); add ("FREEZE", "CONGELAR"); add ("DRONE", "DRONE");
        add ("TYPE", "TIPO"); add ("CUTOFF", "CORTE"); add ("RESONANCE", "RESONANCIA"); add ("ENV AMT", "ENV CANT"); add ("ENV DECAY", "ENV CAÍDA");
        add ("KEYTRK", "SEGUIR TECLA"); add ("DRIVE", "SATURACIÓN"); add ("ATTACK", "ATAQUE"); add ("SUSTAIN", "SOSTÉN"); add ("RELEASE", "SOLTAR");
        add ("SHAPE", "FORMA"); add ("RATE", "VELOCIDAD"); add ("SYNC", "SYNC"); add ("TEMPO", "TEMPO"); add ("DIVISION", "DIVISIÓN");
        add ("DESTINATION", "DESTINO"); add ("DEPTH", "PROFUNDIDAD");
        add ("CHANCE", "PROBABILIDAD"); add ("LENGTH", "LARGO"); add ("CRUSH", "CRUSH"); add ("DELAY", "DELAY"); add ("TIME", "TIEMPO");
        add ("FEEDBACK", "REPETICIÓN"); add ("WOW", "WOW"); add ("LOW CUT", "CORTE GRAVES"); add ("HIGH CUT", "CORTE AGUDOS");
        add ("PING", "PING"); add ("PONG", "PONG"); add ("DUCK", "DUCK"); add ("SPRING", "RESORTE"); add ("PRE-DELAY", "PRE-DELAY");
        add ("TENSION", "TENSIÓN"); add ("DAMP", "APAGAR"); add ("DRIP", "GOTEO");
        add ("RUN", "MARCHA"); add ("HOST SYNC", "SYNC DAW"); add ("MIDI", "MIDI"); add ("MIDI OUT", "SALIDA MIDI"); add ("FILL", "FILL");
        add ("DIRECTION", "DIRECCIÓN"); add ("STEPS", "PASOS"); add ("PULSES", "GOLPES"); add ("ROTATE", "ROTAR"); add ("SWING", "SWING");
        add ("BPM (NO HOST)", "BPM (SIN DAW)"); add ("ROOT", "TÓNICA"); add ("SCALE", "ESCALA"); add ("OCTAVE", "OCTAVA"); add ("RANGE", "RANGO");
        add ("GATE", "DURACIÓN"); add ("PROB", "PROB"); add ("DENSITY", "DENSIDAD"); add ("AMOUNT", "CANTIDAD"); add ("LONG / SHORT", "LARGA / CORTA");
        add ("MARBLES", "MARBLES"); add ("DEJA VU", "DEJA VU"); add ("BIAS", "TENDENCIA"); add ("EDIT", "EDITAR"); add ("SEQ 2", "SEQ 2");
        add ("STORE A", "GUARDAR A"); add ("STORE B", "GUARDAR B"); add ("MORPH", "MORPH"); add ("PEDAL", "PEDAL"); add ("SUSTAIN ", "SUSTAIN");
        add ("SYMPATHY", "SIMPATÍA"); add ("LIFE", "VIDA"); add ("LOCK DEST", "DESTINO LOCK"); add ("SEQ 2 FOLLOW", "SEQ 2 SIGUE"); add ("MUTATE", "MUTAR");
        add ("HARMONICS", "ARMÓNICOS"); add ("TIMBRE", "TIMBRE"); add ("TIMBRE MOD", "MOD TIMBRE"); add ("MORPH MOD", "MOD MORPH");
        add ("LPG", "LPG"); add ("ON / DRONE", "ON / DRONE"); add ("COLOUR", "COLOR"); add ("C > A FEED", "C > A"); add ("SEQ 2 PLAYS", "SEQ 2 TOCA");
        add ("MODE", "MODO"); add ("SIZE", "TAMAÑO"); add ("REVERB", "REVERB"); add ("HOLD", "MANTENER"); add ("PLATE", "PLACA"); add ("MOTION", "MOVIMIENTO");
        add ("LOW", "GRAVES"); add ("MID", "MEDIOS"); add ("MID FREQ", "FREC MEDIOS"); add ("HIGH", "AGUDOS"); add ("LIMITER", "LIMITADOR"); add ("QUALITY", "CALIDAD");
        add ("MATERIAL ", "MATERIAL"); add ("SPACE", "ESPACIO");
        add ("SEQ 1  A", "SEQ 1  A"); add ("SEQ 2  B", "SEQ 2  B");
        // ---- grilla del secuenciador
        add ("NOTE", "NOTA"); add ("OCT", "OCT"); add ("VELO", "VELO"); add ("RTCH", "RTCH"); add ("COND", "COND"); add ("LOCK", "LOCK"); add ("MODEL", "MODELO"); add ("C MODEL", "MODELO C");
        add ("KNOB MODEL", "MODELO DE LA PERILLA"); add ("C MODEL: a different engine C model on each step", "MODELO C: otro modelo del motor C en cada paso");
        add ("PAGE", "PÁGINA"); add ("STEP", "PASO"); add ("PATTERN", "PATRÓN"); add ("CHAIN", "CADENA"); add ("AUTO", "AUTO");
        add ("AUTO (= STEPS)", "AUTO (= PASOS)"); add ("FIT SCALE", "AJUSTAR ESCALA"); add ("RND LANE", "AZAR CARRIL"); add ("RND RHYTHM", "AZAR RITMO");
        add ("RND ALL", "AZAR TODO"); add ("GENERATE", "GENERAR"); add ("RESET", "REINICIAR"); add ("COPY", "COPIAR"); add ("PASTE", "PEGAR");
        add ("PROBABILITY", "PROBABILIDAD"); add ("RATCHET", "RATCHET"); add ("CONDITION", "CONDICIÓN"); add ("1 HIT", "1 GOLPE");
        // ---- buscador de presets
        add ("SEARCH PRESETS...", "BUSCAR PRESETS..."); add ("CLOSE", "CERRAR");
        add ("PREVIEW", "ESCUCHAR"); add ("Preview: plays a note every time you pick a preset", "Escuchar: toca una nota cada vez que elegís un preset");
        add ("CABLES", "CABLES"); add ("CABLES: SHOWN", "CABLES: A LA VISTA"); add ("CABLES: FADED", "CABLES: TRANSPARENTES"); add ("CABLES: HIDDEN", "CABLES: OCULTOS");
        add ("Cables: shown / faded / hidden (they keep working)", "Cables: a la vista / transparentes / ocultos (siguen funcionando)");
        add ("NOT PATCHED", "SIN CABLE"); add ("CABLE", "CABLE"); add ("CABLES OUT", "CABLES"); add ("ALL", "TODOS"); add ("FAVORITES", "FAVORITOS"); add ("USER", "USUARIO");
        add ("RESONATOR", "RESONADOR"); add ("IMPACT", "IMPACTO"); add ("HYBRID", "HÍBRIDOS"); add ("SEQUENCE ", "SECUENCIAS");
        add ("ACOUSTIC", "ACÚSTICOS"); add ("BASS", "BAJOS"); add ("PAD", "PADS"); add ("KEYS", "TECLADOS"); add ("PERC", "PERCUSIÓN");
        add ("CLICK = LOAD", "CLIC = CARGAR"); add ("ARROWS = BROWSE", "FLECHAS = RECORRER"); add ("STAR = FAVORITE", "ESTRELLA = FAVORITO");
        add ("ENTER / ESC = CLOSE", "ENTER / ESC = CERRAR");
        // ---- acerca de
        add ("ENGINES", "MOTORES"); add ("CREDITS", "CRÉDITOS"); add ("COMPUTER KEYBOARD", "TECLADO DE LA COMPU");
        add ("FREE & OPEN SOURCE", "LIBRE Y GRATIS"); add ("LANGUAGE", "IDIOMA"); add ("SOURCE CODE", "CÓDIGO FUENTE");
        add ("\xe2\x99\xa5 DONATE \xc2\xb7 PAYPAL", "\xe2\x99\xa5 DONAR \xc2\xb7 PAYPAL"); add ("\xe2\x99\xa5 MERCADO PAGO", "\xe2\x99\xa5 MERCADO PAGO");
        add ("\xe2\x9c\x93 ALIAS COPIED", "\xe2\x9c\x93 ALIAS COPIADO"); add ("Mercado Pago (Argentina): alias", "Mercado Pago (Argentina): alias"); add ("(the button copies it)", "(el botón lo copia)");
        add ("CLICK ANYWHERE ELSE TO CLOSE", "CLIC EN CUALQUIER OTRO LUGAR PARA CERRAR"); add ("SOON", "PRONTO");
        add ("A  Resonator (modal, string, piano, tine, reed)    B  Impact (bars, plates, bells, drums)\n"
             "C  Macro: 24 synthesis models    + granular TEXTURE, plate reverb, 7 filter types",
             "A  Resonador (modal, cuerda, piano, tine, reed)    B  Impacto (barras, placas, campanas, parches)\n"
             "C  Macro: 24 modelos de síntesis    + TEXTURA granular, reverb de placa, 7 filtros");
        add ("Engine C, TEXTURE and MARBLES use Plaits, Clouds and Marbles by Mutable Instruments\n"
             "(\xc3\x89milie Gillet), MIT license. Typeface: Montserrat, SIL Open Font License.",
             "El motor C, TEXTURA y MARBLES usan Plaits, Clouds y Marbles de Mutable Instruments\n"
             "(\xc3\x89milie Gillet), licencia MIT. Tipografía: Montserrat, SIL Open Font License.");
        add ("A W S E D F T G Y H U J K = notes     Z / X = octave     C / V = velocity\n"
             "Right-click a knob: MIDI learn     Double-click: default value",
             "A W S E D F T G Y H U J K = notas     Z / X = octava     C / V = velocidad\n"
             "Clic derecho en una perilla: aprender MIDI     Doble clic: valor por defecto");
        add ("URing is free software (GNU AGPL-3.0). If it makes your music better,\n"
             "a small donation helps keep it alive and growing.",
             "URing es software libre (GNU AGPL-3.0). Si te ayuda en tu música,\n"
             "una pequeña donación ayuda a que siga vivo y creciendo.");
        // ---- opciones de los selectores (valores de parámetros)
        add ("Modal", "Modal"); add ("String", "Cuerda"); add ("Sympathetic", "Simpático"); add ("Metallic", "Metálico"); add ("Piano", "Piano");
        add ("Tine Keys", "Tine Keys"); add ("Reed Keys", "Reed Keys");
        add ("Pluck", "Púa"); add ("Strike", "Golpe"); add ("Bow", "Arco"); add ("Blow", "Soplido"); add ("Hammer", "Martillo"); add ("Engine C", "Motor C");
        add ("C > B FEED", "C > B");
        add ("Membrane", "Parche"); add ("Bar", "Barra"); add ("Plate", "Placa"); add ("Bell", "Campana"); add ("Tube", "Tubo"); add ("Glass", "Vidrio");
        add ("Marimba", "Marimba"); add ("Vibes", "Vibráfono"); add ("Mbira", "Mbira"); add ("Steel Pan", "Steel Pan"); add ("Hand Drum", "Tambor de mano"); add ("Gong", "Gong");
        add ("Mallet", "Maza"); add ("Noise", "Ruido"); add ("Bounce", "Rebote"); add ("Crackle", "Chisporroteo");
        add ("Low Pass", "Pasa-bajos"); add ("Band Pass", "Pasa-banda"); add ("High Pass", "Pasa-altos"); add ("Notch", "Notch");
        add ("Ladder 24", "Ladder 24"); add ("Comb", "Peine"); add ("Formant", "Formante");
        add ("Sine", "Seno"); add ("Triangle", "Triángulo"); add ("Saw", "Sierra"); add ("Square", "Cuadrada"); add ("S&H", "S&H"); add ("Drift", "Deriva");
        add ("4 Bars", "4 compases"); add ("2 Bars", "2 compases"); add ("1 Bar", "1 compás"); add ("Step", "Paso");
        add ("Off", "Apagado"); add ("Pitch", "Afinación"); add ("A Structure", "A Estructura"); add ("A Bright", "A Brillo"); add ("A Damping", "A Amortiguación");
        add ("A Position", "A Posición"); add ("B Material", "B Material"); add ("B Decay", "B Caída"); add ("B Tone", "B Tono"); add ("B Spread", "B Dispersión");
        add ("Cutoff", "Corte"); add ("Resonance", "Resonancia"); add ("A/B Mix", "Mezcla A/B"); add ("Delay Time", "Tiempo de delay");
        add ("Glitch Prob", "Prob. glitch"); add ("Spring Mix", "Mezcla resorte"); add ("A > B Feed", "A > B"); add ("Volume (Tremolo)", "Volumen (trémolo)");
        add ("C Timbre", "C Timbre"); add ("C Morph", "C Morph"); add ("C Harmonics", "C Armónicos");
        add ("A Brightness", "A Brillo"); add ("Filter Cutoff", "Corte del filtro");
        add ("Forward", "Adelante"); add ("Backward", "Atrás"); add ("Ping-Pong", "Ping-pong"); add ("Random", "Al azar");
        add ("Minor", "Menor"); add ("Major", "Mayor"); add ("Dorian", "Dórica"); add ("Phrygian", "Frigia"); add ("Mixolydian", "Mixolidia");
        add ("Harmonic Minor", "Menor armónica"); add ("Phrygian Dom", "Frigia dominante"); add ("Minor Penta", "Penta menor"); add ("Major Penta", "Penta mayor");
        add ("Blues", "Blues"); add ("Custom", "Personalizada");
        add ("Unison", "Unísono"); add ("+3rd (m)", "+3ra (m)"); add ("+3rd (M)", "+3ra (M)"); add ("+4th", "+4ta"); add ("+5th", "+5ta");
        add ("+Octave", "+Octava"); add ("-Octave", "-Octava");
        add ("Engine B", "Motor B"); add ("Engine C", "Motor C"); add ("Main", "Principal"); add ("Aux", "Aux"); add ("Stereo", "Estéreo");
        add ("Seq Cycle", "Vuelta del SEQ");
        add ("Granular", "Granular"); add ("Stretch", "Estirar"); add ("Looping Delay", "Delay en bucle"); add ("Spectral", "Espectral");
        add ("Eco", "Eco"); add ("High (2x)", "Alta (2x)");
        return m;
    }();
    return t;
}

// Traduce un texto exacto; si no está en la tabla lo devuelve igual (inglés)
inline juce::String tr (const juce::String& en)
{
    if (! spanish()) return en;
    const auto& t = table();
    if (auto it = t.find (en); it != t.end()) return it->second;
    return en;
}

// Para mensajes armados (prefijo fijo + dato): traduce el texto entero o, si no, el prefijo conocido
inline juce::String ui (const juce::String& en)
{
    if (! spanish() || en.isEmpty()) return en;
    if (auto exact = tr (en); exact != en) return exact;
    if (auto upper = tr (en.toUpperCase()); upper != en.toUpperCase()) return upper;   // nombres de parámetros ("Volume")
    static const char* prefixes[] { "AUTOMATION  -  PATTERN ", "ENGINE C  MODEL ", "MIDI LEARN  ", "NOTHING TO ", "NEW MELODY IN ",
                                    "PATTERN ", "PRESET ", "PATCH ", "STEP ", "STEPS ", "PAGE " };
    static const std::unordered_map<juce::String, juce::String> es {
        { "AUTOMATION  -  PATTERN ", "AUTOMATIZACIÓN  -  PATRÓN " }, { "ENGINE C  MODEL ", "MOTOR C  MODELO " },
        { "MIDI LEARN  ", "APRENDER MIDI  " }, { "NOTHING TO ", "NADA PARA " }, { "NEW MELODY IN ", "MELODÍA NUEVA EN " },
        { "PATTERN ", "PATRÓN " }, { "PRESET ", "PRESET " }, { "PATCH ", "CABLE " }, { "STEP ", "PASO " }, { "STEPS ", "PASOS " },
        { "PAGE ", "PÁGINA " } };
    for (auto* p : prefixes)
        if (en.startsWith (p))
            return juce::String::fromUTF8 (es.at (p).toRawUTF8()) + tr (en.substring ((int) strlen (p)));
    return en;
}


} // namespace mm::lang
