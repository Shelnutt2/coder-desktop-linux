#ifndef ICONTHEMEPROVIDER_H
#define ICONTHEMEPROVIDER_H

#include <QQuickImageProvider>

/// Image provider that resolves XDG icon theme names to pixmaps via QIcon::fromTheme().
/// Usage in QML: Image { source: "image://icon-theme/firefox" }
class IconThemeProvider : public QQuickImageProvider {
public:
    IconThemeProvider();

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;
};

#endif  // ICONTHEMEPROVIDER_H
