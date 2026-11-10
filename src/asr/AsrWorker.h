#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>
#include <vector>

#include "asr/IAsrEngine.h"
#include "audio/RingBuffer.h"

// Runs on its own QThread. Polls the ring buffer every ~50 ms, feeds the
// engine, and reports partial and finalized text through queued signals.
class AsrWorker : public QObject {
    Q_OBJECT
public:
    explicit AsrWorker(RingBuffer &ring);
    ~AsrWorker() override;

public slots:
    void loadModel(const QString &modelDir, int numThreads);
    void unloadModel();
    void setActive(bool active);

signals:
    void partialChanged(const QString &text);
    void finalized(const QString &text);
    void engineReady(const QString &modelDir);
    void error(const QString &message);

private:
    void tick();

    RingBuffer &m_ring;
    std::unique_ptr<IAsrEngine> m_engine;
    QTimer *m_timer = nullptr;
    std::vector<float> m_chunk;
    std::string m_lastPartial;
    bool m_active = false;
};
