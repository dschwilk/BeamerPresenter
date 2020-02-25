/*
 * This file is part of BeamerPresenter.
 * Copyright (C) 2020  stiglers-eponym

 * BeamerPresenter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * BeamerPresenter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with BeamerPresenter. If not, see <https://www.gnu.org/licenses/>.
 */

#include "pathoverlay.h"
#include "../slide/drawslide.h"
#include "../names.h"

/// This function is required for sorting and searching in a QMap.
bool operator<(FullDrawTool tool1, FullDrawTool tool2)
{
    if (tool1.tool < tool2.tool)
        return true;
    if (tool1.tool > tool2.tool)
        return false;
    if (tool1.size < tool2.size)
        return true;
    if (tool1.size > tool2.size)
        return false;
    // TODO: Does operaror<(QRgb, QRgb) together with == define a total order?
    if (tool1.color.rgb() < tool2.color.rgb())
        return true;
    return false;
}


PathOverlay::PathOverlay(DrawSlide* parent) :
    QWidget(parent),
    master(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_AlwaysStackOnTop);
    if (!master->isPresentation())
        setMouseTracking(true);
}

PathOverlay::~PathOverlay()
{
    clearAllAnnotations();
    delete enlargedPageRenderer;
}

void PathOverlay::clearAllAnnotations()
{
    for (QMap<QString, QList<DrawPath*>>::iterator it=paths.begin(); it!=paths.end(); it++) {
        qDeleteAll(*it);
        it->clear();
    }
    paths.clear();
    end_cache = -1;
    if (!pixpaths.isNull())
        pixpaths = QPixmap();
    update();
}

void PathOverlay::clearPageAnnotations()
{
    end_cache = -1;
    if (!pixpaths.isNull())
        pixpaths = QPixmap();
    if (master->page != nullptr && paths.contains(master->page->label())) {
        qDeleteAll(paths[master->page->label()]);
        paths[master->page->label()].clear();
        update();
        if (tool.tool == Magnifier)
            updateEnlargedPage();
    }
}

void PathOverlay::paintEvent(QPaintEvent*)
{
    if (master->isShowingTransition())
        return;
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (end_cache >= 0)
        painter.drawPixmap(0, 0, pixpaths);
    if (master->page == nullptr)
        return;
    painter.setRenderHint(QPainter::Antialiasing);
    drawPaths(painter, master->page->label());
    switch (tool.tool) {
    case Pointer:
        painter.setCompositionMode(QPainter::CompositionMode_Darken);
        painter.setPen(QPen(tool.color, tool.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPoint(pointerPosition);
        break;
    case Torch:
        if (!pointerPosition.isNull()) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            QPainterPath rectpath;
            rectpath.addRect(master->shiftx, master->shifty, master->pixmap.width(), master->pixmap.height());
            QPainterPath circpath;
            circpath.addEllipse(pointerPosition, tool.size, tool.size);
            painter.fillPath(rectpath-circpath, tool.color);
        }
        break;
    case Magnifier:
        if (!pointerPosition.isNull() && !enlargedPage.isNull()) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setClipping(true);
            QPainterPath path;
            path.addEllipse(pointerPosition, tool.size, tool.size);
            painter.setClipPath(path, Qt::ReplaceClip);
            painter.drawPixmap(QRectF(pointerPosition.x()-tool.size, pointerPosition.y()-tool.size, 2*tool.size, 2*tool.size),
                              enlargedPage,
                              QRectF(tool.extras.magnification*pointerPosition.x() - tool.size, tool.extras.magnification*pointerPosition.y() - tool.size, 2*tool.size, 2*tool.size));
            painter.setPen(QPen(tool.color, 2));
            painter.drawEllipse(pointerPosition, tool.size, tool.size);
        }
        break;
    default:
        break;
    }
}

void PathOverlay::rescale(qint16 const oldshiftx, qint16 const oldshifty, double const oldRes)
{
    end_cache = -1;
    enlargedPage = QPixmap();
    delete enlargedPageRenderer;
    enlargedPageRenderer = nullptr;
    QPointF shift = QPointF(master->shiftx, master->shifty) - master->resolution/oldRes*QPointF(oldshiftx, oldshifty);
    for (QMap<QString, QList<DrawPath*>>::iterator page_it = paths.begin(); page_it != paths.end(); page_it++)
        for (QList<DrawPath*>::iterator path_it = page_it->begin(); path_it != page_it->end(); path_it++)
            (*path_it)->transform(shift, master->resolution/oldRes);
}

