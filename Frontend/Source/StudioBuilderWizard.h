#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_osc/juce_osc.h>
#include "GenisysProtocol.h"
#include "StudioConfig.h"

// A minimal wizard for defining a studio and sending its profile to the Backend.
// Each "page" is a Component swapped in/out of the wizard's content area.
class StudioBuilderWizard  : public juce::Component
{
public:
    // Called when the user finishes the wizard and confirms the studio.
    std::function<void (const StudioConfig&)> onStudioReady;

    StudioBuilderWizard()
    {
        addAndMakeVisible (pageContainer);
        addAndMakeVisible (backButton);
        addAndMakeVisible (nextButton);

        backButton.setButtonText ("Back");
        nextButton.setButtonText ("Next");

        backButton.onClick = [this] { goBack(); };
        nextButton.onClick = [this] { goNext(); };

        showPage (0);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (16);
        auto buttonRow = area.removeFromBottom (40);
        nextButton.setBounds (buttonRow.removeFromRight (100).reduced (4));
        backButton.setBounds (buttonRow.removeFromRight (100).reduced (4));
        pageContainer.setBounds (area);
        if (currentPage != nullptr)
            currentPage->setBounds (pageContainer.getLocalBounds());
    }

    // --- Page 0: Studio Name ---
    struct NamePage  : public juce::Component
    {
        juce::Label     label   { {}, "Studio Name:" };
        juce::TextEditor editor;

        NamePage()
        {
            addAndMakeVisible (label);
            addAndMakeVisible (editor);
            editor.setText ("My Studio");
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            label.setBounds  (area.removeFromTop (24));
            editor.setBounds (area.removeFromTop (32));
        }

        juce::String getName() const { return editor.getText().trim(); }
    };

    // --- Page 1: Add Devices ---
    struct DevicesPage  : public juce::Component
    {
        juce::Label         title { {}, "Add devices to your studio:" };
        juce::TextButton    addMidi  { "Add MIDI Controller" };
        juce::TextButton    addSynth { "Add OSC Synth (SurgeXT)" };
        juce::TextButton    addDaw   { "Add DAW Host (REAPER)" };
        juce::ListBox       deviceList;
        juce::Array<DeviceConfig> devices;

        struct Model  : public juce::ListBoxModel
        {
            juce::Array<DeviceConfig>* data;
            int getNumRows() override { return data->size(); }
            void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
            {
                if (selected) g.fillAll (juce::Colours::lightblue);
                if (row < data->size())
                    g.drawText ((*data)[row].name + "  [" + DeviceConfig::typeToString ((*data)[row].type) + "]",
                                4, 0, w - 8, h, juce::Justification::centredLeft);
            }
        } model;

        DevicesPage()
        {
            model.data = &devices;
            deviceList.setModel (&model);

            addAndMakeVisible (title);
            addAndMakeVisible (addMidi);
            addAndMakeVisible (addSynth);
            addAndMakeVisible (addDaw);
            addAndMakeVisible (deviceList);

            addMidi.onClick = [this]
            {
                DeviceConfig d;
                d.name     = "MIDI Controller " + juce::String (countOfType (DeviceConfig::Type::MidiController) + 1);
                d.type     = DeviceConfig::Type::MidiController;
                d.midiName = "";
                devices.add (d);
                deviceList.updateContent();
            };

            addSynth.onClick = [this]
            {
                DeviceConfig d;
                d.name    = "SurgeXT";
                d.type    = DeviceConfig::Type::OscSynth;
                d.address = "127.0.0.1";
                d.port    = 53280;
                devices.add (d);
                deviceList.updateContent();
            };

            addDaw.onClick = [this]
            {
                DeviceConfig d;
                d.name    = "REAPER";
                d.type    = DeviceConfig::Type::DawHost;
                d.address = "127.0.0.1";
                d.port    = 0;  // DAW integration TBD
                devices.add (d);
                deviceList.updateContent();
            };
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            title.setBounds (area.removeFromTop (24));
            area.removeFromTop (4);
            addMidi.setBounds  (area.removeFromTop (32).removeFromLeft (200));
            addSynth.setBounds (area.removeFromTop (32).removeFromLeft (200));
            addDaw.setBounds   (area.removeFromTop (32).removeFromLeft (200));
            area.removeFromTop (8);
            deviceList.setBounds (area);
        }

        int countOfType (DeviceConfig::Type t) const
        {
            int n = 0;
            for (auto& d : devices) if (d.type == t) ++n;
            return n;
        }
    };

    // --- Page 2: Confirm ---
    struct ConfirmPage  : public juce::Component
    {
        juce::TextEditor summary;
        juce::Label      title { {}, "Review your studio:" };

        ConfirmPage()
        {
            summary.setMultiLine (true);
            summary.setReadOnly (true);
            addAndMakeVisible (title);
            addAndMakeVisible (summary);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            title.setBounds   (area.removeFromTop (24));
            summary.setBounds (area);
        }

        void populate (const StudioConfig& cfg)
        {
            juce::String s = "Studio: " + cfg.studioName + "\n\nDevices:\n";
            for (auto& d : cfg.devices)
                s += "  " + d.name + " [" + DeviceConfig::typeToString (d.type) + "]\n";
            summary.setText (s);
        }
    };

private:
    juce::Component   pageContainer;
    juce::TextButton  backButton, nextButton;
    juce::Component*  currentPage = nullptr;
    int               pageIndex   = 0;

    NamePage    namePage;
    DevicesPage devicesPage;
    ConfirmPage confirmPage;

    juce::Component* pageForIndex (int idx)
    {
        switch (idx)
        {
            case 0: return &namePage;
            case 1: return &devicesPage;
            case 2: return &confirmPage;
            default: return nullptr;
        }
    }

    void showPage (int idx)
    {
        if (currentPage != nullptr)
            pageContainer.removeChildComponent (currentPage);

        pageIndex   = idx;
        currentPage = pageForIndex (idx);

        if (currentPage != nullptr)
        {
            pageContainer.addAndMakeVisible (currentPage);
            currentPage->setBounds (pageContainer.getLocalBounds());
        }

        backButton.setEnabled (idx > 0);
        nextButton.setButtonText (idx == 2 ? "Finish" : "Next");
    }

    void goBack()  { showPage (juce::jmax (0, pageIndex - 1)); }

    void goNext()
    {
        if (pageIndex < 2)
        {
            if (pageIndex == 1)
            {
                StudioConfig cfg;
                cfg.studioName = namePage.getName();
                cfg.devices    = devicesPage.devices;
                confirmPage.populate (cfg);
            }
            showPage (pageIndex + 1);
        }
        else
        {
            StudioConfig cfg;
            cfg.studioName = namePage.getName();
            cfg.devices    = devicesPage.devices;
            if (onStudioReady) onStudioReady (cfg);
        }
    }
};
