#include "DeviceProfile.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kFadeSeconds = 0.01;
    constexpr double kAttackSeconds = 0.005, kReleaseSeconds = 0.1;

    float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
}

// Coefficients from the RBJ Audio EQ Cookbook. juce::dsp::IIR::Coefficients allocates, and this
// runs on the device callback when the profile changes.
void DeviceProfileChain::Biquad::set(const DeviceProfile::Filter& filter, double sampleRate)
{
    *this = {};

    if (filter.type == DeviceProfile::FilterType::none)
        return;

    const auto w0 = 2.0 * kPi * std::min((double) filter.hz, sampleRate * 0.45) / sampleRate;
    const auto cosW0 = std::cos(w0);
    const auto alpha = std::sin(w0) / (2.0 * filter.q);
    const auto A = std::pow(10.0, filter.gainDb / 40.0);

    double c0 = 1.0, c1 = 0.0, c2 = 0.0, d0 = 1.0, d1 = 0.0, d2 = 0.0;

    switch (filter.type)
    {
        case DeviceProfile::FilterType::highPass:
            c0 = (1.0 + cosW0) / 2.0; c1 = -(1.0 + cosW0); c2 = c0;
            d0 = 1.0 + alpha; d1 = -2.0 * cosW0; d2 = 1.0 - alpha;
            break;

        case DeviceProfile::FilterType::lowPass:
            c0 = (1.0 - cosW0) / 2.0; c1 = 1.0 - cosW0; c2 = c0;
            d0 = 1.0 + alpha; d1 = -2.0 * cosW0; d2 = 1.0 - alpha;
            break;

        case DeviceProfile::FilterType::peak:
            c0 = 1.0 + alpha * A; c1 = -2.0 * cosW0; c2 = 1.0 - alpha * A;
            d0 = 1.0 + alpha / A; d1 = -2.0 * cosW0; d2 = 1.0 - alpha / A;
            break;

        case DeviceProfile::FilterType::lowShelf:
        {
            const auto rootA = 2.0 * std::sqrt(A) * alpha;
            c0 = A * ((A + 1.0) - (A - 1.0) * cosW0 + rootA);
            c1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
            c2 = A * ((A + 1.0) - (A - 1.0) * cosW0 - rootA);
            d0 = (A + 1.0) + (A - 1.0) * cosW0 + rootA;
            d1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW0);
            d2 = (A + 1.0) + (A - 1.0) * cosW0 - rootA;
            break;
        }

        case DeviceProfile::FilterType::none:
            break;
    }

    b0 = c0 / d0; b1 = c1 / d0; b2 = c2 / d0; a1 = d1 / d0; a2 = d2 / d0;
}

float DeviceProfileChain::Biquad::process(float input, int channel)
{
    const auto output = b0 * input + z1[channel];
    z1[channel] = b1 * input - a1 * output + z2[channel];
    z2[channel] = b2 * input - a2 * output;
    return (float) output;
}

void DeviceProfileChain::prepare(double newSampleRate, int profileIndex)
{
    sampleRate = newSampleRate;
    fade = 1.0f;
    fadeStep = (float) (1.0 / (kFadeSeconds * sampleRate));
    attack = (float) std::exp(-1.0 / (kAttackSeconds * sampleRate));
    release = (float) std::exp(-1.0 / (kReleaseSeconds * sampleRate));
    load(profileIndex);
}

void DeviceProfileChain::load(int profileIndex)
{
    activeProfile = profileIndex;
    const auto& profile = kDeviceProfiles[(size_t) profileIndex];

    for (size_t i = 0; i < biquads.size(); ++i)
        biquads[i].set(profile.filters[i], sampleRate);

    envelope = 0.0f;
    delayLine.fill(0.0f);
    delayWrite = 0;
    delaySamples = std::min((int) std::lround(profile.rightDelayMs * 0.001 * sampleRate), kDelaySize - 1);
    rightGain = dbToGain(profile.rightGainDb);
    trim = dbToGain(profile.trimDb);
}

void DeviceProfileChain::process(int requestedProfile, float* left, float* right, int numSamples)
{
    if (activeProfile == 0 && requestedProfile == 0 && fade >= 1.0f)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        if (requestedProfile != activeProfile)
        {
            fade = std::max(0.0f, fade - fadeStep);
            if (fade <= 0.0f)
                load(requestedProfile);
        }
        else
        {
            fade = std::min(1.0f, fade + fadeStep);
        }

        // Read each time: a profile loaded above changes every value used below.
        const auto& profile = kDeviceProfiles[(size_t) activeProfile];

        auto l = left[i];
        auto r = right != nullptr ? right[i] : l;

        if (activeProfile != 0)
        {
            const auto mid = (l + r) * 0.5f;
            const auto side = (l - r) * 0.5f * profile.width;
            l = mid + side;
            r = mid - side;

            for (auto& biquad : biquads)
            {
                l = biquad.process(l, 0);
                r = biquad.process(r, 1);
            }

            if (profile.compRatio > 1.0f)
            {
                const auto level = std::max(std::abs(l), std::abs(r));
                const auto coefficient = level > envelope ? attack : release;
                envelope = level + coefficient * (envelope - level);

                const auto overDb = 20.0f * std::log10(std::max(envelope, 1.0e-6f)) - profile.compThresholdDb;
                if (overDb > 0.0f)
                {
                    const auto gain = dbToGain(-overDb * (1.0f - 1.0f / profile.compRatio));
                    l *= gain;
                    r *= gain;
                }
            }

            if (delaySamples > 0)
            {
                delayLine[(size_t) delayWrite] = r;
                r = delayLine[(size_t) ((delayWrite + kDelaySize - delaySamples) % kDelaySize)];
                delayWrite = (delayWrite + 1) % kDelaySize;
            }

            l = std::clamp(l * trim, -1.0f, 1.0f);
            r = std::clamp(r * trim * rightGain, -1.0f, 1.0f);
        }

        left[i] = l * fade;
        if (right != nullptr)
            right[i] = r * fade;
    }
}
