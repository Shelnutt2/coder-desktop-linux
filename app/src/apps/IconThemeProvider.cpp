#include "apps/IconThemeProvider.h"

#include <QIcon>

IconThemeProvider::IconThemeProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

QPixmap IconThemeProvider::requestPixmap(const QString& id, QSize* size,
                                         const QSize& requestedSize) {
    int width = requestedSize.width() > 0 ? requestedSize.width() : 48;
    int height = requestedSize.height() > 0 ? requestedSize.height() : 48;

    QIcon icon = QIcon::fromTheme(id);
    if (icon.isNull()) {
        // Return empty pixmap — QML Image will have status != Ready, triggering fallback emoji.
        if (size) *size = QSize(0, 0);
        return QPixmap();
    }

    QPixmap pixmap = icon.pixmap(QSize(width, height));
    if (size) *size = pixmap.size();
    return pixmap;
}
