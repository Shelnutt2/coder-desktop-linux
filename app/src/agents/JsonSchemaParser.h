#ifndef JSONSCHEMAPARSER_H
#define JSONSCHEMAPARSER_H

#include <QJsonObject>
#include <QVariantList>

/// Turns a JSON-schema object into a flat form description consumable by
/// QML (ActionRequiredForm.qml).
///
/// Supported shapes:
/// - The top level must be an object schema ("type": "object", or a missing
///   type with a "properties" map).
/// - Property types: string, number, integer, boolean, and string enums.
///   A "type" given as an array (nullable union) uses its first non-"null"
///   entry.
/// - Nested objects are supported one level deep; their own properties must
///   all be scalar.
///
/// Anything else (arrays, deeper nesting, missing/unknown types, oneOf and
/// friends) makes the whole parse fail so the caller falls back to a raw
/// JSON editor, guaranteeing every schema stays actionable.
namespace JsonSchemaParser {

/// Returns a list of field maps, each with keys:
///   key (QString), type ("string"|"number"|"integer"|"boolean"|"enum"|
///   "object"), description (QString), required (bool),
///   enumOptions (QStringList, enum only), fields (QVariantList, object
///   only, one nested level of the same shape).
/// Returns an empty list and sets *ok to false when the schema is
/// unsupported.
[[nodiscard]] QVariantList parse(const QJsonObject& schema, bool* ok = nullptr);

}  // namespace JsonSchemaParser

#endif  // JSONSCHEMAPARSER_H
