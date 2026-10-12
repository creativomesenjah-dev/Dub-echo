// MainComponent.h — ventana principal: audio, MIDI, interfaz y atajos de teclado.
#pragma once

#include <map>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Ui.h"

namespace dubecho
{

// Pasa el audio al procesador, pero silencia la salida si macOS cambió a otra placa
// (por ejemplo, al desconectar la UCA222 JUCE podría caer en el micrófono y parlantes
// internos de la Mac: eso generaría acople). Sólo suena por la placa elegida.
class GuardedCallback : public juce::AudioIODeviceCallback
{
public:
    explicit GuardedCallback (juce::AudioIODeviceCallback& inner) : target (inner) {}

    void setPreferred (const juce::String& outputName) { const juce::SpinLock::ScopedLockType sl (lock); preferred = outputName; }
    bool isAllowed() const { return allowed.load(); }

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext& ctx) override
    {
        target.audioDeviceIOCallbackWithContext (in, numIn, out, numOut, numSamples, ctx);
        if (! allowed.load())
            for (int ch = 0; ch < numOut; ++ch)
                if (out[ch] != nullptr) juce::FloatVectorOperations::clear (out[ch], numSamples);
    }

    void evaluate (const juce::String& deviceName)
    {
        juce::String pref;
        { const juce::SpinLock::ScopedLockType sl (lock); pref = preferred; }
        allowed.store (pref.isEmpty() || deviceName.contains (pref) || pref.contains (deviceName));
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        evaluate (device->getName());
        target.audioDeviceAboutToStart (device);
    }

    void audioDeviceStopped() override                  { target.audioDeviceStopped(); }
    void audioDeviceError (const juce::String& e) override { target.audioDeviceError (e); }

private:
    juce::AudioIODeviceCallback& target;
    juce::SpinLock lock;
    juce::String preferred;
    std::atomic<bool> allowed { true };
};

class MainComponent : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType) override;

    void saveSettings();

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void setPreferredDevices (const juce::String& input, const juce::String& output);
    void initialiseAudio();
    void watchDevices();
    void updateStatus();
    void showAudioSettings();
    void showMidiMap();
    void releaseHeldKeys();

    DubLookAndFeel lnf;

    juce::ApplicationProperties appProperties;
    juce::AudioDeviceManager deviceManager;
    DubEchoProcessor processor;
    juce::AudioProcessorPlayer player;
    GuardedCallback guarded { player };
    juce::String preferredInput, preferredOutput;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;
    std::map<juce::String, bool> midiSeen;   // identificador -> estaba habilitado

    // interfaz
    juce::Label title, subtitle, status;
    juce::TextButton audioButton { "Audio / MIDI" }, mapButton { "Mapeo MIDI" };
    XYPad xy { processor };
    juce::OwnedArray<Knob> knobs;
    Meter inMeter { "IN" }, outMeter { "OUT" };
    HoldButton throwButton { "THROW" }, freezeButton { "FREEZE" };
    juce::TextButton sendButton { u8 ("ENVÍO") }, killButton { "KILL" }, tapButton { "TAP" };
    juce::OwnedArray<juce::TextButton> divButtons;
    juce::Label help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sendAttachment;

    bool keyThrow = false, keyFreeze = false;
    int ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace dubecho
