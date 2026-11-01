#include "PluginProcessor.h"
#include "PluginEditor.h"

SoundForkAudioProcessor::SoundForkAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

SoundForkAudioProcessor::~SoundForkAudioProcessor() = default;

void SoundForkAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    secondaryOutput.prepare(sampleRate, getMainBusNumInputChannels(), samplesPerBlock);
}

void SoundForkAudioProcessor::releaseResources()
{
    secondaryOutput.reset();
}

bool SoundForkAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && !layouts.getMainInputChannelSet().isDisabled();
}

void SoundForkAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // The signal passes through completely untouched; we only mirror it out.
    secondaryOutput.pushBlock(buffer);
}

juce::AudioProcessorEditor* SoundForkAudioProcessor::createEditor()
{
    return new SoundForkAudioProcessorEditor(*this);
}

void SoundForkAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = secondaryOutput.getState();
    juce::MemoryOutputStream stream(destData, true);
    state.writeToStream(stream);
}

void SoundForkAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, (size_t) sizeInBytes);
    if (tree.isValid())
        secondaryOutput.setState(tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoundForkAudioProcessor();
}
