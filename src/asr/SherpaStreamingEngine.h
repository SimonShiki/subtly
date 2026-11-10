#pragma once
#include <memory>
#include <string>

#include "asr/IAsrEngine.h"

struct SherpaOnnxOnlineRecognizer;
struct SherpaOnnxOnlineStream;

// sherpa-onnx streaming transducer (Zipformer) on onnxruntime CPU.
// modelDir must contain encoder, decoder, and joiner *.onnx files (int8 preferred)
// and tokens.txt.
class SherpaStreamingEngine final : public IAsrEngine {
public:
    static std::unique_ptr<SherpaStreamingEngine> create(const std::string &modelDir, int numThreads,
                                                         std::string *error);
    ~SherpaStreamingEngine() override;

    void acceptWaveform(std::span<const float> samples) override;
    std::string partial() override;
    std::vector<std::string> takeFinals() override;
    void reset() override;

private:
    SherpaStreamingEngine() = default;
    const SherpaOnnxOnlineRecognizer *m_rec = nullptr;
    const SherpaOnnxOnlineStream *m_stream = nullptr;
};
