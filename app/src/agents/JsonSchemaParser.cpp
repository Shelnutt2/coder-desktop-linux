#include "agents/JsonSchemaParser.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <QVariantMap>

namespace JsonSchemaParser {

namespace {

/// Resolves the "type" keyword; arrays (nullable unions) collapse to the
/// first non-"null" entry. Missing type with "properties" implies object.
QString resolveType(const QJsonObject& schema) {
    const QJsonValue t = schema.value(QLatin1String("type"));
    if (t.isString()) return t.toString().toLower();
    if (t.isArray()) {
        for (const QJsonValue& v : t.toArray()) {
            const QString s = v.toString().toLower();
            if (!s.isEmpty() && s != QLatin1String("null")) return s;
        }
        return {};
    }
    if (schema.contains(QLatin1String("properties"))) return QStringLiteral("object");
    return {};
}

/// Parses a scalar property schema into a field map. Returns false when the
/// property is not a supported scalar.
bool parseScalarField(const QString& key, const QJsonObject& schema, bool required,
                      QVariantMap* out) {
    const QString type = resolveType(schema);

    QVariantMap m;
    m.insert(QStringLiteral("key"), key);
    m.insert(QStringLiteral("description"), schema.value(QLatin1String("description")).toString());
    m.insert(QStringLiteral("required"), required);

    const QJsonValue enumVal = schema.value(QLatin1String("enum"));
    if (enumVal.isArray()) {
        QStringList options;
        for (const QJsonValue& v : enumVal.toArray()) {
            if (v.isString())
                options.append(v.toString());
            else if (v.isDouble())
                options.append(QString::number(v.toDouble()));
            else
                return false;  // non-scalar enum entries are unsupported
        }
        if (options.isEmpty()) return false;
        m.insert(QStringLiteral("type"), QStringLiteral("enum"));
        m.insert(QStringLiteral("enumOptions"), options);
        *out = m;
        return true;
    }

    if (type == QLatin1String("string") || type == QLatin1String("number") ||
        type == QLatin1String("integer") || type == QLatin1String("boolean")) {
        m.insert(QStringLiteral("type"), type);
        *out = m;
        return true;
    }
    return false;
}

/// Parses the properties of an object schema. depth 0 is the top level;
/// nested objects are only allowed at depth 0 and their own members must be
/// scalar. Returns false on any unsupported member.
bool parseObjectFields(const QJsonObject& schema, int depth, QVariantList* out) {
    if (resolveType(schema) != QLatin1String("object")) return false;
    const QJsonValue propsVal = schema.value(QLatin1String("properties"));
    if (!propsVal.isObject()) return false;
    const QJsonObject props = propsVal.toObject();

    QStringList required;
    for (const QJsonValue& v : schema.value(QLatin1String("required")).toArray())
        required.append(v.toString());

    for (auto it = props.begin(); it != props.end(); ++it) {
        if (!it.value().isObject()) return false;
        const QJsonObject propSchema = it.value().toObject();
        const bool isRequired = required.contains(it.key());

        QVariantMap field;
        if (parseScalarField(it.key(), propSchema, isRequired, &field)) {
            out->append(field);
            continue;
        }
        // One nested object level below the top level.
        if (depth == 0 && resolveType(propSchema) == QLatin1String("object")) {
            QVariantList children;
            if (!parseObjectFields(propSchema, depth + 1, &children)) return false;
            field.insert(QStringLiteral("key"), it.key());
            field.insert(QStringLiteral("type"), QStringLiteral("object"));
            field.insert(QStringLiteral("description"),
                         propSchema.value(QLatin1String("description")).toString());
            field.insert(QStringLiteral("required"), isRequired);
            field.insert(QStringLiteral("fields"), children);
            out->append(field);
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

QVariantList parse(const QJsonObject& schema, bool* ok) {
    QVariantList fields;
    const bool parsed = parseObjectFields(schema, 0, &fields) && !fields.isEmpty();
    if (ok) *ok = parsed;
    return parsed ? fields : QVariantList{};
}

}  // namespace JsonSchemaParser
