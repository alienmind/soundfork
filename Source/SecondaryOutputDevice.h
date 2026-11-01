#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

// Owns a second, independently-buffered output stream so a stalling or
// disconnected secondary device (e.g. Bluetooth) can never block the
// host's real-time audio thread. Audio crosses from the host thread to
// this device's own thread through a lock-free ring buffer; if the
// consumer falls behind, we drop samples rather than wait.
class SecondaryOutputDevice : public juce::AudioIODeviceCallback
{
public:
    SecondaryOutputDevice();
    ~SecondaryOutputDevice() override;

    void prepare(double hostSampleRate, int numChannels, int hostBlockSize);
    void reset();

    void pushBlock(const juce::AudioBuffer<float>& block);

    juce::StringArray getAvailableOutputDeviceNames();
    void setOutputDevice(const juce::String& deviceName);
    void setLatencyBufferSize(int samples);
    void setEnabled(bool shouldBeEnabled);

    juce::String getCurrentDeviceName() const { return currentDeviceName; }
    int getLatencyBufferSize() const { return latencyBufferSizeSamples; }
    bool isEnabled() const { return enabled.load(); }
    int getUnderrunCount() const { return underrunCount.load(); }
    uint32_t getSignalBlockCount() const { return signalBlockCount.load(); }
    bool hasSampleRateMismatch();
    juce::String getStatusText();

    juce::ValueTree getState() const;
    void setState(const juce::ValueTree& tree);

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    void openCurrentDevice();
    void selectOutputDeviceType();

    juce::AudioDeviceManager deviceManager;
    juce::AbstractFifo fifo { 1 };
    juce::AudioBuffer<float> ringBuffer;

    juce::String currentDeviceName;
    juce::String lastOpenError;
    int latencyBufferSizeSamples = 1024;
    int hostNumChannels = 2;
    double sampleRate = 44100.0;
    std::atomic<bool> enabled { false };
    std::atomic<bool> deviceRunning { false };

    std::atomic<int> underrunCount { 0 };
    std::atomic<uint32_t> signalBlockCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SecondaryOutputDevice)
};
