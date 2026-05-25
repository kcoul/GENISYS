#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Hailo NPU voice pipeline for the GENISYS backend.
// Compiled with implementation only when GENISYS_HAS_HAILO=1;
// otherwise start() returns false and all callbacks are never fired.
class VoiceEngine final : private juce::AudioIODeviceCallback
{
public:
    // All callbacks are invoked on the JUCE message thread.
    std::function<void (const juce::String& transcript, int latencyMs)>               onTranscript;
    std::function<void (const juce::String& transcript, const juce::String& commandId)> onCommandMatch;
    std::function<void (float speechProbability)>                                      onVadStart;
    std::function<void (int utteranceSamples)>                                         onVadEnd;

    VoiceEngine() = default;
    ~VoiceEngine();

    // Each entry is { keyword, commandId }.
    // Matching: first keyword whose lowercase form appears in the lowercased transcript wins.
    void setVocabulary (std::vector<std::pair<juce::String, juce::String>> vocab);

    bool start (const juce::String& modelName = "Tiny");
    void stop();
    bool isRunning() const noexcept { return running.load(); }

private:
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const*, int,
                                           float* const*, int, int,
                                           const juce::AudioIODeviceCallbackContext&) override;

    void        workerLoop (std::string hefPath);
    juce::String matchVocabulary (const juce::String& transcript) const;
    static std::vector<float> resampleToWhisperRate (const std::vector<float>&, double);

    juce::AudioDeviceManager audioDeviceManager;
    std::atomic<bool>        running    { false };
    std::atomic<bool>        shouldStop { false };
    std::thread              worker;
    std::mutex               mutex;
    std::condition_variable  cv;
    std::vector<float>       micBuffer;
    double                   micSampleRate { 16000.0 };

    mutable std::mutex                                  vocabMutex;
    std::vector<std::pair<juce::String, juce::String>>  vocabulary;
};
