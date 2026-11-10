#include "ui/CaptionModel.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
#include <thread>

#include "asr/AsrWorker.h"

// Enable with QT_LOGGING_RULES="subtly.captions.info=true"
Q_LOGGING_CATEGORY(lcCaptions, "subtly.captions", QtWarningMsg)

CaptionModel::CaptionModel(QObject *parent) : QObject(parent) {
    m_worker = new AsrWorker(m_ring);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &AsrWorker::partialChanged, this, [this](const QString &t) {
        if (t != m_partial) {
            qCDebug(lcCaptions).noquote() << "partial:" << t;
            m_partial = t;
            emit partialChanged();
        }
    });
    connect(m_worker, &AsrWorker::finalized, this, &CaptionModel::onFinalized);
    connect(m_worker, &AsrWorker::engineReady, this, [this](const QString &path) {
        if (QFileInfo(path).fileName() != m_loadingModel)
            return;
        m_model = QFileInfo(path).fileName();
        QSettings().setValue("asr/model", m_model);
        emit modelChanged();
        setStatus(tr("Model %1 loaded").arg(m_model));
    });
    connect(m_worker, &AsrWorker::error, this, &CaptionModel::setStatus);
    m_thread.setObjectName("asr");
    m_thread.start();

    QSettings s;
    m_source = s.value("audio/source", 0).toInt();
    refreshModels();
    QString saved = s.value("asr/model").toString();
    if (saved.isEmpty() || !m_available.contains(saved)) {
        saved = "moonshine-tiny-streaming-en";
        if (!m_available.contains(saved)) {
            const QDir dir(modelsDir());
            auto preferred = std::find_if(m_available.begin(), m_available.end(),
                [&](const QString &name) { return dir.exists(name + "/moonshine-language.txt"); });
            if (preferred == m_available.end())
                preferred = std::find_if(m_available.begin(), m_available.end(),
                    [](const QString &name) { return name.startsWith("sherpa-onnx"); });
            saved = preferred != m_available.end() ? *preferred : m_available.value(0);
        }
    }
    if (saved.isEmpty())
        setStatus(tr("No models installed. Download a model in Settings."));
    else
        setModel(saved);
    setRunning(true);
}

CaptionModel::~CaptionModel() {
    m_capture.stop();
    m_thread.quit();
    m_thread.wait();
}

QString CaptionModel::modelsDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           "/subtly/models";
}

void CaptionModel::refreshModels() {
    QDir dir(modelsDir());
    QStringList found = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    found.erase(std::remove_if(found.begin(), found.end(), [&](const QString &d) {
                    if (dir.exists(d + "/tokens.txt"))
                        return false;
                    if (dir.exists(d + "/moonshine-language.txt"))
                        return false;
#ifdef SUBTLY_NEMOTRON
                    return QDir(dir.filePath(d)).entryList({"*.gguf"}, QDir::Files).size() != 1;
#else
                    return true;
#endif
                }),
                found.end());
    if (found != m_available) {
        m_available = found;
        emit availableModelsChanged();
    }
}

void CaptionModel::setModel(const QString &name) {
    if (!m_available.contains(name) || (name == m_model && name == m_loadingModel))
        return;
    m_loadingModel = name;
    setStatus(tr("Loading %1…").arg(name));
    // One thread decodes ~28x faster than real time (RTF 0.036 on Core Ultra 7 255H).
    // More threads gain little and onnxruntime's pool busy-waits between our
    // 50 ms decode calls, burning several cores. Override: SUBTLY_THREADS.
    const int threads = std::clamp(qEnvironmentVariableIntValue("SUBTLY_THREADS"), 1, 8);
    QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                              Q_ARG(QString, modelsDir() + "/" + name), Q_ARG(int, threads));
}

void CaptionModel::unloadModel(const QString &name) {
    if (name != m_model && name != m_loadingModel)
        return;
    setRunning(false);
    QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::BlockingQueuedConnection);
    m_model.clear();
    m_loadingModel.clear();
    QSettings().remove("asr/model");
    emit modelChanged();
    setStatus(tr("Select or download a model in Settings."));
}

void CaptionModel::setRunning(bool on) {
    if (on == m_running)
        return;
    m_running = on;
    if (on) {
        restartCapture();
    } else {
        m_capture.stop();
    }
    QMetaObject::invokeMethod(m_worker, "setActive", Qt::QueuedConnection, Q_ARG(bool, on));
    emit runningChanged();
}

void CaptionModel::setSource(int s) {
    if (s == m_source)
        return;
    m_source = s;
    QSettings().setValue("audio/source", s);
    emit sourceChanged();
    if (m_running) {
        QMetaObject::invokeMethod(m_worker, "setActive", Qt::QueuedConnection, Q_ARG(bool, false));
        restartCapture();
        QMetaObject::invokeMethod(m_worker, "setActive", Qt::QueuedConnection, Q_ARG(bool, true));
    }
}

void CaptionModel::restartCapture() {
    const auto src = m_source == 1 ? PipeWireCapture::Source::Microphone
                                   : PipeWireCapture::Source::System;
    if (!m_capture.start(src))
        setStatus(tr("Could not connect to PipeWire"));
}

void CaptionModel::clear() {
    m_lines.clear();
    m_partial.clear();
    emit linesChanged();
    emit partialChanged();
}

void CaptionModel::onFinalized(const QString &text) {
    if (text.isEmpty())
        return;
    qCInfo(lcCaptions).noquote() << text;
    m_lines.append(text);
    while (m_lines.size() > kMaxHistory)
        m_lines.removeFirst();
    emit linesChanged();
}

void CaptionModel::setStatus(const QString &s) {
    if (s == m_status)
        return;
    m_status = s;
    emit statusChanged();
}
