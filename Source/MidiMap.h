// MidiMap.h — destinos controlables por MIDI y su asignación (MIDI learn).
#pragma once

#include <atomic>
#include <array>
#include <juce_core/juce_core.h>

namespace dubecho
{

// Los textos del código están en UTF-8 (acentos, "·"): JUCE necesita que se lo digamos.
inline juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

enum class Target : int
{
    // continuos (perillas / ejes del XY)
    Time = 0, Feedback, Inertia, Wow, Drive, LowCut, HighCut, Input, Output,
    // acciones (pads / teclas)
    Throw, Send, Freeze, Kill, Tap, Div1, Div2, Div3, Div4, Div5,
    TimeShorter, TimeLonger,   // mantener: el tiempo se acorta / alarga (con glide de pitch)
    Count
};

constexpr int kNumTargets = (int) Target::Count;
constexpr int kFirstAction = (int) Target::Throw;

inline bool isContinuous (Target t) { return (int) t < kFirstAction; }

inline const char* targetName (Target t)
{
    switch (t)
    {
        case Target::Time:     return "Tiempo (eje X)";
        case Target::Feedback: return "Feedback (eje Y)";
        case Target::Inertia:  return "Inercia de cinta";
        case Target::Wow:      return "Wow & flutter";
        case Target::Drive:    return "Saturación";
        case Target::LowCut:   return "Corte de graves";
        case Target::HighCut:  return "Corte de agudos";
        case Target::Input:    return "Nivel de entrada";
        case Target::Output:   return "Nivel de salida";
        case Target::Throw:    return "Throw (mantener)";
        case Target::Send:     return "Envío continuo on/off";
        case Target::Freeze:   return "Freeze (mantener)";
        case Target::Kill:     return "Kill (cortar repeticiones)";
        case Target::Tap:      return "Tap tempo";
        case Target::Div1:     return "División 1/8";
        case Target::Div2:     return "División 1/8 con puntillo";
        case Target::Div3:     return "División 1/4";
        case Target::Div4:     return "División 1/4 con puntillo";
        case Target::Div5:     return "División 1/4 tresillo";
        case Target::TimeShorter: return "Tiempo: acortar (mantener)";
        case Target::TimeLonger:  return "Tiempo: alargar (mantener)";
        case Target::Count:    break;
    }
    return "";
}

// Fuente MIDI empaquetada en un int: tipo << 8 | número.
enum SourceType : int { None = 0, Note = 1, CC = 2, PitchBend = 3, Pressure = 4, Other = 9 };

inline int packSource (int type, int number) { return (type << 8) | (number & 0x7f); }
inline int sourceType (int packed)   { return packed >> 8; }
inline int sourceNumber (int packed) { return packed & 0x7f; }

inline juce::String describeSource (int packed)
{
    switch (sourceType (packed))
    {
        case Note:      return "Nota " + juce::String (sourceNumber (packed))
                             + " (" + juce::MidiMessage::getMidiNoteName (sourceNumber (packed), true, true, 3) + ")";
        case CC:        return "CC " + juce::String (sourceNumber (packed));
        case PitchBend: return "Pitch bend";
        case Pressure:  return "Aftertouch";
        default:        return u8 ("—");
    }
}

// Asignación por defecto pensada para el KORG nanoPAD2 (escena 1 de fábrica):
// pad XY: X = pitch bend, Y = CC 1; pads: notas desde 36.
// Si tu nanoPAD2 está configurado distinto, se reasigna con "Mapeo MIDI".
inline std::array<int, kNumTargets> defaultMappings()
{
    std::array<int, kNumTargets> m {};
    m.fill (packSource (None, 0));
    m[(int) Target::Time]     = packSource (PitchBend, 0);
    m[(int) Target::Feedback] = packSource (CC, 1);
    m[(int) Target::Throw]    = packSource (Note, 36);
    m[(int) Target::Tap]      = packSource (Note, 37);
    m[(int) Target::Kill]     = packSource (Note, 38);
    m[(int) Target::Freeze]   = packSource (Note, 39);
    m[(int) Target::Send]     = packSource (Note, 40);
    m[(int) Target::Div1]     = packSource (Note, 41);
    m[(int) Target::Div2]     = packSource (Note, 42);
    m[(int) Target::Div3]     = packSource (Note, 43);
    m[(int) Target::Div4]     = packSource (Note, 44);
    m[(int) Target::Div5]     = packSource (Note, 45);
    m[(int) Target::TimeShorter] = packSource (Note, 46);
    m[(int) Target::TimeLonger]  = packSource (Note, 47);
    return m;
}

} // namespace dubecho
