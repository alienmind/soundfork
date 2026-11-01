#include "SecondaryOutputDevice.h"

SecondaryOutputDevice::SecondaryOutputDevice() = default;

SecondaryOutputDevice::~SecondaryOutputDevice()
{
    reset();
}

void SecondaryOutputDevice::prepare(double hostSampleRate, int numChannels, int /*hostBlockSize*/)
{
    // The device thread reads ringBuffer, so it must stop before the resize.
    reset();

    sampleRate = hostSampleRate;
    hostNumChannels = numChannels;

    // Capacity covers a couple of seconds so a slow-starting secondary
    // device never forces us to drop the very first blocks it misses.
    const int capacity = (int) (sampleRate * 2.0);
    ringBuffer.setSize(hostNumChannels, capacity);
    ringBuffer.clear();
    fifo.setTotalSize(capacity);

    if (enabled)
        openCurrentDevice();
}

void SecondaryOutputDevice::reset()
{
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    deviceRunning = false;
}

void SecondaryOutputDevice::pushBlock(const juce::AudioBuffer<float>& block)
{
    // Nothing is consuming until the device runs; queueing now would only
    // add stale audio that the device then plays late.
    if (!enabled || !deviceRunning)
        return;

    const int numSamples = block.getNumSamples();

    // Hosts keep calling processBlock while stopped, so a block arriving does
    // not mean music is. -80 dB is below any real signal, above denormal noise.
    if (block.getMagnitude(0, numSamples) > 1.0e-4f)
        ++signalBlockCount;

    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    if (size1 + size2 < numSamples)
    {
        // Consumer can't keep up; drop this block rather than risk
        // ever blocking the host's real-time thread.
        ++underrunCount;
        fifo.finishedWrite(0);
        return;
    }

    for (int ch = 0; ch < ringBuffer.getNumChannels(); ++ch)
    {
        const auto* src = block.getReadPointer(juce::jmin(ch, block.getNumChannels() - 1));
        auto* dst = ringBuffer.getWritePointer(ch);

        if (size1 > 0) juce::FloatVectorOperations::copy(dst + start1, src, size1);
        if (size2 > 0) juce::FloatVectorOperations::copy(dst + start2, src + size1, size2);
    }

    fifo.finishedWrite(size1 + size2);
}

juce::StringArray SecondaryOutputDevice::getAvailableOutputDeviceNames()
{
    selectOutputDeviceType();

    juce::StringArray names;
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        names = type->getDeviceNames(false); // false = output devices
    }
    return names;
}

void SecondaryOutputDevice::selectOutputDeviceType()
{
    // Forces JUCE to create its device types, without which opening a device
    // silently does nothing (e.g. restoring state before the editor exists).
    deviceManager.getAvailableDeviceTypes();

    // Shared-mode WASAPI, so we never take exclusive hold of a device. Other
    // types list the same device names; ignored where this type doesn't exist.
    deviceManager.setCurrentAudioDeviceType("Windows Audio", false);
}

bool SecondaryOutputDevice::hasSampleRateMismatch()
{
    auto* device = deviceManager.getCurrentAudioDevice();
    return device != nullptr && device->getCurrentSampleRate() != sampleRate;
}

juce::String SecondaryOutputDevice::getStatusText()
{
    if (lastOpenError.isNotEmpty())
        return "Open failed: " + lastOpenError;

    auto text = "Dropped blocks: " + juce::String(underrunCount.load());

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        text << " | device " << device->getCurrentSampleRate() << " Hz, host " << sampleRate << " Hz";

        if (hasSampleRateMismatch())
            text << "\nWARNING: sample rates must match. Set the project or the device to the same "
                 << "rate, or the secondary device plays at the wrong pitch.";
    }

    return text;
}

void SecondaryOutputDevice::setOutputDevice(const juce::String& deviceName)
{
    currentDeviceName = deviceName;
    if (enabled)
        openCurrentDevice();
}

void SecondaryOutputDevice::setLatencyBufferSize(int samples)
{
    latencyBufferSizeSamples = samples;
    if (enabled)
        openCurrentDevice();
}

