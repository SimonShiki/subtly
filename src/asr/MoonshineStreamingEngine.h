#pragma once
#include "asr/IAsrEngine.h"
#include <cstdint>

// Moonshine Voice Tiny Streaming, with one language-specific checkpoint loaded.
class MoonshineStreamingEngine final : public IAsrEngine {
public:
    static std::unique_ptr<MoonshineStreamingEngine> create(const std::string &modelDir,
                                                           std::string *error);
    ~MoonshineStreamingEngine() override;
    void acceptWaveform(std::span<const float> samples) override;
    std::string partial() override { return m_partial; }
    std::vector<std::string> takeFinals() override;
    void reset() override;
    void finish() override;

private:
    void update();
    void restartStream();
    int32_t m_model = -1;
    int32_t m_stream = -1;
    size_t m_pendingSamples = 0;
    size_t m_streamSamples = 0;
    uint64_t m_nextFinal = 0;
    bool m_finished = false;
    bool m_atEndpoint = false;
    std::string m_partial;
    std::vector<std::string> m_finals;
};