void PathOverlay::setTool(FullDrawTool const& newtool)
{
    tool = newtool;
    if (tool.tool == Magnifier && tool.extras.magnification < 1e-12)
        tool.extras.magnification = defaultToolConfig[Magnifier].extras.magnification;
    if (tool.size <= 1e-12)
        tool.size = defaultToolConfig[tool.tool].size;
    if (!tool.color.isValid())
        tool.color = defaultToolConfig[tool.tool].color;
    // TODO: fancy cursors
    if (cursor() == Qt::BlankCursor && tool.tool != Pointer)
        setMouseTracking(false);
    if (tool.tool == Torch) {
        enlargedPage = QPixmap();
        pointerPosition = QPointF();
    }
    else if (tool.tool == Pointer) {
        enlargedPage = QPixmap();
        setMouseTracking(true);
        if (underMouse()) {
            pointerPosition = mapFromGlobal(QCursor::pos());
            emit pointerPositionChanged(pointerPosition, master->shiftx, master->shifty, master->resolution);
        }
    }
    else if (tool.tool == Magnifier) {
        pointerPosition = QPointF();
        if (enlargedPage.isNull())
            updateEnlargedPage();
    }
    else
        enlargedPage = QPixmap();
    update();
}

void PathOverlay::updatePathCache()
{
    if (master->page == nullptr)
        return;
    if (paths[master->page->label()].isEmpty()) {
        end_cache = -1;
        if (!pixpaths.isNull())
            pixpaths = QPixmap();
    }
    else {
        if (pixpaths.isNull())
            end_cache = -1;
        if (end_cache == -1) {
            pixpaths = QPixmap(size());
            pixpaths.fill(QColor(0,0,0,0));
        }
        QPainter painter;
        painter.begin(&pixpaths);
        painter.setRenderHint(QPainter::Antialiasing);
        drawPaths(painter, master->page->label(), false, true);
    }
}

void PathOverlay::drawPaths(QPainter &painter, QString const label, bool const animation, bool const toCache)
{
    // TODO: reorganize the different conditions (especially animation)
    if (animation)
        painter.setClipRect(master->shiftx, master->shifty, width()-2*master->shiftx, height()-2*master->shifty);

    // Draw edges of the slide: If they are not drawn explicitly, they can be transparent.
    // Drawing with the highlighter on transparent edges can look ugly.
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    painter.drawPixmap(master->shiftx, master->shifty, width(), 1, master->pixmap, 0, 0, width(), 1);
    painter.drawPixmap(master->shiftx, height() - master->shifty - 1, width(), 1, master->pixmap, 0, master->pixmap.height()-1, width(), 1);
    painter.drawPixmap(master->shiftx, master->shifty, 1, height(), master->pixmap, 0, 0, 1, height());
    painter.drawPixmap(width() - master->shiftx - 1, master->shifty, 1, height(), master->pixmap, master->pixmap.width()-1, 0, 1, height());

    // Draw the paths.
    if (paths.contains(label)) {
        QList<DrawPath*>::const_iterator path_it = paths[label].cbegin();
        if (!animation) {
            // If end_cache >= 0: some paths have been drawn already. Skip them.
            if (label == master->page->label() && end_cache > 0)
                path_it += end_cache;
        }
        // Iterate over all remaining paths.
        for (; path_it!=paths[label].cend(); path_it++) {
            FullDrawTool const& tool = (*path_it)->getTool();
            switch (tool.tool) {
            case Pen:
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
                painter.setPen(QPen(tool.color, tool.size));
                painter.drawPolyline((*path_it)->data(), (*path_it)->number());
                break;
            case Highlighter:
            {
                if (!animation) {
                    // Highlighter needs a background to draw on (because of CompositionMode_Darken).
                    // Drawing this background is only reasonable if there is no video widget in the background.
                    // Check this.
                    QRectF const outer = (*path_it)->getOuterDrawing();
                    if (hasVideoOverlap(outer)) {
                        if (toCache) {
                            end_cache = path_it - paths[label].cbegin();
                            qDebug() << "Stopped caching paths:" << end_cache;
                            return;
                        }
                    }
                    else {
                        // Draw the background form master->pixmap.
                        painter.setClipRect(master->shiftx, master->shifty, width()-2*master->shiftx, height()-2*master->shifty);
                        painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
                        painter.drawPixmap(outer, master->pixmap, QRectF(outer.x()-master->shiftx, outer.y()-master->shifty, outer.width(), outer.height()));
                    }
                }
                // Draw the highlighter path.
                painter.setCompositionMode(QPainter::CompositionMode_Darken);
                painter.setPen(QPen(tool.color, tool.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawPolyline((*path_it)->data(), (*path_it)->number());
                if (!animation)
                    painter.setClipRect(rect());
            }
                break;
            default:
                break;
            }
        }
    }
    if (animation)
        painter.setClipRect(rect());
    if (toCache)
        end_cache = paths[master->page->label()].length();
}

bool PathOverlay::hasVideoOverlap(QRectF const& rect) const
{
    if (master->videoPositions.isEmpty())
        return false;
    for (auto pos: master->videoPositions) {
        if (rect.intersects(pos))
            return true;
    }
    return false;
}

void PathOverlay::mousePressEvent(QMouseEvent *event)
{
    if (master->page == nullptr)
        return;
    switch (event->buttons())
    {
    case Qt::LeftButton:
        // Left mouse button is used for painting.
        switch (tool.tool)
        {
        case Pen:
        case Highlighter:
            if (!paths.contains(master->page->label()))
                paths[master->page->label()] = QList<DrawPath*>();
            paths[master->page->label()].append(new DrawPath(tool, event->localPos()));
            break;
        case Eraser:
            erase(event->localPos());
            update();
            break;
        case Magnifier:
            if (enlargedPage.isNull())
                updateEnlargedPage();
        [[clang::fallthrough]];
        case Torch:
        case Pointer:
            pointerPosition = event->localPos();
            update();
            emit pointerPositionChanged(pointerPosition, master->shiftx, master->shifty, master->resolution);
            break;
        default:
            return;
        }
        break;
    case Qt::RightButton:
        erase(event->localPos());
        update();
        break;
    default:
        break;
    }
    event->accept();
}

void PathOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (master->page == nullptr)
        return;
    switch (event->button())
    {
    case Qt::RightButton:
        updatePathCache();
        emit sendUpdatePathCache();
        if (tool.tool == Magnifier) {
            updateEnlargedPage();
            emit sendUpdateEnlargedPage();
            update();
        }
        event->accept();
        break;
    case Qt::LeftButton:
        switch (tool.tool) {
        case NoTool:
        case Pointer:
            event->ignore();
            break;
        case Torch:
        case Magnifier:
            pointerPosition = QPointF();
            update();
            break;
        case Pen:
        case Highlighter:
            paths[master->page->label()].last()->endDrawing();
            emit pathsChangedQuick(master->page->label(), paths[master->page->label()], master->shiftx, master->shifty, master->resolution);
            update();
            [[clang::fallthrough]];
        case Eraser:
            updatePathCache();
            emit sendUpdatePathCache();
            event->accept();
            break;
        default:
            break;
        }
        break;
    case Qt::MidButton:
        event->ignore();
        break;
    default:
        event->accept();
        break;
    }
    emit sendRelax();
}

void PathOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (master->page == nullptr)
        return;
    if (tool.tool == Pointer) {
        pointerPosition = event->localPos();
        emit pointerPositionChanged(pointerPosition, master->shiftx, master->shifty, master->resolution);
        update();
    }
    switch (event->buttons())
    {
    case Qt::NoButton:
        if (cursor() != Qt::BlankCursor) {
            if (master->hoverLink(event->pos()))
                setCursor(Qt::PointingHandCursor);
            else
                setCursor(Qt::ArrowCursor);
        }
        break;
    case Qt::LeftButton:
        switch (tool.tool)
        {
        case Pen:
        case Highlighter:
            if (!paths[master->page->label()].isEmpty()) {
                paths[master->page->label()].last()->append(event->localPos());
                update();
                emit pathsChangedQuick(master->page->label(), paths[master->page->label()], master->shiftx, master->shifty, master->resolution);
            }
            break;
        case Eraser:
            erase(event->localPos());
            break;
        case Torch:
        case Magnifier:
            pointerPosition = event->localPos();
            update();
            emit pointerPositionChanged(pointerPosition, master->shiftx, master->shifty, master->resolution);
            break;
        case Pointer:
            break;
        default:
            if (cursor() != Qt::BlankCursor) {
                if (master->hoverLink(event->pos()))
                    setCursor(Qt::PointingHandCursor);
                else
                    setCursor(Qt::ArrowCursor);
            }
        }
        break;
    case Qt::RightButton:
        erase(event->localPos());
        break;
    }
    event->accept();
}

