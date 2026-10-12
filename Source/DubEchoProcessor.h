// DubEchoProcessor.h — el eco como AudioProcessor de JUCE.
// Hoy lo usa la app standalone; el día de mañana sirve tal cual para compilarlo como plugin AU.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TapeEchoDSP.h"
#include "SirenDSP.h"
#include "MidiMap.h"

namespace dubecho
{

namespace ParamID
{
    static constexpr const char* time     = "time";
    static constexpr const char* feedback = "feedback";
    static constexpr const char* inertia  = "inertia";
    static constexpr const char* wow      = "wow";
    static constexpr const char* drive    = "drive";
    static constexpr const char* lowCut   = "lowcut";
    static constexpr const char* highCut  = "highcut";
    static constexpr const char* input    = "input";
    static constexpr const char* output   = "output";
    static constexpr const char* send     = "send";
    static constexpr const char* siren    = "siren";
}

// Divisiones de tempo (en negras). Índice 0 = libre.
static constexpr int kNumDivisions = 5;
inline float divisionInBeats (int idx)
{
    static const float d[kNumDivisions + 1] = { 0.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f / 3.0f };
    return d[juce::jlimit (0, kNumDivisions, idx)];
}
inline const char* divisionLabel (int idx)
{
    static const char* l[kNumDivisions + 1] = { "libre", "1/8", "1/8·", "1/4", "1/4·", "1/4T" };
    return l[juce::jlimit (0, kNumDivisions, idx)];
}

class DubEchoProcessor : public juce::AudioProcessor
{
public:
    DubEchoProcessor();

    // --- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Dub Echo"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- control desde la interfaz (hilo de mensajes) o desde MIDI (hilo de audio)
    enum Source { FromKeyboard = 0, FromMouse, FromMidi, NumSources };
    void setThrow (Source s, bool down)  { throwDown[s].store (down); }
    void setFreeze (Source s, bool down) { freezeDown[s].store (down); }
    bool isThrowing() const;
    bool isFrozen() const;
    void kill()                          { killRequests.fetch_add (1); }
    void tap();
    void toggleSend();
    void selectDivision (int idx);          // 1..5
    void nudge (const char* paramID, float deltaNormalised);

    // --- sirena (0..3). Mientras se mantiene, el pad XY toca la sirena en vez del eco.
    void setSiren (Source s, int preset, bool down);
    void setSirenXY (float x01, float y01) { sirenX.store (x01); sirenY.store (y01); }
    bool isSirenHeld() const;
    int  getSirenHeldMask() const;

    int   getDivision() const              { return division.load(); }
    float getBpm() const                   { return bpm.load(); }
    bool  hasTappedBpm() const             { return bpmFromTap.load(); }
    bool  timeMatchesDivision() const;
    float getGlidingDelayMs() const        { return glidingDelayMs.load(); }

    // medidores (pico, se resetean al leer)
    float takeInputPeak()  { return inPeak.exchange (0.0f); }
    float takeOutputPeak() { return outPeak.exchange (0.0f); }

    // --- MIDI learn
    void startLearning (Target t)          { learnTarget.store ((int) t); }
    void cancelLearning()                  { learnTarget.store (-1); }
    int  getLearningTarget() const         { return learnTarget.load(); }
    int  getMapping (Target t) const       { return mappings[(size_t) t].load(); }
    void clearMapping (Target t)           { mappings[(size_t) t].store (packSource (None, 0)); }
    void resetMappings();
    juce::String getLastMidiDescription() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage&);
    void applyDivision();
    void setParamNormalised (const char* id, float v);

    TapeEcho echo;
    Siren siren;
    double currentSampleRate = 48000.0;
    juce::HeapBlock<float> monoScratch, sirenScratch;
    int scratchSize = 0;

    std::atomic<int> sirenMask[NumSources] {};
    std::atomic<int> sirenPressCount { 0 }, sirenLastPreset { 0 };
    int sirenPressHandled = 0;
    std::atomic<float> sirenX { -1.0f }, sirenY { -1.0f };

    std::atomic<bool> throwDown[NumSources] {}, freezeDown[NumSources] {};
    std::atomic<int>  killRequests { 0 };
    int killHandled = 0;

    juce::SpinLock tapLock;
    double lastTapMs = 0.0;
    juce::Array<double> tapIntervals;
    std::atomic<float> bpm { 75.0f };
    std::atomic<bool>  bpmFromTap { false };
    std::atomic<int>   division { 2 };      // 1/8 con puntillo: el clásico de dub
    std::atomic<float> glidingDelayMs { 0.0f };

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };

    std::array<std::atomic<int>, kNumTargets> mappings;
    std::array<float, kNumTargets> lastContinuous {};
    std::atomic<int> learnTarget { -1 };

    // Aprender un eje: durante ~0,6 s se escuchan todas las fuentes y gana la que más se movió.
    // (El pad XY manda X e Y a la vez; así no se asigna por error el eje equivocado.)
    struct LearnCandidate { int src; float minV, maxV; };
    std::array<LearnCandidate, 8> learnCands {};
    int numLearnCands = 0, learnCandTarget = -1;
    double learnStartMs = 0.0;
    void finishContinuousLearn (bool force);

    std::atomic<bool> timeShorterHeld { false }, timeLongerHeld { false };
    std::atomic<int> lastMidiType { 0 }, lastMidiNumber { 0 }, lastMidiValue { 0 }, lastMidiChannel { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubEchoProcessor)
};

} // namespace dubecho
