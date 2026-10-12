// Ui.h — estética y componentes de la interfaz.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "DubEchoProcessor.h"

namespace dubecho
{

namespace Colours
{
    const juce::Colour bg        { 0xff15120f };
    const juce::Colour panel     { 0xff201b16 };
    const juce::Colour panelHi   { 0xff2b241d };
    const juce::Colour line      { 0xff3a3128 };
    const juce::Colour text      { 0xffefe6d8 };
    const juce::Colour textDim   { 0xff9c8f7c };
    const juce::Colour amber     { 0xffe8a33d };
    const juce::Colour amberDim  { 0xff7a5622 };
    const juce::Colour teal      { 0xff4fb3a5 };
    const juce::Colour red       { 0xffd9534f };
    const juce::Colour ice       { 0xff8fc3e8 };
}

class DubLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DubLookAndFeel()
    {
        using namespace juce;
        // el esquema primero: setColourScheme() pisa todos los colores individuales
        auto scheme = getCurrentColourScheme();
        scheme.setUIColour (ColourScheme::windowBackground, Colours::bg);
        scheme.setUIColour (ColourScheme::widgetBackground, Colours::panelHi);
        scheme.setUIColour (ColourScheme::menuBackground, Colours::panel);
        scheme.setUIColour (ColourScheme::outline, Colours::line);
        scheme.setUIColour (ColourScheme::defaultText, Colours::text);
        scheme.setUIColour (ColourScheme::defaultFill, Colours::amberDim);
        scheme.setUIColour (ColourScheme::highlightedText, Colours::bg);
        scheme.setUIColour (ColourScheme::highlightedFill, Colours::amber);
        scheme.setUIColour (ColourScheme::menuText, Colours::text);
        setColourScheme (scheme);
        setColour (ResizableWindow::backgroundColourId, Colours::bg);
        setColour (Label::textColourId, Colours::text);
        setColour (TextButton::buttonColourId, Colours::panelHi);
        setColour (TextButton::buttonOnColourId, Colours::amber);
        setColour (TextButton::textColourOffId, Colours::text);
        setColour (TextButton::textColourOnId, Colours::bg);
        setColour (Slider::textBoxTextColourId, Colours::text);
        setColour (Slider::textBoxOutlineColourId, Colours::bg.withAlpha (0.0f));
        setColour (Slider::textBoxBackgroundColourId, Colours::bg.withAlpha (0.0f));
        setColour (Slider::textBoxHighlightColourId, Colours::amberDim);
        setColour (Label::outlineColourId, Colours::bg.withAlpha (0.0f));
        setColour (Label::outlineWhenEditingColourId, Colours::amber);
        setColour (Slider::rotarySliderFillColourId, Colours::amber);
        setColour (Slider::rotarySliderOutlineColourId, Colours::line);
        setColour (ComboBox::backgroundColourId, Colours::panelHi);
        setColour (ComboBox::outlineColourId, Colours::line);
        setColour (ComboBox::textColourId, Colours::text);
        setColour (PopupMenu::backgroundColourId, Colours::panel);
        setColour (PopupMenu::textColourId, Colours::text);
        setColour (PopupMenu::highlightedBackgroundColourId, Colours::amberDim);
        setColour (ListBox::backgroundColourId, Colours::panel);
        setColour (ListBox::textColourId, Colours::text);
        setColour (ToggleButton::textColourId, Colours::text);
        setColour (ToggleButton::tickColourId, Colours::amber);
        setColour (ToggleButton::tickDisabledColourId, Colours::textDim);
        setColour (ScrollBar::thumbColourId, Colours::line);
        setColour (DocumentWindow::textColourId, Colours::text);

    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        using namespace juce;
        const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float stroke = jmax (3.0f, r * 0.12f);

        Path track; track.addCentredArc (c.x, c.y, r - stroke * 0.5f, r - stroke * 0.5f, 0, startAngle, endAngle, true);
        g.setColour (Colours::line);
        g.strokePath (track, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));