void PathOverlay::erase(const QPointF &point)
{
    if (master->page == nullptr || paths[master->page->label()].isEmpty())
        return;
    QList<DrawPath*>& path_list = paths[master->page->label()];
    int const oldsize=path_list.size();
    bool changed = false;
    for (int i=0; i<oldsize; i++) {
        QVector<int> splits = path_list[i]->intersects(point, eraserSize);
        if (splits.isEmpty())
            continue;
        changed = true;
        if (splits.first() > 1)
            path_list.insert(i+1, path_list[i]->split(0, splits.first()-1));
        for (int s=0; s<splits.size()-1; s++) {
            if (splits[s+1]-splits[s] > 3) {
                path_list.insert(i+1, path_list[i]->split(splits[s]+1, splits[s+1]-1));
                s++;
            }
        }
        if (splits.last() < path_list[i]->number()-2)
            path_list.insert(i+1, path_list[i]->split(splits.last()+1, path_list[i]->number()));
        delete path_list[i];
        path_list[i] = nullptr;
    }
    for (int i=0; i<path_list.size();) {
        if (path_list[i] == nullptr)
            path_list.removeAt(i);
        //else if (path_list[i]->isEmpty()) { // this should never happen...
        //    qDebug() << "this should no happen.";
        //    delete path_list[i];
        //    path_list.removeAt(i);
        //}
        else
            i++;
    }
    if (changed) {
        end_cache = -1;
        update();
        emit pathsChanged(master->page->label(), path_list, master->shiftx, master->shifty, master->resolution);
    }
}

