#include "MainComponent.h"
#include "BinaryHelper.h"
namespace GuiApp
{

//==============================================================================
MainComponent::MainComponent()
{
    backgroundThread.startThread();
    formatManager.registerBasicFormats();

    //On RPi: this is the resolution of a Waveshare 3.5" touchscreen display.
    //(On Mobile: setSize() is ignored but height/width must be set and non-zero)
    setSize (800, 480);

    setOpaque(true);

//Selector Pane
    addAndMakeVisible(selector);
    selector.setVisible(false);

    quitCPanelLabel.setText("Quit CPanel", juce::NotificationType::dontSendNotification);
    quitCPanelLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(quitCPanelLabel);
    quitCPanelLabel.setVisible(false);

    quitCPanelButton.setButtonStyle(HackAudio::Button::ButtonStyle::BarToggle);
    quitCPanelButton.onClick = [] { quitCPanelButtonClicked(); };
    quitCPanelButton.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(quitCPanelButton);
    quitCPanelButton.setVisible(false);

    enablePassthroughLabel.setText("Enable Passthrough",
                                   juce::NotificationType::dontSendNotification);
    enablePassthroughLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(enablePassthroughLabel);
    enablePassthroughLabel.setVisible(false);

    enablePassthroughButton.setButtonStyle(HackAudio::Button::ButtonStyle::BarToggle);
    enablePassthroughButton.onClick = [this] { enablePassthroughButtonClicked(); };
    addAndMakeVisible(enablePassthroughButton);
    enablePassthroughButton.setVisible(false);

//Main Pane
    auto images = getBinaryDataImages();

    logoImageComponent.setImages(true, true, true,
                                 images.front(), 1.0f, juce::Colours::transparentWhite,
                                 images.front(), 1.0f, juce::Colours::transparentWhite,
                                 images.front(), 1.0f, juce::Colours::transparentWhite);
    logoImageComponent.onClick = [this] { settingsButtonClicked(); };
    addAndMakeVisible(logoImageComponent);

    topButton.setButtonStyle(HackAudio::Button::ButtonStyle::BarToggle);
    topButton.onClick = [this] { topButtonClicked(); };
    addAndMakeVisible(topButton);

    topButtonLabel.setText("Record", juce::NotificationType::dontSendNotification);
    topButtonLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(topButtonLabel);

    middleButton.setButtonStyle(HackAudio::Button::ButtonStyle::BarToggle);
    middleButton.onClick = [this] { middleButtonClicked(); };
    middleButton.setEnabled(false);
    addAndMakeVisible(middleButton);

    middleButtonLabel.setText("Stop", juce::NotificationType::dontSendNotification);
    middleButtonLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(middleButtonLabel);

    bottomButton.setButtonStyle(HackAudio::Button::ButtonStyle::BarToggle);
    bottomButton.onClick = [this] { bottomButtonClicked(); };
    addAndMakeVisible(bottomButton);

    bottomButtonLabel.setText("Play", juce::NotificationType::dontSendNotification);
    bottomButtonLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bottomButtonLabel);

    textDisplay.setText("The quick brown fox jumps over the lazy dog", juce::dontSendNotification);
    textDisplay.setMultiLine(true, true);
    textDisplay.setReadOnly(true);
    textDisplay.setColour(juce::TextEditor::backgroundColourId, quitCPanelButton.findColour(HackAudio::midgroundColourId));
    textDisplay.setFont(juce::Font(64));
    addAndMakeVisible(textDisplay);

    lnf.setColour(foleys::LevelMeter::lmBackgroundColour, quitCPanelButton.findColour(HackAudio::midgroundColourId));
    lnf.setColour (foleys::LevelMeter::lmMeterGradientLowColour, juce::Colours::green);
    lnf.setColour(foleys::LevelMeter::lmTicksColour,
                  quitCPanelButton.findColour(HackAudio::backgroundColourId));

