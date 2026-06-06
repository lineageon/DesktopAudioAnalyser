#include "MainComponent.h"

// ==============================================================================
MainComponent::MainComponent()
    : forwardFFT(10),
    window(1024, juce::dsp::WindowingFunction<float>::hann)
{
    // 1. Configure GUI Elements
    gainSlider.setSliderStyle(juce::Slider::LinearVertical);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    gainSlider.setRange(0.0, 4.0, 0.01);
    gainSlider.setValue(1.0);
    gainSlider.onValueChange = [this] { currentGain = (float)gainSlider.getValue(); };
    addAndMakeVisible(gainSlider);

    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.attachToComponent(&gainSlider, false);
    gainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gainLabel);

    // --- NEW: SETUP SETTINGS BUTTON ---
    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this] {
        deviceSelector->setVisible(true);
        closeButton.setVisible(true);
        };

    // --- NEW: SETUP DEVICE SELECTOR (Hidden by default) ---
    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager, 0, 2, 0, 2, true, true, true, false);
    addChildComponent(deviceSelector.get());

    // --- NEW: SETUP RED CLOSE BUTTON (Hidden by default) ---
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addChildComponent(closeButton);
    closeButton.onClick = [this] {
        deviceSelector->setVisible(false);
        closeButton.setVisible(false);
        };

    waveBuffer.resize(512, 0.0f);
    setSize(800, 600);

    // 2. Programmatic Audio Device Setup 
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    juce::String targetDeviceType = "ASIO";
    setup.inputDeviceName = "";
    setup.outputDeviceName = "";

    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    setup.inputChannels.clear();
    setup.inputChannels.setBit(0);
    setup.inputChannels.setBit(1);
    setup.outputChannels.clear();
    setup.outputChannels.setBit(0);
    setup.outputChannels.setBit(1);

    setup.sampleRate = 48000.0;
    setup.bufferSize = 256;

    juce::String initializationError = deviceManager.initialise(2, 2, nullptr, true, "", &setup);

    if (initializationError.isNotEmpty())
    {
        juce::Logger::writeToLog("ASIO Setup Failed: " + initializationError);
    }

    // CRITICAL FIX: This links the active device manager to your getNextAudioBlock!
    setAudioChannels(2, 2);

    // 3. Start UI Timer (~30 FPS)
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

// ==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected, sampleRate);
    std::fill(fifo, fifo + 1024, 0.0f);
    std::fill(fftData, fftData + 2048, 0.0f);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Safety check
    int numChannels = bufferToFill.buffer->getNumChannels();
    if (numChannels == 0) return;

    float localPeak = 0.0f;

    // Get pointers to both the Left (Input 1) and Right (Input 2) channels
    auto* leftChannel = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* rightChannel = (numChannels > 1) ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample) : nullptr;

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        // 1. Grab the left channel sample
        float monoSample = leftChannel[i];

        // 2. If the right channel exists, add it to create a summed mono mix
        if (rightChannel != nullptr)
        {
            monoSample += rightChannel[i];

            // Halve the volume so summing two loud inputs doesn't cause digital clipping
            monoSample *= 0.5f;
        }

        // 3. Apply your software Gain slider
        monoSample *= currentGain;

        // 4. Send the centered mono mix BACK out to both Left and Right speakers
        leftChannel[i] = monoSample;
        if (rightChannel != nullptr)
        {
            rightChannel[i] = monoSample;
        }

        // 5. Send the signal to the Peak Meter
        localPeak = juce::jmax(localPeak, std::abs(monoSample));

        // 6. Send the signal to the FFT Spectrum Analyzer
        pushNextSampleIntoFifo(monoSample);

        // 7. Send the signal to the Green Waveform
        waveBuffer[waveIndex] = monoSample;
        waveIndex = (waveIndex + 1) % (int)waveBuffer.size();
    }

    // Update atomic variable for the UI thread
    peakValue.store(localPeak);
}

void MainComponent::releaseResources() {}

// ==============================================================================
void MainComponent::pushNextSampleIntoFifo(float sample)
{
    if (fifoIndex == 1024)
    {
        if (!nextFFTBlockReady)
        {
            juce::zeromem(fftData, sizeof(fftData));
            memcpy(fftData, fifo, sizeof(fifo));
            nextFFTBlockReady = true;
        }
        fifoIndex = 0;
    }
    fifo[fifoIndex++] = sample;
}

