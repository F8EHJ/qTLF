#include "RemoteFiles.h"

#include "ContestRules.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

static constexpr qint64 MaxSize = 5 * 1024 * 1024;

RemoteFiles::RemoteFiles(QObject *parent) : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

void RemoteFiles::update(const ContestRules &rules)
{
    if (m_pending > 0 || rules.remoteFiles.isEmpty())
        return;
    m_updated.clear();
    m_unchanged.clear();
    m_failed.clear();
    for (auto it = rules.remoteFiles.cbegin(); it != rules.remoteFiles.cend(); ++it) {
        QNetworkRequest req{QUrl(it.value())};
        req.setHeader(QNetworkRequest::UserAgentHeader, "qTLF");
        req.setTransferTimeout(15000);
        QNetworkReply *reply = m_nam->get(req);
        ++m_pending;
        const QString file = it.key();
        const QString rulesPath = rules.filePath(file);
        const QString userCopy = rules.userDataCopy(file);
        const QString currentPath = rules.dataFilePath(file);
        connect(reply, &QNetworkReply::finished, this, [this, reply, file, rulesPath, userCopy, currentPath] {
            onReply(reply, file, rulesPath, userCopy, currentPath);
        });
    }
}

static bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);                       // atomic: the old file stays if anything fails
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(data);
    return f.commit();
}

void RemoteFiles::onReply(QNetworkReply *reply, const QString &file, const QString &rulesPath,
                          const QString &userCopy, const QString &currentPath)
{
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        m_failed << QString("%1: %2").arg(file, reply->errorString());
        return done();
    }
    const QByteArray data = reply->readAll();
    const QByteArray head = data.left(512).trimmed().toLower();
    if (status != 200)
        m_failed << QString("%1: HTTP %2").arg(file).arg(status);
    else if (data.isEmpty())
        m_failed << QString("%1: empty file").arg(file);
    else if (data.size() > MaxSize)
        m_failed << QString("%1: too large").arg(file);
    else if (data.contains('\0'))
        m_failed << QString("%1: not a text file").arg(file);
    else if (head.startsWith("<!doctype") || head.startsWith("<html"))
        m_failed << QString("%1: got a web page — use the \"raw\" URL").arg(file);
    else {
        // content of the copy used today (rules folder, or newer user-data copy)
        QByteArray current;
        QFile cf(currentPath);
        if (cf.open(QIODevice::ReadOnly))
            current = cf.readAll();
        if (current == data) {
            m_unchanged << file;
        } else if (writeFile(rulesPath, data)) {
            QFile::remove(userCopy);            // the rules folder copy is now the newest
            m_updated << file;
        } else if (writeFile(userCopy, data)) {
            m_updated << file + " (user data folder)";
        } else {
            m_failed << QString("%1: cannot write").arg(file);
        }
    }
    done();
}

void RemoteFiles::done()
{
    if (--m_pending == 0)
        emit finished(m_updated, m_unchanged, m_failed);
}
