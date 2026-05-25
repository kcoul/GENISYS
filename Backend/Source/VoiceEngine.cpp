// Hailo NPU voice engine for GENISYS backend.
// Same Hailo + silero VAD pipeline as VREngine/Bridge, extended with:
//   - per-utterance latency measurement
//   - keyword vocabulary matching
//   - VAD start/end callbacks for diagnostic telemetry
//
// Compiled only when GENISYS_HAS_HAILO=1.

#ifndef GENISYS_HAS_HAILO
#define GENISYS_HAS_HAILO 0
#endif

#if GENISYS_HAS_HAILO

#ifdef _WIN32
#  define _HAILO_HAILORT_DEFAULTS_HPP_
#endif
#include <hailo/genai/speech2text/speech2text.hpp>
#include <hailo/hailort.hpp>
#ifdef _WIN32
#  undef _HAILO_HAILORT_DEFAULTS_HPP_
#  undef interface
#  include <hailo/hailort_defaults.hpp>
#endif

#include <whisper.h>

#ifndef GENISYS_HAS_EMBEDDED_VAD
#define GENISYS_HAS_EMBEDDED_VAD 0
#endif

#if GENISYS_HAS_EMBEDDED_VAD
#include "embedded_vad_model.h"
#endif

#endif // GENISYS_HAS_HAILO

#include "VoiceEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>

// =============================================================================
// Non-Hailo stubs — keeps the rest of the codebase compiling on any platform.
// =============================================================================
#if !GENISYS_HAS_HAILO

VoiceEngine::~VoiceEngine() {}

void VoiceEngine::setVocabulary (std::vector<std::pair<juce::String, juce::String>> vocab)
{
    std::lock_guard<std::mutex> lock (vocabMutex);
    vocabulary = std::move (vocab);
}

bool VoiceEngine::start() { return false; }
void VoiceEngine::stop()  {}

void VoiceEngine::audioDeviceAboutToStart (juce::AudioIODevice*)          {}
void VoiceEngine::audioDeviceStopped()                                    {}
void VoiceEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const*, int, int,
                                                    const juce::AudioIODeviceCallbackContext&) {}
void VoiceEngine::workerLoop (std::string) {}

juce::String VoiceEngine::matchVocabulary (const juce::String&) const { return {}; }
std::vector<float> VoiceEngine::resampleToWhisperRate (const std::vector<float>& v, double) { return v; }

#else // GENISYS_HAS_HAILO

// =============================================================================
// Hailo implementation
// =============================================================================

namespace
{
constexpr int    whisperSampleRate      = 16000;
constexpr double vadFrameSeconds        = 0.04;
constexpr double vadPreRollSeconds      = 0.25;
constexpr double vadMinSpeechSeconds    = 0.18;
constexpr double vadEndSilenceSeconds   = 0.28;
constexpr double vadMaxUtteranceSeconds = 2.5;
constexpr float  vadSpeechThreshold     = 0.50f;
constexpr auto   hailoVDeviceGroupId    = "SHARED";
constexpr auto   hailoHefFilename       = "Whisper-Tiny.hef";

static std::string findHailoWhisperHef()
{
    const auto binary = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const auto home   = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const auto name   = juce::String (hailoHefFilename);

    const juce::File candidates[] = {
        binary.getParentDirectory().getChildFile ("models/hailo10h").getChildFile (name),
        home.getChildFile ("bridge/models/hailo10h").getChildFile (name),
        home.getChildFile ("bridge").getChildFile (name),
    };

    for (const auto& f : candidates)
        if (f.existsAsFile())
            return f.getFullPathName().toStdString();

    throw std::runtime_error (std::string ("Hailo HEF not found: ") + hailoHefFilename
                              + ". Place it next to the binary in models/hailo10h/");
}

static std::shared_ptr<hailort::VDevice> createSharedHailoVDevice()
{
    hailo_vdevice_params_t params {};
    hailo_init_vdevice_params (&params);
    params.group_id = hailoVDeviceGroupId;
    auto vdevice = hailort::VDevice::create_shared (params);
    if (! vdevice)
        throw hailort::hailort_error (vdevice.status(), "Failed to create Hailo VDevice");
    return vdevice.release();
}

#if GENISYS_HAS_EMBEDDED_VAD
struct EmbeddedBufferState { const unsigned char* data; size_t size; size_t pos { 0 }; };
static size_t embeddedRead  (void* ctx, void* out, size_t n)
{
    auto* s = static_cast<EmbeddedBufferState*> (ctx);
    auto  toRead = std::min (n, s->size - s->pos);
    std::memcpy (out, s->data + s->pos, toRead);
    s->pos += toRead;
    return toRead;
}
static bool   embeddedEof   (void* ctx) { auto* s = static_cast<EmbeddedBufferState*>(ctx); return s->pos >= s->size; }
static void   embeddedClose (void*)     {}
#endif
} // namespace

