#include "audio/PipeWireCapture.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <span>

struct PipeWireCapture::Hook {
    spa_hook listener{};
    pw_stream_events events{};
};

PipeWireCapture::PipeWireCapture(RingBuffer &sink) : m_sink(sink), m_hook(std::make_unique<Hook>()) {
    pw_init(nullptr, nullptr);
    m_loop = pw_thread_loop_new("subtly-capture", nullptr);
    pw_thread_loop_start(m_loop);
}

PipeWireCapture::~PipeWireCapture() {
    stop();
    pw_thread_loop_stop(m_loop);
    pw_thread_loop_destroy(m_loop);
    pw_deinit();
}

void PipeWireCapture::onProcess(void *userdata) {
    auto *self = static_cast<PipeWireCapture *>(userdata);
    pw_buffer *b = pw_stream_dequeue_buffer(self->m_stream);
    if (!b)
        return;
    spa_buffer *buf = b->buffer;
    if (buf->datas[0].data && buf->datas[0].chunk) {
        const auto *chunk = buf->datas[0].chunk;
        const auto *base = static_cast<const uint8_t *>(buf->datas[0].data) + chunk->offset;
        const size_t n = chunk->size / sizeof(float);
        self->m_sink.push(std::span(reinterpret_cast<const float *>(base), n));
    }
    pw_stream_queue_buffer(self->m_stream, b);
}

bool PipeWireCapture::start(Source source) {
    stop();
    pw_thread_loop_lock(m_loop);

    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Accessibility",
        PW_KEY_APP_NAME, "subtly",
        PW_KEY_NODE_NAME, "subtly-capture",
        PW_KEY_NODE_LATENCY, "1600/16000", // Use a 100 ms quantum for caption capture.
        nullptr);
    if (source == Source::System)
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");

    m_hook->events = {};
    m_hook->events.version = PW_VERSION_STREAM_EVENTS;
    m_hook->events.process = &PipeWireCapture::onProcess;

    m_stream = pw_stream_new_simple(pw_thread_loop_get_loop(m_loop), "subtly capture", props,
                                    &m_hook->events, this);
    if (!m_stream) {
        pw_thread_loop_unlock(m_loop);
        return false;
    }

    uint8_t podBuf[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuf, sizeof(podBuf));
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = kSampleRate;
    info.channels = 1;
    info.position[0] = SPA_AUDIO_CHANNEL_MONO;
    const spa_pod *params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};

    int res = pw_stream_connect(m_stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                                static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                             PW_STREAM_FLAG_MAP_BUFFERS |
                                                             PW_STREAM_FLAG_RT_PROCESS),
                                params, 1);
    if (res < 0) {
        pw_stream_destroy(m_stream);
        m_stream = nullptr;
    }
    pw_thread_loop_unlock(m_loop);
    return m_stream != nullptr;
}

void PipeWireCapture::stop() {
    if (!m_stream)
        return;
    pw_thread_loop_lock(m_loop);
    pw_stream_destroy(m_stream);
    m_stream = nullptr;
    pw_thread_loop_unlock(m_loop);
}
