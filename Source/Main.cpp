#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

class DubEchoApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Dub Echo"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override { window = std::make_unique<MainWindow>(); }
    void shutdown() override                       { window = nullptr; }
    void systemRequestedQuit() override            { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : DocumentWindow ("Dub Echo", dubecho::Colours::bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new dubecho::MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (820, 640, 1600, 1150);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

        void activeWindowStatusChanged() override
        {
            if (isActiveWindow())
                if (auto* c = getContentComponent())
                    c->grabKeyboardFocus();
        }
    };

    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (DubEchoApplication)
