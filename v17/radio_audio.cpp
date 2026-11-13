#include "radio_audio.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>
#include <x3daudio.h>
#endif

namespace cse {
struct RadioAudio::Impl {
#ifdef _WIN32
    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    IXAudio2SourceVoice* voices[4]{};
    X3DAUDIO_HANDLE x3d{};
    std::array<RadioSpeaker, 4> speakerPositions{};
    std::vector<uint8_t> pcm;
    unsigned outputChannels = 0;
#endif
    bool active = false;
    float gain = .24f;
};
RadioAudio::RadioAudio():impl_(new Impl()){}
RadioAudio::~RadioAudio(){stop();}

#ifdef _WIN32
namespace {
uint16_t u16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8)
        | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
bool readPcmMono16(const std::wstring& filename, std::vector<uint8_t>& pcm, WAVEFORMATEX& fmt) {
    std::ifstream in(std::filesystem::path(filename), std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto length = in.tellg();
    if (length < 44 || length > std::streamoff(80 * 1024 * 1024)) return false;
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length))) return false;
    if (std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
        return false;
    bool hasFmt = false, hasData = false;
    size_t at = 12;
    uint16_t channels = 0, bits = 0, format = 0;
    uint32_t rate = 0;
    while (at + 8 <= bytes.size()) {
        const uint32_t size = u32(bytes.data() + at + 4);
        const size_t dataStart = at + 8;
        if (size > bytes.size() - dataStart) return false;
        const auto* data = bytes.data() + dataStart;
        if (!std::memcmp(bytes.data() + at, "fmt ", 4)) {
            if (size < 16) return false;
            format = u16(data);
            channels = u16(data + 2);
            rate = u32(data + 4);
            bits = u16(data + 14);
            hasFmt = true;
        } else if (!std::memcmp(bytes.data() + at, "data", 4)) {
            pcm.assign(data, data + size);
            hasData = true;
        }
        at = dataStart + size + (size & 1u);
    }
    if (!hasFmt || !hasData || format != WAVE_FORMAT_PCM || channels != 1 ||
        bits != 16 || rate < 8000 || rate > 48000 || pcm.empty() ||
        (pcm.size() & 1u) || pcm.size() > UINT32_MAX) return false;
    fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = rate * 2;
    return true;
}
X3DAUDIO_VECTOR vec(const float v[3]) {return X3DAUDIO_VECTOR{v[0],v[1],v[2]};}
X3DAUDIO_VECTOR unit(const float v[3]) {
    const float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (!(len > .00001f) || !std::isfinite(len)) return X3DAUDIO_VECTOR{0,0,1};
    return X3DAUDIO_VECTOR{v[0]/len,v[1]/len,v[2]/len};
} 
X3DAUDIO_VECTOR orientedUp(const X3DAUDIO_VECTOR& front) {
    // X3DAudio expects perpendicular, normalized front/up vectors.
    const float y = front.y;
    const float v[3]{-front.x*y, 1.f-y*y, -front.z*y};
    if (v[0]*v[0]+v[1]*v[1]+v[2]*v[2] < .0001f)
        return X3DAUDIO_VECTOR{1.f,0.f,0.f};
    return unit(v);
}
}
#endif

void RadioAudio::stop() {
#ifdef _WIN32
    if (impl_->engine) impl_->engine->StopEngine();
    for (auto& voice : impl_->voices) {
        if (voice) {voice->DestroyVoice(); voice = nullptr;}
    }
    if (impl_->master) {impl_->master->DestroyVoice(); impl_->master = nullptr;}
    if (impl_->engine) {impl_->engine->Release(); impl_->engine = nullptr;}
    impl_->pcm.clear();
    impl_->outputChannels = 0;
#endif
    impl_->active = false;
}

