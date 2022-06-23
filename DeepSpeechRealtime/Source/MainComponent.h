#pragma once

#include <JuceHeader.h>

#include <stdlib.h>
#include <stdio.h>

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sstream>
#include <string>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__ANDROID__) || defined(_MSC_VER) || TARGET_OS_IPHONE
#define NO_SOX
#endif

#if defined(_MSC_VER)
#define NO_DIR
#endif

#ifndef NO_SOX
#include <sox.h>
#endif

#ifndef NO_DIR
#include <dirent.h>
#include <unistd.h>
#endif // NO_DIR
#include <vector>

#include "deepspeech.h"
//args.h won't work OOB in a non CLI/main.cpp scenario
//The contents have been refactored into this class
//#include "args.h"

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

class MainComponent  : public juce::AudioAppComponent
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //State variables
    ModelState* ctx;
    
    //args.h variables
    //TODO: Convert to GUI tools to load these hard dependencies, or embed to binary if they never change
    const char* model = "/Users/catdev/repos/GENISYS/DeepSpeechRealtime/Builds/MacOSX/build/Debug/deepspeech-0.9.3-models.pbmm";
    const char* scorer = "/Users/catdev/repos/GENISYS/DeepSpeechRealtime/Builds/MacOSX/build/Debug/deepspeech-0.9.3-models.scorer";
    //TODO: 1 Replace with GUI-based Start/Stop Recording functionality
    //TODO: 2 Replace with circular buffer-based keyword detector and ViAD logic (Voice inActivity Detector)
    char* audio = NULL;
    
    const char* hot_words = "genesis";
    
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
    ds_audio_buffer GetAudioBuffer(const char* path, int desired_sample_rate);
    void ProcessFile(ModelState* context, const char* path, bool show_times);
    std::vector<std::string> SplitStringOnDelim(std::string in_string, std::string delim);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
