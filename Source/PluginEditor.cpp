#include "PluginEditor.h"

#if SOUNDFORK_HAS_IMAGES
 #include <BinaryData.h>
#endif

namespace
{
    constexpr int kBufferSizeOptions[] { 256, 512, 1024, 2048, 4096, 8192 };
    constexpr int kStatusTimerHz = 4;

    // Quiet passages must not make the image flicker between states.
    constexpr int kSilentTicksBeforeStopped = kStatusTimerHz;

    juce::Image loadImage(const char* resourceName)
    {
       #if SOUNDFORK_HAS_IMAGES
        int size = 0;
        if (const auto* data = BinaryData::getNamedResource(resourceName, size))
            return juce::ImageCache::getFromMemory(data, size);
       #else
        juce::ignoreUnused(resourceName);
       #endif
        return {};
    }
}

SoundForkMainTab::SoundForkMainTab(SoundForkAudioProcessor& p)
    : processor(p)
{
    titleLabel.setText("SoundFork", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    addAndMakeVisible(titleLabel);

    addAndMakeVisible(enabledToggle);
    enabledToggle.onClick = [this]
    {
        processor.secondaryOutput.setEnabled(enabledToggle.getToggleState());
    };

    addAndMakeVisible(deviceBox);
    deviceBox.onChange = [this]
    {
        processor.secondaryOutput.setOutputDevice(deviceBox.getText());
    };

    addAndMakeVisible(bufferSizeBox);
    int itemId = 1;
    for (auto size : kBufferSizeOptions)
    {
        bufferSizeBox.addItem(juce::String(size) + " samples", itemId);
        if (size == processor.secondaryOutput.getLatencyBufferSize())
            bufferSizeBox.setSelectedId(itemId, juce::dontSendNotification);
        ++itemId;
    }
    bufferSizeBox.onChange = [this]
    {
        const auto index = bufferSizeBox.getSelectedItemIndex();
        if (index >= 0)
            processor.secondaryOutput.setLatencyBufferSize(kBufferSizeOptions[index]);
    };

    // A rate warning is longer than one line at this window width.
    statusLabel.setMinimumHorizontalScale(1.0f);
    statusLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(statusLabel);

    refreshDeviceList();

    enabledToggle.setToggleState(processor.secondaryOutput.isEnabled(), juce::dontSendNotification);
    if (processor.secondaryOutput.getCurrentDeviceName().isNotEmpty())
        deviceBox.setText(processor.secondaryOutput.getCurrentDeviceName(), juce::dontSendNotification);

    startTimerHz(kStatusTimerHz);
}

SoundForkMainTab::~SoundForkMainTab()
{
    stopTimer();
}

void SoundForkMainTab::refreshDeviceList()
{
    deviceBox.clear();
    auto names = processor.secondaryOutput.getAvailableOutputDeviceNames();
    int itemId = 1;
    for (auto& name : names)
        deviceBox.addItem(name, itemId++);
}

void SoundForkMainTab::paint(juce::Graphics& g)
{
    const char* resourceName = "disabled_png";
    const char* stateName = "disabled";

    if (state == ForkState::stopped)      { resourceName = "stopped_png"; stateName = "stopped"; }
    else if (state == ForkState::playing) { resourceName = "playing_png"; stateName = "playing"; }

    const auto image = loadImage(resourceName);

    if (image.isValid())
    {
        g.drawImage(image, imageArea.toFloat(), juce::RectanglePlacement::centred);
    }
    else
    {
        g.setColour(findColour(juce::Label::textColourId).withAlpha(0.5f));
        g.drawRect(imageArea);
        g.drawText(stateName, imageArea, juce::Justification::centred);
    }
}

void SoundForkMainTab::resized()
{
    auto area = getLocalBounds().reduced(12);
    titleLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    statusLabel.setBounds(area.removeFromBottom(48));
    area.removeFromBottom(8);
    bufferSizeBox.setBounds(area.removeFromBottom(28));
    area.removeFromBottom(8);
    deviceBox.setBounds(area.removeFromBottom(28));
    area.removeFromBottom(8);
    enabledToggle.setBounds(area.removeFromBottom(24));
    area.removeFromBottom(8);

    imageArea = area;
}

void SoundForkMainTab::updateState()
{
    auto& output = processor.secondaryOutput;

    const auto signalBlockCount = output.getSignalBlockCount();
    silentTicks = signalBlockCount != lastSignalBlockCount ? 0 : silentTicks + 1;
    lastSignalBlockCount = signalBlockCount;

    const auto newState = ! output.isEnabled()                    ? ForkState::disabled
                        : silentTicks < kSilentTicksBeforeStopped ? ForkState::playing
                                                                  : ForkState::stopped;
    if (newState != state)
    {
        state = newState;
        repaint(imageArea);
    }
}

void SoundForkMainTab::timerCallback()
{
    auto& output = processor.secondaryOutput;

    updateState();

    statusLabel.setText(output.getStatusText(), juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId,
                          output.hasSampleRateMismatch() ? juce::Colours::orange
                                                         : findColour(juce::ToggleButton::textColourId));
}

SoundForkAboutTab::SoundForkAboutTab()
    : repoLink("github.com/alienmind/soundfork", juce::URL("https://github.com/alienmind/soundfork"))
{
    nameLabel.setText("SoundFork " JucePlugin_VersionString, juce::dontSendNotification);
    nameLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    nameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nameLabel);

    copyrightLabel.setText("(C) AlienMind 2026", juce::dontSendNotification);
    copyrightLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(copyrightLabel);

    addAndMakeVisible(repoLink);
}

void SoundForkAboutTab::resized()
{
    auto area = getLocalBounds().withSizeKeepingCentre(getWidth(), 84);
    nameLabel.setBounds(area.removeFromTop(28));
    copyrightLabel.setBounds(area.removeFromTop(28));
    repoLink.setBounds(area);
}

SoundForkAudioProcessorEditor::SoundForkAudioProcessorEditor(SoundForkAudioProcessor& p)
    : AudioProcessorEditor(&p), mainTab(p)
{
    const auto background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    tabs.addTab("Main", background, &mainTab, false);
    tabs.addTab("About", background, &aboutTab, false);
    addAndMakeVisible(tabs);

    setSize(420, 620);
}

SoundForkAudioProcessorEditor::~SoundForkAudioProcessorEditor() = default;

void SoundForkAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void SoundForkAudioProcessorEditor::resized()
{
    tabs.setBounds(getLocalBounds());
}
