#pragma once

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    void checkForUpdates();

signals:
    void updateAvailable(const QString& latestVersion, const QString& releaseUrl);
    void upToDate();
    void checkFailed(const QString& errorMessage);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    static bool isNewerVersion(const QString& remote, const QString& local);

    QNetworkAccessManager* m_networkManager;
};
