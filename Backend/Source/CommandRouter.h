#pragma once
#include <juce_osc/juce_osc.h>
#include "StudioConfig.h"

// Routes a command intent to the appropriate device in the loaded studio profile.
// For now: OscSynth destinations receive the raw command string as /genisys/command.
class CommandRouter
{
public:
    void loadStudio (const StudioConfig& cfg) { studio = cfg; }

    void routeCommand (const juce::String& destination, const juce::String& intent)
    {
        for (auto& device : studio.devices)
        {
            if (device.name != destination)
                continue;

            if (device.type == DeviceConfig::Type::OscSynth)
            {
                juce::OSCSender sender;
                auto host = device.address.isEmpty() ? "127.0.0.1" : device.address;
                if (sender.connect (host, device.port))
                {
                    sender.send ("/genisys/command",
                                 juce::String (destination),
                                 juce::String (intent));
                }
            }
            // MidiController and DawHost routing to be added as the wizard grows
        }
    }

private:
    StudioConfig studio;
};
