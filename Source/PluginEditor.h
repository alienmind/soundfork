#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SoundForkMainTab : public juce::Component,
                         private juce::Timer
{
public:
    explicit SoundForkMainTab(SoundForkAudioProcessor&);
    ~SoundForkMainTab() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class ForkState { disabled, stopped, playing };

    void timerCallback() override;
    void refreshDeviceList();
    void updateState();

    SoundForkAudioProcessor& processor;

    juce::Label titleLabel;
    juce::ToggleButton enabledToggle { "Fork to secondary device" };
    juce::ComboBox deviceBox;
    juce::ComboBox bufferSizeBox;
    juce::Label statusLabel;

    juce::Rectangle<int> imageArea;
    ForkState state = ForkState::disabled;
    uint32_t lastSignalBlockCount = 0;
    int silentTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundForkMainTab)
};

class SoundForkAboutTab : public juce::Component
{
public:
    SoundForkAboutTab();

    void resized() override;

private:
    juce::Label nameLabel;
    juce::Label copyrightLabel;
    juce::Label licenceLabel;
    juce::HyperlinkButton repoLink;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundForkAboutTab)
};

class SoundForkAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SoundForkAudioProcessorEditor(SoundForkAudioProcessor&);
    ~SoundForkAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SoundForkMainTab mainTab;
    SoundForkAboutTab aboutTab;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundForkAudioProcessorEditor)
};
