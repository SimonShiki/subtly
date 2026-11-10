#include "asr/AsrWorker.h"

#include <exception>

AsrWorker::AsrWorker(RingBuffer &ring) : m_ring(ring), m_chunk(1600) {}

AsrWorker::~AsrWorker() = default;

void AsrWorker::loadModel(const QString &modelDir, int numThreads) {
    if (!m_timer) { // create the timer here so it belongs to the worker thread.
        m_timer = new QTimer(this);
        m_timer->setInterval(50);
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout, this, &AsrWorker::tick);
    }
    std::string err;
    auto engine = createAsrEngine(modelDir.toStdString(), numThreads, &err);
    m_ring.clear(); // discard audio captured during model loading.
    if (!engine) {
        emit error(QString::fromStdString(err));
        return;
    }
    m_engine = std::move(engine);
    m_lastPartial.clear();
    emit partialChanged({});
    emit engineReady(modelDir);
    if (m_active)
        m_timer->start();
}

void AsrWorker::unloadModel() {
    if (m_timer)
        m_timer->stop();
    m_engine.reset();
    m_lastPartial.clear();
    m_ring.clear();
    emit partialChanged({});
}

void AsrWorker::setActive(bool active) {
    m_active = active;
    if (!m_timer)
        return;
    m_timer->stop();
    if (m_engine) {
        try {
            m_engine->reset(); // discard utterance state across pause/resume
        } catch (const std::exception &e) {
            emit error(QString::fromUtf8(e.what()));
            return;
        }
    }
    m_lastPartial.clear();
    emit partialChanged({});
    m_ring.clear();
    if (active && m_engine)
        m_timer->start();
}

void AsrWorker::tick() {
    if (!m_engine)
        return;
    try {
        // limit each tick so the worker can also handle pause and model-switch requests.
        for (int i = 0; i < 10; ++i) {
            const size_t n = m_ring.pop(m_chunk);
            if (n == 0)
                break;
            m_engine->acceptWaveform({m_chunk.data(), n});
            for (const auto &text : m_engine->takeFinals())
                emit finalized(QString::fromStdString(text).trimmed());
        }
        const std::string text = m_engine->partial();
        if (text != m_lastPartial) {
            m_lastPartial = text;
            emit partialChanged(QString::fromStdString(text).trimmed());
        }
    } catch (const std::exception &e) {
        m_timer->stop();
        emit error(QString::fromUtf8(e.what()));
    }
}