void PathOverlay::setPathsQuick(QString const pagelabel, QList<DrawPath*> const& list, qint16 const refshiftx, qint16 const refshifty, double const refresolution)
{
    QPointF shift = QPointF(master->shiftx, master->shifty) - master->resolution/refresolution*QPointF(refshiftx, refshifty);
    int const diff = list.length() - paths[pagelabel].length();
    if (diff == 0) {
        if (!paths[pagelabel].last()->update(*list.last(), shift, master->resolution/refresolution))
            setPaths(pagelabel, list, refshiftx, refshifty, refresolution);
    }
    else if (diff == 1)
        paths[pagelabel].append(new DrawPath(*list.last(), shift, master->resolution/refresolution));
    else if (diff < 0) {
        if (-diff >= paths[pagelabel].length()) {
            qDeleteAll(paths[pagelabel]);
            paths[pagelabel].clear();
        }
        else {
            for (int i=diff; i++<0;)
                delete paths[pagelabel].takeLast();
        }
        end_cache = -1;
        pixpaths = QPixmap();
    }
    else {
        qDebug() << "set paths quick failed!" << this;
        setPaths(pagelabel, list, refshiftx, refshifty, refresolution);
    }
    update();
}

void PathOverlay::setPaths(QString const pagelabel, QList<DrawPath*> const& list, qint16 const refshiftx, qint16 const refshifty, double const refresolution)
{
    QPointF shift = QPointF(master->shiftx, master->shifty) - master->resolution/refresolution*QPointF(refshiftx, refshifty);
    if (!paths.contains(pagelabel)) {
        paths[pagelabel] = QList<DrawPath*>();
        for (QList<DrawPath*>::const_iterator it = list.cbegin(); it!=list.cend(); it++)
            paths[pagelabel].append(new DrawPath(**it, shift, master->resolution/refresolution));
    }
    else {
        // Basic assumption: If list and paths[pagelabel] both contain two elements, then these elements appear in the same order in both lists.
        QList<DrawPath*>::const_iterator new_it=list.cbegin();
        QList<DrawPath*>::iterator old_it=paths[pagelabel].begin();
        for (;new_it<list.cend() && old_it<paths[pagelabel].end() && (*new_it)->getHash() == (*old_it)->getHash(); new_it++, old_it++) {}
        if (old_it >= paths[pagelabel].end()-1) {
            if (old_it == paths[pagelabel].end()-1) {
                delete *old_it;
                paths[pagelabel].pop_back();
            }
            while (new_it < list.cend())
                paths[pagelabel].append(new DrawPath(**(new_it++), shift, master->resolution/refresolution));
        }
        else {
            // create look up table for new hashs
            QMap<quint32, QList<DrawPath*>::const_iterator> newHashs;
            for (QList<DrawPath*>::const_iterator it=new_it; it<list.cend(); it++)
                newHashs[(*it)->getHash()] = it;
            // adjust paths
            for (QList<DrawPath*>::const_iterator next; old_it<paths[pagelabel].end();) {
                // next new path which exists already in the old paths:
                next = newHashs.value((*old_it)->getHash(), list.cend());
                if (next == list.cend()) {
                    delete *old_it;
                    old_it = paths[pagelabel].erase(old_it);
                }
                else {
                    while (new_it < next)
                        old_it = paths[pagelabel].insert(old_it, new DrawPath(**(new_it++), shift, master->resolution/refresolution)) + 1;
                    new_it++;
                    old_it++;
                }
            }
            while (new_it < list.cend())
                paths[pagelabel].append(new DrawPath(**(new_it++), shift, master->resolution/refresolution));
        }
    }
    end_cache = -1;
    updatePathCache();
    update();
}

void PathOverlay::setPointerPosition(QPointF const point, qint16 const refshiftx, qint16 const refshifty, double const refresolution)
{
    pointerPosition = (point - QPointF(refshiftx, refshifty)) * master->resolution/refresolution + QPointF(master->shiftx, master->shifty);
    if (tool.tool == Magnifier && enlargedPage.isNull())
        updateEnlargedPage();
    if (tool.tool == Pointer || tool.tool == Magnifier || tool.tool == Torch)
        update();
}

void PathOverlay::relax()
{
    if (tool.tool == Torch || tool.tool == Magnifier) {
        pointerPosition = QPointF();
        update();
    }
}