// =============================================================================

VoiceEngine::~VoiceEngine() { stop(); }

void VoiceEngine::setVocabulary (std::vector<std::pair<juce::String, juce::String>> vocab)
{
    std::lock_guard<std::mutex> lock (vocabMutex);
    vocabulary = std::move (vocab);
}

bool VoiceEngine::start()
{
    if (running.load()) return true;

    std::string hefPath;
    try { hefPath = findHailoWhisperHef(); }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog (juce::String ("VoiceEngine: ") + e.what());
        return false;
    }

    const auto result = audioDeviceManager.initialise (1, 0, nullptr, true);
    if (result.isNotEmpty())
    {
        juce::Logger::writeToLog ("VoiceEngine: audio init failed: " + result);
        return false;
    }

    { std::lock_guard<std::mutex> lock (mutex); micBuffer.clear(); micSampleRate = whisperSampleRate; }

    shouldStop.store (false);
    running.store (true);
    audioDeviceManager.addAudioCallback (this);

    worker = std::thread ([this, hef = std::move (hefPath)]() mutable {
        workerLoop (std::move (hef));
    });

    return true;
}

void VoiceEngine::stop()
{
    if (! running.exchange (false)) return;

    audioDeviceManager.removeAudioCallback (this);
    audioDeviceManager.closeAudioDevice();

    { std::lock_guard<std::mutex> lock (mutex); shouldStop.store (true); }
    cv.notify_all();

    if (worker.joinable())
        worker.join();
}

void VoiceEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    std::lock_guard<std::mutex> lock (mutex);
    micSampleRate = device != nullptr ? device->getCurrentSampleRate() : whisperSampleRate;
    micBuffer.clear();
}

void VoiceEngine::audioDeviceStopped()
{
    std::lock_guard<std::mutex> lock (mutex);
    micBuffer.clear();
}

void VoiceEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                     int numInputChannels,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch]) juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    if (! running.load() || numInputChannels <= 0 || numSamples <= 0) return;

    std::vector<float> mono (static_cast<size_t> (numSamples));
    for (int i = 0; i < numSamples; ++i)
    {
        float sum = 0.0f; int used = 0;
        for (int ch = 0; ch < numInputChannels; ++ch)
            if (inputChannelData[ch]) { sum += inputChannelData[ch][i]; ++used; }
        mono[static_cast<size_t> (i)] = used > 0 ? sum / static_cast<float> (used) : 0.0f;
    }

    { std::lock_guard<std::mutex> lock (mutex); micBuffer.insert (micBuffer.end(), mono.begin(), mono.end()); }
    cv.notify_one();
}

juce::String VoiceEngine::matchVocabulary (const juce::String& transcript) const
{
    const auto lower = transcript.toLowerCase();
    std::lock_guard<std::mutex> lock (vocabMutex);
    for (const auto& [keyword, commandId] : vocabulary)
        if (lower.contains (keyword.toLowerCase()))
            return commandId;
    return {};
}

