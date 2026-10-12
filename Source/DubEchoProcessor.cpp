#include "DubEchoProcessor.h"

namespace dubecho
{

static const char* continuousIDs[kFirstAction] = {
    ParamID::time, ParamID::feedback, ParamID::inertia, ParamID::wow, ParamID::drive,
    ParamID::lowCut, ParamID::highCut, ParamID::input, ParamID::output
};

DubEchoProcessor::DubEchoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Entrada", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Salida",  juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DubEcho", createLayout())
{
    resetMappings();
}

juce::AudioProcessorValueTreeState::ParameterLayout DubEchoProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto range = [] (float lo, float hi, float centre) {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    auto ms  = AudioParameterFloatAttributes().withLabel ("ms")
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; });
    auto pct = AudioParameterFloatAttributes().withLabel ("%")
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto hz  = AudioParameterFloatAttributes().withLabel ("Hz")
                   .withStringFromValueFunction ([] (float v, int) {
                       return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto db  = AudioParameterFloatAttributes().withLabel ("dB")
                   .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
    auto sec = AudioParameterFloatAttributes().withLabel ("s")
                   .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " s"; });

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::time, 1 },     "Tiempo",     range (20.0f, 2000.0f, 450.0f), 600.0f, ms));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::feedback, 1 }, "Feedback",   NormalisableRange<float> (0.0f, 110.0f), 55.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::inertia, 1 },  "Inercia",    range (0.02f, 2.0f, 0.35f), 0.35f, sec));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::wow, 1 },      "Wow",        NormalisableRange<float> (0.0f, 100.0f), 25.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::drive, 1 },    u8 ("Saturación"), NormalisableRange<float> (0.0f, 100.0f), 35.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::lowCut, 1 },   "Graves",     range (20.0f, 1000.0f, 150.0f), 180.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::highCut, 1 },  "Agudos",     range (800.0f, 12000.0f, 3000.0f), 3200.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::input, 1 },    "Entrada",    NormalisableRange<float> (-24.0f, 12.0f), 0.0f, db));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 },   "Salida",     NormalisableRange<float> (-24.0f, 12.0f), 6.0f, db));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ParamID::send, 1 },     u8 ("Envío"),  true));

    return { p.begin(), p.end() };
}

bool DubEchoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void DubEchoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    echo.prepare (sampleRate);
}

// ---------------------------------------------------------------------------
void DubEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (const auto meta : midi)
        handleMidi (meta.getMessage());
    midi.clear();

    const int pending = killRequests.load();
    if (pending != killHandled) { killHandled = pending; echo.triggerKill(); }

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    TapeEchoParams p;
    p.timeMs     = get (ParamID::time);
    p.feedback   = get (ParamID::feedback) * 0.01f;
    p.inertiaSec = get (ParamID::inertia);
    p.wowFlutter = get (ParamID::wow) * 0.01f;
    p.drive      = get (ParamID::drive) * 0.01f;
    p.lowCutHz   = get (ParamID::lowCut);
    p.highCutHz  = get (ParamID::highCut);
    p.inputGain  = juce::Decibels::decibelsToGain (get (ParamID::input));
    p.outputGain = juce::Decibels::decibelsToGain (get (ParamID::output));
    p.sendOpen   = get (ParamID::send) > 0.5f;
    p.throwHeld  = isThrowing();
    p.freeze     = isFrozen();
    echo.setParams (p);

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();
    if (n == 0 || numOut == 0) return;

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    if (numIn == 0)
    {
        // Sin entrada (placa sin canales de entrada): el eco sigue sonando con lo que tenga.
        buffer.clear();
        echo.process (outL, nullptr, outL, outR, n);
    }
    else
    {
        const float* inL = buffer.getReadPointer (0);
        const float* inR = numIn > 1 ? buffer.getReadPointer (1) : nullptr;

        float pk = juce::jmax (inPeak.load(), buffer.getMagnitude (0, 0, n));
        if (numIn > 1) pk = juce::jmax (pk, buffer.getMagnitude (1, 0, n));
        inPeak.store (pk);

        echo.process (inL, inR, outL, outR, n); // en el lugar: lee y escribe la misma muestra
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    outPeak.store (juce::jmax (outPeak.load(), buffer.getMagnitude (0, 0, n)));
    glidingDelayMs.store (echo.getCurrentDelayMs());
}