bool RadioAudio::start(const std::wstring& path, const std::array<RadioSpeaker,4>& speakers) {
    stop();
#ifdef _WIN32
    WAVEFORMATEX fmt{};
    if (!readPcmMono16(path, impl_->pcm, fmt)) return false;
    impl_->speakerPositions = speakers;
    if (FAILED(XAudio2Create(&impl_->engine, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        stop(); return false;
    }
    if (FAILED(impl_->engine->CreateMasteringVoice(&impl_->master))) {
        stop(); return false;
    }
    XAUDIO2_VOICE_DETAILS details{};
    impl_->master->GetVoiceDetails(&details);
    impl_->outputChannels = details.InputChannels;
    if (impl_->outputChannels == 0 || impl_->outputChannels > 8) {
        stop(); return false;
    }
    DWORD channelMask = 0;
    if (FAILED(impl_->master->GetChannelMask(&channelMask))) {
        stop(); return false;
    }
    X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, impl_->x3d);
    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = static_cast<UINT32>(impl_->pcm.size());
    buffer.pAudioData = impl_->pcm.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    for (auto& voice : impl_->voices) {
        if (FAILED(impl_->engine->CreateSourceVoice(&voice, &fmt)) ||
            FAILED(voice->SubmitSourceBuffer(&buffer)) ||
            FAILED(voice->SetVolume(impl_->gain))) {
            stop(); return false;
        }
    }
    impl_->active = true;
    // Start all four voices from the same buffer, only after full initialization.
    for (auto* voice : impl_->voices) {
        if (FAILED(voice->Start(0))) {stop(); return false;}
    }
    return true;
#else
    (void)path; (void)speakers;
    return false;
#endif
}

void RadioAudio::setVolume(float level) {
    impl_->gain = std::clamp(level, 0.f, .6f);
#ifdef _WIN32
    for (auto* v : impl_->voices) if(v) v->SetVolume(impl_->gain);
#endif
}
bool RadioAudio::playing() const {return impl_->active;}

void RadioAudio::update(const float eye[3], const float target[3]) {
#ifdef _WIN32
    if (!impl_->active || !impl_->master) return;
    // Camera is the listener. The renderer does not lock or wait on this work.
    const float dir[3] = {target[0]-eye[0],target[1]-eye[1],target[2]-eye[2]};
    X3DAUDIO_LISTENER listener{};
    listener.Position = vec(eye);
    listener.OrientFront = unit(dir);
    listener.OrientTop = orientedUp(listener.OrientFront);
    for (size_t n = 0; n < 4; ++n) {
        const auto& sp = impl_->speakerPositions[n];
        X3DAUDIO_CONE cone{};
        cone.InnerAngle = 65.f * 3.1415926535f / 180.f;
        cone.OuterAngle = 125.f * 3.1415926535f / 180.f;
        cone.InnerVolume = 1.f;
        cone.OuterVolume = .20f;
        cone.InnerLPF = cone.OuterLPF = 1.f;
        cone.InnerReverb = cone.OuterReverb = 0.f;
        X3DAUDIO_EMITTER emitter{};
        emitter.pCone = &cone;
        emitter.OrientFront = unit(sp.front);
        emitter.OrientTop = orientedUp(emitter.OrientFront);
        emitter.Position = vec(sp.position);
        emitter.ChannelCount = 1;
        emitter.CurveDistanceScaler = 20.f; // metres of useful outdoor range
        emitter.DopplerScaler = 0.f; // stationary public-address system
        std::vector<float> matrix(impl_->outputChannels, 0.f);
        X3DAUDIO_DSP_SETTINGS dsp{};
        dsp.SrcChannelCount = 1;
        dsp.DstChannelCount = impl_->outputChannels;
        dsp.pMatrixCoefficients = matrix.data();
        X3DAudioCalculate(impl_->x3d, &listener, &emitter, X3DAUDIO_CALCULATE_MATRIX, &dsp);
        // User listening tests found L/R reversed compared with world-space
        // camera yaw. Correct ONLY stereo output; do not alter volume,
        // attenuations or multichannel speaker layout.
        if (impl_->outputChannels == 2 && matrix.size() == 2)
            std::swap(matrix[0],matrix[1]);
        impl_->voices[n]->SetOutputMatrix(impl_->master, 1, impl_->outputChannels, matrix.data());
    }
#else
    (void)eye; (void)target;
#endif
}
}
