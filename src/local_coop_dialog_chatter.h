#ifndef LOCAL_COOP_DIALOG_CHATTER_H
#define LOCAL_COOP_DIALOG_CHATTER_H

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "object.h"

namespace fallout {

// COOP_DOS_DIALOG_CHATTER_V1
// Quiet, non-verbal DOS-style chatter. This does not speak dialogue text and
// does not replace any Fallout portrait/window/lipsync asset. It only adds a
// tiny rounded mid/low synthetic pulse while NPC/creature text appears.
inline SDL_AudioDeviceID gLocalCoopChatterDevice = 0;
inline SDL_AudioSpec gLocalCoopChatterSpec {};

inline std::string localCoopChatterLowerName(Object* speaker)
{
    const char* raw = speaker != nullptr ? objectGetName(speaker) : nullptr;
    std::string name = raw != nullptr ? raw : "";
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name;
}

inline void localCoopChatterProfile(Object* speaker, double& frequency, double& roughness, int& pulseMs, int& gapMs)
{
    std::string name = localCoopChatterLowerName(speaker);
    frequency = 210.0;
    roughness = 0.06;
    pulseMs = 48;
    gapMs = 70;

    if (name.find("robot") != std::string::npos || name.find("bot") != std::string::npos) {
        frequency = 285.0; roughness = 0.02; pulseMs = 42; gapMs = 62;
    } else if (name.find("deathclaw") != std::string::npos) {
        frequency = 145.0; roughness = 0.11; pulseMs = 58; gapMs = 82;
    } else if (name.find("mutant") != std::string::npos) {
        frequency = 165.0; roughness = 0.10; pulseMs = 56; gapMs = 78;
    } else if (name.find("rat") != std::string::npos || name.find("dog") != std::string::npos
        || name.find("wolf") != std::string::npos) {
        frequency = 235.0; roughness = 0.08; pulseMs = 40; gapMs = 72;
    } else if (name.find("scorpion") != std::string::npos || name.find("mantis") != std::string::npos
        || name.find("ant") != std::string::npos || name.find("roach") != std::string::npos) {
        frequency = 305.0; roughness = 0.09; pulseMs = 34; gapMs = 76;
    } else if (name.find("gecko") != std::string::npos || name.find("lizard") != std::string::npos) {
        frequency = 255.0; roughness = 0.08; pulseMs = 38; gapMs = 70;
    } else if (name.find("raider") != std::string::npos || name.find("bandit") != std::string::npos) {
        frequency = 185.0; roughness = 0.12; pulseMs = 50; gapMs = 66;
    } else if (name.find("scientist") != std::string::npos || name.find("doctor") != std::string::npos) {
        frequency = 250.0; roughness = 0.035; pulseMs = 42; gapMs = 68;
    } else if (name.find("merchant") != std::string::npos || name.find("trader") != std::string::npos) {
        frequency = 220.0; roughness = 0.045; pulseMs = 44; gapMs = 66;
    } else if (name.find("elder") != std::string::npos) {
        frequency = 175.0; roughness = 0.04; pulseMs = 54; gapMs = 82;
    } else if (name.find("guard") != std::string::npos) {
        frequency = 200.0; roughness = 0.05; pulseMs = 46; gapMs = 68;
    }
}

inline bool localCoopChatterEnsureAudio()
{
    if (gLocalCoopChatterDevice != 0) return true;

    SDL_AudioSpec want {};
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = nullptr;

    gLocalCoopChatterDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &gLocalCoopChatterSpec, 0);
    if (gLocalCoopChatterDevice == 0) return false;
    SDL_PauseAudioDevice(gLocalCoopChatterDevice, 0);
    return true;
}

inline void localCoopDialogChatter(Object* speaker, int textLengthHint)
{
    if (speaker == nullptr || !localCoopChatterEnsureAudio()) return;

    double frequency;
    double roughness;
    int pulseMs;
    int gapMs;
    localCoopChatterProfile(speaker, frequency, roughness, pulseMs, gapMs);

    int pulses = std::clamp(2 + textLengthHint / 28, 2, 8);
    const int sampleRate = gLocalCoopChatterSpec.freq > 0 ? gLocalCoopChatterSpec.freq : 22050;
    std::vector<int16_t> pcm;
    pcm.reserve(static_cast<size_t>(pulses) * sampleRate * (pulseMs + gapMs) / 1000);

    uint32_t noise = static_cast<uint32_t>(speaker->id * 2654435761u + textLengthHint * 97u);
    constexpr double kPi = 3.14159265358979323846;
    for (int p = 0; p < pulses; ++p) {
        double pulseFreq = frequency * (1.0 + ((p % 3) - 1) * 0.035);
        int toneSamples = sampleRate * pulseMs / 1000;
        for (int i = 0; i < toneSamples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double phase = static_cast<double>(i) / std::max(1, toneSamples - 1);
            double envelope = std::sin(kPi * phase);
            noise = noise * 1664525u + 1013904223u;
            double n = (static_cast<int>((noise >> 16) & 0xFFFF) - 32768) / 32768.0;
            double rounded = std::sin(2.0 * kPi * pulseFreq * t)
                + 0.18 * std::sin(2.0 * kPi * pulseFreq * 2.0 * t);
            double sample = envelope * (rounded * 0.72 + n * roughness);
            // Deliberately quiet: small text chatter, not a foreground sound.
            pcm.push_back(static_cast<int16_t>(std::clamp(sample * 1350.0, -2400.0, 2400.0)));
        }
        int silenceSamples = sampleRate * gapMs / 1000;
        pcm.insert(pcm.end(), silenceSamples, 0);
    }

    // New dialogue supersedes old chatter; never allow old syllables to queue up.
    SDL_ClearQueuedAudio(gLocalCoopChatterDevice);
    SDL_QueueAudio(gLocalCoopChatterDevice, pcm.data(), static_cast<Uint32>(pcm.size() * sizeof(int16_t)));
}

} // namespace fallout

#endif
