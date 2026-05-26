#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_osc/juce_osc.h>
#include "GenisysProtocol.h"
#include "StudioConfig.h"
#include "StudioBuilderWizard.h"

class MainWindow  : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, const juce::String& backendHost)
        : DocumentWindow (name,
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent (backendHost), true);
        setResizable (true, true);
        centreWithSize (600, 500);
        setVisible (true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    struct MainComponent  : public juce::Component,
                            private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
                            private juce::Timer
    {
        StudioBuilderWizard wizard;
        juce::Label         statusLabel;
        juce::OSCSender     sender;
        juce::OSCReceiver   receiver;

        juce::String  backendHost;
        int           backendPort = GenisysProtocol::backendPort;
        bool          connected   = false;

        explicit MainComponent (const juce::String& host) : backendHost (host)
        {
            addAndMakeVisible (wizard);
            addAndMakeVisible (statusLabel);

            statusLabel.setJustificationType (juce::Justification::centred);
            setStatus ("Connecting to backend at " + backendHost + "...");

            if (receiver.connect (GenisysProtocol::frontendPort))
                receiver.addListener (this);

            sender.connect (backendHost, backendPort);

            wizard.onStudioReady = [this] (const StudioConfig& cfg)
            {
                sender.send (GenisysProtocol::studioLoad, cfg.toJson());
                setStatus ("Sent studio \"" + cfg.studioName + "\" to backend");
            };

            startTimer (2000);
            timerCallback();   // ping immediately on startup
            setSize (600, 500);
        }

        ~MainComponent() override
        {
            stopTimer();
            receiver.removeListener (this);
        }

        void timerCallback() override
        {
            if (! connected)
                sender.send (GenisysProtocol::ping);
        }

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
            {
                const auto acked = msg[0].getString();
                if (! connected)
                {
                    connected = true;
                    stopTimer();
                    setStatus ("Connected to backend at " + backendHost);
                }
                if (acked != GenisysProtocol::ping)
                    setStatus ("Backend acknowledged: " + acked);
            }
            else if (addr == GenisysProtocol::error && msg.size() == 1 && msg[0].isString())
            {
                setStatus ("Backend error: " + msg[0].getString());
            }
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

    void initialise (const juce::String& commandLine) override
    {
        juce::String backendHost = "127.0.0.1";

        juce::ArgumentList args (getApplicationName(), commandLine);
        if (args.containsOption ("--backend"))
            backendHost = args.getValueForOption ("--backend");

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), backendHost);
    }

    void shutdown() override { mainWindow.reset(); }

private:
    std::unique_ptr<MainWindow> mainWindow;
};
