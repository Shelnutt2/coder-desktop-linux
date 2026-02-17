#ifndef CODER_DTO_USER_H
#define CODER_DTO_USER_H

#include <QJsonObject>
#include <QMetaType>
#include <QString>

/// Coder API user object — GET /api/v2/users/me
struct User {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString username MEMBER username)
    Q_PROPERTY(QString email MEMBER email)
    Q_PROPERTY(QString avatarUrl MEMBER avatarUrl)

public:
    QString id;
    QString username;
    QString email;
    QString avatarUrl;

    static User fromJson(const QJsonObject& obj) {
        User u;
        u.id = obj.value(QLatin1String("id")).toString();
        u.username = obj.value(QLatin1String("username")).toString();
        u.email = obj.value(QLatin1String("email")).toString();
        u.avatarUrl = obj.value(QLatin1String("avatar_url")).toString();
        return u;
    }
};

Q_DECLARE_METATYPE(User)

#endif  // CODER_DTO_USER_H
