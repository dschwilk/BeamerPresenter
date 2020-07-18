#include "src/slideview.h"
#include "src/rendering/pixcache.h"
#include "src/slidescene.h"
#include "src/preferences.h"
#include "src/pdfmaster.h"

SlideView::SlideView(SlideScene *scene, PixCache *cache, QWidget *parent) :
    QGraphicsView(scene, parent),
    GuiWidget(WidgetType::Slide)
{
    setAttribute(Qt::WA_AcceptTouchEvents);
    setMinimumSize(4, 3);
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    cache->updateFrame(size());
    connect(this, &SlideView::requestPage, cache, &PixCache::requestPage, Qt::QueuedConnection);
    connect(cache, &PixCache::pageReady, this, &SlideView::pageReady, Qt::QueuedConnection);
    connect(this, &SlideView::resizeCache, cache, &PixCache::updateFrame, Qt::QueuedConnection);
    QPalette newpalette = palette();
    newpalette.setColor(QPalette::Base, Qt::black);
    newpalette.setColor(QPalette::Background, Qt::black);
    setPalette(newpalette);
}

void SlideView::pageChanged(const int page, const QSizeF &pageSize)
{
    qreal resolution;
    if (pageSize.width() * height() > pageSize.height() * width())
        // page is too wide, determine resolution by x direction
        resolution = width() / pageSize.width();
    else
        // page is too high, determine resolution by y direction
        resolution = height() / pageSize.height();
    resetTransform();
    scale(resolution, resolution);
    waitingForPage = page;
    qDebug() << "Request page" << page << this;
    emit requestPage(page, resolution);
}

void SlideView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->drawPixmap(scene()->sceneRect(), currentPixmap, currentPixmap.rect());
}

void SlideView::pageReady(const QPixmap pixmap, const int page)
{
    if (waitingForPage != page)
        return;
    currentPixmap = pixmap;
    waitingForPage = -1;
    updateScene({sceneRect()});
}

void SlideView::resizeEvent(QResizeEvent *event)
{
    emit resizeCache(event->size());
}

void SlideView::keyPressEvent(QKeyEvent *event)
{
    emit sendKeyEvent(event);
}

const QSizeF SlideView::preferedSize(const QSizeF &parent_size) const
{
    if (prefered_size.isEmpty())
        return prefered_size;
    QSizeF boundary = parent_size;
    if (prefered_size.width() > 0.)
        boundary.rwidth() *= prefered_size.width();
    if (prefered_size.height() > 0.)
        boundary.rheight() *= prefered_size.height();
    const QSizeF reference = scene()->sceneRect().size();
    const qreal aspect = reference.width() / reference.height();
    if (aspect * boundary.height() > boundary.width())
        // page is wider than available geometry.
        boundary.setHeight(boundary.width()/aspect);
    else
        // page is higher than available geometry.
        boundary.setWidth(boundary.height()*aspect);
    qDebug() << parent_size << boundary << this;
    return boundary;
}
