cmake_minimum_required(VERSION 3.22)

# macOS Catalina (10.15) es la última que soporta la MacBook Pro mid 2012: compilamos para 10.13+
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Versión mínima de macOS")

project(DubEcho VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.10
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(DubEcho
    PRODUCT_NAME "Dub Echo"
    COMPANY_NAME "PaloGol"
    BUNDLE_ID "com.palogol.dubecho"
    VERSION ${PROJECT_VERSION}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Dub Echo necesita la entrada de audio para recibir la música desde la placa USB.")

target_sources(DubEcho PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/DubEchoProcessor.cpp)

target_include_directories(DubEcho PRIVATE Source)

target_compile_definitions(DubEcho PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DubEcho,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:DubEcho,JUCE_VERSION>")

target_link_libraries(DubEcho
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_devices
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Prueba de la lógica MIDI (aprender ejes, pads de tiempo). Sólo para desarrollo:
#   cmake -B build -DDUBECHO_TESTS=ON && cmake --build build --target MidiLogicTest
option(DUBECHO_TESTS "Compilar pruebas de lógica MIDI" OFF)
if(DUBECHO_TESTS)
    juce_add_console_app(MidiLogicTest PRODUCT_NAME "MidiLogicTest")
    target_sources(MidiLogicTest PRIVATE tests/midi_logic_test.cpp Source/DubEchoProcessor.cpp)
    target_include_directories(MidiLogicTest PRIVATE Source)
    target_compile_definitions(MidiLogicTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MidiLogicTest PRIVATE juce::juce_audio_processors PUBLIC juce::juce_recommended_config_flags)
endif()