// ---------------------------------------------------------------------------
bool DubEchoProcessor::isThrowing() const
{
    for (auto& t : throwDown) if (t.load()) return true;
    return false;
}

bool DubEchoProcessor::isFrozen() const
{
    for (auto& f : freezeDown) if (f.load()) return true;
    return false;
}

void DubEchoProcessor::setParamNormalised (const char* id, float v)
{
    if (auto* param = apvts.getParameter (id))
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
}

void DubEchoProcessor::nudge (const char* id, float delta)
{
    if (auto* param = apvts.getParameter (id))
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, param->getValue() + delta));
}

void DubEchoProcessor::toggleSend()
{
    if (auto* param = apvts.getParameter (ParamID::send))
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
}

void DubEchoProcessor::tap()
{
    bool changed = false;
    {
        const juce::SpinLock::ScopedLockType sl (tapLock);
        const double now = juce::Time::getMillisecondCounterHiRes();
        const double interval = now - lastTapMs;
        lastTapMs = now;

        if (interval > 250.0 && interval < 1500.0)  // 40..240 BPM
        {
            tapIntervals.add (interval);
            if (tapIntervals.size() > 4) tapIntervals.remove (0);
            double avg = 0;
            for (auto i : tapIntervals) avg += i;
            avg /= tapIntervals.size();
            bpm.store ((float) juce::jlimit (40.0, 240.0, 60000.0 / avg));
            bpmFromTap.store (true);
            changed = true;
        }
        else
        {
            tapIntervals.clearQuick();
        }
    }

    if (changed)
    {
        if (division.load() == 0) division.store (2);
        applyDivision();
    }
}

void DubEchoProcessor::selectDivision (int idx)
{
    division.store (juce::jlimit (1, kNumDivisions, idx));
    applyDivision();
}

void DubEchoProcessor::applyDivision()
{
    const int d = division.load();
    if (d <= 0) return;
    const float ms = juce::jlimit (TapeEcho::kMinTimeMs, TapeEcho::kMaxTimeMs, 60000.0f / bpm.load() * divisionInBeats (d));
    if (auto* param = apvts.getParameter (ParamID::time))
        param->setValueNotifyingHost (param->convertTo0to1 (ms));
}

bool DubEchoProcessor::timeMatchesDivision() const
{
    const int d = division.load();
    if (d <= 0) return false;
    const float expected = juce::jlimit (TapeEcho::kMinTimeMs, TapeEcho::kMaxTimeMs, 60000.0f / bpm.load() * divisionInBeats (d));
    return std::abs (apvts.getRawParameterValue (ParamID::time)->load() - expected) < 1.5f;
}

// ---------------------------------------------------------------------------
void DubEchoProcessor::resetMappings()
{
    const auto defaults = defaultMappings();
    for (int i = 0; i < kNumTargets; ++i)
        mappings[(size_t) i].store (defaults[(size_t) i]);
    lastContinuous.fill (-1.0f);
}

juce::String DubEchoProcessor::getLastMidiDescription() const
{
    const int type = lastMidiType.load();
    if (type == None) return u8 ("Sin MIDI todavía");
    const juce::String ch = u8 (" · canal ") + juce::String (lastMidiChannel.load());
    switch (type)
    {
        case Note:      return describeSource (packSource (Note, lastMidiNumber.load())) + " vel " + juce::String (lastMidiValue.load()) + ch;
        case CC:        return "CC " + juce::String (lastMidiNumber.load()) + " = " + juce::String (lastMidiValue.load()) + ch;
        case PitchBend: return "Pitch bend = " + juce::String (lastMidiValue.load()) + ch;
        default:        return {};
    }
}