    meter.setLookAndFeel (&lnf);
    meter.setMeterSource (&meterSource);
    addAndMakeVisible (meter);

    setAudioChannels(2,2);

    //DeepSpeech
    char* modelPath = (char*)calloc(strlen(homeDir)+strlen(model)+1, sizeof(char));
    strcpy(modelPath,homeDir); // copy homeDir into the modelPath.
    strcat(modelPath,model); // append model to the modelPath.

    int status = DS_CreateModel(modelPath, &ctx);
    if (status != 0)
    {
        /*TODO: Post failure to console */
        int i = 1;
    }
    else { /*TODO: Post success to console */ }

    free(modelPath);

    char* scorerPath = (char*)calloc(strlen(homeDir)+strlen(scorer)+1, sizeof(char));
    strcpy(scorerPath,homeDir); // copy homeDir into the modelPath.
    strcat(scorerPath,scorer); // append scorer to the modelPath.

    status = DS_EnableExternalScorer(ctx, scorerPath);
    if (status != 0)
    {
        /*TODO: Post failure to GUI console */
        int i = 1;
    }
    else { /*TODO: Post success to GUI console */ }

    free(scorerPath);

    if (hot_words)
    {
        std::vector<std::string> hot_words_ = SplitStringOnDelim(hot_words, ",");
        for ( std::string hot_word_ : hot_words_ )
        {
            std::vector<std::string> pair_ = SplitStringOnDelim(hot_word_, ":");
            const char* word = (pair_[0]).c_str();
            // the strtof function will return 0 in case of non numeric characters
            // so, check the boost string before we turn it into a float
            bool boost_is_valid = (pair_[1].find_first_not_of("-.0123456789") == std::string::npos);
            float boost = strtof((pair_[1]).c_str(),0);
            status = DS_AddHotWord(ctx, word, boost);
            if (status != 0 || !boost_is_valid)
            {
                /*TODO: Post failure to GUI console */
                int i = 1;
            }
            else { /*TODO: Post success to GUI console */ }
        }
    }

    auto parentDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    lastRecording = parentDir.getChildFile("GenisysTestRecording.wav");

    juce::AudioFormatReader* reader = formatManager.createReaderFor(lastRecording);
    lastRecordingSize = sizeof(unsigned char) * reader->lengthInSamples * 2;
    lastNumSamples = reader->lengthInSamples;

    loadedAudioBuffer = juce::AudioBuffer<float>(1, reader->lengthInSamples);
    reader->read(&loadedAudioBuffer, 0, reader->lengthInSamples,
                 0, true, true);

    newSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);   // [11]
    transportSource.setSource (newSource.get(), 0, nullptr, reader->sampleRate);

    ProcessFile(ctx, lastRecording.getFullPathName().toRawUTF8(), true);
}

ds_audio_buffer MainComponent::GetAudioBuffer(const char* path)
{
    ds_audio_buffer res = {0};

    FILE* wave = fopen(path, "rb");

    size_t rv;

    res.buffer_size = lastRecordingSize;
    res.buffer = (char*)malloc(sizeof(char) * res.buffer_size);
    size_t result = fread(res.buffer, sizeof(char), res.buffer_size, wave);
    //TODO: There is a bug related to this assert, maybe file-handle-related?
    //assert(result == res.buffer_size);
    fclose(wave);
    return res;
}

void MainComponent::ProcessFile(ModelState* context, const char* path, bool show_times)
{
    ds_audio_buffer audio = GetAudioBuffer(path);

    // Pass audio to DeepSpeech
    // We take half of buffer_size because buffer is a char* while
    // LocalDsSTT() expected a short*
    ds_result result = LocalDsSTT(context,
                                  (const short*)audio.buffer,
                                  audio.buffer_size / 2,
                                  extended_metadata,
                                  json_output);
    free(audio.buffer);

    if (result.string)
    {
        printf("%s\n", result.string);
        DS_FreeString((char*)result.string);
        textDisplay.setText(juce::String(result.string), juce::dontSendNotification);
    }

    if (show_times) {
        printf("cpu_time_overall=%.05f\n",
               result.cpu_time_overall);
    }
}

