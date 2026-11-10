// Speech-to-text subset of Moonshine Voice v0.1.6's C ABI (header version 30000).
// https://github.com/moonshine-ai/moonshine/blob/v0.1.6/core/moonshine-c-api.h
// Copyright (c) 2025 Moonshine AI. MIT; see third_party/moonshine-LICENSE.
// Keep transcript_line_t's complete layout, including fields unused by this app.
#pragma once
#include <cstddef>
#include <cstdint>

inline constexpr uint32_t MOONSHINE_MODEL_ARCH_TINY_STREAMING = 2;
inline constexpr int32_t MOONSHINE_HEADER_VERSION = 30000;

extern "C" {
struct moonshine_option_t {
    const char *name;
    const char *value;
};
struct speaker_span_t;
struct transcript_word_t;
struct transcript_line_t {
    const char *text;
    const float *audio_data;
    size_t audio_data_count;
    float start_time;
    float duration;
    uint64_t id;
    int8_t is_complete;
    int8_t is_updated;
    int8_t is_new;
    int8_t has_text_changed;
    int8_t have_speakers_changed;
    const speaker_span_t *speaker_spans;
    uint64_t speaker_span_count;
    uint32_t last_transcription_latency_ms;
    const transcript_word_t *words;
    uint64_t word_count;
};
struct transcript_t {
    transcript_line_t *lines;
    uint64_t line_count;
};
int32_t moonshine_load_transcriber_from_files(const char *, uint32_t,
                                             const moonshine_option_t *, uint64_t, int32_t);
void moonshine_free_transcriber(int32_t);
const char *moonshine_error_to_string(int32_t);
int32_t moonshine_create_stream(int32_t, uint32_t);
int32_t moonshine_free_stream(int32_t, int32_t);
int32_t moonshine_start_stream(int32_t, int32_t);
int32_t moonshine_stop_stream(int32_t, int32_t);
int32_t moonshine_transcribe_add_audio_to_stream(int32_t, int32_t, const float *, uint64_t,
                                                int32_t, uint32_t);
int32_t moonshine_transcribe_stream(int32_t, int32_t, uint32_t, transcript_t **);
}