void DubEchoProcessor::handleMidi (const juce::MidiMessage& m)
{
    int type = None, number = 0, raw = 0;
    float value = 0.0f;
    bool pressed = false, isRest = false;

    if (m.isNoteOn())
    {
        type = Note; number = m.getNoteNumber(); raw = m.getVelocity(); value = 1.0f; pressed = true;
    }
    else if (m.isNoteOff())
    {
        type = Note; number = m.getNoteNumber(); raw = 0; value = 0.0f; pressed = false;
    }
    else if (m.isController())
    {
        type = CC; number = m.getControllerNumber(); raw = m.getControllerValue();
        value = raw / 127.0f; pressed = raw >= 64; isRest = raw == 0;
    }
    else if (m.isPitchWheel())
    {
        type = PitchBend; raw = m.getPitchWheelValue();
        value = raw / 16383.0f; isRest = raw == 8192;
    }
    else
    {
        return;
    }

    lastMidiType.store (type); lastMidiNumber.store (number); lastMidiValue.store (raw); lastMidiChannel.store (m.getChannel());

    const int src = packSource (type, number);

    // --- aprendiendo: el primer mensaje válido queda asignado
    const int learning = learnTarget.load();
    if (learning >= 0)
    {
        const bool continuous = isContinuous ((Target) learning);
        const bool valid = continuous ? (type == CC || type == PitchBend)
                                      : ((type == Note && pressed) || type == CC);
        if (! valid) return;

        for (int i = 0; i < kNumTargets; ++i)      // una fuente controla un solo destino
            if (mappings[(size_t) i].load() == src) mappings[(size_t) i].store (packSource (None, 0));

        mappings[(size_t) learning].store (src);
        lastContinuous[(size_t) learning] = value;
        learnTarget.store (-1);
        return;
    }

    for (int i = 0; i < kNumTargets; ++i)
    {
        if (mappings[(size_t) i].load() != src) continue;
        const auto t = (Target) i;

        if (isContinuous (t))
        {
            // El pad XY del nanoPAD2 vuelve al reposo al soltar el dedo (centro / 0).
            // Un salto grande directo al valor de reposo se ignora, para que el tiempo
            // y el feedback queden donde los dejaste.
            if (isRest && lastContinuous[(size_t) i] >= 0.0f && std::abs (value - lastContinuous[(size_t) i]) > 0.3f)
                continue;
            lastContinuous[(size_t) i] = value;
            setParamNormalised (continuousIDs[i], value);
            continue;
        }

        if      (t == Target::Throw)  setThrow (FromMidi, pressed);
        else if (t == Target::Freeze) setFreeze (FromMidi, pressed);
        else if (! pressed)           continue;
        else if (t == Target::Send)   toggleSend();
        else if (t == Target::Kill)   kill();
        else if (t == Target::Tap)    tap();
        else                          selectDivision (i - (int) Target::Div1 + 1);   // Div1..Div5
    }
}

// ---------------------------------------------------------------------------
void DubEchoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree extra ("extra");
    extra.setProperty ("bpm", bpm.load(), nullptr);
    extra.setProperty ("bpmFromTap", bpmFromTap.load(), nullptr);
    extra.setProperty ("division", division.load(), nullptr);
    for (int i = 0; i < kNumTargets; ++i)
        extra.setProperty ("m" + juce::String (i), mappings[(size_t) i].load(), nullptr);

    state.removeChild (state.getChildWithName ("extra"), nullptr);
    state.appendChild (extra, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void DubEchoProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType())) return;

    if (auto extra = tree.getChildWithName ("extra"); extra.isValid())
    {
        bpm.store ((float) extra.getProperty ("bpm", 75.0f));
        bpmFromTap.store ((bool) extra.getProperty ("bpmFromTap", false));
        division.store ((int) extra.getProperty ("division", 2));
        for (int i = 0; i < kNumTargets; ++i)
            if (extra.hasProperty ("m" + juce::String (i)))
                mappings[(size_t) i].store ((int) extra.getProperty ("m" + juce::String (i)));
        tree.removeChild (extra, nullptr);
    }
    apvts.replaceState (tree);
}

} // namespace dubecho