void MainComponent::quitCPanelButtonClicked()
{
    juce::JUCEApplicationBase::quit();
}

void MainComponent::enablePassthroughButtonClicked()
{
    enablePassthrough = enablePassthroughButton.getToggleState();
}

MainComponent::~MainComponent()
{
    shutdownAudio();
    meter.setLookAndFeel (nullptr);
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    meterSource.resize (1, static_cast<int>(sampleRate * 0.1 / samplesPerBlockExpected));

    if (sampleRate < targetSampleRate)
    {
        fprintf(stderr, "Warning: original sample rate (%d) is lower than %dkHz. "
                        "Up-sampling might produce erratic speech recognition.\n", targetSampleRate, (int)sampleRate);
    }

    const int resamplerBlockSize = samplesPerBlockExpected;
    const int resamplerMaxSamples = maxInputSampleRate * 2;

    inputResampler = std::make_unique<gin::ResamplingFifo>(resamplerBlockSize, 1, resamplerMaxSamples);

    if (currentBlockSize != samplesPerBlockExpected || currentInputSampleRate != sampleRate)
        meterSource.resize (1, static_cast<int>(sampleRate * 0.1 / samplesPerBlockExpected));

    if (currentBlockSize != samplesPerBlockExpected)
    {
        currentBlockSize = samplesPerBlockExpected;
        modelBuffer = juce::AudioBuffer<float>(1, currentBlockSize);
    }

    if (currentInputSampleRate != sampleRate)
    {
        currentInputSampleRate = sampleRate;
        inputResampler->setResamplingRatio(currentInputSampleRate, targetSampleRate);
        inputResampler->reset();
    }

    transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (quitCPanelButton.findColour(HackAudio::midgroundColourId));
}

void MainComponent::resized()
{
    //Tic-tac-toe resizable layout (3x3)
    auto bounds = getBounds();

    int thirdOfWidth = bounds.getWidth()/3;
    int thirdOfWidthPlusOne = thirdOfWidth+1;

    int mainHeight = bounds.getHeight();
    int thirdOfMainHeight = mainHeight / 3;

    int labelHeight = thirdOfMainHeight / 5;

    int margin = 5;

    logoImageComponent.setBounds(thirdOfWidthPlusOne, 0, thirdOfWidth, thirdOfMainHeight);

    topButton.setBounds(0,
                        0, thirdOfWidth,
                        thirdOfMainHeight - labelHeight);
    topButtonLabel.setBounds(0,
                             thirdOfMainHeight - labelHeight, thirdOfWidth, labelHeight);

    middleButton.setBounds(0,
                           thirdOfMainHeight, thirdOfWidth,
                           thirdOfMainHeight - labelHeight);
    middleButtonLabel.setBounds(0, (2*thirdOfMainHeight)-labelHeight, thirdOfWidth, labelHeight);

    bottomButton.setBounds(0,
                           (2*thirdOfMainHeight), thirdOfWidth,
                           thirdOfMainHeight - labelHeight);
    bottomButtonLabel.setBounds(0, mainHeight-labelHeight, thirdOfWidth, labelHeight);

    textDisplay.setBounds(thirdOfWidth + (margin * 2), thirdOfMainHeight + margin,
                          thirdOfWidth - (margin * 2), (thirdOfMainHeight * 2) - (margin * 2));

    meter.setBounds(thirdOfWidthPlusOne * 2, 0, thirdOfWidth, mainHeight);

    selector.setBounds(2 * thirdOfWidth, thirdOfMainHeight, thirdOfWidth, mainHeight - 150);

    quitCPanelButton.setBounds(getWidth()-(thirdOfWidth/2),
                               getHeight()-(thirdOfMainHeight/2), thirdOfWidth/2,
                               (thirdOfMainHeight - labelHeight)/2);
    quitCPanelLabel.setBounds(getWidth()-(thirdOfWidth/2),
                              getHeight()-(labelHeight/2), thirdOfWidth/2, labelHeight/2);

    enablePassthroughButton.setBounds(getWidth() - (thirdOfWidth/2),
                                      0, thirdOfWidth/2,
                                      (thirdOfMainHeight - labelHeight)/2);
    enablePassthroughLabel.setBounds(getWidth() - (thirdOfWidth/2),
                                     (thirdOfMainHeight - labelHeight)/2, thirdOfWidth/2, labelHeight/2);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    meterSource.measureBlock (*bufferToFill.buffer);

    if (currentlyRecording)
    {
        inputResampler.get()->pushAudioBuffer(*bufferToFill.buffer);

        while (inputResampler.get()->samplesReady() >= currentBlockSize)
        {
            inputResampler.get()->popAudioBuffer(modelBuffer);

            const juce::ScopedLock sl (writerLock);

            if (activeWriter.load() != nullptr)
            {
                activeWriter.load()->write(modelBuffer.getArrayOfReadPointers(), currentBlockSize);
            }
        }
    }

    if (currentlyPlaying)
        transportSource.getNextAudioBlock (bufferToFill);
    else if (!enablePassthrough)
        bufferToFill.clearActiveBufferRegion();

    //Some hardware devices already duplicate mic in on ch.1 to ch.2, so below is not necessary (i.e. macOS)

    //if (channel2Enabled)
    //{
        //Mix input 2 with input 1
    //    bufferToFill.buffer->addFrom(0, 0, *bufferToFill.buffer,
    //                                  1, 0, bufferToFill.buffer->getNumSamples());
    //}
}

