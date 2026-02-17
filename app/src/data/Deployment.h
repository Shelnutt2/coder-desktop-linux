#ifndef DEPLOYMENT_H
#define DEPLOYMENT_H

#include <QDateTime>
#include <QString>

/// Data struct describing a single Coder deployment the user has logged into.
/// Tokens are NOT stored here — they live in SecureStorage.
struct Deployment {
    QString id;
    QString name;
    QString url;
    QString username;
    QString avatarUrl;
    bool isActive = false;
    QDateTime addedAt;
};

#endif  // DEPLOYMENT_H
