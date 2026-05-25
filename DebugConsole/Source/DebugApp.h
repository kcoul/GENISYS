#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_osc/juce_osc.h>
#include "GenisysProtocol.h"
#include <algorithm>
#include <vector>

// ─── WER helper ──────────────────────────────────────────────────────────────
// Word Error Rate = edit_distance(ref_words, hyp_words) / len(ref_words)
static float calculateWer (const juce::String& reference, const juce::String& hypothesis)
{
    auto ref = juce::StringArray::fromTokens (reference.toLowerCase().trim(),  " ", "");
    auto hyp = juce::StringArray::fromTokens (hypothesis.toLowerCase().trim(), " ", "");
    const int n = ref.size(), m = hyp.size();
    if (n == 0) return 0.0f;

    std::vector<std::vector<int>> dp (n + 1, std::vector<int> (m + 1, 0));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i)
        for (int j = 1; j <= m; ++j)
            dp[i][j] = (ref[i - 1] == hyp[j - 1])
                ? dp[i - 1][j - 1]
                : 1 + std::min ({ dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1] });

    return static_cast<float> (dp[n][m]) / static_cast<float> (n);
}

// ─── Main component ───────────────────────────────────────────────────────────
class DebugComponent  : public juce::Component,
                        private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
                        private juce::Timer
{
public:
    DebugComponent()
    {
        // ── Log ──────────────────────────────────────────────────────────────
        logBox.setMultiLine (true);
        logBox.setReadOnly (true);
        logBox.setScrollbarsShown (true);
        logBox.setCaretVisible (false);
        logBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1A1A2E));
        logBox.setColour (juce::TextEditor::textColourId,       juce::Colours::lightgrey);
        logBox.setColour (juce::TextEditor::outlineColourId,    juce::Colour (0xFF444466));
        logBox.setFont (juce::Font (juce::FontOptions ("Courier New", 13.0f, juce::Font::plain)));
        addAndMakeVisible (logBox);

        // ── Stats ─────────────────────────────────────────────────────────────
        statsLabel.setJustificationType (juce::Justification::centredLeft);
        statsLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (statsLabel);

        signalLabel.setJustificationType (juce::Justification::centredRight);
        signalLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFAAAAAA));
        addAndMakeVisible (signalLabel);

        clearBtn.setButtonText ("Clear");
        clearBtn.onClick = [this] { logBox.clear(); resetStats(); };
        addAndMakeVisible (clearBtn);

        // ── WER panel ────────────────────────────────────────────────────────
        werRefLabel.setText ("Reference phrase:", juce::dontSendNotification);
        addAndMakeVisible (werRefLabel);

        addAndMakeVisible (werRefInput);
        werRefInput.setTextToShowWhenEmpty ("type what you intended to say...",
                                            juce::Colours::grey);

        werCalcBtn.setButtonText ("Calc WER");
        werCalcBtn.onClick = [this]
        {
            const float wer = calculateWer (werRefInput.getText(), lastTranscript);
            werResultLabel.setText ("WER: " + juce::String (wer, 3)
                                    + "   last: \"" + lastTranscript + "\"",
                                    juce::dontSendNotification);
        };
        addAndMakeVisible (werCalcBtn);

        werResultLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCCCCCC));
        addAndMakeVisible (werResultLabel);

        // ── OSC ──────────────────────────────────────────────────────────────
        if (receiver.connect (GenisysProtocol::diagPort))
        {
            receiver.addListener (this);
            setSignalStatus (false);
        }
        else
        {
            signalLabel.setText ("ERROR: port " + juce::String (GenisysProtocol::diagPort)
                                 + " unavailable", juce::dontSendNotification);
        }

        startTimer (3000);  // signal-lost watchdog
        setSize (780, 560);
    }

    ~DebugComponent() override { receiver.removeListener (this); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);

        // Top row: title/signal + clear button
        auto topRow = area.removeFromTop (28);
        clearBtn.setBounds (topRow.removeFromRight (70));
        signalLabel.setBounds (topRow.removeFromRight (220));
        topRow.removeFromLeft (4);  // gap

        // Stats bar
        statsLabel.setBounds (area.removeFromTop (22));

        area.removeFromTop (4);

        // WER panel at bottom
        auto werRow1 = area.removeFromBottom (22);
        werResultLabel.setBounds (werRow1);
        auto werRow0 = area.removeFromBottom (26).reduced (0, 2);
        werCalcBtn.setBounds (werRow0.removeFromRight (90));
        werRefInput.setBounds (werRow0.removeFromRight (300));
        werRow0.removeFromRight (4);
        werRefLabel.setBounds (werRow0);

        area.removeFromBottom (6);

        // Log fills the rest
        logBox.setBounds (area);
    }

    void oscMessageReceived (const juce::OSCMessage& msg) override
    {
        lastSignalTime = juce::Time::getCurrentTime();
        setSignalStatus (true);

        const auto addr = msg.getAddressPattern().toString();

        if (addr == GenisysProtocol::diagTranscript
            && msg.size() == 2 && msg[0].isString() && msg[1].isInt32())
        {
            const auto text      = msg[0].getString();
            const auto latencyMs = msg[1].getInt32();
            lastTranscript = text;
            ++totalUtterances;
            totalLatencyMs += latencyMs;
            appendLog ("[" + timestamp() + "] " + juce::String (latencyMs).paddedLeft (' ', 4)
                       + "ms  \"" + text + "\"", juce::Colours::lightgrey);
        }
        else if (addr == GenisysProtocol::diagMatch
                 && msg.size() == 2 && msg[0].isString() && msg[1].isString())
        {
            const auto transcript = msg[0].getString();
            const auto commandId  = msg[1].getString();

            if (commandId.isNotEmpty())
            {
                ++matchedUtterances;
                appendLog ("         → MATCHED: " + commandId, juce::Colour (0xFF44CC77));
            }
            else
            {
                appendLog ("         → no command match", juce::Colour (0xFFCC4444));
            }
            updateStatsLabel();
        }
        else if (addr == GenisysProtocol::diagStats
                 && msg.size() == 3)
        {
            // Stats come from the backend too; use our local counts for display
            // (they should agree, but local counts are always up-to-date)
        }
    }

    void timerCallback() override
    {
        const auto elapsed = juce::Time::getCurrentTime() - lastSignalTime;
        if (elapsed.inSeconds() > 3.0)
            setSignalStatus (false);
    }

