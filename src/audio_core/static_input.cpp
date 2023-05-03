// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "audio_core/input.h"
#include "audio_core/static_input.h"

namespace AudioCore {

constexpr std::array<u8, 16> NOISE_SAMPLE_8_BIT = {0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                   0xFF, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x8E, 0xFF};

constexpr std::array<u8, 32> NOISE_SAMPLE_16_BIT = {
    0x64, 0x61, 0x74, 0x61, 0x56, 0xD7, 0x00, 0x00, 0x48, 0xF7, 0x86, 0x05, 0x77, 0x1A, 0xF4, 0x1F,
    0x28, 0x0F, 0x6B, 0xEB, 0x1C, 0xC0, 0xCB, 0x9D, 0x46, 0x90, 0xDF, 0x98, 0xEA, 0xAE, 0xB5, 0xC4};

StaticInput::StaticInput()
    : CACHE_8_BIT{NOISE_SAMPLE_8_BIT.begin(), NOISE_SAMPLE_8_BIT.end()},
      CACHE_16_BIT{NOISE_SAMPLE_16_BIT.begin(), NOISE_SAMPLE_16_BIT.end()} {}

StaticInput::~StaticInput() = default;

void StaticInput::StartSampling(const InputParameters& params) {
    sample_rate = params.sample_rate;
    sample_size = params.sample_size;

    parameters = params;
    is_sampling = true;
}

void StaticInput::StopSampling() {
    is_sampling = false;
}

void StaticInput::AdjustSampleRate(u32 sample_rate) {}

Samples StaticInput::Read() {
    return (sample_size == 8) ? CACHE_8_BIT : CACHE_16_BIT;
}

} // namespace AudioCore
