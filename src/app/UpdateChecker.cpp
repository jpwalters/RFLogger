#include "UpdateChecker.h"
#include "Version.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>

static const char* RELEASES_URL =
    "https://api.github.com/repos/jpwalters/RFLogger/releases/latest";

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onReplyFinished);
}

void UpdateChecker::checkForUpdates()
{
    QNetworkRequest request(QUrl(QString::fromLatin1(RELEASES_URL)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("RFLogger/%1").arg(RFLOGGER_VERSION_STRING));
    request.setRawHeader("Accept", "application/vnd.github+json");
    m_networkManager->get(request);
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit checkFailed(tr("Invalid response from GitHub."));
        return;
    }

    const QJsonObject obj = doc.object();
    QString tagName = obj.value(QLatin1String("tag_name")).toString();
    const QString releaseUrl = obj.value(QLatin1String("html_url")).toString();

    if (tagName.isEmpty()) {
        emit checkFailed(tr("No release tag found in response."));
        return;
    }

    if (releaseUrl.isEmpty()) {
        emit checkFailed(tr("No release URL found in response."));
        return;
    }

    // Strip leading 'v' if present
    if (tagName.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        tagName = tagName.mid(1);

    const QString localVersion = QStringLiteral(RFLOGGER_VERSION_STRING);

    if (isNewerVersion(tagName, localVersion))
        emit updateAvailable(tagName, releaseUrl);
    else
        emit upToDate();
}

bool UpdateChecker::isNewerVersion(const QString& remote, const QString& local)
{
    auto toParts = [](const QString& v) -> QList<int> {
        QList<int> parts;
        for (const auto& s : v.split(QLatin1Char('.')))
            parts.append(s.toInt());
        // Pad to at least 3 components
        while (parts.size() < 3)
            parts.append(0);
        return parts;
    };

    const QList<int> r = toParts(remote);
    const QList<int> l = toParts(local);

    for (int i = 0; i < 3; ++i) {
        if (r[i] > l[i]) return true;
        if (r[i] < l[i]) return false;
    }
    return false;
}
