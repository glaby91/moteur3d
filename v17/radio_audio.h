#pragma once
#include <array>
#include <memory>
#include <string>

namespace cse {
struct RadioSpeaker {
    float position[3]{};
    float front[3]{};
};
// Optional, CPU-driven Windows spatial audio. The renderer never waits for audio.
class RadioAudio {
public:
    RadioAudio();
    ~RadioAudio();
    RadioAudio(const RadioAudio&) = delete;
    RadioAudio& operator=(const RadioAudio&) = delete;
    // WAV: PCM 16-bit mono; source bytes remain owned until stop().
    bool start(const std::wstring& wavPath, const std::array<RadioSpeaker, 4>& speakers);
    void update(const float eye[3], const float lookAt[3]);
    void setVolume(float level);
    void stop();
    bool playing() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