void SecondaryOutputDevice::setEnabled(bool shouldBeEnabled)
{
    enabled = shouldBeEnabled;
    if (enabled)
        openCurrentDevice();
    else
        reset();
}

void SecondaryOutputDevice::setProfile(int profileIndex)
{
    profile = juce::jlimit(0, (int) kDeviceProfiles.size() - 1, profileIndex);
}

void SecondaryOutputDevice::openCurrentDevice()
{
    if (currentDeviceName.isEmpty())
        return;

    reset();
    selectOutputDeviceType();

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    setup.outputDeviceName = currentDeviceName;
    setup.useDefaultOutputChannels = true;
    setup.inputDeviceName = {};
    setup.useDefaultInputChannels = false;
    setup.sampleRate = sampleRate;
    setup.bufferSize = latencyBufferSizeSamples;

    lastOpenError = deviceManager.setAudioDeviceSetup(setup, true);
    if (lastOpenError.isNotEmpty())
    {
        DBG("SecondaryOutputDevice: failed to open '" << currentDeviceName << "': " << lastOpenError);
        return;
    }

    deviceManager.addAudioCallback(this);
}

void SecondaryOutputDevice::audioDeviceIOCallbackWithContext(const float* const*,
                                                               int,
                                                               float* const* outputChannelData,
                                                               int numOutputChannels,
                                                               int numSamples,
                                                               const juce::AudioIODeviceCallbackContext&)
{
    // Not prepared yet: the device can be opened from restored state first.
    if (ringBuffer.getNumChannels() == 0)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        return;
    }

    int start1, size1, start2, size2;
    fifo.prepareToRead(numSamples, start1, size1, start2, size2);
    const int available = size1 + size2;

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        auto* out = outputChannelData[ch];
        if (out == nullptr)
            continue;

        const auto* src = ringBuffer.getReadPointer(juce::jmin(ch, ringBuffer.getNumChannels() - 1));

        if (size1 > 0) juce::FloatVectorOperations::copy(out, src + start1, size1);
        if (size2 > 0) juce::FloatVectorOperations::copy(out + size1, src + start2, size2);
        if (available < numSamples)
            juce::FloatVectorOperations::clear(out + available, numSamples - available);
    }

    fifo.finishedRead(available);

    if (numOutputChannels > 0 && outputChannelData[0] != nullptr)
    {
        juce::ScopedNoDenormals noDenormals;
        auto* right = numOutputChannels > 1 ? outputChannelData[1] : nullptr;
        profileChain.process(profile.load(), outputChannelData[0], right, numSamples);

        // Any further channels carry the last host channel, so they follow the processed right.
        for (int ch = 2; right != nullptr && ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::copy(outputChannelData[ch], right, numSamples);
    }
}

void SecondaryOutputDevice::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    // The filters are tuned to the rate the device really runs at, which may not be the host's.
    profileChain.prepare(device->getCurrentSampleRate(), profile.load());

    // Discard anything left from the last run so playback starts current.
    fifo.finishedRead(fifo.getNumReady());
    deviceRunning = true;
}

void SecondaryOutputDevice::audioDeviceStopped()
{
    deviceRunning = false;
}

juce::ValueTree SecondaryOutputDevice::getState() const
{
    juce::ValueTree tree("SECONDARY_OUTPUT");
    tree.setProperty("deviceName", currentDeviceName, nullptr);
    tree.setProperty("bufferSize", latencyBufferSizeSamples, nullptr);
    tree.setProperty("enabled", enabled.load(), nullptr);
    // By name, so saved sets survive a reordering of the profile list.
    tree.setProperty("profile", juce::String(kDeviceProfiles[(size_t) profile.load()].name), nullptr);
    return tree;
}

void SecondaryOutputDevice::setState(const juce::ValueTree& tree)
{
    if (!tree.isValid())
        return;

    currentDeviceName = tree.getProperty("deviceName", "");
    latencyBufferSizeSamples = tree.getProperty("bufferSize", 1024);

    const auto profileName = tree.getProperty("profile", "").toString();
    profile = 0;
    for (size_t i = 0; i < kDeviceProfiles.size(); ++i)
        if (profileName == kDeviceProfiles[i].name)
            profile = (int) i;

    setEnabled((bool) tree.getProperty("enabled", false));
}
