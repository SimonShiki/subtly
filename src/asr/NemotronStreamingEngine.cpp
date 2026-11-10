#ifdef SUBTLY_NEMOTRON
#include "asr/NemotronStreamingEngine.h"

#include <stdexcept>
#include <utility>

namespace {
void check(nemo_speech_asr_status status) {
    if (status != NEMO_SPEECH_ASR_OK)
        throw std::runtime_error(std::string("NeMo-Speech.cpp: ") + nemo_speech_asr_last_error());
}
}

std::unique_ptr<NemotronStreamingEngine> NemotronStreamingEngine::create(
    const std::string &path, std::string *error) {
    try {
        auto engine = std::unique_ptr<NemotronStreamingEngine>(new NemotronStreamingEngine);
        nemo_speech_asr_backend_config backend{};
        backend.size = sizeof(backend);
        backend.gpu = -1;
        nemo_speech_asr_model_config model{};
        model.size = sizeof(model);
        model.path = path.c_str();
        nemo_speech_asr_streaming_config streaming{};
        streaming.size = sizeof(streaming);
        streaming.chunk_size = 0.16f;
        streaming.ctc_left_padding = streaming.ctc_right_padding = 1.92f;
        streaming.rnnt_right_context = 1; // 160 ms chunks for either checkpoint
        nemo_speech_asr_endpointing_config endpointing{};
        endpointing.size = sizeof(endpointing);
        endpointing.enable = true;
        endpointing.stop_history_eou_ms = 800;
        nemo_speech_asr_recognizer_config config{};
        config.size = sizeof(config);
        config.backend = &backend;
        config.model = &model;
        config.streaming = &streaming;
        config.endpointing = &endpointing;
        nemo_speech_asr_recognizer *recognizer = nullptr;
        check(nemo_speech_asr_create(&config, &recognizer));
        engine->m_recognizer.reset(recognizer);
        engine->reset();
        return engine;
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return nullptr;
    }
}

void NemotronStreamingEngine::reset() {
    m_stream.reset();
    m_partial.clear();
    m_finals.clear();
    m_utteranceSamples = 0;
    auto options = nemo_speech_asr_recognition_options_default();
    options.interim_results = true;
    options.enable_automatic_punctuation = true;
    // Leave language unset to use the model's default or automatic language selection.
    nemo_speech_asr_stream *stream = nullptr;
    check(nemo_speech_asr_streaming_recognize(m_recognizer.get(), &options, &stream));
    m_stream.reset(stream);
}

void NemotronStreamingEngine::acceptWaveform(std::span<const float> samples) {
    check(nemo_speech_asr_stream_push_f32(m_stream.get(), samples.data(), samples.size(), 16000));
    m_utteranceSamples += samples.size();
    if (m_utteranceSamples >= 15 * 16000)
        check(nemo_speech_asr_stream_force_endpoint(m_stream.get()));
    drain();
}

void NemotronStreamingEngine::drain() {
    for (;;) {
        nemo_speech_asr_result *raw = nullptr;
        check(nemo_speech_asr_stream_next(m_stream.get(), &raw));
        if (!raw)
            break;
        std::unique_ptr<nemo_speech_asr_result, decltype(&nemo_speech_asr_result_destroy)>
            result(raw, nemo_speech_asr_result_destroy);
        const char *text = nemo_speech_asr_result_transcript(raw, 0);
        m_partial = text ? text : "";
        if (nemo_speech_asr_result_is_final(raw)) {
            if (!m_partial.empty())
                m_finals.push_back(std::move(m_partial));
            m_partial.clear();
            m_utteranceSamples = 0;
        }
    }
}

std::vector<std::string> NemotronStreamingEngine::takeFinals() {
    return std::exchange(m_finals, {});
}

void NemotronStreamingEngine::finish() {
    check(nemo_speech_asr_stream_finish(m_stream.get()));
    drain();
}
#endif
