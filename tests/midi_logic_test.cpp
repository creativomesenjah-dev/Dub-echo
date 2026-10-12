// Prueba de la lógica MIDI del procesador: aprender un eje del pad XY y pads de tiempo.
#include "DubEchoProcessor.h"
#include <cstdio>

using namespace dubecho;

static int failures = 0;
static void check (bool ok, const char* what) { std::printf ("[%s] %s\n", ok ? " OK " : "FALLA", what); if (! ok) ++failures; }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    DubEchoProcessor p;
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);

    auto run = [&] (juce::MidiBuffer& m) { buf.clear(); p.processBlock (buf, m); };

    // 1) Aprender "Tiempo" moviendo el pad en diagonal: X (CC 12) se mueve mucho, Y (CC 1) poco.
    //    Y llega primero: antes se asignaba Y por error.
    p.startLearning (Target::Time);
    for (int k = 0; k < 20; ++k)
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::controllerEvent (1, 1, 60 + k / 4), 0);
        m.addEvent (juce::MidiMessage::controllerEvent (1, 12, 10 + k * 5), 1);
        run (m);
        juce::Thread::sleep (40);
    }
    juce::Thread::sleep (700);
    { juce::MidiBuffer m; run (m); }
    check (p.getLearningTarget() == -1, "el aprendizaje termina solo");
    check (p.getMapping (Target::Time) == packSource (CC, 12), "Tiempo queda en el eje que más se movió (CC 12), no en CC 1");
    check (p.getMapping (Target::Feedback) == packSource (CC, 1), "Feedback conserva CC 1");

    // 2) El eje aprendido controla el tiempo
    auto* time = p.apvts.getParameter (ParamID::time);
    { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::controllerEvent (1, 12, 127), 0); run (m); }
    check (time->getValue() > 0.99f, "CC 12 al máximo lleva el tiempo al máximo");

    // 3) Pad "acortar" (nota 46) mantenido 1 s baja el tiempo; al soltar se detiene
    const float before = time->getValue();
    { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 46, (juce::uint8) 100), 0); run (m); }
    for (int k = 0; k < 94; ++k) { juce::MidiBuffer m; run (m); }        // ~1 s de audio
    { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOff (1, 46), 0); run (m); }
    const float after = time->getValue();
    for (int k = 0; k < 20; ++k) { juce::MidiBuffer m; run (m); }
    std::printf ("     tiempo normalizado: %.3f -> %.3f (luego %.3f)\n", before, after, time->getValue());
    check (after < before - 0.25f && std::abs (time->getValue() - after) < 1e-6f, "acortar baja el tiempo mientras se mantiene y para al soltar");

    // 4) Pad "alargar" (nota 47)
    { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 47, (juce::uint8) 100), 0); run (m); }
    for (int k = 0; k < 47; ++k) { juce::MidiBuffer m; run (m); }
    { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOff (1, 47), 0); run (m); }
    check (time->getValue() > after + 0.1f, "alargar sube el tiempo");

    // 5) Aftertouch también se puede aprender
    p.startLearning (Target::Wow);
    for (int k = 0; k < 5; ++k) { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::channelPressureChange (1, k * 20), 0); run (m); }
    juce::Thread::sleep (700);
    { juce::MidiBuffer m; run (m); }
    check (p.getMapping (Target::Wow) == packSource (Pressure, 0), "aftertouch asignable");

    // 6) Sirena: pad 48 suena seca aunque el envío esté apagado; el XY toca la sirena y no el eco
    {
        if (auto* send = p.apvts.getParameter (ParamID::send)) send->setValueNotifyingHost (0.0f);
        { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 0); run (m); }
        auto* fb = p.apvts.getParameter (ParamID::feedback);
        const float fbBefore = fb->getValue();

        { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0); run (m); }
        check (p.isSirenHeld(), "pad 48 dispara la sirena");
        float pk = 0;
        for (int k = 0; k < 40; ++k)
        {
            juce::MidiBuffer m;
            if (k % 4 == 0) m.addEvent (juce::MidiMessage::controllerEvent (1, 1, 10 + k * 2), 0);
            run (m); pk = juce::jmax (pk, buf.getMagnitude (0, 0, 512));
        }
        check (pk > 0.1f, "la sirena se escucha en la salida (seca)");
        check (std::abs (fb->getValue() - fbBefore) < 1e-6f, "mientras suena la sirena, el eje Y no toca el feedback");

        { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOff (1, 48), 0); run (m); }
        for (int k = 0; k < 200; ++k) { juce::MidiBuffer m; run (m); }   // ~2 s
        check (! p.isSirenHeld() && buf.getMagnitude (0, 0, 512) < 1e-4f, "al soltar, la sirena se apaga y sin envío no deja eco");

        { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0); run (m); }
        check (fb->getValue() > 0.99f, "al soltar la sirena, el eje Y vuelve a manejar el feedback");
    }

    std::printf ("\n%s (%d fallas)\n", failures == 0 ? "TODO OK" : "HAY FALLAS", failures);
    return failures == 0 ? 0 : 1;
}
