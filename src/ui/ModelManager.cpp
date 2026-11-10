#include "ui/ModelManager.h"
#include "ui/CaptionModel.h"

#include <QDir>
#include <QJsonDocument>
#include <QNetworkReply>
#include <array>

namespace {
bool supported(const QJsonObject &model) {
#ifndef SUBTLY_NEMOTRON
    return model.value("backend") != "nemotron";
#else
    return true;
#endif
}

qint64 sizeOf(const QJsonObject &model) {
    qint64 size = 0;
    for (const auto &file : model.value("files").toArray())
        size += file.toObject().value("size").toInteger();
    return size;
}

quint32 crc32c(quint32 crc, const QByteArray &data) {
    static const auto table = [] {
        std::array<quint32, 256> result{};
        for (quint32 i = 0; i < result.size(); ++i) {
            quint32 value = i;
            for (int bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1) ? 0x82f63b78 : 0);
            result[i] = value;
        }
        return result;
    }();
    for (unsigned char byte : data)
        crc = table[(crc ^ byte) & 255] ^ (crc >> 8);
    return crc;
}
}

ModelManager::ModelManager(CaptionModel *captions, QObject *parent)
    : QObject(parent), m_captions(captions) {
    QFile manifest(":/models.json");
    if (manifest.open(QIODevice::ReadOnly))
        m_catalog = QJsonDocument::fromJson(manifest.readAll()).array();
    m_network.setTransferTimeout(30000);
}

ModelManager::~ModelManager() { cancel(); }

QVariantList ModelManager::catalog() const {
    QVariantList result;
    QStringList known;
    const QDir dir(m_captions->modelsDir());
    for (const auto &entry : m_catalog) {
        const auto model = entry.toObject();
        const QString name = model.value("id").toString();
        known.append(name);
        // Weights may be installed even if this build does not support the backend.
        const bool installed = dir.exists(name);
        result.append(QVariantMap{{"name", name}, {"label", model.value("label").toString()},
            {"size", sizeOf(model)}, {"installed", installed}, {"supported", supported(model)}});
    }
    for (const auto &name : m_captions->availableModels()) {
        if (!known.contains(name))
            result.append(QVariantMap{{"name", name}, {"label", name},
                {"size", 0}, {"installed", true}, {"supported", true}});
    }
    return result;
}

void ModelManager::refresh() {
    m_captions->refreshModels();
    emit catalogChanged();
}

void ModelManager::download(const QString &name) {
    if (!downloading().isEmpty())
        return;
    for (const auto &entry : m_catalog) {
        const auto model = entry.toObject();
        if (model.value("id") != name || !supported(model))
            continue;
        const QDir dir(m_captions->modelsDir());
        if (dir.exists(name)) {
            emit error(tr("The model folder already exists. Delete it before downloading again."));
            return;
        }
        if (!QDir().mkpath(dir.path())) {
            emit error(tr("Could not create the models folder."));
            return;
        }
        m_staging = std::make_unique<QTemporaryDir>(dir.filePath(".download-XXXXXX"));
        if (!m_staging->isValid()) {
            finish(tr("Could not create a temporary download folder."));
            return;
        }
        m_model = model;
        m_index = 0;
        m_received = 0;
        m_total = sizeOf(model);
        emit progressChanged();
        emit downloadChanged();
        nextFile();
        return;
    }
}

void ModelManager::nextFile() {
    const auto files = m_model.value("files").toArray();
    if (m_index == files.size()) {
        const auto language = m_model.value("language").toString();
        if (!language.isEmpty()) {
            QFile marker(m_staging->filePath("moonshine-language.txt"));
            if (!marker.open(QIODevice::WriteOnly) || marker.write(language.toUtf8() + '\n') < 0) {
                finish(tr("Could not save the model language."));
                return;
            }
        }
        const QString name = downloading();
        if (!QDir().rename(m_staging->path(), QDir(m_captions->modelsDir()).filePath(name))) {
            finish(tr("Could not install the downloaded model."));
            return;
        }
        finish();
        refresh();
        if (m_captions->model().isEmpty())
            m_captions->setModel(name);
        return;
    }
    const auto file = files[m_index].toObject();
    m_file.setFileName(m_staging->filePath(file.value("name").toString()));
    if (!m_file.open(QIODevice::WriteOnly)) {
        finish(m_file.errorString());
        return;
    }
    m_hash.reset();
    m_crc = 0xffffffff;
    const QUrl url(m_model.value("base_url").toString() + '/' + file.value("name").toString());
    m_reply = m_network.get(QNetworkRequest(url));
    m_reply->setReadBufferSize(256 * 1024);
    connect(m_reply, &QIODevice::readyRead, this, &ModelManager::receive);
    connect(m_reply, &QNetworkReply::finished, this, [this, file] {
        receive();
        if (!m_reply)
            return;
        const QString failure = m_reply->error() == QNetworkReply::NoError ? QString() : m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        const qint64 size = m_file.size();
        const quint32 crc = m_crc ^ 0xffffffff;
        QByteArray digest;
        for (int shift = 24; shift >= 0; shift -= 8)
            digest.append(char(crc >> shift));
        const bool valid = file.contains("sha256")
            ? m_hash.result().toHex() == file.value("sha256").toString().toLatin1()
            : digest.toBase64() == file.value("checksum").toString().toLatin1();
        m_file.close();
        if (!failure.isEmpty() || size != file.value("size").toInteger() || !valid) {
            finish(failure.isEmpty() ? tr("Size or checksum mismatch: %1").arg(file.value("name").toString()) : failure);
            return;
        }
        ++m_index;
        nextFile();
    });
}

void ModelManager::receive() {
    if (!m_reply)
        return;
    const QByteArray data = m_reply->readAll();
    if (m_file.write(data) != data.size()) {
        finish(m_file.errorString());
        return;
    }
    if (m_model.value("files").toArray()[m_index].toObject().contains("sha256"))
        m_hash.addData(data);
    else
        m_crc = crc32c(m_crc, data);
    m_received += data.size();
    emit progressChanged();
}

void ModelManager::finish(const QString &message) {
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_file.close();
    m_staging.reset();
    m_model = {};
    emit downloadChanged();
    if (!message.isEmpty())
        emit error(message);
}

void ModelManager::cancel() { finish(); }

void ModelManager::remove(const QString &name) {
    for (const auto &entry : catalog()) {
        const auto model = entry.toMap();
        if (model.value("name") != name || !model.value("installed").toBool() || downloading() == name)
            continue;
        m_captions->unloadModel(name);
        if (!QDir(QDir(m_captions->modelsDir()).filePath(name)).removeRecursively())
            emit error(tr("Could not delete %1.").arg(name));
        refresh();
        return;
    }
}
