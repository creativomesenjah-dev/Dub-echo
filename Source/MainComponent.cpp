#include "MainComponent.h"
#include "MidiMapPanel.h"

namespace dubecho
{

static bool looksLikeUca (const juce::String& name)
{
    return name.containsIgnoreCase ("USB Audio CODEC") || name.containsIgnoreCase ("UCA");
}

MainComponent::MainComponent()
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    juce::PropertiesFile::Options opts;
    opts.applicationName     = "Dub Echo";
    opts.folderName          = "Dub Echo";
    opts.filenameSuffix      = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (opts);

    auto* settings = appProperties.getUserSettings();
    juce::MemoryBlock state;
    if (state.fromBase64Encoding (settings->getValue ("processorState")) && state.getSize() > 0)
        processor.setStateInformation (state.getData(), (int) state.getSize());

    // ----- interfaz
    title.setText ("DUB ECHO", juce::dontSendNotification);
    title.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Colours::amber);
    addAndMakeVisible (title);

    subtitle.setText ("eco de cinta", juce::dontSendNotification);
    subtitle.setFont (juce::FontOptions (13.0f));
    subtitle.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (subtitle);

    status.setJustificationType (juce::Justification::centredRight);
    status.setFont (juce::FontOptions (12.5f));
    addAndMakeVisible (status);

    audioButton.onClick = [this] { showAudioSettings(); };
    mapButton.onClick   = [this] { showMidiMap(); };
    addAndMakeVisible (audioButton);
    addAndMakeVisible (mapButton);

    addAndMakeVisible (xy);

    auto addKnob = [this] (const char* id, const juce::String& name) { addAndMakeVisible (knobs.add (new Knob (processor.apvts, id, name))); };
    addKnob (ParamID::inertia, "INERCIA");
    addKnob (ParamID::wow,     "WOW / FLUTTER");
    addKnob (ParamID::drive,   u8 ("SATURACIÓN"));
    addKnob (ParamID::lowCut,  "CORTE GRAVES");
    addKnob (ParamID::highCut, "CORTE AGUDOS");
    addKnob (ParamID::input,   "ENTRADA");
    addKnob (ParamID::output,  "SALIDA");

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    throwButton.onHold  = [this] (bool down) { processor.setThrow (DubEchoProcessor::FromMouse, down); };
    freezeButton.onHold = [this] (bool down) { processor.setFreeze (DubEchoProcessor::FromMouse, down); };
    throwButton.setColour (juce::TextButton::buttonOnColourId, Colours::amber);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, Colours::ice);
    killButton.setColour (juce::TextButton::buttonColourId, Colours::red.withAlpha (0.35f));
    sendButton.setColour (juce::TextButton::buttonOnColourId, Colours::teal);
    sendButton.setClickingTogglesState (true);
    sendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamID::send, sendButton);
    killButton.onClick = [this] { processor.kill(); };
    tapButton.onClick  = [this] { processor.tap(); };

    for (juce::Button* b : { (juce::Button*) &throwButton, (juce::Button*) &freezeButton, (juce::Button*) &sendButton,
                             (juce::Button*) &killButton, (juce::Button*) &tapButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    for (int d = 1; d <= kNumDivisions; ++d)
    {
        auto* b = divButtons.add (new juce::TextButton (u8 (divisionLabel (d))));
        b->onClick = [this, d] { processor.selectDivision (d); };
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    // ----- sirena
    sirenLabel.setText ("SIRENA", juce::dontSendNotification);
    sirenLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    sirenLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (sirenLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto* b = sirenButtons.add (new HoldButton (u8 (sirenPreset (i).name)));
        b->onHold = [this, i] (bool down) { processor.setSiren (DubEchoProcessor::FromMouse, i, down); };
        b->setColour (juce::TextButton::buttonOnColourId, Colours::red);
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    sirenLevel.setSliderStyle (juce::Slider::LinearHorizontal);
    sirenLevel.setTextBoxStyle (juce::Slider::TextBoxRight, true, 64, 20);
    sirenLevel.setColour (juce::Slider::trackColourId, Colours::red.withAlpha (0.7f));
    sirenLevel.setColour (juce::Slider::backgroundColourId, Colours::panelHi);
    sirenLevel.setColour (juce::Slider::thumbColourId, Colours::text);
    sirenLevel.setWantsKeyboardFocus (false);
    sirenLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamID::siren, sirenLevel);
    addAndMakeVisible (sirenLevel);

    audioButton.setWantsKeyboardFocus (false);
    mapButton.setWantsKeyboardFocus (false);

    help.setText (u8 ("Teclado:  ESPACIO throw  ·  E envío  ·  F freeze  ·  K kill  ·  T tap  ·  1-5 divisiones  ·  flechas tiempo/feedback  ·  Z X C V sirenas"),
                  juce::dontSendNotification);
    help.setFont (juce::FontOptions (12.0f));
    help.setColour (juce::Label::textColourId, Colours::textDim);
    help.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (help);

    initialiseAudio();

    setWantsKeyboardFocus (true);
    setSize (940, 700);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    saveSettings();
    deviceManager.removeChangeListener (this);
    deviceManager.removeMidiInputDeviceCallback ({}, &player);
    deviceManager.removeAudioCallback (&guarded);
    player.setProcessor (nullptr);
    sendAttachment.reset();
    sirenLevelAttachment.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
void MainComponent::initialiseAudio()
{
    auto* settings = appProperties.getUserSettings();
    auto saved = settings->getXmlValue ("audioDeviceState");
    preferredInput  = settings->getValue ("preferredInput");
    preferredOutput = settings->getValue ("preferredOutput");

    deviceManager.initialise (2, 2, saved.get(), true);

    if (saved == nullptr)
    {
        // Primera vez: si está la Behringer UCA222 conectada, usarla directamente.
        if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        {
            type->scanForDevices();
            juce::String in, out;
            for (auto& n : type->getDeviceNames (true))  if (looksLikeUca (n)) in = n;
            for (auto& n : type->getDeviceNames (false)) if (looksLikeUca (n)) out = n;

            auto setup = deviceManager.getAudioDeviceSetup();
            if (out.isNotEmpty())
            {
                setup.outputDeviceName = out;
                setup.inputDeviceName  = in.isNotEmpty() ? in : setup.inputDeviceName;
                setup.useDefaultInputChannels = setup.useDefaultOutputChannels = true;
            }
            setup.bufferSize = 512; // la latencia no importa en un envío de eco; prioridad: cero cortes
            deviceManager.setAudioDeviceSetup (setup, true);
        }
    }

    if (preferredOutput.isEmpty())
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        setPreferredDevices (setup.inputDeviceName, setup.outputDeviceName);
    }
    guarded.setPreferred (preferredOutput);

    player.setProcessor (&processor);
    deviceManager.addAudioCallback (&guarded);
    deviceManager.addMidiInputDeviceCallback ({}, &player);
    deviceManager.addChangeListener (this);

    // si la placa preferida está pero se abrió otra, volver a la preferida
    watchDevices();
    updateStatus();
}

void MainComponent::setPreferredDevices (const juce::String& input, const juce::String& output)
{
    preferredInput = input;
    preferredOutput = output;
    guarded.setPreferred (output);
    auto* settings = appProperties.getUserSettings();
    settings->setValue ("preferredInput", input);
    settings->setValue ("preferredOutput", output);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Sólo lo que se elige a mano en "Audio / MIDI" pasa a ser la placa preferida.
    if (settingsWindow != nullptr)
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        if (setup.outputDeviceName.isNotEmpty())
            setPreferredDevices (setup.inputDeviceName, setup.outputDeviceName);
    }
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        guarded.evaluate (dev->getName());
    updateStatus();
}

void MainComponent::watchDevices()
{
    // ---- audio: reconectar la placa preferida si volvió (por ejemplo, después de un tirón del USB)
    auto* dev = deviceManager.getCurrentAudioDevice();
    const auto setup = deviceManager.getAudioDeviceSetup();
    const bool onPreferred = dev != nullptr && preferredOutput.isNotEmpty() && setup.outputDeviceName == preferredOutput;

    if (! onPreferred && preferredOutput.isNotEmpty())
    {
        if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        {
            type->scanForDevices();
            if (type->getDeviceNames (false).contains (preferredOutput))
            {
                auto s = setup;
                s.outputDeviceName = preferredOutput;
                if (type->getDeviceNames (true).contains (preferredInput))
                    s.inputDeviceName = preferredInput;
                s.useDefaultInputChannels = s.useDefaultOutputChannels = true;
                deviceManager.setAudioDeviceSetup (s, true);
            }
        }
    }

    // ---- MIDI: habilitar el nanoPAD2 automáticamente cuando aparece (también al reconectarlo)
    const auto available = juce::MidiInput::getAvailableDevices();
    std::map<juce::String, bool> stillHere;
    for (auto& d : available)
    {
        const bool isPad = d.name.containsIgnoreCase ("nanoPAD");
        if (midiSeen.find (d.identifier) == midiSeen.end() && isPad
            && ! deviceManager.isMidiInputDeviceEnabled (d.identifier))
            deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
        stillHere[d.identifier] = true;
    }
    midiSeen = stillHere;
}

void MainComponent::updateStatus()
{
    juce::String s;
    auto* dev = deviceManager.getCurrentAudioDevice();
    const bool ok = dev != nullptr && guarded.isAllowed();

    if (ok)
        s << "Placa: " << dev->getName() << u8 ("  ·  ") << juce::roundToInt (dev->getCurrentSampleRate() / 1000.0 * 10) / 10.0
          << u8 (" kHz  ·  buffer ") << dev->getCurrentBufferSizeSamples();
    else
        s << "Placa desconectada: " << (preferredOutput.isNotEmpty() ? preferredOutput : u8 ("elegí una en Audio / MIDI"))
          << "  (salida en silencio)";

    juce::StringArray midiNames;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled (d.identifier))
            midiNames.add (d.name);
    s << "\nMIDI: " << (midiNames.isEmpty() ? juce::String ("ninguno activo") : midiNames.joinIntoString (", "));

    status.setText (s, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, ok ? Colours::textDim : Colours::red);
}

