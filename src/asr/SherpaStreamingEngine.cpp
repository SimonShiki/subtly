#include "asr/SherpaStreamingEngine.h"

#include <sherpa-onnx/c-api/c-api.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace {
// Finds "<prefix>*.onnx" in dir, preferring the int8-quantized variant.
std::string findModel(const fs::path &dir, const std::string &prefix) {
    std::string plain;
    for (const auto &e : fs::directory_iterator(dir)) {
        const std::string name = e.path().filename().string();
        if (!name.starts_with(prefix) || !name.ends_with(".onnx"))
            continue;
        if (name.ends_with(".int8.onnx"))
            return e.path().string();
        plain = e.path().string();
    }
    return plain;
}
} // namespace

std::unique_ptr<SherpaStreamingEngine> SherpaStreamingEngine::create(const std::string &modelDir,
                                                                     int numThreads,
                                                                     std::string *error) {
    std::error_code ec;
    const fs::path dir(modelDir);
    if (!fs::is_directory(dir, ec)) {
        if (error)
            *error = "Model directory not found: " + modelDir;
        return nullptr;
    }
    const std::string encoder = findModel(dir, "encoder");
    const std::string decoder = findModel(dir, "decoder");
    const std::string joiner = findModel(dir, "joiner");
    const std::string tokens = (dir / "tokens.txt").string();
    if (encoder.empty() || decoder.empty() || joiner.empty() || !fs::exists(tokens)) {
        if (error)
            *error = "Incomplete transducer model in " + modelDir;
        return nullptr;
    }

    SherpaOnnxOnlineRecognizerConfig cfg{};
    cfg.feat_config.sample_rate = 16000;
    cfg.feat_config.feature_dim = 80;
    cfg.model_config.transducer.encoder = encoder.c_str();
    cfg.model_config.transducer.decoder = decoder.c_str();
    cfg.model_config.transducer.joiner = joiner.c_str();
    cfg.model_config.tokens = tokens.c_str();
    cfg.model_config.num_threads = numThreads;
    cfg.model_config.provider = "cpu";
    cfg.decoding_method = "greedy_search";
    cfg.max_active_paths = 4;
    cfg.enable_endpoint = 1;
    cfg.rule1_min_trailing_silence = 2.4f; // silence-only utterance
    cfg.rule2_min_trailing_silence = 0.8f; // pause after speech ends a line
    cfg.rule3_min_utterance_length = 15.0f; // force a line break on long speech

    const SherpaOnnxOnlineRecognizer *rec = SherpaOnnxCreateOnlineRecognizer(&cfg);
    if (!rec) {
        if (error)
            *error = "sherpa-onnx failed to load model from " + modelDir;
        return nullptr;
    }
    std::unique_ptr<SherpaStreamingEngine> self(new SherpaStreamingEngine);
    self->m_rec = rec;
    self->m_stream = SherpaOnnxCreateOnlineStream(rec);
    return self;
}

SherpaStreamingEngine::~SherpaStreamingEngine() {
    if (m_stream)
        SherpaOnnxDestroyOnlineStream(m_stream);
    if (m_rec)
        SherpaOnnxDestroyOnlineRecognizer(m_rec);
}

void SherpaStreamingEngine::acceptWaveform(std::span<const float> samples) {
    SherpaOnnxOnlineStreamAcceptWaveform(m_stream, 16000, samples.data(),
                                         static_cast<int32_t>(samples.size()));
    while (SherpaOnnxIsOnlineStreamReady(m_rec, m_stream))
        SherpaOnnxDecodeOnlineStream(m_rec, m_stream);
}

std::string SherpaStreamingEngine::partial() {
    const SherpaOnnxOnlineRecognizerResult *r = SherpaOnnxGetOnlineStreamResult(m_rec, m_stream);
    std::string text = (r && r->text) ? r->text : "";
    if (r)
        SherpaOnnxDestroyOnlineRecognizerResult(r);
    return text;
}

std::vector<std::string> SherpaStreamingEngine::takeFinals() {
    if (!SherpaOnnxOnlineStreamIsEndpoint(m_rec, m_stream))
        return {};
    auto text = partial();
    reset();
    return text.empty() ? std::vector<std::string>{} : std::vector<std::string>{std::move(text)};
}

void SherpaStreamingEngine::reset() {
    SherpaOnnxOnlineStreamReset(m_rec, m_stream);
}