        Path value; value.addCentredArc (c.x, c.y, r - stroke * 0.5f, r - stroke * 0.5f, 0, startAngle, angle, true);
        g.setColour (Colours::amber);
        g.strokePath (value, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));

        const float knobR = r - stroke * 1.9f;
        g.setColour (Colours::panelHi);
        g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2);
        g.setColour (Colours::line);
        g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2, 1.0f);

        const Point<float> tip = c.getPointOnCircumference (knobR * 0.8f, angle);
        g.setColour (Colours::text);
        g.drawLine ({ c.getPointOnCircumference (knobR * 0.25f, angle), tip }, 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour,
                               bool highlighted, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto col = b.getToggleState() ? b.findColour (TextButton::buttonOnColourId) : bgColour;
        if (down) col = col.brighter (0.15f);
        else if (highlighted) col = col.brighter (0.06f);
        g.setColour (col);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (b.getToggleState() ? col.brighter (0.3f) : Colours::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (15.0f, h * 0.38f), juce::Font::bold));
    }
};

// ---------------------------------------------------------------------------
// Perilla con nombre arriba y valor abajo.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramID, const juce::String& name)
        : attachment (state, paramID, slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 90, 18);
        slider.setWantsKeyboardFocus (false);
        slider.setMouseDragSensitivity (160);
        slider.setDoubleClickReturnValue (true, state.getParameter (paramID)->convertFrom0to1 (state.getParameter (paramID)->getDefaultValue()));
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, Colours::textDim);
        label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (18));
        slider.setBounds (r);
    }

private:
    juce::Slider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// ---------------------------------------------------------------------------
// Botón momentáneo: activo mientras se mantiene apretado (throw / freeze).
class HoldButton : public juce::TextButton
{
public:
    std::function<void (bool)> onHold;
    using juce::TextButton::TextButton;

    void mouseDown (const juce::MouseEvent& e) override { TextButton::mouseDown (e); if (onHold) onHold (true); }
    void mouseUp (const juce::MouseEvent& e) override   { TextButton::mouseUp (e);   if (onHold) onHold (false); }
};

// ---------------------------------------------------------------------------
// Medidor vertical de pico con caída suave.
class Meter : public juce::Component
{
public:
    explicit Meter (juce::String name) : title (std::move (name)) {}

    void push (float peak)
    {
        const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
        level = juce::jmax (db, level - 1.5f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        auto labelArea = r.removeFromBottom (16);
        g.setColour (Colours::textDim);
        g.setFont (FontOptions (11.0f, Font::bold));
        g.drawText (title, labelArea, Justification::centred);

        r = r.withSizeKeepingCentre (jmin (12.0f, r.getWidth()), r.getHeight());
        g.setColour (Colours::panelHi);
        g.fillRoundedRectangle (r, 3.0f);
        const float norm = jlimit (0.0f, 1.0f, (level + 60.0f) / 60.0f);
        auto fill = r.withTop (r.getBottom() - r.getHeight() * norm);
        g.setColour (level > -3.0f ? Colours::red : level > -12.0f ? Colours::amber : Colours::teal);
        g.fillRoundedRectangle (fill, 3.0f);
    }

private:
    juce::String title;
    float level = -60.0f;
};

// ---------------------------------------------------------------------------
// Pad XY: X = tiempo, Y = feedback. Marca las divisiones del tempo actual.
class XYPad : public juce::Component
{
public:
    explicit XYPad (DubEchoProcessor& p) : proc (p)
    {
        timeParam = proc.apvts.getParameter (ParamID::time);
        fbParam   = proc.apvts.getParameter (ParamID::feedback);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 10.0f);

        auto area = padArea();

        // zona de autooscilación (feedback > 100%)
        const float y100 = yForFeedback (100.0f);
        g.setColour (Colours::red.withAlpha (0.10f));
        g.fillRect (area.withBottom (y100));
        g.setColour (Colours::red.withAlpha (0.5f));
        g.drawHorizontalLine ((int) y100, area.getX(), area.getRight());
        g.setFont (FontOptions (11.0f));
        g.drawText (u8 ("autooscilación"), area.withHeight (y100 - area.getY()).reduced (6, 0), Justification::centredRight);

        // grilla de feedback
        g.setColour (Colours::line);
        for (float fb : { 25.0f, 50.0f, 75.0f })
            g.drawHorizontalLine ((int) yForFeedback (fb), area.getX(), area.getRight());

