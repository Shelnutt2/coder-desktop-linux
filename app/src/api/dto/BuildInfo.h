#ifndef CODER_DTO_BUILDINFO_H
#define CODER_DTO_BUILDINFO_H

#include <QJsonObject>
#include <QMetaType>
#include <QString>

/// Coder API build/deployment info — GET /api/v2/buildinfo
struct BuildInfo {
    Q_GADGET
    Q_PROPERTY(QString version MEMBER version)
    Q_PROPERTY(QString dashboardUrl MEMBER dashboardUrl)
    Q_PROPERTY(bool externalAuthEnabled MEMBER externalAuthEnabled)

public:
    QString version;
    QString dashboardUrl;
    bool externalAuthEnabled = false;

    static BuildInfo fromJson(const QJsonObject& obj) {
        BuildInfo b;
        b.version = obj.value(QLatin1String("version")).toString();
        b.dashboardUrl = obj.value(QLatin1String("dashboard_url")).toString();
        b.externalAuthEnabled = obj.value(QLatin1String("external_auth_enabled")).toBool();
        return b;
    }
};

Q_DECLARE_METATYPE(BuildInfo)

#endif  // CODER_DTO_BUILDINFO_H
