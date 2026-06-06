#pragma once

#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent,
    public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // Audio Lifecycle
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // UI Lifecycle
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void pushNextSampleIntoFifo(float sample);
    void drawNextFrameOfSpectrum();

    // ==============================================================================
    // GUI Controls
    juce::Slider gainSlider;
    juce::Label gainLabel;

    // NEW: Settings UI Elements
    juce::TextButton settingsButton{ "Settings" };
    juce::TextButton closeButton{ "X" };
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    // ==============================================================================
    // DSP & Audio Variables
    float currentGain = 1.0f;
    std::atomic<float> peakValue{ 0.0f };

    // FFT Variables
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fifo[1024];
    float fftData[2048];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    // Waveform Buffer
    std::vector<float> waveBuffer;
    int waveIndex = 0;

    // Add this right under std::atomic<float> peakValue { 0.0f };
    float smoothedPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};