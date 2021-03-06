/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * @file ResvgQt.h
 *
 * Qt wrapper for resvg C-API
 */

#ifndef RESVGQT_H
#define RESVGQT_H

#define RESVG_QT_BACKEND

extern "C" {
#include <resvg.h>
}

#include <QString>
#include <QScopedPointer>
#include <QRectF>
#include <QTransform>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QFile>
#include <QDebug>

namespace ResvgPrivate {

static void initOptions(resvg_options &opt)
{
    resvg_init_options(&opt);

    const auto screens = qApp->screens();
    if (!screens.isEmpty()) {
        const auto screen = screens.at(0);
        opt.dpi = screen->logicalDotsPerInch() * screen->devicePixelRatio();
    }
}

class Data
{
public:
    Data()
    {
        resvg_init_options(&opt);
    }

    ~Data()
    {
        reset();
    }

    void reset()
    {
        if (tree) {
            resvg_tree_destroy(tree);
            tree = nullptr;
        }

        if (opt.path) {
            delete[] opt.path; // do not use free() because was allocated via qstrdup()
            opt.path = NULL;
        }

        initOptions(opt);
        viewBox = QRectF();
        errMsg = QString();
    }

    resvg_render_tree *tree = nullptr;
    resvg_options opt;
    QRectF viewBox;
    QString errMsg;
};

static QString errorToString(const int err)
{
    switch (err) {
        case RESVG_OK :
            return QString(); break;
        case RESVG_ERROR_NOT_AN_UTF8_STR :
            return QLatin1Literal("The SVG content has not an UTF-8 encoding."); break;
        case RESVG_ERROR_FILE_OPEN_FAILED :
            return QLatin1Literal("Failed to open the file."); break;
        case RESVG_ERROR_FILE_WRITE_FAILED :
            return QLatin1Literal("Failed to write to the file."); break;
        case RESVG_ERROR_INVALID_FILE_SUFFIX :
            return QLatin1Literal("Invalid file suffix."); break;
        case RESVG_ERROR_MALFORMED_GZIP :
            return QLatin1Literal("Not a GZip compressed data."); break;
        case RESVG_ERROR_PARSING_FAILED :
            return QLatin1Literal("Failed to parse an SVG data."); break;
        case RESVG_ERROR_NO_CANVAS :
            return QLatin1Literal("Failed to allocate the canvas."); break;
    }

    Q_UNREACHABLE();
}

} //ResvgPrivate

/**
 * @brief QSvgRenderer-like wrapper for resvg C-API
 */
class ResvgRenderer {
public:
    /**
     * @brief Constructs a new renderer.
     */
    ResvgRenderer();

    /**
     * @brief Constructs a new renderer and loads the contents of the SVG(Z) file.
     */
    ResvgRenderer(const QString &filePath);

    /**
     * @brief Constructs a new renderer and loads the SVG data.
     */
    ResvgRenderer(const QByteArray &data);

    /**
     * @brief Destructs the renderer.
     */
    ~ResvgRenderer();

    /**
     * @brief Loads the contents of the SVG(Z) file.
     */
    bool load(const QString &filePath);

    /**
     * @brief Loads the SVG data.
     */
    bool load(const QByteArray &data);

    /**
     * @brief Returns \b true if the file or data were loaded successful.
     */
    bool isValid() const;

    /**
     * @brief Returns an underling error when #isValid is \b false.
     */
    QString errorString() const;

    /**
     * @brief Checks that underling tree has any nodes.
     *
     * #ResvgRenderer and #ResvgRenderer constructors
     * will set an error only if a file does not exist or it has a non-UTF-8 encoding.
     * All other errors will result in an empty tree with a 100x100px size.
     *
     * @return Returns \b true if tree has any nodes.
     */
    bool isEmpty() const;

    /**
     * @brief Returns an SVG size.
     */
    QSize defaultSize() const;

    /**
     * @brief Returns an SVG size.
     */
    QSizeF defaultSizeF() const;

    /**
     * @brief Returns an SVG viewbox.
     */
    QRect viewBox() const;

    /**
     * @brief Returns an SVG viewbox.
     */
    QRectF viewBoxF() const;

    /**
     * @brief Returns bounding rectangle of the item with the given \b id.
     *        The transformation matrix of parent elements is not affecting
     *        the bounds of the element.
     */
    QRectF boundsOnElement(const QString &id) const;

    /**
     * @brief Returns \b true if element with such an ID exists.
     */
    bool elementExists(const QString &id) const;

    /**
     * @brief Returns element's transform.
     */
    QTransform transformForElement(const QString &id) const;

    /**
     * @brief Renders the SVG data to canvas.
     */
    void render(QPainter *p);

    /**
     * @brief Renders the SVG data to canvas with the specified \b bounds.
     *
     * If the bounding rectangle is not specified
     * the SVG file is mapped to the whole paint device.
     */
    void render(QPainter *p, const QRectF &bounds);

    /**
     * @brief Renders the given element with \b elementId on the specified \b bounds.
     *
     * If the bounding rectangle is not specified
     * the SVG element is mapped to the whole paint device.
     */
    void render(QPainter *p, const QString &elementId,
                const QRectF &bounds = QRectF());

    /**
     * @brief Initializes the library log.
     *
     * Use it if you want to see any warnings.
     *
     * Must be called only once.
     *
     * All warnings will be printed to the \b stderr.
     */
    static void initLog();

private:
    QScopedPointer<ResvgPrivate::Data> d;
};

// Implementation.

inline ResvgRenderer::ResvgRenderer()
    : d(new ResvgPrivate::Data())
{
}