        // marcas de las divisiones del tempo
        const float bpm = proc.getBpm();
        g.setFont (FontOptions (11.0f, Font::bold));
        {
            // ordenadas por posición; las etiquetas muy juntas se escalonan en dos filas
            std::array<std::pair<float, int>, kNumDivisions> marks;
            for (int d = 1; d <= kNumDivisions; ++d)
                marks[(size_t) d - 1] = { xForTime (60000.0f / bpm * divisionInBeats (d)), d };
            std::sort (marks.begin(), marks.end());
            float lastX = -1000.0f; int row = 0;
            for (auto [x, d] : marks)
            {
                const float ms = 60000.0f / bpm * divisionInBeats (d);
                if (ms < TapeEcho::kMinTimeMs || ms > TapeEcho::kMaxTimeMs) continue;
                row = (x - lastX < 40.0f) ? 1 - row : 0;
                lastX = x;
                const bool active = proc.getDivision() == d && proc.timeMatchesDivision();
                g.setColour (active ? Colours::amber.withAlpha (0.6f) : Colours::line);
                g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
                g.setColour (active ? Colours::amber : Colours::textDim);
                g.drawText (u8 (divisionLabel (d)), Rectangle<float> (x - 24, area.getBottom() + 2 + row * 13.0f, 48, 13), Justification::centred);
            }
        }

        // posición actual de la "cinta" (lo que realmente suena mientras hace el glide)
        if (proc.getGlidingDelayMs() > 1.0f)
        {
            const float glideX = xForTime (proc.getGlidingDelayMs());
            g.setColour (Colours::teal.withAlpha (0.8f));
            g.drawVerticalLine ((int) glideX, area.getY(), area.getBottom());
        }

        // punto de control
        const float tx = xForTime (timeParam->convertFrom0to1 (timeParam->getValue()));
        const float fy = yForFeedback (fbParam->convertFrom0to1 (fbParam->getValue()));
        g.setColour (Colours::amber.withAlpha (0.25f));
        g.fillEllipse (tx - 18, fy - 18, 36, 36);
        g.setColour (Colours::amber);
        g.fillEllipse (tx - 8, fy - 8, 16, 16);

        // lectura
        auto readout = r.reduced (16, 12).removeFromTop (24);
        const float timeMs = timeParam->convertFrom0to1 (timeParam->getValue());
        String t = String (roundToInt (timeMs)) + " ms";
        if (proc.timeMatchesDivision())
            t << u8 ("  ·  ") << u8 (divisionLabel (proc.getDivision())) << " a " << String (roundToInt (proc.getBpm())) << " BPM";
        g.setColour (Colours::text);
        g.setFont (FontOptions (20.0f, Font::bold));
        g.drawText (t, readout, Justification::centredLeft);
        g.setColour (fbParam->convertFrom0to1 (fbParam->getValue()) > 100.0f ? Colours::red : Colours::amber);
        g.drawText ("feedback " + String (roundToInt (fbParam->convertFrom0to1 (fbParam->getValue()))) + "%", readout, Justification::centredRight);

        g.setFont (FontOptions (11.0f));
        g.setColour (Colours::textDim);
        g.drawText (u8 ("← tiempo →     ↑ feedback"), area.reduced (8, 6), Justification::bottomLeft);
        g.setColour (Colours::teal);
        g.drawText (u8 ("│ cinta (lo que suena)"), area.reduced (8, 6), Justification::bottomRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        timeParam->beginChangeGesture();
        fbParam->beginChangeGesture();
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto area = padArea();
        const float nx = juce::jlimit (0.0f, 1.0f, (e.position.x - area.getX()) / area.getWidth());
        const float ny = juce::jlimit (0.0f, 1.0f, (area.getBottom() - e.position.y) / area.getHeight());
        timeParam->setValueNotifyingHost (nx);
        fbParam->setValueNotifyingHost (ny);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        timeParam->endChangeGesture();
        fbParam->endChangeGesture();
    }

private:
    juce::Rectangle<float> padArea() const
    {
        return getLocalBounds().toFloat().reduced (16).withTrimmedTop (34).withTrimmedBottom (32);
    }
    float xForTime (float ms) const
    {
        auto a = padArea();
        return a.getX() + a.getWidth() * timeParam->convertTo0to1 (juce::jlimit (TapeEcho::kMinTimeMs, TapeEcho::kMaxTimeMs, ms));
    }
    float yForFeedback (float pct) const
    {
        auto a = padArea();
        return a.getBottom() - a.getHeight() * fbParam->convertTo0to1 (pct);
    }

    DubEchoProcessor& proc;
    juce::RangedAudioParameter* timeParam = nullptr;
    juce::RangedAudioParameter* fbParam = nullptr;
};

} // namespace dubecho
