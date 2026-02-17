#ifndef CODER_DTO_TEMPLATE_H
#define CODER_DTO_TEMPLATE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

/// Coder API template object — GET /api/v2/templates
struct Template {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString displayName MEMBER displayName)
    Q_PROPERTY(QString description MEMBER description)
    Q_PROPERTY(QString icon MEMBER icon)
    Q_PROPERTY(QString activeVersionId MEMBER activeVersionId)
    Q_PROPERTY(QString organizationId MEMBER organizationId)

public:
    QString id;
    QString name;
    QString displayName;
    QString description;
    QString icon;
    QString activeVersionId;
    QString organizationId;

    static Template fromJson(const QJsonObject& obj) {
        Template t;
        t.id = obj.value(QLatin1String("id")).toString();
        t.name = obj.value(QLatin1String("name")).toString();
        t.displayName = obj.value(QLatin1String("display_name")).toString();
        t.description = obj.value(QLatin1String("description")).toString();
        t.icon = obj.value(QLatin1String("icon")).toString();
        t.activeVersionId = obj.value(QLatin1String("active_version_id")).toString();
        t.organizationId = obj.value(QLatin1String("organization_id")).toString();
        return t;
    }

    static QList<Template> listFromJson(const QJsonArray& arr) {
        QList<Template> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(Template)

#endif  // CODER_DTO_TEMPLATE_H
