#pragma once
#include <juce_core/juce_core.h>

// A Device is any addressable piece of equipment in the studio.
struct DeviceConfig
{
    enum class Type { MidiController, OscSynth, DawHost, Unknown };

    juce::String name;
    Type         type      = Type::Unknown;
    juce::String address;   // IP address or empty for local
    int          port      = 0;
    juce::String midiName;  // for MidiController: JUCE device name

    juce::var toVar() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("name",     name);
        obj->setProperty("type",     typeToString (type));
        obj->setProperty("address",  address);
        obj->setProperty("port",     port);
        obj->setProperty("midiName", midiName);
        return juce::var (obj.release());
    }

    static DeviceConfig fromVar (const juce::var& v)
    {
        DeviceConfig d;
        d.name     = v["name"].toString();
        d.type     = stringToType (v["type"].toString());
        d.address  = v["address"].toString();
        d.port     = (int) v["port"];
        d.midiName = v["midiName"].toString();
        return d;
    }

    static juce::String typeToString (Type t)
    {
        switch (t)
        {
            case Type::MidiController: return "MidiController";
            case Type::OscSynth:       return "OscSynth";
            case Type::DawHost:        return "DawHost";
            default:                   return "Unknown";
        }
    }

    static Type stringToType (const juce::String& s)
    {
        if (s == "MidiController") return Type::MidiController;
        if (s == "OscSynth")       return Type::OscSynth;
        if (s == "DawHost")        return Type::DawHost;
        return Type::Unknown;
    }
};

// A Studio is a named collection of devices.
struct StudioConfig
{
    juce::String              studioName;
    juce::Array<DeviceConfig> devices;

    juce::String toJson() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("studioName", studioName);

        juce::Array<juce::var> devArray;
        for (auto& d : devices)
            devArray.add (d.toVar());
        obj->setProperty ("devices", devArray);

        return juce::JSON::toString (juce::var (obj.release()));
    }

    static StudioConfig fromJson (const juce::String& json)
    {
        StudioConfig cfg;
        auto v = juce::JSON::parse (json);
        if (v.isObject())
        {
            cfg.studioName = v["studioName"].toString();
            if (auto* arr = v["devices"].getArray())
                for (auto& d : *arr)
                    cfg.devices.add (DeviceConfig::fromVar (d));
        }
        return cfg;
    }
};