private:
    void appendLog (const juce::String& line, juce::Colour colour)
    {
        logBox.setColour (juce::TextEditor::textColourId, colour);
        logBox.moveCaretToEnd();
        logBox.insertTextAtCaret (line + "\n");
        logBox.setColour (juce::TextEditor::textColourId, juce::Colours::lightgrey);
    }

    void updateStatsLabel()
    {
        const float matchRate = totalUtterances > 0
            ? 100.0f * static_cast<float> (matchedUtterances) / static_cast<float> (totalUtterances)
            : 0.0f;
        const float avgMs = totalUtterances > 0
            ? static_cast<float> (totalLatencyMs) / static_cast<float> (totalUtterances)
            : 0.0f;

        statsLabel.setText (
            "utterances: " + juce::String (totalUtterances)
            + "   matched: " + juce::String (matchedUtterances)
            + "   unmatched: " + juce::String (totalUtterances - matchedUtterances)
            + "   match rate: " + juce::String (matchRate, 1) + "%"
            + "   avg latency: " + juce::String (avgMs, 0) + "ms",
            juce::dontSendNotification);
    }

    void resetStats()
    {
        totalUtterances = matchedUtterances = totalLatencyMs = 0;
        updateStatsLabel();
        werResultLabel.setText ({}, juce::dontSendNotification);
    }

    void setSignalStatus (bool live)
    {
        signalLabel.setText (live ? "● backend signal" : "○ waiting for backend",
                             juce::dontSendNotification);
        signalLabel.setColour (juce::Label::textColourId,
                               live ? juce::Colour (0xFF44CC77) : juce::Colour (0xFFAAAAAA));
    }

    static juce::String timestamp()
    {
        return juce::Time::getCurrentTime().formatted ("%H:%M:%S");
    }

    juce::TextEditor logBox;
    juce::Label      statsLabel, signalLabel, werRefLabel, werResultLabel;
    juce::TextEditor werRefInput;
    juce::TextButton clearBtn, werCalcBtn;
    juce::OSCReceiver receiver;

    juce::Time lastSignalTime;
    juce::String lastTranscript;
    int totalUtterances = 0, matchedUtterances = 0, totalLatencyMs = 0;
};

// ─── Window / App ─────────────────────────────────────────────────────────────
class DebugApp  : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Genisys Debug Console"; }
    const juce::String getApplicationVersion() override { return "0.1"; }

    void initialise (const juce::String&) override
    {
        window = std::make_unique<AppWindow> (getApplicationName());
    }

    void shutdown() override { window.reset(); }

private:
    class AppWindow : public juce::DocumentWindow
    {
    public:
        explicit AppWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xFF12121F), allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new DebugComponent(), true);
            setResizable (true, true);
            centreWithSize (780, 560);
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<AppWindow> window;
};
