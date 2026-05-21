#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_osc/juce_osc.h>
#include "GenisysProtocol.h"
#include "StudioConfig.h"
#include "CommandRouter.h"

class BackendApp  : public juce::JUCEApplication,
                    private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    const juce::String getApplicationName() override    { return "GenisysBackend"; }
    const juce::String getApplicationVersion() override { return "0.1"; }

    void initialise (const juce::String&) override
    {
        if (! receiver.connect (GenisysProtocol::backendPort))
        {
            juce::Logger::writeToLog ("Backend: failed to bind OSC port "
                                      + juce::String (GenisysProtocol::backendPort));
            quit();
            return;
        }
        receiver.addListener (this);
        juce::Logger::writeToLog ("GenisysBackend listening on port "
                                  + juce::String (GenisysProtocol::backendPort));
    }

    void shutdown() override { receiver.removeListener (this); }

    void oscMessageReceived (const juce::OSCMessage& msg) override
    {
        auto addr = msg.getAddressPattern().toString();

        if (addr == GenisysProtocol::studioLoad && msg.size() == 1 && msg[0].isString())
        {
            auto cfg = StudioConfig::fromJson (msg[0].getString());
            router.loadStudio (cfg);
            juce::Logger::writeToLog ("Backend: loaded studio \"" + cfg.studioName + "\"");
            sendToFrontend (GenisysProtocol::ack, juce::String (GenisysProtocol::studioLoad));
        }
        else if (addr == GenisysProtocol::commandSend
                 && msg.size() == 2
                 && msg[0].isString() && msg[1].isString())
        {
            router.routeCommand (msg[0].getString(), msg[1].getString());
        }
    }

private:
    void sendToFrontend (const char* address, const juce::String& payload)
    {
        juce::OSCSender sender;
        if (sender.connect ("127.0.0.1", GenisysProtocol::frontendPort))
            sender.send (address, payload);
    }

    juce::OSCReceiver receiver;
    CommandRouter     router;
};