void MainComponent::topButtonClicked()
{
    topButton.setEnabled(false);
    middleButton.setEnabled(true);

    auto parentDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    lastRecording = parentDir.getNonexistentChildFile ("GenisysTestRecording", ".wav");

    currentlyRecording = true;
    startRecording (lastRecording);
}

void MainComponent::middleButtonClicked()
{
    stopRecordingAndConvert();
    topButton.setEnabled(true);
    middleButton.setEnabled(false);
}

void MainComponent::bottomButtonClicked()
{
    transportSource.start();
    currentlyPlaying = true;
}

void MainComponent::startRecording(const juce::File& file)
{
    stop();

    if (currentInputSampleRate > 0)
    {
        // Create an OutputStream to write to our destination file...
        file.deleteFile();

        if (auto fileStream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
        {
            // Now create a WAV writer object that writes to our output stream...
            juce::WavAudioFormat wavFormat;

            if (auto writer = wavFormat.createWriterFor (fileStream.get(), targetSampleRate, 1, 16, {}, 0))
            {
                fileStream.release(); // (passes responsibility for deleting the stream to the writer object that is now using it)

                // Now we'll create one of these helper objects which will act as a FIFO buffer, and will
                // write the data to disk on our background thread.
                threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (writer, backgroundThread, 32768));

                // And now, swap over our active writer pointer so that the audio callback will start using it..
                const juce::ScopedLock sl (writerLock);
                activeWriter = threadedWriter.get();
            }
        }
    }
}

void MainComponent::stop()
{
    // First, clear this pointer to stop the audio callback from using our writer object..
    {
        const juce::ScopedLock sl (writerLock);
        activeWriter = nullptr;
    }

    // Now we can delete the writer object. It's done in this order because the deletion could
    // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
    // the audio callback while this happens.
    threadedWriter.reset();
}

void MainComponent::stopRecordingAndConvert()
{
    currentlyRecording = false;
    stop();

    ProcessFile(ctx, lastRecording.getFullPathName().toRawUTF8(), true);
}