void MainComponent::saveSettings()
{
    auto* settings = appProperties.getUserSettings();
    if (auto xml = deviceManager.createStateXml())
        settings->setValue ("audioDeviceState", xml.get());

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    settings->setValue ("processorState", state.toBase64Encoding());
    settings->saveIfNeeded();
}

// ---------------------------------------------------------------------------
void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager, 1, 2, 1, 2, true, false, true, false);
    selector->setSize (520, 460);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Audio / MIDI";
    o.dialogBackgroundColour = Colours::bg;
    o.componentToCentreAround = this;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.escapeKeyTriggersCloseButton = true;
    settingsWindow = o.launchAsync();
}

void MainComponent::showMidiMap()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new MidiMapPanel (processor));
    o.dialogTitle = "Mapeo MIDI";
    o.dialogBackgroundColour = Colours::bg;
    o.componentToCentreAround = this;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.escapeKeyTriggersCloseButton = true;
    o.launchAsync();
}

// ---------------------------------------------------------------------------
void MainComponent::timerCallback()
{
    ++ticks;
    inMeter.push (processor.takeInputPeak());
    outMeter.push (processor.takeOutputPeak());
    xy.repaint();

    throwButton.setToggleState (processor.isThrowing(), juce::dontSendNotification);
    freezeButton.setToggleState (processor.isFrozen(), juce::dontSendNotification);
    tapButton.setButtonText ("TAP  " + juce::String (juce::roundToInt (processor.getBpm())) + (processor.hasTappedBpm() ? "" : "?"));
    const bool synced = processor.timeMatchesDivision();
    for (int i = 0; i < divButtons.size(); ++i)
        divButtons[i]->setToggleState (synced && processor.getDivision() == i + 1, juce::dontSendNotification);
    const int sirenMask = processor.getSirenHeldMask();
    for (int i = 0; i < sirenButtons.size(); ++i)
        sirenButtons[i]->setToggleState ((sirenMask & (1 << i)) != 0, juce::dontSendNotification);

    // devolverle el teclado a la ventana principal (después de cerrar diálogos, clicks, etc.)
    if (auto* peer = getPeer())
        if (peer->isFocused() && ! hasKeyboardFocus (true))
            grabKeyboardFocus();

    if (ticks % 60 == 0) { watchDevices(); updateStatus(); }  // cada 2 s
    if (ticks % 600 == 0) saveSettings();                      // cada 20 s
}

