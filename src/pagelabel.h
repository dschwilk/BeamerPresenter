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

#ifndef PAGE_H
#define PAGE_H

#include <QtDebug>
#include <QWidget>
#include <QWindow>
#include <QLabel>
#include <QProcess>
#include <QTimer>
#include <QSlider>
#include <QMouseEvent>
#include <QBuffer>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QAudioDeviceInfo>
#include <QDesktopServices>
#include <poppler-qt5.h>
#include "videowidget.h"
#include "mediaslider.h"
#include "pidwidcaller.h"

class PageLabel : public QLabel
{
    Q_OBJECT

public:
    PageLabel(QWidget* parent);
    PageLabel(Poppler::Page* page, QWidget* parent);
    ~PageLabel();
    void renderPage(Poppler::Page* page, bool const setDuration=true, QPixmap* pixmap=nullptr);
    bool hasActiveMultimediaContent() const;
    long int updateCache(Poppler::Page const * page);
    long int updateCache(QPixmap const* pixmap, int const index);
    QPixmap getPixmap(Poppler::Page const * page) const;
    long int clearCachePage(int const index);
    void clearCache();
    void startAllEmbeddedApplications();
    long int getCacheSize() const;
    int getCacheNumber() const {return cache.size();}

    void setMultimediaSliders(QList<MediaSlider*> sliderList);
    void setPresentationStatus(bool const status) {isPresentation=status;}
    void setShowMultimedia(bool const showMultimedia) {this->showMultimedia=showMultimedia;}
    void setUrlSplitCharacter(QString const& splitCharacter) {urlSplitCharacter=splitCharacter;}
    void setPagePart(int const state) {pagePart=state;}
    void setEmbedFileList(const QStringList& files) {embedFileList=files;}

    QPixmap* getCache(int const index);
    int pageNumber() const {return pageIndex;}
    Poppler::Page* getPage() {return page;}
    double getDuration() const {return  duration;}
    bool cacheContains(int const index) {return cache.contains(index);}

private:
    void clearLists();
    void clearProcessCallers();
    QMap<int,QByteArray*> cache;
    QList<Poppler::Link*> links;
    QList<QRect*> linkPositions;
    QList<VideoWidget*> videoWidgets;
    QList<QRect*> videoPositions;
    QList<QMediaPlayer*> soundPlayers;
    QList<QRect*> soundPositions;
    QMap<int,QMediaPlayer*> linkSoundPlayers;
    QList<MediaSlider*> sliders;
    QMap<int,QMap<int,QProcess*>> processes;
    QMap<int,QMap<int,QWidget*>> embeddedWidgets;
    QSet<PidWidCaller*> pidWidCallers;
    QString pid2wid;
    QTimer* processTimer = nullptr;
    QStringList embedFileList;
    QTimer* timer = nullptr;
    double resolution;
    bool isPresentation = true;
    bool showMultimedia = true;
    double autostartDelay = 0.; // delay for starting multimedia content in s
    int minimumAnimationDelay = 40; // minimum frame time in ms
    int pagePart = 0;
    QString urlSplitCharacter = "";
    int minDelayEmbeddedWindows = 50;
    int pageIndex = 0;

protected:
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);

private:
    Poppler::Page* page;
    double duration;
    bool pointer_visible = true;

public slots:
    void togglePointerVisibility();
    void pauseAllMultimedia();
    void startAllMultimedia();
    void createEmbeddedWindow();
    void createEmbeddedWindowsFromPID();
    void receiveWid(WId const wid, int const index);
    void clearProcesses(int const exitCode, QProcess::ExitStatus const exitStatus);
    void setAutostartDelay(double const delay) {autostartDelay=delay;}
    void setAnimationDelay(int const delay_ms) {minimumAnimationDelay=delay_ms;}
    void setPid2Wid(QString const & program) {pid2wid=program;}

signals:
    void sendNewPageNumber(int const pageNumber);
    void requestMultimediaSliders(int const n);
    void sendCloseSignal();
    void focusPageNumberEdit();
    void timeoutSignal();
    void sendShowFullscreen();
    void sendEndFullscreen();
    void slideChange();
};

#endif // PAGE_H