void MainComponent::settingsButtonClicked()
{
    logoImageComponent.setToggleState(!logoImageComponent.getToggleState(), juce::dontSendNotification);
    if (logoImageComponent.getToggleState())
    {
        meter.setVisible(false);

        quitCPanelButton.setVisible(true);
        quitCPanelLabel.setVisible(true);
        enablePassthroughButton.setVisible(true);
        enablePassthroughLabel.setVisible(true);
        selector.setVisible(true);
    }
    else
    {
        meter.setVisible(true);

        quitCPanelButton.setVisible(false);
        quitCPanelLabel.setVisible(false);
        enablePassthroughButton.setVisible(false);
        enablePassthroughLabel.setVisible(false);
        selector.setVisible(false);
    }
}

//DeepSpeech
char* MainComponent::CandidateTranscriptToString(const CandidateTranscript* transcript)
{
    std::string retval = "";
    for (int i = 0; i < transcript->num_tokens; i++)
    {
        const TokenMetadata& token = transcript->tokens[i];
        retval += token.text;
    }
    return strdup(retval.c_str());
}

std::vector<meta_word> MainComponent::CandidateTranscriptToWords(const CandidateTranscript* transcript)
{
    std::vector<meta_word> word_list;
    std::string word = "";
    float word_start_time = 0;

    // Loop through each token
    for (int i = 0; i < transcript->num_tokens; i++)
    {
        const TokenMetadata& token = transcript->tokens[i];

        // Append token to word if it's not a space
        if (strcmp(token.text, u8" ") != 0)
        {
            // Log the start time of the new word
            if (word.length() == 0)
            {
                word_start_time = token.start_time;
            }
            word.append(token.text);
        }

        // Word boundary is either a space or the last token in the array
        if (strcmp(token.text, u8" ") == 0 || i == transcript->num_tokens-1)
        {
            float word_duration = token.start_time - word_start_time;

            if (word_duration < 0)
            {
                word_duration = 0;
            }

            meta_word w;
            w.word = word;
            w.start_time = word_start_time;
            w.duration = word_duration;
            word_list.push_back(w);

            // Reset
            word = "";
            word_start_time = 0;
        }
    }
    return word_list;
}

std::string MainComponent::CandidateTranscriptToJSON(const CandidateTranscript *transcript)
{
    std::ostringstream out_string;

    std::vector<meta_word> words = CandidateTranscriptToWords(transcript);

    out_string << R"("metadata":{"confidence":)" << transcript->confidence << R"(},"words":[)";

    for (int i = 0; i < words.size(); i++)
    {
        meta_word w = words[i];
        out_string << R"({"word":")" << w.word << R"(","time":)" << w.start_time << R"(,"duration":)" << w.duration << "}";

        if (i < words.size() - 1)
        {
            out_string << ",";
        }
    }

    out_string << "]";

    return out_string.str();
}

char* MainComponent::MetadataToJSON(Metadata* result)
{
    std::ostringstream out_string;
    out_string << "{\n";

    for (int j=0; j < result->num_transcripts; ++j)
    {
        const CandidateTranscript *transcript = &result->transcripts[j];

        if (j == 0)
        {
            out_string << CandidateTranscriptToJSON(transcript);

            if (result->num_transcripts > 1)
            {
                out_string << ",\n" << R"("alternatives")" << ":[\n";
            }
        }
        else
        {
            out_string << "{" << CandidateTranscriptToJSON(transcript) << "}";
            if (j < result->num_transcripts - 1)
            {
                out_string << ",\n";
            }
            else
            {
                out_string << "\n]";
            }
        }
    }

    out_string << "\n}\n";

    return strdup(out_string.str().c_str());
}

