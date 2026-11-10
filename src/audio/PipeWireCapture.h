#pragma once
#include <memory>

#include "audio/RingBuffer.h"

struct pw_thread_loop;
struct pw_stream;

// Captures 16 kHz mono float audio from PipeWire into a RingBuffer.
// Source::System records the default sink's monitor (system audio).
// Source::Microphone records the default source. PipeWire resamples, downmixes,
// and follows default-device changes automatically.
class PipeWireCapture {
public:
    enum class Source { System, Microphone };
    static constexpr int kSampleRate = 16000;

    explicit PipeWireCapture(RingBuffer &sink);
    ~PipeWireCapture();
    PipeWireCapture(const PipeWireCapture &) = delete;
    PipeWireCapture &operator=(const PipeWireCapture &) = delete;

    bool start(Source source);
    void stop();
    bool isRunning() const { return m_stream != nullptr; }

private:
    static void onProcess(void *userdata);

    RingBuffer &m_sink;
    pw_thread_loop *m_loop = nullptr;
    pw_stream *m_stream = nullptr;
    struct Hook;
    std::unique_ptr<Hook> m_hook;
};