inline ResvgRenderer::ResvgRenderer(const QString &filePath)
    : d(new ResvgPrivate::Data())
{
    load(filePath);
}

inline ResvgRenderer::ResvgRenderer(const QByteArray &data)
    : d(new ResvgPrivate::Data())
{
    load(data);
}

inline ResvgRenderer::~ResvgRenderer() {}

inline bool ResvgRenderer::load(const QString &filePath)
{
    // Check for Qt resource path.
    if (filePath.startsWith(QLatin1String(":/"))) {
        QFile file(filePath);
        if (file.open(QFile::ReadOnly)) {
            return load(file.readAll());
        } else {
            return false;
        }
    }

    d->reset();

    const auto utf8Str = filePath.toUtf8();
    const auto rawFilePath = utf8Str.constData();
    d->opt.path = qstrdup(rawFilePath);

    const auto err = resvg_parse_tree_from_file(rawFilePath, &d->opt, &d->tree);
    if (err != RESVG_OK) {
        d->errMsg = ResvgPrivate::errorToString(err);
        return false;
    }

    const auto r = resvg_get_image_viewbox(d->tree);
    d->viewBox = QRectF(r.x, r.y, r.width, r.height);

    return true;
}

inline bool ResvgRenderer::load(const QByteArray &data)
{
    d->reset();

    const auto err = resvg_parse_tree_from_data(data.constData(), data.size(), &d->opt, &d->tree);
    if (err != RESVG_OK) {
        d->errMsg = ResvgPrivate::errorToString(err);
        return false;
    }

    const auto r = resvg_get_image_viewbox(d->tree);
    d->viewBox = QRectF(r.x, r.y, r.width, r.height);

    return true;
}

inline bool ResvgRenderer::isValid() const
{
    return d->tree;
}

inline QString ResvgRenderer::errorString() const
{
    return d->errMsg;
}

inline bool ResvgRenderer::isEmpty() const
{
    if (d->tree)
        return !resvg_is_image_empty(d->tree);
    else
        return true;
}

inline QSize ResvgRenderer::defaultSize() const
{
    return defaultSizeF().toSize();
}

inline QSizeF ResvgRenderer::defaultSizeF() const
{
    if (d->tree)
        return d->viewBox.size();
    else
        return QSizeF();
}

inline QRect ResvgRenderer::viewBox() const
{
    return viewBoxF().toRect();
}

inline QRectF ResvgRenderer::viewBoxF() const
{
    if (d->tree)
        return d->viewBox;
    else
        return QRectF();
}

inline QRectF ResvgRenderer::boundsOnElement(const QString &id) const
{
    if (!d->tree)
        return QRectF();

    const auto utf8Str = id.toUtf8();
    const auto rawId = utf8Str.constData();
    resvg_rect bbox;
    if (resvg_qt_get_node_bbox(d->tree, &d->opt, rawId, &bbox)) {
        return QRectF(bbox.x, bbox.y, bbox.height, bbox.width);
    }

    return QRectF();
}

inline bool ResvgRenderer::elementExists(const QString &id) const
{
    if (!d->tree)
        return false;

    const auto utf8Str = id.toUtf8();
    const auto rawId = utf8Str.constData();
    return resvg_node_exists(d->tree, rawId);
}

inline QTransform ResvgRenderer::transformForElement(const QString &id) const
{
    if (!d->tree)
        return QTransform();

    const auto utf8Str = id.toUtf8();
    const auto rawId = utf8Str.constData();
    resvg_transform ts;
    if (resvg_get_node_transform(d->tree, rawId, &ts)) {
        return QTransform(ts.a, ts.b, ts.c, ts.d, ts.e, ts.f);
    }

    return QTransform();
}

inline void ResvgRenderer::render(QPainter *p)
{
    render(p, QRectF());
}

inline void ResvgRenderer::render(QPainter *p, const QRectF &bounds)
{
    if (!d->tree)
        return;

    const auto r = bounds.isValid() ? bounds : p->viewport();

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const double sx = (double)r.width() / d->viewBox.width();
    const double sy = (double)r.height() / d->viewBox.height();

    p->setTransform(QTransform(sx, 0, 0, sy, r.x(), r.y()), true);

    resvg_size imgSize { (uint)d->viewBox.width(), (uint)d->viewBox.height() };
    resvg_qt_render_to_canvas(d->tree, &d->opt, imgSize, p);

    p->restore();
}

inline void ResvgRenderer::render(QPainter *p, const QString &elementId, const QRectF &bounds)
{
    if (!d->tree)
        return;

    const auto utf8Str = elementId.toUtf8();
    const auto rawId = utf8Str.constData();

    resvg_rect bbox;
    if (!resvg_qt_get_node_bbox(d->tree, &d->opt, rawId, &bbox)) {
        qWarning() << QString(QStringLiteral("Element '%1' has no bounding box.")).arg(elementId);
        return;
    }

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const auto r = bounds.isValid() ? bounds : p->viewport();

    const double sx = (double)r.width() / bbox.width;
    const double sy = (double)r.height() / bbox.height;
    p->setTransform(QTransform(sx, 0, 0, sy, bounds.x(), bounds.y()), true);

    resvg_size imgSize { (uint)bbox.width, (uint)bbox.height };
    resvg_qt_render_to_canvas_by_id(d->tree, &d->opt, imgSize, rawId, p);

    p->restore();
}

inline void ResvgRenderer::initLog()
{
    resvg_init_log();
}

#undef RESVG_QT_BACKEND

#endif // RESVGQT_H
