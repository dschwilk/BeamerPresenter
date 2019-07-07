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

#ifndef DRAWSLIDE_H
#define DRAWSLIDE_H

#include <QObject>
#include <QDebug>
#include "mediaslide.h"
#include "drawpath.h"

// TODO: move pen strokes to different layer (extra widget) above multimedia widgets

class DrawSlide : public MediaSlide
{
    Q_OBJECT
public:
    explicit DrawSlide(QWidget* parent=nullptr) : MediaSlide(parent) {}
    explicit DrawSlide(Poppler::Page* page, QWidget* parent=nullptr) : MediaSlide(page, parent) {}
    ~DrawSlide() override {clearAll();}
    void clearPageAnnotations();
    void clearAllAnnotations();
    virtual void clearCache() override;
    void setSize(DrawTool const tool, int const size);
    QMap<QString, QMap<DrawTool, QList<DrawPath>>> const& getPaths() const {return paths;}
    int const& getXshift() const {return shiftx;}
    int const& getYshift() const {return shifty;}
    double const& getResolution() const {return resolution;}
    DrawTool getTool() const {return tool;}
    QMap<DrawTool, int> const& getSizes() const {return sizes;}
    int getSize(DrawTool const tool) {return sizes[tool];}
    void setSizes(QMap<DrawTool, int> const& newSizes) {sizes = newSizes;}

protected:
    void drawAnnotations(QPainter& painter);
    virtual void paintEvent(QPaintEvent*) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    void erase(QPointF const& point);
    DrawTool tool = None;
    QMap<QString, QMap<DrawTool, QList<DrawPath>>> paths;
    QPointF pointerPosition = QPointF();
    QImage enlargedPage;
    int enlargedPageNumber;
    QMap<DrawTool, int> sizes = {{Magnifier,120}, {Torch,80}, {Pointer,10}, {Highlighter,30}, {RedPen,3}, {GreenPen,3}, {Eraser,10}};

public slots:
    virtual void togglePointerVisibility() override;
    void setPaths(QString const page, DrawTool const tool, QList<DrawPath> const& list, int const xshift, int const yshift, double const resolution);
    void setPointerPosition(QPointF const point, int const xshift, int const yshift, double const resolution);
    void setTool(DrawTool const newtool);
    void relax();

signals:
    void pointerPositionChanged(QPointF const point, int const xshift, int const yshift, double const resolution);
    void pathsChanged(QString const page, DrawTool const tool, QList<DrawPath> const& list, int const xshift, int const yshift, double const resolution);
    void sendToolChanged(DrawTool const tool);
    void sendRelax();
};

#endif // DRAWSLIDE_H