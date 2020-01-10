/*
 * This file is part of BeamerPresenter.
 * Copyright (C) 2019  stiglers-eponym

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

#include "cachemap.h"

CacheMap::~CacheMap()
{
    cacheTimer->stop();
    cacheThread->requestInterruption();
    cacheThread->wait(10000);
    cacheThread->exit();
    delete cacheThread;
    qDeleteAll(data);
    data.clear();
    delete cacheTimer;
}

CacheMap::CacheMap(CacheMap& other) :
    QObject(other.parent()),
    data(other.data),
    pdf(other.pdf),
    resolution(other.resolution),
    pagePart(other.pagePart),
    renderCommand(other.renderCommand),
    cacheThread(other.cacheThread),
    cacheTimer(other.cacheTimer)
{
    cacheThread->setCacheMap(this);
}

CacheMap::CacheMap(PdfDoc const* doc, PagePart const part, QObject* parent)
    : QObject(parent),
      data(),
      pdf(doc),
      pagePart(part),
      cacheThread(new CacheThread(this, this)),
      cacheTimer(new QTimer(this))
{
    cacheTimer->setSingleShot(true);
    connect(cacheTimer, &QTimer::timeout, this, [&](){cacheTimer->start();}); // TODO: find a more elegant way.
    connect(cacheThread, &CacheThread::finished, this, &CacheMap::receiveBytes);
}

/// Write the pixmap in png format to a QBytesArray at *value(page).
qint64 CacheMap::setPixmap(int const page, QPixmap const* pix)
{
    // Check whether the pixmap is empty.
    if (pix->isNull())
        return 0;
    QByteArray* bytes = new QByteArray();
    QBuffer buffer(bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!pix->save(&buffer, "PNG")) {
        qWarning() << "Rendering failed." << this;
        return 0;
    }
    data[page] = bytes;
    qint64 currentSize = qint64(bytes->size());
    return currentSize;
}

void CacheMap::clearCache()
{
    qDebug() << "Clear cache" << this << parent();
    qDeleteAll(data);
    data.clear();
}

void CacheMap::changeResolution(const double res)
{
    if (res == resolution)
        return;
    qDebug() << "Change resolution" << res << resolution << this << parent();
    clearCache();
    resolution = res;
}

QPixmap const CacheMap::getCachedPixmap(int const page) const
{
    //qDebug() << "get cached page" << page << this << data.contains(page);
    QPixmap pixmap;
    if (data.contains(page))
        pixmap.loadFromData(*data.value(page), "PNG");
    return pixmap;
}

QPixmap const CacheMap::renderPixmap(int const page) const
{
    // This should only be called from within CacheThread and CacheMap!
    Poppler::Page const* cachePage = pdf->getPage(page);
    QImage image = cachePage->renderToImage(72*resolution, 72*resolution);
    if (pagePart == FullPage)
        return QPixmap::fromImage(image);
    else if (pagePart == LeftHalf)
        return QPixmap::fromImage(image.copy(0, 0, image.width()/2, image.height()));
    else
        return QPixmap::fromImage(image.copy(image.width()/2, 0, image.width()/2, image.height()));
}

QPixmap const CacheMap::getPixmap(int const page)
{
    //qDebug() << "get page" << page << this << data.contains(page);
    QPixmap pixmap;
    if (data.contains(page) && data.value(page) != nullptr) {
        pixmap.loadFromData(*data.value(page), "PNG");
        // Check whether pixmap has the correct size.
        QSizeF pageSize = resolution*pdf->getPageSize(page);
        if (pagePart != FullPage)
            pageSize.setWidth(pageSize.width()/2);
        if (abs(pixmap.height() - pageSize.height()) < 2 && abs(pixmap.width() - pageSize.width()) < 2)
            return pixmap;
        qDebug() << "Size changed:" << pixmap.size() << pageSize;
        // The size was wrong. Delete the old cached page.
        emit cacheSizeChanged(-data[page]->size());
        data.remove(page);
    }
    if (resolution <= 0.)
        return pixmap;
    if (renderCommand.isEmpty()) {
        pixmap = renderPixmap(page);
        emit cacheSizeChanged(setPixmap(page, &pixmap));
    }
    else {
        ExternalRenderer* renderer = new ExternalRenderer(page);
        renderer->start(getRenderCommand(page));
        QByteArray const* bytes = nullptr;
        if (renderer->waitForFinished(60000))
            bytes = renderer->getBytes();
        else
            renderer->kill();
        delete renderer;
        pixmap.loadFromData(*bytes, "PNG");
        if (pagePart == FullPage) {
            data[page] = bytes;
            emit cacheSizeChanged(bytes->size());
        }
        else {
            delete bytes;
            if (pagePart == LeftHalf)
                pixmap = pixmap.copy(0, 0, pixmap.width()/2, pixmap.height());
            else
                pixmap = pixmap.copy(pixmap.width()/2, 0, pixmap.width()/2, pixmap.height());
            emit cacheSizeChanged(setPixmap(page, &pixmap));
        }
    }
    return pixmap;
}

QString const CacheMap::getRenderCommand(int const page) const
{
    if (renderCommand.isEmpty())
        return renderCommand;
    QString command = renderCommand;
    command.replace("%file", pdf->getPath());
    command.replace("%page", QString::number(page+1));
    if (pagePart==FullPage)
        command.replace("%width", QString::number(int(resolution*pdf->getPageSize(page).width()+0.5)));
    else
        command.replace("%width", QString::number(int(2*resolution*pdf->getPageSize(page).width()+0.5)));
    command.replace("%height", QString::number(int(resolution*pdf->getPageSize(page).height()+0.5)));
    return command;
}

qint64 CacheMap::clearPage(const int page)
{
    if (!data.contains(page))
        return 0;
    qint64 pageSize(data[page]->size());
    delete data[page];
    data.remove(page);
    return pageSize;
}

void CacheMap::receiveBytes()
{
    QByteArray const* bytes = cacheThread->getBytes();
    if (bytes != nullptr && !bytes->isEmpty()) {
        qint64 size_diff = bytes->size();
        int const page = cacheThread->getPage();
        if (data.contains(page))
            size_diff -= data[page]->size();
        data[page] = bytes;
        emit cacheSizeChanged(size_diff);
    }
    //qDebug() << "Cache thread finished:" << this << parent();
    emit cacheThreadFinished();
}

bool CacheMap::updateCache(int const page)
{
    if (resolution <= 0.)
        return false;
    if (data.contains(page))
        return false;
    cacheThread->setPage(page);
    cacheThread->start();
    return true;
}

qint64 CacheMap::getSizeBytes() const
{
    qint64 size = 0;
    for (QMap<int, QByteArray const*>::const_iterator it=data.cbegin(); it!=data.cend(); it++)
        size += (*it)->size();
    return size;
}