void VoiceEngine::workerLoop (std::string hefPath)
{
    // ── VAD init ─────────────────────────────────────────────────────────────
    auto vadParams      = whisper_vad_default_context_params();
    vadParams.n_threads = juce::jlimit (1, 4, static_cast<int> (std::thread::hardware_concurrency()));
    vadParams.use_gpu   = false;

#if GENISYS_HAS_EMBEDDED_VAD
    EmbeddedBufferState vadBuf { ggml_silero_vad_model, ggml_silero_vad_model_size };
    whisper_model_loader vadLoader { &vadBuf, embeddedRead, embeddedEof, embeddedClose };
    std::unique_ptr<whisper_vad_context, decltype (&whisper_vad_free)> vadCtx (
        whisper_vad_init_with_params (&vadLoader, vadParams), whisper_vad_free);
#else
    const auto vadFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                             .getParentDirectory().getChildFile ("models/ggml-silero-v6.2.0.bin");
    std::unique_ptr<whisper_vad_context, decltype (&whisper_vad_free)> vadCtx (
        whisper_vad_init_from_file_with_params (vadFile.getFullPathName().toRawUTF8(), vadParams),
        whisper_vad_free);
#endif

    if (! vadCtx)
    {
        juce::MessageManager::callAsync ([this] {
            juce::Logger::writeToLog ("VoiceEngine: VAD model load failed");
            running.store (false);
        });
        return;
    }

    // ── Hailo Speech2Text init ────────────────────────────────────────────────
    std::shared_ptr<hailort::VDevice>            hailoVDevice;
    std::unique_ptr<hailort::genai::Speech2Text> speech2Text;
    try
    {
        hailoVDevice     = createSharedHailoVDevice();
        auto s2tExpected = hailort::genai::Speech2Text::create (
            hailoVDevice, hailort::genai::Speech2TextParams (hefPath));
        if (! s2tExpected)
            throw hailort::hailort_error (s2tExpected.status(), "Speech2Text create failed");
        speech2Text = std::make_unique<hailort::genai::Speech2Text> (s2tExpected.release());
    }
    catch (const std::exception& e)
    {
        juce::MessageManager::callAsync ([this, msg = juce::String (e.what())] {
            juce::Logger::writeToLog ("VoiceEngine: Hailo init failed: " + msg);
            running.store (false);
        });
        return;
    }

    { std::lock_guard<std::mutex> lock (mutex); micBuffer.clear(); }
    whisper_vad_reset_state (vadCtx.get());

    const auto preRollSamples    = static_cast<size_t> (vadPreRollSeconds    * whisperSampleRate);
    const auto minSpeechSamples  = static_cast<size_t> (vadMinSpeechSeconds  * whisperSampleRate);
    const auto endSilenceSamples = static_cast<size_t> (vadEndSilenceSeconds * whisperSampleRate);
    const auto maxUtterSamples   = static_cast<size_t> (vadMaxUtteranceSeconds * whisperSampleRate);

    std::vector<float> preRoll, utterance;
    bool   speechActive   = false;
    size_t silenceSamples = 0;
    std::chrono::steady_clock::time_point speechEndTime;

    while (! shouldStop.load())
    {
        std::vector<float> chunk;
        double sampleRate = whisperSampleRate;

        {
            std::unique_lock<std::mutex> lock (mutex);
            cv.wait (lock, [this] {
                const auto needed = static_cast<size_t> (micSampleRate * vadFrameSeconds);
                return shouldStop.load() || micBuffer.size() >= needed;
            });
            if (shouldStop.load()) break;

            sampleRate        = micSampleRate;
            const auto needed = static_cast<size_t> (sampleRate * vadFrameSeconds);
            if (micBuffer.size() < needed) continue;

            if (! speechActive && micBuffer.size() > needed * 20)
                micBuffer.erase (micBuffer.begin(), micBuffer.end() - static_cast<std::ptrdiff_t> (needed * 20));

            chunk.assign (micBuffer.begin(), micBuffer.begin() + static_cast<std::ptrdiff_t> (needed));
            micBuffer.erase  (micBuffer.begin(), micBuffer.begin() + static_cast<std::ptrdiff_t> (needed));
        }

        auto pcm16k = resampleToWhisperRate (chunk, sampleRate);
        if (pcm16k.empty()) continue;

        if (! whisper_vad_detect_speech_no_reset (vadCtx.get(), pcm16k.data(), static_cast<int> (pcm16k.size())))
            continue;

        const auto* probs  = whisper_vad_probs (vadCtx.get());
        const auto  nProbs = whisper_vad_n_probs (vadCtx.get());
        float maxProb = 0.0f;
        for (int i = 0; probs && i < nProbs; ++i) maxProb = std::max (maxProb, probs[i]);

        const bool frameHasSpeech = maxProb >= vadSpeechThreshold;

        if (! speechActive)
        {
            preRoll.insert (preRoll.end(), pcm16k.begin(), pcm16k.end());
            if (preRoll.size() > preRollSamples)
                preRoll.erase (preRoll.begin(), preRoll.end() - static_cast<std::ptrdiff_t> (preRollSamples));

            if (! frameHasSpeech) continue;

            speechActive   = true;
            silenceSamples = 0;
            utterance      = preRoll;

            const float prob = maxProb;
            juce::MessageManager::callAsync ([this, prob] { if (onVadStart) onVadStart (prob); });
        }
        else
        {
            utterance.insert (utterance.end(), pcm16k.begin(), pcm16k.end());
        }

        if (frameHasSpeech) silenceSamples = 0;
        else                 silenceSamples += pcm16k.size();

        const bool hasEnoughSpeech   = utterance.size() >= minSpeechSamples;
        const bool reachedEndSilence = silenceSamples   >= endSilenceSamples;
        const bool reachedMaxDuration = utterance.size() >= maxUtterSamples;

        if (! reachedMaxDuration && (! hasEnoughSpeech || ! reachedEndSilence))
            continue;

        const int utterSamples = static_cast<int> (utterance.size());
        juce::MessageManager::callAsync ([this, utterSamples] { if (onVadEnd) onVadEnd (utterSamples); });

        speechEndTime = std::chrono::steady_clock::now();

        // ── Transcribe ───────────────────────────────────────────────────────
        try
        {
            auto genParams = speech2Text->create_generator_params()
                                 .expect ("create_generator_params");
            genParams.set_task (hailort::genai::Speech2TextTask::TRANSCRIBE);
            genParams.set_language ("en");

            const auto textResult = speech2Text->generate_all_text (
                hailort::MemoryView (const_cast<float*> (utterance.data()),
                                     utterance.size() * sizeof (float)),
                genParams, std::chrono::milliseconds (15000))
                    .expect ("generate_all_text");

            const int latencyMs = static_cast<int> (
                std::chrono::duration_cast<std::chrono::milliseconds> (
                    std::chrono::steady_clock::now() - speechEndTime).count());

            juce::String transcript = juce::String (textResult).trim();
            if (transcript.isEmpty()) { /* skip empty results */ }
            else
            {
                const juce::String commandId = matchVocabulary (transcript);
                juce::MessageManager::callAsync ([this, transcript, commandId, latencyMs] {
                    if (onTranscript)    onTranscript    (transcript, latencyMs);
                    if (onCommandMatch)  onCommandMatch  (transcript, commandId);
                });
            }
        }
        catch (const std::exception& e)
        {
            juce::MessageManager::callAsync ([this, msg = juce::String (e.what())] {
                juce::Logger::writeToLog ("VoiceEngine: transcription error: " + msg);
                running.store (false);
            });
            break;
        }

        speechActive   = false;
        silenceSamples = 0;
        utterance.clear();
        preRoll.clear();
        whisper_vad_reset_state (vadCtx.get());
    }
}

std::vector<float> VoiceEngine::resampleToWhisperRate (const std::vector<float>& input, double inputRate)
{
    if (input.empty()) return {};
    if (std::abs (inputRate - whisperSampleRate) < 1.0) return input;

    const auto outputSize = static_cast<size_t> (
        std::max (1.0, std::floor (static_cast<double> (input.size()) * whisperSampleRate / inputRate)));
    std::vector<float> output (outputSize);
    const auto ratio = inputRate / static_cast<double> (whisperSampleRate);
    for (size_t i = 0; i < output.size(); ++i)
    {
        const auto pos   = static_cast<double> (i) * ratio;
        const auto index = static_cast<size_t> (pos);
        const auto frac  = static_cast<float> (pos - static_cast<double> (index));
        const auto a     = input[std::min (index,     input.size() - 1)];
        const auto b     = input[std::min (index + 1, input.size() - 1)];
        output[i] = a + (b - a) * frac;
    }
    return output;
}

#endif // GENISYS_HAS_HAILO
