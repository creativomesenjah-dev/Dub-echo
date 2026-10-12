// MidiMapPanel.h — ventana de "Mapeo MIDI": asignar pads y ejes del nanoPAD2 (o cualquier controlador).
#pragma once

#include "Ui.h"

namespace dubecho
{

class MidiMapPanel : public juce::Component, private juce::Timer
{
public:
    explicit MidiMapPanel (DubEchoProcessor& p) : proc (p)
    {
        intro.setText (u8 ("Tocá \"Aprender\" y después mové el control o apretá el pad. "
                           "Los ejes del pad XY se asignan a Tiempo y Feedback."),
                       juce::dontSendNotification);
        intro.setColour (juce::Label::textColourId, Colours::textDim);
        intro.setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (intro);

        for (int i = 0; i < kNumTargets; ++i)
        {
            auto* row = rows.add (new Row());
            row->name.setText (u8 (targetName ((Target) i)), juce::dontSendNotification);
            row->name.setFont (juce::FontOptions (14.0f));
            row->source.setColour (juce::Label::textColourId, Colours::amber);
            row->source.setFont (juce::FontOptions (14.0f, juce::Font::bold));

            row->learn.setButtonText ("Aprender");
            row->learn.setClickingTogglesState (false);
            row->learn.onClick = [this, i] {
                if (proc.getLearningTarget() == i) proc.cancelLearning();
                else proc.startLearning ((Target) i);
            };
            row->clear.setButtonText ("Borrar");
            row->clear.onClick = [this, i] { proc.clearMapping ((Target) i); };

            for (juce::Component* c : { (juce::Component*) &row->name, (juce::Component*) &row->source,
                                        (juce::Component*) &row->learn, (juce::Component*) &row->clear })
                list.addAndMakeVisible (c);
        }

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        monitor.setColour (juce::Label::textColourId, Colours::teal);
        monitor.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (monitor);

        resetButton.setButtonText ("Restaurar nanoPAD2");
        resetButton.onClick = [this] { proc.resetMappings(); };
        addAndMakeVisible (resetButton);

        setSize (560, 620);
        startTimerHz (15);
        timerCallback();
    }

    ~MidiMapPanel() override { proc.cancelLearning(); }

    void paint (juce::Graphics& g) override { g.fillAll (Colours::bg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        intro.setBounds (r.removeFromTop (40));
        auto bottom = r.removeFromBottom (36);
        resetButton.setBounds (bottom.removeFromRight (170));
        monitor.setBounds (bottom);
        r.removeFromBottom (8);
        viewport.setBounds (r);

        const int rowH = 32, w = r.getWidth() - viewport.getScrollBarThickness();
        list.setSize (w, rowH * rows.size());
        for (int i = 0; i < rows.size(); ++i)
        {
            auto rr = juce::Rectangle<int> (0, i * rowH, w, rowH).reduced (0, 3);
            auto* row = rows[i];
            row->clear.setBounds (rr.removeFromRight (64));
            rr.removeFromRight (6);
            row->learn.setBounds (rr.removeFromRight (104));
            rr.removeFromRight (8);
            row->name.setBounds (rr.removeFromLeft (rr.getWidth() * 55 / 100));
            row->source.setBounds (rr);
        }
    }

private:
    struct Row { juce::Label name, source; juce::TextButton learn, clear; };

    void timerCallback() override
    {
        const int learning = proc.getLearningTarget();
        for (int i = 0; i < rows.size(); ++i)
        {
            auto* row = rows[i];
            row->source.setText (describeSource (proc.getMapping ((Target) i)), juce::dontSendNotification);
            const bool isLearning = learning == i;
            row->learn.setToggleState (isLearning, juce::dontSendNotification);
            row->learn.setButtonText (isLearning ? "Esperando..." : "Aprender");
        }
        monitor.setText (u8 ("Último MIDI: ") + proc.getLastMidiDescription(), juce::dontSendNotification);
    }

    DubEchoProcessor& proc;
    juce::Label intro, monitor;
    juce::OwnedArray<Row> rows;
    juce::Component list;
    juce::Viewport viewport;
    juce::TextButton resetButton;
};

} // namespace dubecho
