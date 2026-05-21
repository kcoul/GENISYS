#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_osc/juce_osc.h>
#include "GenisysProtocol.h"
#include "StudioConfig.h"
#include "StudioBuilderWizard.h"

class MainWindow  : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name)
        : DocumentWindow (name,
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent(), true);
        setResizable (true, true);
        centreWithSize (600, 500);
        setVisible (true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    struct MainComponent  : public juce::Component,
                            private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
    {
        StudioBuilderWizard wizard;
        juce::Label         statusLabel;
        juce::OSCSender     sender;
        juce::OSCReceiver   receiver;

        // The IP/port of the backend — configurable for remote deployment
        juce::String  backendHost = "127.0.0.1";
        int           backendPort = GenisysProtocol::backendPort;

        MainComponent()
        {
            addAndMakeVisible (wizard);
            addAndMakeVisible (statusLabel);

            statusLabel.setJustificationType (juce::Justification::centred);
            setStatus ("Not connected to backend");

            if (receiver.connect (GenisysProtocol::frontendPort))
                receiver.addListener (this);

            wizard.onStudioReady = [this] (const StudioConfig& cfg)
            {
                if (sender.connect (backendHost, backendPort))
                {
                    sender.send (GenisysProtocol::studioLoad, cfg.toJson());
                    setStatus ("Sent studio \"" + cfg.studioName + "\" to backend");
                }
                else
                {
                    setStatus ("ERROR: could not reach backend at "
                               + backendHost + ":" + juce::String (backendPort));
                }
            };

            setSize (600, 500);
        }

        ~MainComponent() override { receiver.removeListener (this); }

        void resized() override
        {
            auto area = getLocalBounds();
            statusLabel.setBounds (area.removeFromBottom (28).reduced (4));
            wizard.setBounds (area);
        }

        void oscMessageReceived (const juce::OSCMessage& msg) override
        {
            auto addr = msg.getAddressPattern().toString();
            if (addr == GenisysProtocol::ack && msg.size() == 1 && msg[0].isString())
                setStatus ("Backend acknowledged: " + msg[0].getString());
            else if (addr == GenisysProtocol::error && msg.size() == 1 && msg[0].isString())
                setStatus ("Backend error: " + msg[0].getString());
        }

        void setStatus (const juce::String& text)
        {
            statusLabel.setText (text, juce::dontSendNotification);
        }
    };
};

class FrontendApp  : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "GenisysFrontend"; }
    const juce::String getApplicationVersion() override { return "0.1"; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { mainWindow.reset(); }

private:
    std::unique_ptr<MainWindow> mainWindow;
};