void PathOverlay::updateEnlargedPage()
{
    // Check whether an update is required.
    if (tool.tool != Magnifier || master->page == nullptr || tool.extras.magnification < 1e-12) {
        if (!enlargedPage.isNull())
            enlargedPage = QPixmap();
        return;
    }
    // Create enlargedPageRenderer if necessary.
    if (enlargedPageRenderer == nullptr) {
        enlargedPageRenderer = new SingleRenderer(master->doc, master->pagePart, this);
        enlargedPageRenderer->changeResolution(tool.extras.magnification*master->resolution);
        connect(enlargedPageRenderer, &BasicRenderer::cacheThreadFinished, this, &PathOverlay::updateEnlargedPage);
    }
    // Render page using enlargedPageRenderer if necessary (the rendering is done in a separate thread).
    if (enlargedPageRenderer->getPage() != master->pageIndex) {
        enlargedPage = QPixmap();
        qDebug() << "Rendering enlarged page" << master->pageIndex;
        enlargedPageRenderer->renderPage(master->pageIndex);
        // Return if the enlarged page image is not needed right now.
        // This makes scanning through the slides much faster.
        if (QApplication::mouseButtons() != Qt::LeftButton)
            return;
    }
    // Draw enlargedPage.
    enlargedPage = QPixmap(tool.extras.magnification*size());
    enlargedPage.fill(QColor(0,0,0,0));
    QPainter painter;
    painter.begin(&enlargedPage);
    // Draw the slide.
    if (enlargedPageRenderer->resultReady())
        // If a rendered page is ready in enlargedPageRenderer: show it in enlargedPage.
        painter.drawPixmap(
                    int(tool.extras.magnification*master->shiftx),
                    int(tool.extras.magnification*master->shifty),
                    enlargedPageRenderer->getPixmap()
                    );
    else
        // Otherwise: show a scaled version of the page image.
        painter.drawPixmap(
                    int(tool.extras.magnification*master->shiftx),
                    int(tool.extras.magnification*master->shifty),
                    master->pixmap.scaled(tool.extras.magnification*master->pixmap.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                    );
    // Draw annotations.
    painter.setRenderHint(QPainter::Antialiasing);
    if (paths.contains(master->page->label())) {
        for (QList<DrawPath*>::const_iterator path_it=paths[master->page->label()].cbegin(); path_it!=paths[master->page->label()].cend(); path_it++) {
            FullDrawTool const& tool = (*path_it)->getTool();
            switch (tool.tool) {
            case Pen:
            {
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
                painter.setPen(QPen(tool.color, this->tool.extras.magnification*tool.size));
                DrawPath tmp(**path_it, QPointF(0,0), this->tool.extras.magnification);
                painter.drawPolyline(tmp.data(), tmp.number());
                break;
            }
            case Highlighter:
            {
                painter.setCompositionMode(QPainter::CompositionMode_Darken);
                painter.setPen(QPen(tool.color, this->tool.extras.magnification*tool.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                DrawPath tmp(**path_it, QPointF(0,0), this->tool.extras.magnification);
                painter.drawPolyline(tmp.data(), tmp.number());
                break;
            }
            default:
                break;
            }
        }
    }
    update();
}

void PathOverlay::saveDrawings(QString const& filename, QString const& notefile) const
{
    // Deprecated
    // Save drawings in a strange data format.
    qWarning() << "The binary file type is deprecated.";
    qWarning() << "The saved file will be unreadable for later versions of BeamerPresenter!";
    QFile file(filename);
    file.open(QIODevice::WriteOnly);
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_0);
    stream  << static_cast<quint32>(0x2CA7D9F8)
            << static_cast<quint16>(stream.version())
            << QFileInfo(master->doc->getPath()).absoluteFilePath()
            << QFileInfo(notefile).absoluteFilePath();
    if (stream.status() == QDataStream::WriteFailed) {
        qCritical() << "Failed to write to file: File is not writable.";
        return;
    }
    {
        QMap<quint16, quint16> newsizes;
        for (QMap<DrawTool, FullDrawTool>::const_iterator size_it=defaultToolConfig.cbegin(); size_it!=defaultToolConfig.cend(); size_it++)
            newsizes[static_cast<quint16>(size_it.key())] = quint16(size_it->size+0.5);
        stream << newsizes;
    }
    stream << quint16(paths.size());
    qint16 const w = qint16(width()-2*master->shiftx), h = qint16(height()-2*master->shifty);
    for (QMap<QString, QList<DrawPath*>>::const_iterator page_it=paths.cbegin(); page_it!=paths.cend(); page_it++) {
        stream << page_it.key() << quint16(page_it->length());
        for (QList<DrawPath*>::const_iterator path_it=page_it->cbegin(); path_it!=page_it->cend(); path_it++) {
            QVector<float> vec;
            (*path_it)->toIntVector(vec, master->shiftx, master->shifty, w, h);
            stream << static_cast<quint16>((*path_it)->getTool().tool) << (*path_it)->getTool().color << vec;
        }
    }
    if (stream.status() != QDataStream::Ok) {
        qCritical() << "Error occurred while writing to file.";
        return;
    }
}

void PathOverlay::loadXML(QString const& filename, PdfDoc const* notesDoc)
{
    // Load drawings from (compressed) XML.
    // TODO: use gunzip and open Xournal files directly.
    qInfo() << "Loading files is experimental. Files might contain errors or might be unreadable for later versions of BeamerPresenter";
    QFile file(filename);
    if (!file.exists()) {
        qCritical() << "Loading file failed: file does not exist.";
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Loading file failed: file is not readable.";
        return;
    }
    QDomDocument doc("BeamerPresenter");
    if (!doc.setContent(&file)) {
        file.close();
        file.open(QIODevice::ReadOnly);
        if (!doc.setContent(qUncompress(file.readAll()))) {
            file.close();
            loadDrawings(filename);
            return;
        }
    }
    QDomElement const root = doc.documentElement();
    if (root.attribute("creator").contains("beamerpresenter", Qt::CaseInsensitive)) {

        // Check whether the presentation file is as expected and warn otherwise.
        {
            QDomElement const pres = root.firstChildElement("presentation");
            QFileInfo const& presentation_fileinfo = QFileInfo(master->doc->getPath());
            if (pres.attribute("file") != presentation_fileinfo.absoluteFilePath())
                qWarning() << "This drawing file was generated for a different PDF file path.";
            if (pres.attribute("modified") != presentation_fileinfo.lastModified().toString("yyyy-MM-dd hh:mm:ss"))
                qWarning() << "The presentation file has been modified since writing the drawing file.";
            if (pres.attribute("pages").toInt() != master->doc->getDoc()->numPages())
                qWarning() << "The numbers of pages in the presentation and drawing file do not match!";
        }

        // Check whether the notes file is as expected and warn otherwise.
        {
            QDomElement const notes = root.firstChildElement("notes");
            QFileInfo const& notes_fileinfo = QFileInfo(notesDoc->getPath());
            if (notes.attribute("file") != notes_fileinfo.absoluteFilePath())
                qWarning() << "This drawing file was generated for a different PDF file path.";
            if (notes.attribute("modified") != notes_fileinfo.lastModified().toString("yyyy-MM-dd hh:mm:ss"))
                qWarning() << "The notes file has been modified since writing the drawing file.";
            if (notes.attribute("pages").toInt() != notesDoc->getDoc()->numPages())
                qWarning() << "The numbers of pages in the notes and drawing file do not match!";
        }

        for (QDomElement page_element = root.firstChildElement("page"); !page_element.isNull(); page_element = page_element.nextSiblingElement("page")) {
            QString const label = page_element.attribute("label");
            qreal scale;
            QPoint shift;
            Poppler::Page const* page = master->doc->getPage(label);
            QSizeF size;
            if (page == nullptr)
                size = master->page->pageSizeF();
            else
                size = page->pageSizeF();
            if (size.width() * height() >= width() * size.height()) {
                shift.setY(( height() - size.height()/size.width() * width() )/2);
                scale = width() / size.width();
            }
            else {
                shift.setX((width() - size.width()/size.height() * height() )/2);
                scale = height() / size.height();
            }
            if (paths.contains(label)) {
                qDeleteAll(paths[label]);
                paths[label].clear();
            }
            else
                paths[label] = QList<DrawPath*>();
            for (QDomElement stroke = page_element.firstChildElement("stroke"); !stroke.isNull(); stroke = stroke.nextSiblingElement("stroke")) {
                DrawTool const tool = toolNames.key(stroke.attribute("tool"), NoTool);
                if (tool != NoTool) {
                    QColor const color = QColor(stroke.attribute("color"));
                    bool ok;
                    qreal size = stroke.attribute("width").toDouble(&ok);
                    if (!ok)
                        size = defaultToolConfig[tool].size;
                    QStringList const data = stroke.text().split(" ");
                    paths[label].append(new DrawPath({tool, color, size}, data, shift, scale));
                }
            }
            emit pathsChanged(label, paths[label], master->shiftx, master->shifty, master->resolution);
        }

    }
    else if (root.attribute("creator").contains("xournal", Qt::CaseInsensitive)) {
        // Try to import strokes from uncompressed Xournal or Xournal++ file
        // Get filename and compare it to presentation file.
        {
            QDomElement const first_page = root.firstChildElement("page");
            if (!first_page.isNull()) {
                QDomElement const first_bg = first_page.firstChildElement("background");
                if (!first_bg.isNull() && first_bg.hasAttribute("filename")) {
                    QFileInfo const& presentation_fileinfo = QFileInfo(master->doc->getPath());
                    if (first_bg.attribute("filename") != presentation_fileinfo.absoluteFilePath())
                        qWarning() << "This Xournal(++) file uses a different PDF file path.";
                }
            }
        }
        for (QDomElement page_element = root.firstChildElement("page"); !page_element.isNull(); page_element = page_element.nextSiblingElement("page")) {
            QDomElement bg = page_element.firstChildElement("background");
            if (bg.isNull())
                continue;
            QDomElement layer = page_element.firstChildElement("layer");
            if (layer.isNull() || !layer.hasChildNodes())
                continue;
            bool ok;
            int const pageno = bg.attribute("pageno").remove(QRegExp("[a-z]")).toInt(&ok) - 1;
            if (!ok)
                continue;
            QString const label = master->doc->getLabel(pageno);
            // TODO: handle text.
            qreal scale;
            QPoint shift;
            Poppler::Page const* page = master->doc->getPage(label);
            QSizeF size;
            if (page == nullptr)
                size = master->page->pageSizeF();
            else
                size = page->pageSizeF();
            if (size.width() * height() >= width() * size.height()) {
                shift.setY(( height() - size.height()/size.width() * width() )/2);
                scale = width() / size.width();
            }
            else {
                shift.setX((width() - size.width()/size.height() * height() )/2);
                scale = height() / size.height();
            }
            for (QDomElement stroke = layer.firstChildElement("stroke"); !stroke.isNull(); stroke = stroke.nextSiblingElement("stroke")) {
                // This requires that tool names are compatible with those used by Xournal(++).
                // But since the only stroke tools are "pen" and "highlighter", this is not a problem.
                DrawTool const tool = toolNames.key(stroke.attribute("tool"), NoTool);
                if (tool != NoTool) {
                    QString colorstr = stroke.attribute("color");
                    // Colors are saved by xournal in the format #RRGGBBAA, but Qt uses #AARRGGBB.
                    // Try to convert between the two formats.
                    if (colorstr[0] == '#' && colorstr.size() == 9) {
                        colorstr.insert(1, colorstr.mid(7));
                        colorstr.truncate(9);
                    }
                    QColor const color = QColor(colorstr);
                    bool ok;
                    qreal size = stroke.attribute("width").toDouble(&ok);
                    if (!ok)
                        size = defaultToolConfig[tool].size;
                    QStringList const data = stroke.text().split(" ");
                    qDebug() << data;
                    paths[label].append(new DrawPath({tool, color, size}, data, shift, scale));
                }
            }
            emit pathsChanged(label, paths[label], master->shiftx, master->shifty, master->resolution);
        }
    }
    else {
        qWarning() << "Could not understand file: Unknown creator" << root.attribute("creator");
    }
    file.close();
    update();
}

void PathOverlay::saveXML(QString const& filename, PdfDoc const* notedoc, bool const compress) const
{
    // Save drawings in compressed XML.
    qInfo() << "Saving files is experimental. Files might contain errors or might be unreadable for later versions of BeamerPresenter";
    QDomDocument doc("BeamerPresenter");
    QDomElement root = doc.createElement("BeamerPresenter");
    root.setAttribute("creator", "BeamerPresenter");
    root.setAttribute("version", APP_VERSION);
    doc.appendChild(root);

    QString const presentation_file = QFileInfo(master->doc->getPath()).absoluteFilePath();
    QString const notes_file = QFileInfo(notedoc->getPath()).absoluteFilePath();

    QDomElement pres = doc.createElement("presentation");
    pres.setAttribute("file", presentation_file);
    pres.setAttribute("pages", master->doc->getDoc()->numPages());
    pres.setAttribute("modified", master->doc->getLastModified().toString("yyyy-MM-dd hh:mm:ss"));
    root.appendChild(pres);

    QDomElement notes = doc.createElement("notes");
    notes.setAttribute("file", notes_file);
    notes.setAttribute("pages", notedoc->getDoc()->numPages());
    notes.setAttribute("modified", notedoc->getLastModified().toString("yyyy-MM-dd hh:mm:ss"));
    root.appendChild(notes);

    qDebug() << "adding pages:" << paths.size();
    for (QMap<QString, QList<DrawPath*>>::const_iterator page_it=paths.cbegin(); page_it!=paths.cend(); page_it++) {
        QDomElement page_element = doc.createElement("page");
        page_element.setAttribute("label", page_it.key());
        root.appendChild(page_element);
        Poppler::Page const* page = master->doc->getPage(page_it.key());
        /// size of the page in points
        QSizeF size;
        if (page == nullptr)
            size = master->page->pageSizeF();
        else
            size = page->pageSizeF();
        /// scale page in points / pixel
        qreal scale;
        /// upper right corner of the page, in pixels
        QPoint shift;
        if (size.width() * height() >= width() * size.height()) {
            shift.setY(( height() - size.height()/size.width() * width() )/2);
            scale = size.width() / width();
        }
        else {
            shift.setX((width() - size.width()/size.height() * height() )/2);
            scale = size.height() / height();
        }
        for (QList<DrawPath*>::const_iterator path_it=page_it->cbegin(); path_it!=page_it->cend(); path_it++) {
            QStringList stringList;
            (*path_it)->toText(stringList, shift, scale);
            QDomElement stroke = doc.createElement("stroke");
            page_element.appendChild(stroke);
            FullDrawTool const& tool = (*path_it)->getTool();
            stroke.setAttribute("tool", toolNames.value(tool.tool, "unkown"));
            stroke.setAttribute("color", tool.color.name());
            stroke.setAttribute("width", tool.size*scale);
            QDomText data = doc.createTextNode(stringList.join(" "));
            stroke.appendChild(data);
        }
    }

    QFile file(filename);
    file.open(QIODevice::WriteOnly);
    if (compress)
        file.write(qCompress(doc.toByteArray()));
        // In order to unzip this compressed file using zlib, you need to remove the first four bytes:
        // tail -c+5 compressed.bp | zlib-flate -uncompress > uncompressed.bp
    else
        file.write(doc.toByteArray());
    file.close();
}

void PathOverlay::loadDrawings(QString const& filename)
{
    // Deprecated
    // Load drawings from the strange data format.
    QFile file(filename);
    if (!file.exists()) {
        qCritical() << "Loading file failed: file does not exist.";
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Loading file failed: file is not readable.";
        return;
    }
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_0);
    {
        quint32 magic;
        stream >> magic;
        if (magic != 0x2CA7D9F8) {
            qCritical() << "Invalid file: wrong magic number.";
            return;
        }
    }
    qWarning() << "The binary file type of this file is deprecated.";
    qWarning() << "This file will be unreadable for later versions of BeamerPresenter!";
    {
        quint16 version;
        stream >> version;
        stream.setVersion(version);
    }
    {
        QString docpath, notepath;
        stream >> docpath >> notepath;
        if (stream.status() != QDataStream::Ok) {
            qCritical() << "Failed to load file: File is corrupt.";
            return;
        }
        if (docpath != master->doc->getPath())
            qWarning() << "This drawing file was generated for a different PDF file path.";
    }
    {
        QMap<quint16, quint16> newsizes;
        stream >> newsizes;
        if (stream.status() != QDataStream::Ok) {
            qCritical() << "Failed to load file: File is corrupt.";
            return;
        }
    }
    quint16 npages, npaths;
    stream >> npages;
    if (stream.status() != QDataStream::Ok) {
        qCritical() << "Failed to load file: File is corrupt.";
        return;
    }
    clearAllAnnotations();
    qint16 const w = qint16(width()-2*master->shiftx), h = qint16(height()-2*master->shifty);
    QString pagelabel;
    quint16 tool;
    QColor color;
    for (int pageidx=0; pageidx<npages; pageidx++) {
        stream >> pagelabel >> npaths;
        if (stream.status() != QDataStream::Ok) {
            qCritical() << "Interrupted reading file: File is corrupt.";
            break;
        }
        paths[pagelabel] = QList<DrawPath*>();
        for (int pathidx=0; pathidx<npaths; pathidx++) {
            QVector<float> vec;
            stream >> tool >> color >> vec;
            if (stream.status() != QDataStream::Ok) {
                qCritical() << "Interrupted reading file: File is corrupt.";
                break;
            }
            paths[pagelabel].append(new DrawPath({static_cast<DrawTool>(tool), color, defaultToolConfig[static_cast<DrawTool>(tool)].size}, vec, master->shiftx, master->shifty, w, h));
        }
        emit pathsChanged(pagelabel, paths[pagelabel], master->shiftx, master->shifty, master->resolution);
    }
    update();
}

void PathOverlay::drawPointer(QPainter& painter)
{
    painter.setOpacity(1.);
    painter.setCompositionMode(QPainter::CompositionMode_Darken);
    if (tool.tool == Pointer) {
        painter.setPen(QPen(QColor(255,0,0,191), tool.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPoint(pointerPosition);
    }
    else if (tool.tool == Torch && !pointerPosition.isNull()) {
        QPainterPath rectpath;
        rectpath.addRect(master->shiftx, master->shifty, master->pixmap.width(), master->pixmap.height());
        QPainterPath circpath;
        circpath.addEllipse(pointerPosition, tool.size, tool.size);
        painter.fillPath(rectpath-circpath, QColor(0,0,0,48));
    }
}

void PathOverlay::undoPath()
{
    if (!paths[master->page->label()].isEmpty()) {
        undonePaths.append(paths[master->page->label()].takeLast());
        end_cache = -1;
        pixpaths = QPixmap();
        repaint();
        emit pathsChangedQuick(master->page->label(), paths[master->page->label()], master->shiftx, master->shifty, master->resolution);
    }
}

void PathOverlay::redoPath()
{
    if (!undonePaths.isEmpty()) {
        paths[master->page->label()].append(undonePaths.takeLast());
        repaint();
        emit pathsChangedQuick(master->page->label(), paths[master->page->label()], master->shiftx, master->shifty, master->resolution);
    }
}

void PathOverlay::resetCache()
{
     end_cache = -1;
     updatePathCache();
     if (tool.tool != Magnifier) {
         delete enlargedPageRenderer;
         enlargedPageRenderer = nullptr;
     }
}

void PathOverlay::togglePointerVisibility()
{
    if (cursor() == Qt::BlankCursor)
        showPointer();
    else
        hidePointer();
}

void PathOverlay::showPointer()
{
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);
}

void PathOverlay::hidePointer()
{
    setCursor(Qt::BlankCursor);
    if (tool.tool != Pointer)
        setMouseTracking(false);
}