// ---------------------------------------------------------------------------
bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    const int code = key.getKeyCode();

    if (code == juce::KeyPress::spaceKey || c == 'f'
        || c == 'z' || c == 'x' || c == 'c' || c == 'v') return true;   // se manejan como "mantener"
    if (c == 'e') { processor.toggleSend(); return true; }
    if (c == 'k') { processor.kill(); return true; }
    if (c == 't') { processor.tap(); return true; }
    if (c >= '1' && c <= '5') { processor.selectDivision ((int) (c - '0')); return true; }

    const float fine = key.getModifiers().isShiftDown() ? 0.25f : 1.0f;
    if (code == juce::KeyPress::leftKey)  { processor.nudge (ParamID::time, -0.01f * fine); return true; }
    if (code == juce::KeyPress::rightKey) { processor.nudge (ParamID::time,  0.01f * fine); return true; }
    if (code == juce::KeyPress::downKey)  { processor.nudge (ParamID::feedback, -0.02f * fine); return true; }
    if (code == juce::KeyPress::upKey)    { processor.nudge (ParamID::feedback,  0.02f * fine); return true; }
    return false;
}

bool MainComponent::keyStateChanged (bool)
{
    const bool t = juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey);
    const bool f = juce::KeyPress::isKeyCurrentlyDown ('f') || juce::KeyPress::isKeyCurrentlyDown ('F');
    if (t != keyThrow)  { keyThrow = t;  processor.setThrow (DubEchoProcessor::FromKeyboard, t); }
    if (f != keyFreeze) { keyFreeze = f; processor.setFreeze (DubEchoProcessor::FromKeyboard, f); }

    static const char sirenKeys[4] = { 'z', 'x', 'c', 'v' };
    bool anySiren = false;
    for (int i = 0; i < 4; ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (sirenKeys[i])
                       || juce::KeyPress::isKeyCurrentlyDown (juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) sirenKeys[i]));
        if (down != keySiren[i]) { keySiren[i] = down; processor.setSiren (DubEchoProcessor::FromKeyboard, i, down); }
        anySiren = anySiren || down;
    }
    return t || f || anySiren;
}

