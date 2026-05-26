#pragma once
#include <juce_osc/juce_osc.h>
#include "GenisysProtocol.h"
#include "StudioConfig.h"
#include "CommandRouter.h"
#include "VoiceEngine.h"

class BackendApp  : public juce::JUCEApplicationBase,
                    private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    const juce::String getApplicationName() override    { return "GenisysBackend"; }
    const juce::String getApplicationVersion() override { return "0.1"; }

    void initialise (const juce::String& commandLine) override
    {
        juce::ArgumentList args (getApplicationName(), commandLine);
        if (args.containsOption ("--frontend"))
            frontendHost = args.getValueForOption ("--frontend");
        if (args.containsOption ("--diag"))
            diagHost = args.getValueForOption ("--diag");
        if (args.containsOption ("--model"))
            modelName = normaliseModelName (args.getValueForOption ("--model"));

        if (! frontendSender.connect (frontendHost, GenisysProtocol::frontendPort))
            juce::Logger::writeToLog ("Backend: warning — could not connect frontend sender to "
                                      + frontendHost);

        if (! diagSender.connect (diagHost, GenisysProtocol::diagPort))
            juce::Logger::writeToLog ("Backend: warning — could not connect diag sender to "
                                      + diagHost);

        if (! receiver.connect (GenisysProtocol::backendPort))
        {
            juce::Logger::writeToLog ("Backend: failed to bind port "
                                      + juce::String (GenisysProtocol::backendPort));
            quit();
            return;
        }
        receiver.addListener (this);
        juce::Logger::writeToLog ("GenisysBackend listening on port "
                                  + juce::String (GenisysProtocol::backendPort)
                                  + "  diag -> " + diagHost
                                  + ":" + juce::String (GenisysProtocol::diagPort)
                                  + "  model -> Whisper-" + modelName);

        wireVoiceEngine();

        if (! voice.start (modelName))
            juce::Logger::writeToLog ("Backend: voice engine not started"
                                      " (build without GENISYS_HAS_HAILO, or Hailo unavailable)");
    }

    void shutdown() override
    {
        voice.stop();
        receiver.removeListener (this);
    }

    bool moreThanOneInstanceAllowed() override          { return false; }
    void anotherInstanceStarted (const juce::String&) override {}
    void systemRequestedQuit() override                 { quit(); }
    void suspended() override                           {}
    void resumed() override                             {}
    void unhandledException (const std::exception*, const juce::String&, int) override { quit(); }

    void oscMessageReceived (const juce::OSCMessage& msg) override
    {
        const auto addr = msg.getAddressPattern().toString();

        if (addr == GenisysProtocol::ping)
        {
            frontendSender.send (GenisysProtocol::ack, juce::String (GenisysProtocol::ping));
            return;
        }

        if (addr == GenisysProtocol::studioLoad && msg.size() == 1 && msg[0].isString())
        {
            auto cfg = StudioConfig::fromJson (msg[0].getString());
            router.loadStudio (cfg);
            refreshVoiceVocabulary (cfg);

            juce::Logger::writeToLog ("Backend: loaded studio \"" + cfg.studioName + "\"");
            frontendSender.send (GenisysProtocol::ack,
                                 juce::String (GenisysProtocol::studioLoad));
        }
        else if (addr == GenisysProtocol::commandSend
                 && msg.size() == 2 && msg[0].isString() && msg[1].isString())
        {
            router.routeCommand (msg[0].getString(), msg[1].getString());
        }
    }

private:
    void wireVoiceEngine()
    {
        voice.onTranscript = [this] (const juce::String& transcript, int latencyMs)
        {
            juce::Logger::writeToLog ("Voice: \"" + transcript + "\" (" + juce::String (latencyMs) + "ms)");
            diagSender.send (GenisysProtocol::diagTranscript, transcript, latencyMs);

            ++totalUtterances;
            totalLatencyMs += latencyMs;
            emitDiagStats();
        };

        voice.onCommandMatch = [this] (const juce::String& transcript, const juce::String& commandId)
        {
            diagSender.send (GenisysProtocol::diagMatch, transcript, commandId);

            if (commandId.isNotEmpty())
            {
                ++matchedUtterances;
                router.routeCommand (commandId, transcript);
                juce::Logger::writeToLog ("Voice matched: \"" + commandId + "\"");
            }
        };

        voice.onVadStart = [this] (float prob)
        {
            juce::Logger::writeToLog ("VAD: speech start  p=" + juce::String (prob, 2));
            diagSender.send (GenisysProtocol::diagVadStart, prob);
        };

        voice.onVadEnd = [this] (int samples)
        {
            juce::Logger::writeToLog ("VAD: end  " + juce::String (samples / 16) + "ms -> Whisper");
            diagSender.send (GenisysProtocol::diagVadEnd, samples);
        };
    }

    void refreshVoiceVocabulary (const StudioConfig& cfg)
    {
        std::vector<std::pair<juce::String, juce::String>> vocab;
        for (const auto& device : cfg.devices)
            vocab.emplace_back (device.name, device.name);
        voice.setVocabulary (std::move (vocab));
    }

    void emitDiagStats()
    {
        if (totalUtterances == 0) return;
        const float avgLatency = static_cast<float> (totalLatencyMs) / static_cast<float> (totalUtterances);
        diagSender.send (GenisysProtocol::diagStats,
                         totalUtterances, matchedUtterances, avgLatency);
    }

    juce::OSCReceiver receiver;
    juce::OSCSender   frontendSender;
    juce::OSCSender   diagSender;
    CommandRouter     router;
    VoiceEngine       voice;

    static juce::String normaliseModelName (const juce::String& s)
    {
        const auto lower = s.toLowerCase().trim();
        if (lower == "base"  || lower == "b") return "Base";
        if (lower == "small" || lower == "s") return "Small";
        return "Tiny";
    }

    juce::String frontendHost { "127.0.0.1" };
    juce::String diagHost     { "127.0.0.1" };
    juce::String modelName    { "Tiny" };

    int   totalUtterances   = 0;
    int   matchedUtterances = 0;
    int   totalLatencyMs    = 0;
};
