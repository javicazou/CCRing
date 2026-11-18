// Genera las diapositivas del video tutorial de URing (1920x1080): captura real del plugin,
// la zona que se explica resaltada, título y subtítulo. Escribe también el texto de la
// narración de cada diapositiva (para `say`) y la lista de diapositivas.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace tutorial
{
inline juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

struct Slide
{
    const char* title;
    const char* caption;     // subtítulo corto en pantalla
    const char* narration;   // lo que dice la voz
    int tab;                 // 0 SYNTH 1 FX 2 SEQ 3 LIVE, -1 = portada
    const char* preset;
    std::vector<juce::Rectangle<int>> highlight; // en coordenadas del editor (1240x800)
    std::function<void (URingAudioProcessor&)> setup;
    float music = 0.08f;     // volumen de fondo de URing durante la diapositiva
    float extraSeconds = 0.8f;
    int seqLane = -1;        // carril de la grilla a mostrar (SEQ)
};

// Graba un barrido de BRIGHTNESS y CUTOFF con REC (para mostrar las perillas en celeste)
struct PlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (true); p.setBpm (bpm); p.setPpqPosition (ppq); return p;
    }
};
inline void recordDemoMoves (URingAudioProcessor& p)
{
    static PlayHead ph;
    ph.ppq = 0.0;
    p.setPlayHead (&ph);
    p.setPlayConfigDetails (0, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    auto* len = p.apvts.getParameter (IDs::autoLen); len->setValueNotifyingHost (len->convertTo0to1 (2.0f));
    auto* rec = p.apvts.getParameter (IDs::autoRec); rec->setValueNotifyingHost (1.0f);
    auto* br = p.apvts.getParameter (IDs::aBright); auto* cu = p.apvts.getParameter (IDs::cutoff);
    br->beginChangeGesture(); cu->beginChangeGesture();
    juce::AudioBuffer<float> b (2, 256);
    const int blocks = (int) (8.0 * 60.0 / ph.bpm * 48000.0 / 256.0);
    for (int n = 0; n < blocks + 20; ++n)
    {
        const float x = (float) n / (float) blocks, tri = x < 0.5f ? x * 2.0f : 2.0f - x * 2.0f;
        if (n < blocks) { br->setValueNotifyingHost (0.9f - 0.6f * tri); cu->setValueNotifyingHost (0.3f + 0.6f * tri); }
        juce::MidiBuffer m; p.processBlock (b, m);
        ph.ppq += 256.0 * ph.bpm / (60.0 * 48000.0);
    }
    br->endChangeGesture(); cu->endChangeGesture();
    rec->setValueNotifyingHost (1.0f); // REC encendido para la captura
}

inline std::vector<Slide> slides()
{
    using R = juce::Rectangle<int>;
    auto setP = [] (URingAudioProcessor& p, const char* id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
    return {
        { "URING  -  TUTORIAL", "Sinte de modelado f\xc3\xads" "ico de Monstri Audio",
          "\xc2\xa1" "Hola! En este tutorial te muestro c\xc3\xb3mo usar URing, el sinte de modelado f\xc3\xadsico de Monstri Audio. "
          "Vas a ver c\xc3\xb3mo cargarlo en Ableton, c\xc3\xb3mo elegir sonidos, armar secuencias, grabar movimientos de perillas y tocar en vivo. Dale, arrancamos.",
          -1, "Felt Upright", {}, {}, 0.35f, 1.5f },

        { "1. CARGARLO EN ABLETON", "Preferencias > Plug-ins > Re-escanear  -  Monstri Audio > URing",
          "Primero, en Ableton and\xc3\xa1 a Preferencias, Plug-ins, y toc\xc3\xa1 Re-escanear. Despu\xc3\xa9s cre\xc3\xa1 una pista MIDI y arrastr\xc3\xa1 URing "
          "desde Plug-ins, en la carpeta Monstri Audio. Si quer\xc3\xa9s probarlo sin Ableton, tambi\xc3\xa9n ten\xc3\xa9s la aplicaci\xc3\xb3n suelta.",
          0, "Felt Upright", { R (6, 4, 1228, 56) }, {} },

        { "2. ELEGIR UN SONIDO", "Flechas o lista de presets  -  el volumen arranca en -20 dB",
          "Arriba est\xc3\xa1n los presets. Con las flechitas pas\xc3\xa1s de uno a otro, o abr\xc3\xads la lista y eleg\xc3\xads por categor\xc3\xad" "a: "
          "resonadores, impactos, ac\xc3\xbasticos, secuencias, drones y los de la p\xc3\xa1gina LIVE. Cada vez que carg\xc3\xa1s un preset, el volumen arranca "
          "en menos veinte decibeles, as\xc3\xad no te vuela la cabeza.",
          0, "Marimba Rosewood", { R (440, 14, 290, 34) }, {} },

        { "ESCUCH\xc3\x81", "Algunos sonidos de URing", "Escuch\xc3\xa1 algunos sonidos.", -1, "Thumb Piano Box", {}, {}, 1.0f, 9.0f },

        { "3. GUARDAR TUS SONIDOS", "* = cambiaste algo  -  SAVE lo guarda como preset tuyo",
          "Si toc\xc3\xa1s cualquier perilla, al lado del nombre aparece un asterisco. Eso quiere decir que cambiaste el sonido, pero el preset original "
          "no se pisa. Si te gusta lo que armaste, apret\xc3\xa1 SAVE y ponele un nombre: queda guardado como preset tuyo.",
          0, "Felt Upright", { R (476, 14, 214, 34), R (730, 14, 64, 34) },
          [setP] (URingAudioProcessor& p) { setP (p, IDs::aBright, 0.7f); } },

        { "4. MOTOR A: EL RESONADOR", "MODEL + EXCITER  -  Structure, Brightness, Damping, Position",
          "En la pesta\xc3\xb1" "a SYNTH est\xc3\xa1 el coraz\xc3\xb3n. El motor A es el resonador: eleg\xc3\xads el modelo, como cuerda, piano, tine o metal, "
          "y el excitador, que es c\xc3\xb3mo se toca: p\xc3\xba" "a, martillo, arco o soplido. Structure, Brightness, Damping y Position cambian el material, "
          "el brillo, cu\xc3\xa1nto dura y d\xc3\xb3nde se golpea.",
          0, "Felt Upright", { R (16, 70, 444, 330) }, {} },

        { "5. MOTOR B Y LA MEZCLA", "Impacto: marimba, vibr\xc3\xa1" "fono, mbira, steel pan, gong...  -  A / B mezcla",
          "El motor B es de impacto: membranas, barras, marimba, vibr\xc3\xa1" "fono, mbira, steel pan y gong. Con la perilla A barra B del m\xc3\xb3" "dulo MIX "
          "mezcl\xc3\xa1s los dos motores, y con A mayor B FEED hac\xc3\xa9s que el resonador excite al impacto.",
          0, "Marimba Rosewood", { R (468, 70, 304, 330), R (780, 70, 444, 330) }, {} },

        { "6. FILTRO, ENVOLVENTE, LFO Y SALIDA", "Arrastr\xc3\xa1 cables desde OUT del LFO a las entradas  -  BODY = caja de madera",
          "Abajo ten\xc3\xa9s el filtro, la envolvente, dos LFO y la salida. Los LFO se conectan con cables: arrastr\xc3\xa1s desde la salida del LFO "
          "hasta cualquier entrada de los m\xc3\xb3" "dulos de arriba. En OUTPUT est\xc3\xa1n el volumen, la respuesta a la velocidad, y BODY, que le suma una caja de madera.",
          0, "Felt Upright", { R (16, 420, 1208, 334) }, {} },

        { "7. EFECTOS", "Chorus > Glitch > Tape Delay > Spring Reverb",
          "En la pesta\xc3\xb1" "a FX est\xc3\xa1 la cadena de efectos: chorus, glitch, tape delay y spring reverb, en ese orden. El delay se sincroniza al tempo "
          "y puede hacer ping pong. Para minimal, un poquito de spring y un delay de tres dieciseisavos quedan b\xc3\xa1rbaro.",
          1, "Tine Chorus", { R (16, 70, 1208, 332) }, {} },

        { "8. EL SECUENCIADOR", "RUN  -  STEPS, PULSES, ROTATE  -  escala y tonalidad",
          "En la pesta\xc3\xb1" "a SEQ est\xc3\xa1 el secuenciador euclidiano. Con RUN lo prend\xc3\xa9s, y sigue el tempo de Ableton. Steps es el largo del patr\xc3\xb3n, "
          "Pulses cu\xc3\xa1ntos golpes reparte, y Rotate los corre de lugar. En NOTES eleg\xc3\xads la t\xc3\xb3nica, la escala y el rango.",
          2, "Mutating Plonks", { R (16, 70, 726, 262) }, {} },

        { "9. LA GRILLA", "NOTE, OCT, VELO, GATE, PROB, RTCH, COND, LOCK  -  cada carril con su largo",
          "Abajo est\xc3\xa1 la grilla. Arriba eleg\xc3\xads el carril: nota, octava, velocidad, gate, probabilidad, ratchet, condici\xc3\xb3n y lock. "
          "Dibuj\xc3\xa1s con el mouse en cada paso. Cada carril puede tener su propio largo, as\xc3\xad la melod\xc3\xad" "a se desfasa y nunca se repite igual.",
          2, "Mutating Plonks", { R (38, 358, 370, 32), R (100, 478, 1104, 304) }, {} },

        { "10. IDEAS AL TOQUE", "GENERATE, la carita y los patrones A a H con CHAIN",
          "Si no se te ocurre nada, toc\xc3\xa1 GENERATE para una melod\xc3\xad" "a nueva en la escala, o la carita para una sorpresa. Con las letras de la A a la H "
          "guard\xc3\xa1s ocho patrones, y con CHAIN los encaden\xc3\xa1s.",
          2, "Mutating Plonks", { R (1006, 358, 118, 30), R (434, 398, 456, 30) }, {} },

        { "11. DOS SECUENCIADORES", "SEQ 2 ON: el 1 toca el motor A, el 2 el motor B  -  EDIT elige cu\xc3\xa1l edit\xc3\xa1s",
          "Si prend\xc3\xa9s SEQ 2, el secuenciador uno toca el motor A y el dos toca el motor B, cada uno con su dibujo. Con EDIT eleg\xc3\xads cu\xc3\xa1l "
          "edit\xc3\xa1s en la grilla, y el segundo se pone celeste.",
          2, "Fifths Machine", { R (1032, 70, 192, 262), R (16, 345, 1208, 445) },
          [setP] (URingAudioProcessor& p) { setP (p, IDs::seq2Follow, 0.0f); p.setEditSequencer (1); } },

        { "12. GRABAR MOVIMIENTOS (REC)", "REC espera el paso 1, graba una vuelta y se apaga solo  -  clic derecho borra",
          "Para grabar movimientos de perillas, dale play y apret\xc3\xa1 REC. Espera al paso uno, graba una vuelta completa del secuenciador y se apaga solo. "
          "Mov\xc3\xa9 lo que quieras mientras graba. Las perillas grabadas se ven en celeste, y con clic derecho en REC borr\xc3\xa1s lo grabado.",
          0, "Mbira Ghost", { R (818, 14, 140, 34), R (205, 98, 64, 66), R (43, 533, 64, 66) }, [] (URingAudioProcessor& p) { recordDemoMoves (p); } },

        { "13. LIVE: MACROS Y MORPH", "MATERIAL, SPACE, MOTION, TONE  -  STORE A / STORE B + MORPH",
          "En la pesta\xc3\xb1" "a LIVE ten\xc3\xa9s cuatro macros: material, espacio, movimiento y tono, y cada una mueve varias cosas a la vez. "
          "En MORPH guard\xc3\xa1s un sonido en A, otro en B, y con la perilla pas\xc3\xa1s de uno al otro.",
          3, "Pedal Grand", { R (16, 70, 600, 332) }, {} },

        { "14. LIVE: PIANO, VIDA Y SECUENCIA", "PEDAL + SYMPATHY  -  LIFE  -  LOCK DEST, SEQ 2 FOLLOW, MUTATE",
          "Con PEDAL y SYMPATHY el piano queda sonando, con la resonancia de las cuerdas. LIFE le da respiraci\xc3\xb3n. En SEQUENCE eleg\xc3\xads a d\xc3\xb3nde van los locks, "
          "hac\xc3\xa9s que el SEQ 2 siga al uno en un intervalo, y con MUTATE el patr\xc3\xb3n va cambiando solo.",
          3, "Pedal Grand", { R (623, 70, 601, 332) }, {} },

        { "15. TRUCO: MIDI LEARN", "Clic derecho en una perilla > MIDI Learn > mov\xc3\xa9 tu controlador",
          "Un truco: hac\xc3\xa9 clic derecho en cualquier perilla y eleg\xc3\xad MIDI Learn. Mov\xc3\xa9s un control de tu controlador y queda asignado. "
          "Ideal para tocar en vivo.",
          3, "Pedal Grand", { R (52, 100, 88, 92) }, {} },

        { "\xc2\xa1" "A HACER MINIMAL!", "URing  -  Monstri Audio",
          "Y eso es todo. Carg\xc3\xa1 un preset de LIVE, prend\xc3\xa9 REC y jug\xc3\xa1 con las macros. \xc2\xa1Que lo disfrutes, y a hacer minimal!",
          3, "Mutating Plonks", {}, {}, 0.5f, 3.0f },
    };
}

// Versión corta (unos 3 minutos): lo esencial + cómo funciona el motor de audio. Pensada para
// grabarla con la voz del usuario (tab -2 = diagrama del motor)
inline std::vector<Slide> slides3()
{
    using R = juce::Rectangle<int>;
    return {
        { "URING 1.2 EN 3 MINUTOS", "Lo esencial y c\xc3\xb3mo funciona el motor de audio",
          "\xc2\xa1" "Hola! Te muestro URing en tres minutos: lo esencial, y c\xc3\xb3mo funciona el motor de audio.",
          -1, "Felt Upright", {}, {}, 0.3f, 1.0f },

        { "CARGARLO Y ELEGIR SONIDOS", "Buscador: categor\xc3\xads" "as, b\xc3\xbasqueda y favoritos  -  deshacer / rehacer  -  * = cambiado  -  SAVE guarda",
          "En Ableton, re-escane\xc3\xa1 los plugins y carg\xc3\xa1 URing desde Monstri Audio en una pista MIDI. Con el bot\xc3\xb3n de lista abr\xc3\xads el buscador de presets: "
          "categor\xc3\xad" "as, b\xc3\xbasqueda y favoritos con estrella. Siempre arrancan en menos veinte decibeles. Si toc\xc3\xa1s algo aparece un asterisco, "
          "con las flechas curvas deshac\xc3\xa9s o rehac\xc3\xa9s, y con SAVE guard\xc3\xa1s tu versi\xc3\xb3n.",
          0, "Felt Upright", { R (422, 14, 334, 34) }, {} },

        { "C\xc3\x93MO FUNCIONA EL MOTOR DE AUDIO", "Excitador > A + B  -  notas > C  -  mezcla > filtro y envolvente > cuerpo > efectos",
          "\xc2\xbf" "C\xc3\xb3mo suena URing? No usa samples: modela objetos f\xc3\xadsicos. Cada nota tiene un excitador, que es el golpe: una p\xc3\xba" "a, un martillo, "
          "un arco o un mazo. Esa energ\xc3\xad" "a hace vibrar al motor A, que es una cuerda, un piano o un metal, y al motor B, que es algo que se golpea, como una marimba o un parche. "
          "Y el motor C es un sintetizador con veinticuatro modelos. Todo se mezcla, pasa por el filtro y la envolvente, el cuerpo de madera y los efectos, y sale.",
          -2, "Felt Upright", {}, {}, 0.08f, 1.4f },

        { "ESCUCH\xc3\x81", "As\xc3\xad suenan los dos motores",
          "Escuch\xc3\xa1 c\xc3\xb3mo suena.", -1, "Thumb Piano Box", {}, {}, 1.0f, 14.0f },

        { "MOTOR A: EL RESONADOR", "Structure = material  -  Brightness = brillo  -  Damping = duraci\xc3\xb3n  -  Position = d\xc3\xb3nde golpe\xc3\xa1s",
          "En el motor A, Structure cambia el material y los arm\xc3\xb3nicos, Brightness el brillo, Damping cu\xc3\xa1nto dura la vibraci\xc3\xb3n, y Position d\xc3\xb3nde golpe\xc3\xa1s: "
          "cerca del borde suena m\xc3\xa1s fino, y en el centro m\xc3\xa1s redondo.",
          0, "Felt Upright", { R (16, 70, 444, 330) }, {} },

        { "MOTOR B Y LA MEZCLA", "Material, Decay, Tone  -  A / B mezcla  -  A > B FEED: la cuerda hace sonar al impacto",
          "El motor B funciona igual, con material, decay y tono. Con A barra B mezcl\xc3\xa1s los dos, y con A mayor B feed el resonador hace sonar al impacto, "
          "como una cuerda apoyada sobre un tambor.",
          0, "Marimba Rosewood", { R (468, 70, 304, 330), R (780, 70, 444, 330) }, {} },

        { "MOTOR C: MACRO", "24 modelos  -  HARMONICS, TIMBRE, MORPH  -  A/B - C mezcla  -  C > A FEED excita al resonador",
          "La pesta\xc3\xb1" "a MACRO es el motor C: veinticuatro modelos de s\xc3\xadntesis, desde anal\xc3\xb3gico y FM hasta acordes, voz, granular y bombo. "
          "Harmonics, Timbre y Morph cambian el sonido de cada modelo. Con la perilla A barra B guion C lo mezcl\xc3\xa1s con los otros motores, "
          "y con C mayor A feed el motor C hace sonar a la cuerda, como un excitador nuevo. Tambi\xc3\xa9n lo puede tocar el SEQ 2.",
          4, "Dub Chord Stab", { R (16, 70, 1208, 520) }, {} },

        { "MOVIMIENTO Y CUERPO", "Cables desde los LFO a cualquier entrada  -  BODY = caja de madera",
          "Abajo est\xc3\xa1n el filtro, la envolvente y dos LFO. Arrastr\xc3\xa1s un cable desde un LFO a cualquier entrada, y ese par\xc3\xa1metro se mueve solo. "
          "BODY suma una caja de madera que resuena con el instrumento.",
          0, "Felt Upright", { R (16, 420, 1208, 334) }, {} },

        { "EL SECUENCIADOR", "STEPS y PULSES  -  grilla: nota, velocidad, probabilidad, LOCK  -  GENERATE  -  SEQ 2",
          "En SEQ, el secuenciador reparte golpes de forma euclidiana: Steps es el largo y Pulses cu\xc3\xa1ntos golpes. En la grilla dibuj\xc3\xa1s notas, velocidad, "
          "probabilidad y locks, que cambian el timbre de cada paso. Generate te arma una melod\xc3\xad" "a nueva, y con SEQ 2 cada motor tiene su propio dibujo.",
          2, "Mutating Plonks", { R (16, 70, 726, 262), R (38, 358, 370, 32), R (1006, 358, 118, 30) }, {} },

        { "GRABAR MOVIMIENTOS", "REC: espera el paso 1, graba una vuelta y se apaga solo",
          "Para grabar movimientos, dale play y apret\xc3\xa1 REC: espera el paso uno, graba una vuelta y se apaga solo. Lo grabado queda en celeste.",
          0, "Mbira Ghost", { R (800, 14, 158, 34), R (205, 98, 64, 66), R (43, 533, 64, 66) }, [] (URingAudioProcessor& p) { recordDemoMoves (p); } },

        { "EN VIVO", "4 macros  -  MORPH  -  PEDAL y LIFE  -  pad X/Y  -  osciloscopio y espectro",
          "En LIVE ten\xc3\xa9s cuatro macros que mueven varias cosas a la vez, el morph entre dos sonidos, el pedal y LIFE, que hace que el sonido respire. "
          "Abajo, el pad X Y: de izquierda a derecha vas de A a B, y para arriba abr\xc3\xads el espacio. Al lado ves la salida: osciloscopio, espectro y nivel.",
          3, "Fifths Machine", { R (16, 70, 1208, 302), R (16, 382, 1208, 408) }, {} },

        { "\xc2\xa1" "A HACER MINIMAL!", "URing - Monstri Audio  |  Voz: Piper es_AR Daniela (datos OpenSLR 61, CC BY-SA 4.0)",
          "Eso es lo esencial. Carg\xc3\xa1 un preset, prend\xc3\xa9 REC y jug\xc3\xa1. \xc2\xa1" "A hacer minimal!",
          3, "Mutating Plonks", {}, {}, 0.5f, 3.0f },
    };
}


// Versión corta en inglés (URing 1.4): misma estructura, con TEXTURE, filtros nuevos y MARBLES
inline bool& english() { static bool e = false; return e; }

inline std::vector<Slide> slides3en()
{
    using R = juce::Rectangle<int>;
    return {
        { "URING 1.4 IN 3 MINUTES", "The essentials, and how the audio engine works",
          "Hi! Here is URing in three minutes: the essentials, and how the audio engine works.",
          -1, "Felt Upright", {}, {}, 0.3f, 1.0f },

        { "LOAD IT AND PICK SOUNDS", "Browser: categories, search and favourites  -  undo / redo  -  * = changed  -  SAVE stores it",
          "In Ableton, rescan your plug-ins and load URing from Monstri Audio on a MIDI track. The list button opens the preset browser, "
          "with categories, search, and favourites with a star. Presets always start at minus twenty decibels. If you change something an asterisk appears, "
          "the curved arrows undo and redo, and SAVE stores your own version.",
          0, "Felt Upright", { R (490, 14, 324, 34) }, {} },

        { "HOW THE AUDIO ENGINE WORKS", "Exciter > A + B  -  notes > C  -  mix > filter and envelope > body > effects",
          "How does URing make sound? It uses no samples: it models physical objects. Every note has an exciter, which is the hit: a pick, a hammer, "
          "a bow or a mallet. That energy makes engine A vibrate, which is a string, a piano or a metal, and engine B, which is something you strike, like a marimba or a drum skin. "
          "Engine C is a synthesizer with twenty-four models. Everything is mixed, goes through the filter and envelope, the wooden body and the effects, and out.",
          -2, "Felt Upright", {}, {}, 0.08f, 1.4f },

        { "LISTEN", "This is how the two engines sound",
          "Have a listen.", -1, "Thumb Piano Box", {}, {}, 1.0f, 14.0f },

        { "ENGINE A: THE RESONATOR", "Structure = material  -  Brightness  -  Damping = how long it rings  -  Position = where you hit",
          "On engine A, Structure changes the material and overtones, Brightness the brightness, Damping how long it rings, and Position where you hit it: "
          "near the edge it sounds thinner, and in the centre rounder.",
          0, "Felt Upright", { R (16, 70, 444, 330) }, {} },

        { "ENGINE B AND THE MIX", "Material, Decay, Tone  -  A / B mix  -  A > B FEED: the string makes the object ring",
          "Engine B works the same way, with material, decay and tone. A slash B mixes the two, and with A to B feed the resonator makes the impact ring, "
          "like a string resting on a drum.",
          0, "Marimba Rosewood", { R (468, 70, 304, 330), R (780, 70, 444, 330) }, {} },

        { "ENGINE C", "24 models  -  HARMONICS, TIMBRE, MORPH  -  ENGINE C level in SYNTH > MIX  -  C > A FEED excites the resonator",
          "The ENGINE C tab is a third engine: twenty-four synthesis models, from analog and FM to chords, speech, granular and kick drum. "
          "Pick a model and it turns on by itself. Harmonics, Timbre and Morph shape each model, and the ENGINE C knob in the mix sets its level. "
          "With C to A feed, engine C makes the string ring, like a new kind of exciter. SEQ 2 can play it too.",
          4, "Dub Chord Stab", { R (16, 70, 1208, 520) }, {} },

        { "MOVEMENT, BODY AND FILTERS", "Cables from the LFOs to any input  -  BODY = wooden box  -  7 filters: ladder, comb, formant",
          "Below are the filter, the envelope and two LFOs. Drag a cable from an LFO to any input, and that parameter moves on its own. "
          "BODY adds a wooden box that resonates with the instrument. The filter has seven types, including a fat ladder, a tuned comb, and a formant filter that sweeps the vowels.",
          0, "Felt Upright", { R (16, 420, 1208, 334) }, {} },

        { "THE SEQUENCER", "STEPS and PULSES  -  grid: note, velocity, probability, LOCK  -  MARBLES  -  GENERATE",
          "In SEQ, the sequencer spreads hits in a Euclidean way: Steps is the length and Pulses how many hits. In the grid you draw notes, velocity, "
          "probability and locks, which change the timbre of each step. Turn on MARBLES and melodies come from a random loop: in the middle of Deja Vu it repeats, at the edges it evolves.",
          2, "Mutating Plonks", { R (16, 70, 1052, 262), R (38, 358, 370, 32) }, {} },

        { "TEXTURE", "Granular based on Clouds  -  4 modes and FREEZE  -  plate reverb  -  EQ, drive and limiter",
          "The TEXTURE tab adds a granular processor based on Clouds, with granular, stretch, looping delay and spectral modes, and a freeze button. "
          "Next to it there is a plate reverb, and the output stage with a three band EQ, drive, and a limiter that never lets the sound clip.",
          5, "Glass Machine", { R (16, 70, 1208, 330) },
          [] (URingAudioProcessor& p) { for (auto* id : { IDs::texOn, IDs::plateOn }) p.apvts.getParameter (id)->setValueNotifyingHost (1.0f); } },

        { "RECORDING MOVEMENTS", "REC: waits for step 1, records one loop and stops by itself",
          "To record movements, press play and hit REC: it waits for step one, records one loop and switches itself off. Recorded knobs turn light blue.",
          0, "Mbira Ghost", { R (818, 14, 140, 34), R (205, 98, 64, 66), R (43, 533, 64, 66) }, [] (URingAudioProcessor& p) { recordDemoMoves (p); } },

        { "PLAYING LIVE", "4 macros  -  MORPH  -  PEDAL and LIFE  -  X/Y pad  -  scope and spectrum",
          "In LIVE you get four macros that move several things at once, morphing between two sounds, the pedal, and LIFE, which makes the sound breathe. "
          "Below is the X Y pad: left to right goes from A to B, and upwards opens up the space. Next to it you see the output: scope, spectrum and level.",
          3, "Fifths Machine", { R (16, 70, 1208, 302), R (16, 382, 1208, 408) }, {} },

        { "NOW MAKE SOME MINIMAL!", "URing - Monstri Audio  |  free and open source  |  paypal.me/JavierCazou",
          "That's the essentials. Load a preset, hit REC and play. URing is free and open source; if you enjoy it, a small donation helps. Now go make some minimal!",
          3, "Mutating Plonks", {}, {}, 0.5f, 3.0f },
    };
}

// Versión corta en español de la 1.4 (interfaz en español)
inline std::vector<Slide> slides3es14()
{
    using R = juce::Rectangle<int>;
    return {
        { "URING 1.4 EN 3 MINUTOS", "Lo esencial y c\xc3\xb3mo funciona el motor de audio",
          "\xc2\xa1" "Hola! Te muestro URing en tres minutos: lo esencial, y c\xc3\xb3mo funciona el motor de audio.",
          -1, "Felt Upright", {}, {}, 0.3f, 1.0f },

        { "CARGARLO Y ELEGIR SONIDOS", "Idioma: logo > ESPA\xc3\x91OL  -  buscador de presets  -  deshacer / rehacer  -  GUARDAR",
          "En Ableton, re-escane\xc3\xa1 los plugins y carg\xc3\xa1 URing desde Monstri Audio en una pista MIDI. Viene en ingl\xc3\xa9s: toc\xc3\xa1 el logo y eleg\xc3\xad espa\xc3\xb1ol. "
          "Con el bot\xc3\xb3n de lista abr\xc3\xads el buscador de presets, con categor\xc3\xad" "as, b\xc3\xbasqueda y favoritos. Si toc\xc3\xa1s algo aparece un asterisco, "
          "con las flechas curvas deshac\xc3\xa9s o rehac\xc3\xa9s, y con guardar te qued\xc3\xa1s con tu versi\xc3\xb3n.",
          0, "Felt Upright", { R (490, 14, 324, 34) }, {} },

        { "C\xc3\x93MO FUNCIONA EL MOTOR DE AUDIO", "Excitador > A + B  -  notas > C  -  mezcla > filtro y envolvente > cuerpo > efectos",
          "\xc2\xbf" "C\xc3\xb3mo suena URing? No usa samples: modela objetos f\xc3\xadsicos. Cada nota tiene un excitador, que es el golpe: una p\xc3\xba" "a, un martillo, "
          "un arco o un mazo. Esa energ\xc3\xad" "a hace vibrar al motor A, que es una cuerda, un piano o un metal, y al motor B, que es algo que se golpea, como una marimba o un parche. "
          "Y el motor C es un sintetizador con veinticuatro modelos. Todo se mezcla, pasa por el filtro y la envolvente, el cuerpo de madera y los efectos, y sale.",
          -2, "Felt Upright", {}, {}, 0.08f, 1.4f },

        { "ESCUCH\xc3\x81", "As\xc3\xad suenan los dos motores",
          "Escuch\xc3\xa1 c\xc3\xb3mo suena.", -1, "Thumb Piano Box", {}, {}, 1.0f, 14.0f },

        { "MOTOR A: EL RESONADOR", "Estructura = material  -  Brillo  -  Amortiguaci\xc3\xb3n = duraci\xc3\xb3n  -  Posici\xc3\xb3n = d\xc3\xb3nde golpe\xc3\xa1s",
          "En el motor A, estructura cambia el material y los arm\xc3\xb3nicos, brillo el brillo, amortiguaci\xc3\xb3n cu\xc3\xa1nto dura la vibraci\xc3\xb3n, y posici\xc3\xb3n d\xc3\xb3nde golpe\xc3\xa1s: "
          "cerca del borde suena m\xc3\xa1s fino, y en el centro m\xc3\xa1s redondo.",
          0, "Felt Upright", { R (16, 70, 444, 330) }, {} },

        { "MOTOR B Y LA MEZCLA", "Material, ca\xc3\xad" "da, tono  -  A / B mezcla  -  A > B: la cuerda hace sonar al impacto",
          "El motor B funciona igual, con material, ca\xc3\xad" "da y tono. Con A barra B mezcl\xc3\xa1s los dos, y con A hacia B el resonador hace sonar al impacto, "
          "como una cuerda apoyada sobre un tambor.",
          0, "Marimba Rosewood", { R (468, 70, 304, 330), R (780, 70, 444, 330) }, {} },

        { "MOTOR C", "24 modelos  -  ARM\xc3\x93NICOS, TIMBRE, MORPH  -  volumen MOTOR C en SINTE > MEZCLA  -  C > A excita al resonador",
          "La pesta\xc3\xb1" "a MOTOR C es un tercer motor: veinticuatro modelos de s\xc3\xadntesis, desde anal\xc3\xb3gico y FM hasta acordes, voz, granular y bombo. "
          "Eleg\xc3\xads un modelo y se prende solo. Arm\xc3\xb3nicos, timbre y morph cambian el sonido de cada modelo, y la perilla MOTOR C de la mezcla le da el volumen. "
          "Con C hacia A, el motor C hace sonar a la cuerda, como un excitador nuevo. Tambi\xc3\xa9n lo puede tocar el SEQ 2.",
          4, "Dub Chord Stab", { R (16, 70, 1208, 520) }, {} },

        { "MOVIMIENTO, CUERPO Y FILTROS", "Cables desde los LFO a cualquier entrada  -  CUERPO = caja de madera  -  7 filtros: ladder, peine, formante",
          "Abajo est\xc3\xa1n el filtro, la envolvente y dos LFO. Arrastr\xc3\xa1s un cable desde un LFO a cualquier entrada, y ese par\xc3\xa1metro se mueve solo. "
          "Cuerpo suma una caja de madera que resuena con el instrumento. El filtro tiene siete tipos, entre ellos un ladder gordo, un peine afinado y un formante que recorre las vocales.",
          0, "Felt Upright", { R (16, 420, 1208, 334) }, {} },

        { "EL SECUENCIADOR", "PASOS y GOLPES  -  grilla: nota, velocidad, probabilidad, LOCK  -  MARBLES  -  GENERAR",
          "En SEQ, el secuenciador reparte golpes de forma euclidiana: pasos es el largo y golpes cu\xc3\xa1ntos golpes. En la grilla dibuj\xc3\xa1s notas, velocidad, "
          "probabilidad y locks, que cambian el timbre de cada paso. Si prend\xc3\xa9s MARBLES, las melod\xc3\xad" "as salen de un bucle al azar: con deja vu al medio se repite, y en los extremos va cambiando.",
          2, "Mutating Plonks", { R (16, 70, 1052, 262), R (38, 358, 370, 32) }, {} },

        { "TEXTURA", "Granular basado en Clouds  -  4 modos y CONGELAR  -  reverb de placa  -  EQ, saturaci\xc3\xb3n y limitador",
          "La pesta\xc3\xb1" "a textura suma un procesador granular basado en Clouds, con modos granular, estirar, delay en bucle y espectral, y un bot\xc3\xb3n para congelar. "
          "Al lado hay una reverb de placa, y la salida con ecualizador de tres bandas, saturaci\xc3\xb3n y un limitador que nunca deja saturar el sonido.",
          5, "Glass Machine", { R (16, 70, 1208, 330) },
          [] (URingAudioProcessor& p) { for (auto* id : { IDs::texOn, IDs::plateOn }) p.apvts.getParameter (id)->setValueNotifyingHost (1.0f); } },

        { "GRABAR MOVIMIENTOS", "REC: espera el paso 1, graba una vuelta y se apaga solo",
          "Para grabar movimientos, dale play y apret\xc3\xa1 REC: espera el paso uno, graba una vuelta y se apaga solo. Lo grabado queda en celeste.",
          0, "Mbira Ghost", { R (818, 14, 140, 34), R (205, 98, 64, 66), R (43, 533, 64, 66) }, [] (URingAudioProcessor& p) { recordDemoMoves (p); } },

        { "EN VIVO", "4 macros  -  MORPH  -  PEDAL y VIDA  -  pad X/Y  -  osciloscopio y espectro",
          "En vivo ten\xc3\xa9s cuatro macros que mueven varias cosas a la vez, el morph entre dos sonidos, el pedal y vida, que hace que el sonido respire. "
          "Abajo, el pad X Y: de izquierda a derecha vas de A a B, y para arriba abr\xc3\xads el espacio. Al lado ves la salida: osciloscopio, espectro y nivel.",
          3, "Fifths Machine", { R (16, 70, 1208, 302), R (16, 382, 1208, 408) }, {} },

        { "\xc2\xa1" "A HACER MINIMAL!", "URing - Monstri Audio  |  gratis y de c\xc3\xb3" "digo libre  |  paypal.me/JavierCazou",
          "Eso es lo esencial. Carg\xc3\xa1 un preset, prend\xc3\xa9 REC y jug\xc3\xa1. URing es gratis y de c\xc3\xb3" "digo libre; si te gusta, una donaci\xc3\xb3n ayuda. \xc2\xa1" "A hacer minimal!",
          3, "Mutating Plonks", {}, {}, 0.5f, 3.0f },
    };
}

// Diagrama del motor de audio (cajas y flechas)
inline void drawEngineDiagram (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace juce;
    const auto accent = Colour (0xffff6a2b), cyan = Colour (0xff3fd8ff), panel = Colour (0xff1c1d21);
    auto box = [&] (Rectangle<float> r, const char* title, const char* sub, Colour c)
    {
        g.setColour (panel); g.fillRoundedRectangle (r, 14.0f);
        g.setColour (c); g.drawRoundedRectangle (r, 14.0f, 3.0f);
        g.setColour (Colours::white); g.setFont (FontOptions (34.0f).withStyle ("Bold"));
        g.drawFittedText (U (title), r.removeFromTop (r.getHeight() * 0.48f).reduced (12.0f, 0.0f).toNearestInt(), Justification::centredBottom, 1);
        g.setColour (Colours::white.withAlpha (0.65f)); g.setFont (FontOptions (24.0f));
        g.drawFittedText (U (sub), r.reduced (14.0f, 6.0f).toNearestInt(), Justification::centredTop, 2);
    };
    auto arrow = [&] (Point<float> a, Point<float> b)
    {
        Path p; p.addArrow ({ a, b }, 5.0f, 22.0f, 20.0f);
        g.setColour (Colours::white.withAlpha (0.8f)); g.fillPath (p);
    };
    const float x0 = area.getX(), y0 = area.getY();
    // Fila 1: notas -> excitador -> motores A / B -> mezcla
    const Rectangle<float> seq (x0 + 20, y0 + 150, 300, 170), exc (x0 + 420, y0 + 80, 330, 150),
                           a (x0 + 850, y0 + 0, 380, 140), b (x0 + 850, y0 + 160, 380, 140), c (x0 + 850, y0 + 320, 380, 140),
                           mix (x0 + 1330, y0 + 150, 300, 170);
    box (seq, english() ? "NOTES" : "NOTAS", english() ? "sequencer, keyboard or Ableton" : "secuenciador, teclado o Ableton", Colours::white);
    box (exc, english() ? "EXCITER" : "EXCITADOR", english() ? "pick, hammer, bow, breath, mallet" : "p\xc3\xba" "a, martillo, arco, soplido, mazo", accent);
    box (a, english() ? "ENGINE A  -  RESONATOR" : "MOTOR A  -  RESONADOR", english() ? "string, piano, tine, metal" : "cuerda, piano, tine, metal", accent);
    box (b, english() ? "ENGINE B  -  IMPACT" : "MOTOR B  -  IMPACTO", english() ? "marimba, drum skin, mbira, gong" : "marimba, parche, mbira, gong", accent);
    box (c, english() ? "ENGINE C" : "MOTOR C", english() ? "24 models: VA, FM, chords, speech..." : "24 modelos: VA, FM, acordes, voz...", Colour (0xff7be3a5));
    box (mix, english() ? "MIX" : "MEZCLA", "A / B  -  A/B - C  -  FEEDS", Colours::white);
    arrow ({ seq.getRight(), seq.getCentreY() - 20 }, { exc.getX(), exc.getCentreY() });
    arrow ({ seq.getRight(), seq.getCentreY() + 40 }, { c.getX(), c.getCentreY() });
    arrow ({ exc.getRight(), exc.getCentreY() - 30 }, { a.getX(), a.getCentreY() });
    arrow ({ exc.getRight(), exc.getCentreY() + 30 }, { b.getX(), b.getCentreY() });
    arrow ({ a.getRight(), a.getCentreY() }, { mix.getX(), mix.getCentreY() - 50 });
    arrow ({ b.getRight(), b.getCentreY() }, { mix.getX(), mix.getCentreY() });
    arrow ({ c.getRight(), c.getCentreY() }, { mix.getX(), mix.getCentreY() + 50 });
    {   // C > A FEED: línea verde punteada por fuera de las cajas, de C hacia A
        const float lx = a.getX() - 22.0f;
        Path feed; feed.startNewSubPath (c.getX(), c.getCentreY() + 30); feed.lineTo (lx, c.getCentreY() + 30);
        feed.lineTo (lx, a.getCentreY() - 30); feed.lineTo (a.getX() - 6, a.getCentreY() - 30);
        Path dashed; const float dl[] { 12.0f, 8.0f };
        PathStrokeType (3.0f).createDashedStroke (dashed, feed, dl, 2);
        g.setColour (Colour (0xff7be3a5)); g.fillPath (dashed);
        Path head; head.addTriangle (a.getX(), a.getCentreY() - 30, a.getX() - 14, a.getCentreY() - 38, a.getX() - 14, a.getCentreY() - 22);
        g.fillPath (head);
        g.setFont (FontOptions (20.0f).withStyle ("Bold"));
        g.drawText ("C > A FEED", Rectangle<float> (lx - 150, c.getCentreY() + 36, 160, 24), Justification::centredRight);
    }    // Fila 2: filtro y envolvente -> cuerpo -> efectos -> salida
    const float y2 = y0 + 580;
    const Rectangle<float> flt (x0 + 120, y2, 380, 170), body (x0 + 600, y2, 300, 170), fx (x0 + 1000, y2, 380, 170), out (x0 + 1480, y2, 250, 170);
    box (flt, english() ? "FILTER + ENVELOPE" : "FILTRO + ENVOLVENTE", english() ? "brightness and shape of each note" : "brillo y forma de cada nota", Colours::white);
    box (body, "BODY", english() ? "wooden box" : "caja de madera", Colours::white);
    box (fx, english() ? "EFFECTS" : "EFECTOS", english() ? "chorus, glitch, delay, reverbs, granular" : "chorus, glitch, delay, spring", Colours::white);
    box (out, english() ? "OUTPUT" : "SALIDA", english() ? "EQ, limiter, volume (-20 dB)" : "volumen (-20 dB)", Colours::white);
    Path link; // de la mezcla baja a la fila 2
    link.startNewSubPath (mix.getCentreX(), mix.getBottom());
    link.lineTo (mix.getCentreX(), y2 - 70); link.lineTo (flt.getCentreX(), y2 - 70);
    g.setColour (Colours::white.withAlpha (0.8f)); g.strokePath (link, PathStrokeType (5.0f));
    arrow ({ flt.getCentreX(), y2 - 72 }, { flt.getCentreX(), y2 });
    arrow ({ flt.getRight(), flt.getCentreY() }, { body.getX(), body.getCentreY() });
    arrow ({ body.getRight(), body.getCentreY() }, { fx.getX(), fx.getCentreY() });
    arrow ({ fx.getRight(), fx.getCentreY() }, { out.getX(), out.getCentreY() });
    // Modulación
    g.setColour (cyan); g.setFont (FontOptions (28.0f).withStyle ("Bold"));
    g.drawText (english() ? String ("LFOs, MACROS, LOCKS AND REC can move any knob along this path") : U ("LFO, MACROS, LOCKS Y REC mueven cualquier perilla de este camino"), Rectangle<float> (x0, y2 + 200, 1760, 40), Justification::centred);
}


// ---- Videos cortos de CCRing 1.5 (menos de 1 minuto): inglés y español
inline std::vector<Slide> slidesShort (bool en)
{
    using R = juce::Rectangle<int>;
    auto seqOn = [] (URingAudioProcessor& p) { p.setEditSequencer (0); auto* prm = p.apvts.getParameter (IDs::seqOn); prm->setValueNotifyingHost (1.0f); };
    std::vector<Slide> v;
    v.push_back ({ "CCRING", en ? "Free physical-modelling synth by CCR" : "Sinte de modelado f\xc3\xadsico gratis de CCR",
                   en ? "This is CCRing, a free physical modelling synth by CCR. Here it is in one minute."
                      : "Este es CCRing, un sinte de modelado f\xc3\xadsico gratis de CCR. Te lo muestro en un minuto.",
                   -1, "Felt Upright", {}, {}, 0.35f, 0.6f });
    v.push_back ({ en ? "165 PRESETS" : "165 PRESETS", en ? "Browser, favourites and PREVIEW" : "Buscador, favoritos y ESCUCHAR",
                   en ? "Pick a sound from a hundred and sixty five presets. Turn on preview to hear each one as you browse."
                      : "Eleg\xc3\xad entre ciento sesenta y cinco presets. Con escuchar, cada uno suena mientras recorr\xc3\xa9s la lista.",
                   0, "Marimba Rosewood", { R (476, 14, 244, 34) }, {} });
    v.push_back ({ en ? "THREE ENGINES" : "TRES MOTORES", en ? "A resonates  -  B strikes  -  C: 24 synth models" : "A resuena  -  B golpea  -  C: 24 modelos de s\xc3\xadntesis",
                   en ? "Engine A resonates, engine B strikes, and engine C adds twenty four synth models that can drive the other two."
                      : "El motor A resuena, el B golpea, y el C suma veinticuatro modelos que pueden hacer vibrar a los otros dos.",
                   0, "FM Struck String", { R (16, 70, 1208, 330) }, {} });
    v.push_back ({ en ? "MOD LANE" : "CARRIL MOD", en ? "A different engine C model on every step" : "Otro modelo del motor C en cada paso",
                   en ? "The new mod lane changes the model on every step. A whole drum kit from one sequencer."
                      : "El carril mod nuevo cambia el modelo en cada paso. Una bater\xc3\xad" "a entera desde un solo secuenciador.",
                   2, "Plaits Kit Walk", { R (16, 346, 1208, 450) }, seqOn, 0.12f, 0.8f, (int) cz::Lane::Model });
    v.push_back ({ en ? "LISTEN" : "ESCUCH\xc3\x81", en ? "Some CCRing sounds" : "Algunos sonidos de CCRing", en ? "Listen." : "Escuch\xc3\xa1.",
                   -1, "Model Hopper", {}, {}, 1.0f, 5.0f });
    v.push_back ({ en ? "PLAY LIVE" : "EN VIVO", en ? "Macros, morph, X/Y pad  -  cables shown, faded or hidden" : "Macros, morph, pad X/Y  -  cables a la vista, transparentes u ocultos",
                   en ? "Play live with macros, morph and the X Y pad. Record knob moves with rec."
                      : "Toc\xc3\xa1 en vivo con macros, morph y el pad equis y. Grab\xc3\xa1 movimientos con rec.",
                   3, "Fifths Machine", { R (16, 382, 700, 412) }, {} });
    v.push_back ({ en ? "FREE  -  MAC & WINDOWS" : "GRATIS  -  MAC Y WINDOWS", "github.com/javicazou/CCRing",
                   en ? "CCRing is free and open source, for Mac and Windows. If you like it, support it with a donation. Enjoy!"
                      : "CCRing es gratis y de c\xc3\xb3" "digo libre, para Mac y Windows. Si te gusta, apoyalo con una donaci\xc3\xb3n. \xc2\xa1" "Disfrutalo!",
                   -1, "Glass Machine", {}, {}, 0.4f, 1.2f });
    return v;
}

inline void drawSlide (juce::Image& canvas, const juce::Image& shot, const Slide& s, int index, int total)
{
    using namespace juce;
    Graphics g (canvas);
    const auto accent = Colour (0xffff6a2b);
    g.fillAll (Colour (0xff0b0c0e));
    // Captura del plugin centrada
    const float sc = jmin (1840.0f / (float) shot.getWidth(), 820.0f / (float) shot.getHeight());
    const auto area = Rectangle<float> ((1920.0f - shot.getWidth() * sc) * 0.5f, 120.0f, shot.getWidth() * sc, shot.getHeight() * sc);
    if (s.tab == -2) { g.setColour (Colour (0xff0b0c0e)); g.fillRect (area); drawEngineDiagram (g, Rectangle<float> (80.0f, 130.0f, 1760.0f, 820.0f)); }
    else g.drawImage (shot, area);
    if (! s.highlight.empty())
    {
        Path dim;
        dim.addRectangle (area);
        for (auto r : s.highlight)
            dim.addRoundedRectangle (Rectangle<float> (area.getX() + r.getX() * sc, area.getY() + r.getY() * sc, r.getWidth() * sc, r.getHeight() * sc).expanded (4.0f), 8.0f);
        dim.setUsingNonZeroWinding (false);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillPath (dim);
        g.setColour (accent);
        for (auto r : s.highlight)
            g.drawRoundedRectangle (Rectangle<float> (area.getX() + r.getX() * sc, area.getY() + r.getY() * sc, r.getWidth() * sc, r.getHeight() * sc).expanded (4.0f), 8.0f, 4.0f);
    }
    // Título
    g.setColour (accent);
    g.fillRect (60.0f, 42.0f, 10.0f, 46.0f);
    g.setColour (Colours::white);
    g.setFont (FontOptions (44.0f).withStyle ("Bold"));
    g.drawText (U (s.title), 90, 38, 1500, 56, Justification::centredLeft);
    g.setColour (Colours::white.withAlpha (0.5f));
    g.setFont (FontOptions (24.0f));
    g.drawText (String (index + 1) + " / " + String (total), 1660, 38, 200, 56, Justification::centredRight);
    // Subtítulo
    const auto bar = Rectangle<float> (60.0f, 960.0f, 1800.0f, 90.0f);
    g.setColour (Colours::black.withAlpha (0.85f));
    g.fillRoundedRectangle (bar, 12.0f);
    g.setColour (Colours::white);
    g.setFont (FontOptions (36.0f));
    g.drawFittedText (U (s.caption), bar.reduced (24.0f, 8.0f).toNearestInt(), Justification::centred, 2, 0.8f);
}

inline int runSlides (URingAudioProcessor& proc, juce::File dir, const std::vector<Slide>& list);

inline int run (URingAudioProcessor& proc, const juce::File& outDir, bool shortVersion = false, bool inEnglish = false, bool spanish14 = false,
                bool short15 = false)
{
    using namespace juce;
    english() = inEnglish;
    if (inEnglish) mm::lang::currentRef() = mm::lang::Lang::English;
    else mm::lang::currentRef() = mm::lang::Lang::Spanish;
    if (short15)
        return runSlides (proc, outDir.getChildFile (inEnglish ? "video15en" : "video15es"), slidesShort (inEnglish));
    auto dir = outDir.getChildFile (inEnglish ? "tutorial3en" : spanish14 ? "tutorial3es" : (shortVersion ? "tutorial3" : "tutorial"));
    return runSlides (proc, dir, inEnglish ? slides3en() : spanish14 ? slides3es14() : (shortVersion ? slides3() : slides()));
}

inline int runSlides (URingAudioProcessor& proc, juce::File dir, const std::vector<Slide>& list)
{
    using namespace juce;
    dir.deleteRecursively();
    dir.createDirectory();
    int presetIdx = 0;
    String index;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& s = list[(size_t) i];
        const auto& pl = presets::factory();
        for (int k = 0; k < (int) pl.size(); ++k) if (String (pl[(size_t) k].name) == s.preset) presetIdx = k;
        proc.loadPreset (presetIdx);
        if (s.setup) s.setup (proc);
        std::unique_ptr<AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        if (auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get()))
        {
            ed->showTab (jmax (0, s.tab));
            if (s.seqLane >= 0) ed->testShowSeqLane (s.seqLane);
        }
        {   // que suene un momento: osciloscopio, espectro, vúmetros y LCD muestran audio real
            static PlayHead ph; ph.ppq = 0.0; ph.bpm = 122.0;
            proc.setPlayHead (&ph);
            proc.setPlayConfigDetails (0, 2, 48000.0, 512);
            proc.prepareToPlay (48000.0, 512);
            AudioBuffer<float> b (2, 512);
            for (int n = 0; n < 200; ++n)
            {
                MidiBuffer m; proc.processBlock (b, m); ph.ppq += 512 * ph.bpm / (60.0 * 48000.0);
                if (n % 4 == 3) MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
            proc.setPlayHead (nullptr);
        }
        MessageManager::getInstance()->runDispatchLoopUntil (250);
        auto shot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        Image canvas (Image::RGB, 1920, 1080, true);
        drawSlide (canvas, shot, s, i, (int) list.size());
        const auto name = String::formatted ("slide%02d", i);
        { auto f = dir.getChildFile (name + ".png"); FileOutputStream os (f); PNGImageFormat().writeImageToStream (canvas, os); }
        dir.getChildFile (name + ".txt").replaceWithText (U (s.narration), false, false, "\n");
        dir.getChildFile (name + ".title").replaceWithText (U (s.title), false, false, "\n");
        index << name << "|" << String (s.music, 2) << "|" << String (s.extraSeconds, 2) << "\n";
        if (s.tab == 2) proc.setEditSequencer (0);
    }
    dir.getChildFile ("slides.txt").replaceWithText (index, false, false, "\n");
    std::cout << "Tutorial: " << list.size() << " slides in " << dir.getFullPathName() << std::endl;
    return 0;
}
} // namespace tutorial
