#pragma once

#include "CommonHeader.h"
#include "deepspeech.h"

typedef struct {
    const char* string;
    double cpu_time_overall;
} ds_result;

struct meta_word {
    std::string word;
    float start_time;
    float duration;
};

typedef struct {
    char*  buffer;
    size_t buffer_size;
} ds_audio_buffer;

namespace GuiApp
{
class MainComponent  : public juce::AudioAppComponent
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    //GUI elements
    juce::ImageButton logoImageComponent;

    HackAudio::Button topButton;
    void topButtonClicked();
    juce::Label topButtonLabel;
    void startRecording(const juce::File& file);
    void stop();

    HackAudio::Button middleButton;
    void middleButtonClicked();
    juce::Label middleButtonLabel;
    void stopRecordingAndConvert();

    HackAudio::Button bottomButton;
    void bottomButtonClicked();
    juce::Label bottomButtonLabel;

    bool currentlyRecording = false;
    bool currentlyPlaying = false;

    juce::ToggleButton settingsButton;
    void settingsButtonClicked();

    HackAudio::Button quitCPanelButton;
    static void quitCPanelButtonClicked();
    juce::Label quitCPanelLabel;

    HackAudio::Button enablePassthroughButton;
    void enablePassthroughButtonClicked();
    juce::Label enablePassthroughLabel;

    juce::TextEditor textDisplay;

    //DSP
    std::unique_ptr<juce::AudioFormatReaderSource> newSource;
    juce::AudioTransportSource transportSource;

    juce::AudioFormatManager formatManager;
    juce::File lastRecording;
    juce::AudioBuffer<float> loadedAudioBuffer;

    bool enablePassthrough = false;
    bool channel2Enabled = false;

    juce::AudioDeviceSelectorComponent selector {
        deviceManager, 1, 1,
        2, 2,
        false, false,
        true, false};

    //Resampler
    std::unique_ptr<gin::ResamplingFifo> inputResampler;
    const int targetSampleRate = 16000;
    const int maxInputSampleRate = 96000;
    int currentInputSampleRate;
    int currentBlockSize;
    juce::AudioBuffer<float> modelBuffer;

    //Meter
    foleys::LevelMeterLookAndFeel lnf;
    foleys::LevelMeter meter { foleys::LevelMeter::MeterFlags::Default };
    foleys::LevelMeterSource meterSource;

    //Recorder
    juce::TimeSliceThread backgroundThread { "Audio Recorder Thread" }; // the thread that will write our audio data to disk
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter; // the FIFO used to buffer the incoming data

    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };

    //DeepSpeech
    //State variables
    ModelState* ctx;

    //args.h variables
    //TODO: Convert to GUI tools to load these hard dependencies, or embed to binary if they never change
    const char* homeDir = getenv("HOME");
    const char* model = "/repos/GENISYS/LFS/deepspeech-0.9.3-models.pbmm";
    const char* scorer = "/repos/GENISYS/LFS/deepspeech-0.9.3-models.scorer";
    //TODO: 1 Replace with GUI-based Start/Stop Recording functionality
    //TODO: 2 Replace with circular buffer-based keyword detector and ViAD logic (Voice inActivity Detector)
    char* audio = NULL;

    //const char* hot_words = "";
    const char* hot_words = "genesis:5";
    float max_boost = 20.0f; //See: https://deepspeech.readthedocs.io/en/master/HotWordBoosting-Examples.html

    //TODO: Study currently unused options
    bool set_beamwidth = false;
    int beam_width = 0;
    bool set_alphabeta = false;
    float lm_alpha = 0.f;
    float lm_beta = 0.f;
    bool show_times = false;
    bool has_versions = false;
    bool extended_metadata = false;
    bool json_output = false;
    int json_candidate_transcripts = 3;
    int stream_size = 0;
    int extended_stream_size = 0;

    //==============================================================================
    char* CandidateTranscriptToString(const CandidateTranscript* transcript);
    std::vector<meta_word> CandidateTranscriptToWords(const CandidateTranscript* transcript);
    std::string CandidateTranscriptToJSON(const CandidateTranscript *transcript);
    char* MetadataToJSON(Metadata* result);
    ds_result LocalDsSTT(ModelState* aCtx, const short* aBuffer, size_t aBufferSize, bool extended_output, bool json_output);
    std::vector<std::string> SplitStringOnDelim(std::string in_string, std::string delim);

    ds_audio_buffer GetAudioBuffer(const char* path);
    void ProcessFile(ModelState* context, const char* path, bool show_times);
    int lastRecordingSize;
    int lastNumSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace GuiApp
