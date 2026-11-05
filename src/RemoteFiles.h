#pragma once

#include <QMap>
#include <QObject>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class ContestRules;

// Downloads the data files of contest.json that have a "url" (e.g. GitHub raw URLs)
// into the rules folder. If the rules folder is not writable, the copy goes to the user data
// folder (ContestRules::userDataCopy) and is read from there.
// A file is replaced only when the download is valid (HTTP 200, non-empty, plain text, < 5 MB);
// otherwise the local copy is kept, so qTLF works offline during the contest.
class RemoteFiles : public QObject
{
    Q_OBJECT
public:
    explicit RemoteFiles(QObject *parent = nullptr);

    bool isRunning() const { return m_pending > 0; }
    void update(const ContestRules &rules);     // asynchronous; emits finished()

signals:
    // updated: files written; failed: "file: reason"
    void finished(const QStringList &updated, const QStringList &unchanged, const QStringList &failed);

private:
    void onReply(QNetworkReply *reply, const QString &file, const QString &rulesPath,
                 const QString &userCopy, const QString &currentPath);
    void done();

    QNetworkAccessManager *m_nam;
    int m_pending = 0;
    QStringList m_updated, m_unchanged, m_failed;
};