void MainComponent::timerCallback()
{
    if (nextFFTBlockReady)
    {
        drawNextFrameOfSpectrum();
        nextFFTBlockReady = false;
    }

    // Smooth the peak meter (0.9f controls how fast it falls. Lower = faster drop)
    smoothedPeak = juce::jmax(peakValue.load(), smoothedPeak * 0.9f);

    // Trigger paint()
    repaint();
}

void MainComponent::drawNextFrameOfSpectrum()
{
    window.multiplyWithWindowingTable(fftData, 1024);
    forwardFFT.performFrequencyOnlyForwardTransform(fftData);

    auto mindB = -100.0f;
    auto maxdB = 0.0f;

    for (int i = 0; i < 512; ++i)
    {
        auto level = juce::Decibels::gainToDecibels(fftData[i]) - juce::Decibels::gainToDecibels((float)1024);
        fftData[i] = juce::jmap(level, mindB, maxdB, 0.0f, 1.0f);
    }
}

// ==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    // --- DIM BACKGROUND IF SETTINGS ARE OPEN ---
    if (deviceSelector->isVisible())
    {
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRect(getLocalBounds());
    }

    auto bounds = getLocalBounds();
    auto leftPanel = bounds.removeFromLeft(120);
    auto analyzerBounds = bounds.reduced(10);

    // --- 1. DRAW FFT SPECTRUM ---
    g.setColour(juce::Colours::cyan.withAlpha(0.7f));
    auto spectrumRect = analyzerBounds.removeFromBottom(analyzerBounds.getHeight() / 2);

    juce::Path spectrumPath;
    for (int i = 0; i < 512; ++i)
    {
        auto x = spectrumRect.getX() + spectrumRect.getWidth() * ((float)i / 512.0f);
        auto y = spectrumRect.getBottom() - spectrumRect.getHeight() * fftData[i];

        if (i == 0) spectrumPath.startNewSubPath(x, spectrumRect.getBottom());
        spectrumPath.lineTo(x, y);
    }
    spectrumPath.lineTo(spectrumRect.getRight(), spectrumRect.getBottom());
    g.fillPath(spectrumPath);

    // --- 2. DRAW WAVEFORM (Top Half) ---
    g.setColour(juce::Colours::lime);
    auto waveRect = analyzerBounds;
    juce::Path wavePath;

    // Visual multiplier (Change this from 4.0f to whatever looks best to you)
    float visualWaveformBoost = 4.0f;

    for (size_t i = 0; i < waveBuffer.size(); ++i)
    {
        auto x = waveRect.getX() + waveRect.getWidth() * ((float)i / waveBuffer.size());
        auto y = waveRect.getCentreY() - (waveBuffer[i] * visualWaveformBoost * waveRect.getHeight() / 2.0f);

        if (i == 0) wavePath.startNewSubPath(x, y);
        else wavePath.lineTo(x, y);
    }
    g.strokePath(wavePath, juce::PathStrokeType(2.0f));

    // --- 3. DRAW PEAK METER ---
    g.setColour(juce::Colours::darkgrey);
    juce::Rectangle<int> peakBackground(leftPanel.getRight() - 40, 40, 20, getHeight() - 80);
    g.fillRect(peakBackground);

    g.setColour(juce::Colours::red);
    // Use smoothedPeak instead of the raw peakValue
    int peakHeight = (int)(peakBackground.getHeight() * smoothedPeak);
    juce::Rectangle<int> peakForeground(peakBackground.getX(), peakBackground.getBottom() - peakHeight, 20, peakHeight);
    g.fillRect(peakForeground);
}

void MainComponent::resized()
{
    // Position Settings Button top left
    settingsButton.setBounds(20, 10, 80, 25);

    // Position the Gain Slider slightly lower now
    gainSlider.setBounds(20, 60, 60, getHeight() - 100);

    // Position the popup settings panel dead center
    if (deviceSelector != nullptr)
    {
        auto panelBounds = getLocalBounds().withSizeKeepingCentre(500, 400);
        deviceSelector->setBounds(panelBounds);

        // Pin the Red X close button to the top right corner of the settings panel
        closeButton.setBounds(panelBounds.getRight() - 30, panelBounds.getY(), 30, 30);
    }
}