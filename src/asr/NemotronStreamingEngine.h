#pragma once
#ifdef SUBTLY_NEMOTRON
#include <nemo_speech/asr.h>

#include "asr/IAsrEngine.h"

// Both Nemotron streaming RNNT checkpoints use the same native C ABI.
class NemotronStreamingEngine final : public IAsrEngine {
public:
    static std::unique_ptr<NemotronStreamingEngine> create(const std::string &model,
                                                          std::string *error);
    void acceptWaveform(std::span<const float> samples) override;
    std::string partial() override { return m_partial; }
    std::vector<std::string> takeFinals() override;
    void reset() override;
    void finish() override;

private:
    void drain();
    std::unique_ptr<nemo_speech_asr_recognizer, decltype(&nemo_speech_asr_destroy)>
        m_recognizer{nullptr, nemo_speech_asr_destroy};
    std::unique_ptr<nemo_speech_asr_stream, decltype(&nemo_speech_asr_stream_close)>
        m_stream{nullptr, nemo_speech_asr_stream_close};
    std::string m_partial;
    std::vector<std::string> m_finals;
    size_t m_utteranceSamples = 0;
};
#endif
