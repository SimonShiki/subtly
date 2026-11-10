#pragma once
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTemporaryDir>
#include <QVariantList>
#include <memory>

class CaptionModel;
class QNetworkReply;

class ModelManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY catalogChanged)
    Q_PROPERTY(QString downloading READ downloading NOTIFY downloadChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    explicit ModelManager(CaptionModel *captions, QObject *parent = nullptr);
    ~ModelManager() override;
    QVariantList catalog() const;
    QString downloading() const { return m_model.value("id").toString(); }
    double progress() const { return m_total ? double(m_received) / m_total : 0; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void download(const QString &name);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void remove(const QString &name);

signals:
    void catalogChanged();
    void downloadChanged();
    void progressChanged();
    void error(const QString &message);

private:
    void nextFile();
    void receive();
    void finish(const QString &error = {});
    CaptionModel *m_captions;
    QJsonArray m_catalog;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QTemporaryDir> m_staging;
    QFile m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    quint32 m_crc = 0xffffffff;
    QJsonObject m_model;
    int m_index = 0;
    qint64 m_total = 0, m_received = 0;
};
