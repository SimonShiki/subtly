#include "asr/MoonshineStreamingEngine.h"
#include "asr/moonshine-c-api.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace {
constexpr size_t kUpdateSamples = 8000; // 500 ms at 16 kHz
constexpr size_t kMaxStreamSamples = 5 * 60 * 16000;

void check(int32_t result) {
    if (result < 0)
        throw std::runtime_error(std::string("Moonshine: ") + moonshine_error_to_string(result));
}
}

std::unique_ptr<MoonshineStreamingEngine> MoonshineStreamingEngine::create(
    const std::string &modelDir, std::string *error) {
    try {
        namespace fs = std::filesystem;
        std::string language;
        std::ifstream(modelDir + "/moonshine-language.txt") >> language;
        if (language != "en" && language != "ja" && language != "zh")
            throw std::runtime_error("Missing or unsupported Moonshine language in " + modelDir);
        for (const char *file : {"adapter.ort", "cross_kv.ort", "decoder_kv.ort", "encoder.ort",
                                 "frontend.model.ort", "frontend.weights.ort",
                                 "streaming_config.json", "tokenizer.bin"}) {
            if (!fs::is_regular_file(fs::path(modelDir) / file))
                throw std::runtime_error("Incomplete Moonshine Tiny Streaming model: " + modelDir);
        }

        // on Linux, the default per-session ORT thread pools consume CPU between
        // updates. The pinned runtime provides only a process-wide threading switch.
        static std::once_flag threading;
        std::call_once(threading, [] {
            if (setenv("MOONSHINE_ORT_SINGLE_THREAD", "1", 1) != 0)
                throw std::runtime_error("Could not configure Moonshine CPU threading");
        });
        const moonshine_option_t options[] = {
            {"transcription_interval", "0.5"},
            {"max_tokens_per_second", language == "en" ? "6.5" : "13.0"},
            {"return_audio_data", "false"},
            {"vad_max_segment_duration", "15"},
        };
        auto engine = std::unique_ptr<MoonshineStreamingEngine>(new MoonshineStreamingEngine);
        engine->m_model = moonshine_load_transcriber_from_files(
            modelDir.c_str(), MOONSHINE_MODEL_ARCH_TINY_STREAMING, options, std::size(options),
            MOONSHINE_HEADER_VERSION);
        check(engine->m_model);
        engine->restartStream();
        return engine;
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return nullptr;
    }
}

MoonshineStreamingEngine::~MoonshineStreamingEngine() {
    if (m_stream >= 0)
        moonshine_free_stream(m_model, m_stream);
    if (m_model >= 0)
        moonshine_free_transcriber(m_model);
}

void MoonshineStreamingEngine::restartStream() {
    if (m_stream >= 0) {
        check(moonshine_free_stream(m_model, m_stream));
        m_stream = -1;
    }
    m_stream = moonshine_create_stream(m_model, 0);
    check(m_stream);
    check(moonshine_start_stream(m_model, m_stream));
    m_pendingSamples = m_streamSamples = 0;
    m_nextFinal = 0;
    m_finished = m_atEndpoint = false;
    m_partial.clear();
}

void MoonshineStreamingEngine::reset() {
    restartStream();
    m_finals.clear();
}

void MoonshineStreamingEngine::acceptWaveform(std::span<const float> samples) {
    if (m_finished)
        throw std::runtime_error("Moonshine stream is finished; reset before feeding audio");
    if (samples.empty())
        return;
    check(moonshine_transcribe_add_audio_to_stream(m_model, m_stream, samples.data(),
                                                  samples.size(), 16000, 0));
    m_pendingSamples += samples.size();
    m_streamSamples += samples.size();
    if (m_pendingSamples >= kUpdateSamples) {
        update();
        // the SDK keeps completed transcripts. Restart the stream at silence
        // to limit history without dropping a new line that started in this feed.
        if (m_atEndpoint)
            restartStream();
    }
    // Limit stream length during continuous speech with no silent boundary.
    if (m_streamSamples >= kMaxStreamSamples) {
        finish();
        restartStream();
    }
}

void MoonshineStreamingEngine::update() {
    transcript_t *transcript = nullptr;
    check(moonshine_transcribe_stream(m_model, m_stream, 0, &transcript));
    m_pendingSamples = 0;
    m_partial.clear();
    m_atEndpoint = false;
    if (!transcript)
        return;
    // Line indexes stay stable, and completed text never changes. Copy text
    // before the next SDK call invalidates the returned pointers.
    while (m_nextFinal < transcript->line_count) {
        const auto &line = transcript->lines[m_nextFinal];
        if (!line.is_complete)
            break;
        if (line.text && *line.text)
            m_finals.emplace_back(line.text);
        ++m_nextFinal;
    }
    if (m_nextFinal < transcript->line_count) {
        const char *text = transcript->lines[transcript->line_count - 1].text;
        m_partial = text ? text : "";
    }
    m_atEndpoint = transcript->line_count > 0 && m_nextFinal == transcript->line_count;
}

std::vector<std::string> MoonshineStreamingEngine::takeFinals() {
    return std::exchange(m_finals, {});
}

void MoonshineStreamingEngine::finish() {
    if (m_finished)
        return;
    // The SDK needs pending audio to flush its VAD, even on an update boundary.
    if (m_pendingSamples == 0) {
        const float silence = 0;
        check(moonshine_transcribe_add_audio_to_stream(m_model, m_stream, &silence, 1, 16000, 0));
    }
    check(moonshine_stop_stream(m_model, m_stream));
    update();
    m_finished = true;
}