ds_result MainComponent::LocalDsSTT(ModelState* aCtx, const short* aBuffer, size_t aBufferSize, bool extended_output, bool json_output)
{
    ds_result res = {0};

    clock_t ds_start_time = clock();

    // sphinx-doc: c_ref_inference_start
    if (extended_output)
    {
        Metadata *result = DS_SpeechToTextWithMetadata(aCtx, aBuffer, (unsigned int)aBufferSize, 1);
        res.string = CandidateTranscriptToString(&result->transcripts[0]);
        DS_FreeMetadata(result);
    }
    else if (json_output)
    {
        Metadata *result = DS_SpeechToTextWithMetadata(aCtx, aBuffer, (unsigned int)aBufferSize, json_candidate_transcripts);
        res.string = MetadataToJSON(result);
        DS_FreeMetadata(result);
    }
    else if (stream_size > 0)
    {
        StreamingState* ctx;
        int status = DS_CreateStream(aCtx, &ctx);

        if (status != DS_ERR_OK)
        {
            res.string = strdup("");
            return res;
        }

        size_t off = 0;
        const char *last = nullptr;
        const char *prev = nullptr;

        while (off < aBufferSize)
        {
            size_t cur = aBufferSize - off > stream_size ? stream_size : aBufferSize - off;
            DS_FeedAudioContent(ctx, aBuffer + off, (unsigned int)cur);
            off += cur;
            prev = last;
            const char* partial = DS_IntermediateDecode(ctx);

            if (last == nullptr || strcmp(last, partial))
            {
                printf("%s\n", partial);
                last = partial;
            }
            else
            {
                DS_FreeString((char *) partial);
            }

            if (prev != nullptr && prev != last)
            {
                DS_FreeString((char *) prev);
            }
        }

        if (last != nullptr)
        {
            DS_FreeString((char *) last);
        }

        res.string = DS_FinishStream(ctx);
    }
    else if (extended_stream_size > 0)
    {
        StreamingState* ctx;
        int status = DS_CreateStream(aCtx, &ctx);

        if (status != DS_ERR_OK)
        {
            res.string = strdup("");
            return res;
        }

        size_t off = 0;
        const char *last = nullptr;
        const char *prev = nullptr;

        while (off < aBufferSize)
        {
            size_t cur = aBufferSize - off > extended_stream_size ? extended_stream_size : aBufferSize - off;
            DS_FeedAudioContent(ctx, aBuffer + off, (unsigned int)cur);
            off += cur;
            prev = last;
            const Metadata* result = DS_IntermediateDecodeWithMetadata(ctx, 1);
            const char* partial = CandidateTranscriptToString(&result->transcripts[0]);

            if (last == nullptr || strcmp(last, partial))
            {
                printf("%s\n", partial);
                last = partial;
            }
            else
            {
                free((char *) partial);
            }

            if (prev != nullptr && prev != last)
            {
                free((char *) prev);
            }
            DS_FreeMetadata((Metadata *)result);
        }

        const Metadata* result = DS_FinishStreamWithMetadata(ctx, 1);
        res.string = CandidateTranscriptToString(&result->transcripts[0]);
        DS_FreeMetadata((Metadata *)result);
        free((char *) last);
    }
    else
    {
        res.string = DS_SpeechToText(aCtx, aBuffer, (unsigned int)aBufferSize);
    }
    // sphinx-doc: c_ref_inference_stop
    clock_t ds_end_infer = clock();

    res.cpu_time_overall = ((double) (ds_end_infer - ds_start_time)) / CLOCKS_PER_SEC;

    return res;
}

std::vector<std::string> MainComponent::SplitStringOnDelim(std::string in_string, std::string delim)
{
    std::vector<std::string> out_vector;
    char * tmp_str = new char[in_string.size() + 1];
    std::copy(in_string.begin(), in_string.end(), tmp_str);
    tmp_str[in_string.size()] = '\0';
    const char* token = strtok(tmp_str, delim.c_str());

    while( token != NULL )
    {
        out_vector.push_back(token);
        token = strtok(NULL, delim.c_str());
    }

    delete[] tmp_str;
    return out_vector;
}

} // namespace GuiApp