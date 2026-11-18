cmake_minimum_required(VERSION 3.22)

# Universal Binary (Apple Silicon + Intel). Se puede sobrescribir con -DCMAKE_OSX_ARCHITECTURES=x86_64
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Arquitecturas macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS mínimo")

project(CCRing VERSION 1.5.0 LANGUAGES C CXX)
add_compile_definitions(URING_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------- JUCE (FetchContent, sin Projucer)
# Para reutilizar una copia local: -DFETCHCONTENT_SOURCE_DIR_JUCE=/ruta/a/JUCE
include(FetchContent)
set(JUCE_GIT_TAG "8.0.4" CACHE STRING "Tag de JUCE a descargar")
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_GIT_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- Plugin
juce_add_plugin(CCRing
    COMPANY_NAME                "CCR"
    BUNDLE_ID                   com.ccr.ccring
    PLUGIN_MANUFACTURER_CODE    Jvto
    PLUGIN_CODE                 Urng
    PRODUCT_NAME                "CCRing"
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/ccr_app_icon_1024.png"   # logo de CCR (app y bundles)
    FORMATS                     VST3 AU Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE     # el secuenciador emite sus notas para grabarlas en el DAW
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE)

target_sources(CCRing PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_include_directories(CCRing PRIVATE Source)
target_compile_definitions(CCRing PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
# Mutable Instruments: Plaits (motor C), Clouds, Marbles + stmlib (licencia MIT, ver ThirdParty/mutable/LICENSE-MIT.txt)
file(GLOB_RECURSE PLAITS_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/mutable/plaits/dsp/*.cc)
add_library(mutable_plaits STATIC
    ${PLAITS_SOURCES}
    ThirdParty/mutable/plaits/resources.cc
    ThirdParty/mutable/stmlib/dsp/atan.cc
    ThirdParty/mutable/stmlib/dsp/units.cc
    ThirdParty/mutable/stmlib/utils/random.cc
    # Clouds (procesador granular de la página TEXTURE) y Marbles (generador aleatorio del secuenciador)
    ThirdParty/mutable/clouds/dsp/correlator.cc
    ThirdParty/mutable/clouds/dsp/granular_processor.cc
    ThirdParty/mutable/clouds/dsp/mu_law.cc
    ThirdParty/mutable/clouds/dsp/pvoc/frame_transformation.cc
    ThirdParty/mutable/clouds/dsp/pvoc/phase_vocoder.cc
    ThirdParty/mutable/clouds/dsp/pvoc/stft.cc
    ThirdParty/mutable/clouds/resources.cc
    ThirdParty/mutable/marbles/resources.cc)
target_include_directories(mutable_plaits PUBLIC ThirdParty/mutable)
target_compile_definitions(mutable_plaits PUBLIC TEST)   # modo "escritorio": sin hardware ni flash
if (MSVC)   # Windows: el código de Plaits usa extensiones de GCC/Clang
    target_compile_definitions(mutable_plaits PUBLIC _USE_MATH_DEFINES NOMINMAX)
    target_compile_options(mutable_plaits PRIVATE /W0)
else()
    target_compile_options(mutable_plaits PRIVATE -w)
endif()
set_target_properties(mutable_plaits PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Tipografía: Montserrat Bold (SIL Open Font License, ver Resources/Montserrat-OFL.txt)
juce_add_binary_data(URingFonts HEADER_NAME URingFonts.h NAMESPACE URingFonts SOURCES Resources/Montserrat-Bold.ttf Resources/ccr_logo.png)
set_target_properties(URingFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(CCRing
    PRIVATE URingFonts mutable_plaits
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- CLAP (Bitwig, Reaper…) con clap-juce-extensions (MIT)
# Para usar una copia local: -DFETCHCONTENT_SOURCE_DIR_CLAP-JUCE-EXTENSIONS=/ruta/clap-juce-extensions
option(CCRING_CLAP "Compilar también el formato CLAP" ON)
if (CCRING_CLAP)
    FetchContent_Declare(clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG        3837f4a6ed2ab0ba46aba69ae5b7041c7c64ce74   # main del 17/11/2026 (compatible con JUCE 8)
        GIT_SHALLOW    FALSE
        GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable(clap-juce-extensions)
    clap_juce_extensions_plugin(TARGET CCRing
        CLAP_ID "com.ccr.ccring"
        CLAP_FEATURES instrument synthesizer stereo
        CLAP_MANUAL_URL "https://github.com/javicazou/CCRing"
        CLAP_SUPPORT_URL "https://github.com/javicazou/CCRing/issues")
endif()

# ---------------------------------------------------------------- Demo (no se instala)
# Renderiza un WAV de prueba y capturas de la GUI: cmake --build build --target URingDemo
juce_add_console_app(URingDemo PRODUCT_NAME "URingDemo")
target_sources(URingDemo PRIVATE
    Tools/Demo.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_include_directories(URingDemo PRIVATE Source)
target_compile_definitions(URingDemo PRIVATE
    URING_VOICE_METER=1
    JucePlugin_Name="CCRing"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(URingDemo
    PRIVATE URingFonts mutable_plaits
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
