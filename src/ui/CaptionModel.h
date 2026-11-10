#pragma once
#include <QObject>
#include <QStringList>
#include <QThread>
#include <memory>

#include "audio/PipeWireCapture.h"
#include "audio/RingBuffer.h"

class AsrWorker;

// App controller exposed to QML as the "captions" context property.
// Owns audio capture and the ASR worker thread, and stores caption text.
class CaptionModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList lines READ lines NOTIFY linesChanged)
    Q_PROPERTY(QString partial READ partial NOTIFY partialChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(int source READ source WRITE setSource NOTIFY sourceChanged) // 0: system audio; 1: microphone
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)
    Q_PROPERTY(QString modelsDir READ modelsDir CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit CaptionModel(QObject *parent = nullptr);
    ~CaptionModel() override;

    QStringList lines() const { return m_lines; }
    QString partial() const { return m_partial; }
    bool running() const { return m_running; }
    void setRunning(bool on);
    int source() const { return m_source; }
    void setSource(int s);
    QString model() const { return m_model; }
    void setModel(const QString &name);
    void unloadModel(const QString &name);
    QStringList availableModels() const { return m_available; }
    QString modelsDir() const;
    QString status() const { return m_status; }

    Q_INVOKABLE void refreshModels();
    Q_INVOKABLE void clear();

signals:
    void linesChanged();
    void partialChanged();
    void runningChanged();
    void sourceChanged();
    void modelChanged();
    void availableModelsChanged();
    void statusChanged();

private:
    void setStatus(const QString &s);
    void onFinalized(const QString &text);
    void restartCapture();

    static constexpr int kMaxHistory = 50;

    RingBuffer m_ring{16000 * 10}; // Buffer up to 10 s of audio.
    PipeWireCapture m_capture{m_ring};
    QThread m_thread;
    AsrWorker *m_worker = nullptr;

    QStringList m_lines;
    QString m_partial;
    QString m_model;
    QString m_loadingModel;
    QStringList m_available;
    QString m_status;
    int m_source = 0;
    bool m_running = false;
};
