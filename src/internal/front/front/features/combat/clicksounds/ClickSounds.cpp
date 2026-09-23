#include "ClickSounds.hpp"
#include "SoundData.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#pragma comment(lib, "winmm.lib")

namespace features::combat::auto_click::clicksounds {
namespace {
struct Buffer {
    WAVEHDR header{};
    std::vector<short> samples;
};
HWAVEOUT output = nullptr;
std::array<Buffer, 4> buffers;
const unsigned char* samples = nullptr;
size_t sample_count = 0;
int selected = -1;

bool open(int index) {
    shutdown();
    selected = index;
    const auto& asset = assets[index];
    if (asset.size < 12 || std::memcmp(asset.data, "RIFF", 4) ||
        std::memcmp(asset.data + 8, "WAVE", 4)) return false;
    WAVEFORMATEX format{};
    const unsigned char* pcm = nullptr;
    size_t bytes = 0;
    for (size_t offset = 12; offset + 8 <= asset.size;) {
        unsigned long length = 0;
        std::memcpy(&length, asset.data + offset + 4, 4);
        const size_t body = offset + 8;
        if (length > asset.size - body) return false;
        if (!std::memcmp(asset.data + offset, "fmt ", 4) && length >= 16)
            std::memcpy(&format, asset.data + body, 16);
        if (!std::memcmp(asset.data + offset, "data", 4)) {
            pcm = asset.data + body;
            bytes = length;
        }
        offset = body + length + (length & 1u);
    }
    if (!pcm || !bytes || format.wFormatTag != WAVE_FORMAT_PCM ||
        format.wBitsPerSample != 16 || !format.nBlockAlign ||
        bytes % format.nBlockAlign) return false;
    if (waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        output = nullptr;
        return false;
    }
    samples = pcm;
    sample_count = bytes / 2;
    for (auto& buffer : buffers) {
        buffer.samples.resize(sample_count);
        buffer.header.lpData = reinterpret_cast<LPSTR>(buffer.samples.data());
        buffer.header.dwBufferLength = static_cast<DWORD>(bytes);
        if (waveOutPrepareHeader(output, &buffer.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            shutdown();
            selected = index; // Do not retry every click on an unavailable device.
            return false;
        }
    }
    return true;
}
}

void play(int selection, float gain) {
    if (selection < 0 || selection >= static_cast<int>(assets.size()) ||
        !std::isfinite(gain) || gain <= 0.f) return;
    if (selection != selected && !open(selection)) return;
    if (!output) return;
    gain = std::clamp(gain, 0.f, 1.f);
    for (auto& buffer : buffers) {
        if (buffer.header.dwFlags & WHDR_INQUEUE) continue;
        for (size_t i = 0; i < sample_count; ++i) {
            short original;
            std::memcpy(&original, samples + i * 2, 2);
            buffer.samples[i] = static_cast<short>(original * gain);
        }
        waveOutWrite(output, &buffer.header, sizeof(WAVEHDR));
        return;
    }
    // Four occupied voices: skip rather than queue delayed clicks.
}

void shutdown() {
    if (output) {
        waveOutReset(output);
        for (auto& buffer : buffers)
            if (buffer.header.dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(output, &buffer.header, sizeof(WAVEHDR));
        waveOutClose(output);
        output = nullptr;
    }
    for (auto& buffer : buffers) buffer = {};
    selected = -1;
    samples = nullptr;
    sample_count = 0;
}
}
