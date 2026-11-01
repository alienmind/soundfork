#pragma once

#include <array>

// Rough models of how a kind of playback device limits a mix. The values are starting points
// from general knowledge of these devices, not measurements of any product.
struct DeviceProfile
{
    enum class FilterType { none, highPass, lowPass, peak, lowShelf };

    struct Filter
    {
        FilterType type;
        float hz, q, gainDb;
    };

    const char* name;
    float width; // 0 is mono, 1 leaves the stereo image alone
    std::array<Filter, 4> filters;
    float compThresholdDb, compRatio; // a ratio of 1 turns the compressor off
    float rightDelayMs, rightGainDb;  // a listener sitting closer to the left speaker
    float trimDb;                     // level match, see the comment on kDeviceProfiles
};

// trimDb brings each profile to the RMS of its input, measured at 48 kHz with pink noise at
// -14 dBFS RMS, identical in both channels. RMS is not loudness, and a mix at another level or
// with wider stereo lands elsewhere, so this removes most of the level difference, not all of it.
inline constexpr std::array<DeviceProfile, 6> kDeviceProfiles {{
    { "Flat (no processing)", 1.0f, {}, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },

    { "Mono check", 0.0f, {}, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },

    { "Phone speaker", 0.0f,
      {{ { DeviceProfile::FilterType::highPass, 400.0f, 0.54f, 0.0f },
         { DeviceProfile::FilterType::highPass, 400.0f, 1.31f, 0.0f },
         { DeviceProfile::FilterType::peak, 3000.0f, 1.0f, 5.0f },
         { DeviceProfile::FilterType::lowPass, 14000.0f, 0.71f, 0.0f } }},
      -18.0f, 6.0f, 0.0f, 0.0f, 7.2f },

    { "Laptop speakers", 0.4f,
      {{ { DeviceProfile::FilterType::highPass, 200.0f, 0.54f, 0.0f },
         { DeviceProfile::FilterType::highPass, 200.0f, 1.31f, 0.0f },
         { DeviceProfile::FilterType::peak, 2500.0f, 1.0f, 4.0f },
         { DeviceProfile::FilterType::lowPass, 16000.0f, 0.71f, 0.0f } }},
      -14.0f, 4.0f, 0.0f, 0.0f, 3.6f },

    { "Stock earbuds", 1.0f,
      {{ { DeviceProfile::FilterType::lowShelf, 100.0f, 0.71f, -6.0f },
         { DeviceProfile::FilterType::peak, 3000.0f, 1.5f, 5.0f } }},
      0.0f, 1.0f, 0.0f, 0.0f, 0.6f },

    { "Car", 1.0f,
      {{ { DeviceProfile::FilterType::lowShelf, 60.0f, 0.71f, 6.0f },
         { DeviceProfile::FilterType::peak, 350.0f, 1.0f, -3.0f } }},
      -20.0f, 2.5f, 2.0f, -2.0f, 5.9f },
}};

// Applies a profile on the secondary device's thread, after the FIFO, so it can only ever cost
// that device a dropout. Everything is preallocated, and Flat leaves the samples untouched.
class DeviceProfileChain
{
public:
    void prepare(double newSampleRate, int profileIndex);

    // right may be null for a one-channel device. A change of profile fades out and back in,
    // because the filter states of the old profile would click in the new one.
    void process(int requestedProfile, float* left, float* right, int numSamples);

private:
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1[2] {}, z2[2] {};

        void set(const DeviceProfile::Filter& filter, double sampleRate);
        float process(float input, int channel);
    };

    void load(int profileIndex);

    static constexpr int kDelaySize = 1024; // over 5 ms at 192 kHz

    double sampleRate = 44100.0;
    int activeProfile = 0;
    float fade = 1.0f, fadeStep = 0.001f;

    std::array<Biquad, 4> biquads;
    float envelope = 0.0f, attack = 0.0f, release = 0.0f;
    std::array<float, kDelaySize> delayLine {};
    int delayWrite = 0, delaySamples = 0;
    float rightGain = 1.0f, trim = 1.0f;
};
