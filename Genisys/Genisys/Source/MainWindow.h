#pragma once

#include "MainComponent.h"

namespace GuiApp
{
class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow(const juce::String& name);

private:
    void closeButtonPressed() override;
    static juce::Colour getBackgroundColour();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
}