void MainComponent::focusLost (FocusChangeType) { releaseHeldKeys(); }

void MainComponent::releaseHeldKeys()
{
    keyThrow = keyFreeze = false;
    for (int i = 0; i < 4; ++i)
        if (keySiren[i]) { keySiren[i] = false; processor.setSiren (DubEchoProcessor::FromKeyboard, i, false); }
    processor.setThrow (DubEchoProcessor::FromKeyboard, false);
    processor.setFreeze (DubEchoProcessor::FromKeyboard, false);
}

// ---------------------------------------------------------------------------
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (56);
    auto body = r.removeFromTop (r.getHeight() - 130 - 58);
    auto right = body.removeFromRight (juce::jmax (360, body.getWidth() * 46 / 100));
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (right.toFloat().withTrimmedLeft (12), 10.0f);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto header = r.removeFromTop (56);
    auto buttons = header.removeFromRight (250).withSizeKeepingCentre (250, 34);
    mapButton.setBounds (buttons.removeFromRight (120));
    buttons.removeFromRight (10);
    audioButton.setBounds (buttons);
    auto titleArea = header.removeFromLeft (220);
    title.setBounds (titleArea.removeFromTop (34));
    subtitle.setBounds (titleArea);
    status.setBounds (header.reduced (10, 4));

    help.setBounds (r.removeFromBottom (22));
    auto perf = r.removeFromBottom (100).withTrimmedTop (12);

    auto sirenRow = r.removeFromBottom (58).withTrimmedTop (12);
    sirenLabel.setBounds (sirenRow.removeFromLeft (70));
    sirenLevel.setBounds (sirenRow.removeFromRight (240).reduced (0, 6));
    sirenRow.removeFromRight (12);
    const int sw = sirenRow.getWidth() / sirenButtons.size();
    for (auto* b : sirenButtons)
        b->setBounds (sirenRow.removeFromLeft (sw).reduced (4, 0));

    auto body = r;
    auto right = body.removeFromRight (juce::jmax (360, body.getWidth() * 46 / 100)).withTrimmedLeft (12).reduced (10);
    xy.setBounds (body);

    // 4 x 2: siete perillas y los medidores en la última celda
    const int cols = 4, rowsN = 2;
    const int kw = right.getWidth() / cols, kh = right.getHeight() / rowsN;
    for (int i = 0; i < knobs.size(); ++i)
    {
        const int col = i % cols, row = i / cols;
        knobs[i]->setBounds (juce::Rectangle<int> (right.getX() + col * kw, right.getY() + row * kh, kw, kh)
                                 .withSizeKeepingCentre (kw, juce::jmin (kh, kw + 40)));
    }
    auto meters = juce::Rectangle<int> (right.getX() + 3 * kw, right.getY() + kh, kw, kh).reduced (kw / 5, 10);
    inMeter.setBounds (meters.removeFromLeft (meters.getWidth() / 2));
    outMeter.setBounds (meters);

    // fila de performance
    auto big = perf.removeFromLeft (perf.getWidth() * 62 / 100);
    const int bw = big.getWidth() / 5;
    for (juce::Button* b : { (juce::Button*) &throwButton, (juce::Button*) &sendButton, (juce::Button*) &freezeButton,
                             (juce::Button*) &killButton, (juce::Button*) &tapButton })
        b->setBounds (big.removeFromLeft (bw).reduced (4));

    perf.removeFromLeft (12);
    auto divRow = perf.withSizeKeepingCentre (perf.getWidth(), juce::jmin (perf.getHeight(), 56));
    const int dw = divRow.getWidth() / divButtons.size();
    for (auto* b : divButtons)
        b->setBounds (divRow.removeFromLeft (dw).reduced (3));
}

} // namespace dubecho
