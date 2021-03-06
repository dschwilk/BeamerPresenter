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

#include "tocbutton.h"

TocButton::TocButton(QString const& prefix, QString const& text, int const dest, QWidget* parent) :
    QPushButton(prefix + text, parent),
    dest(dest)
{
    setStyleSheet("Text-align:left");
    connect(this, &TocButton::clicked, this, [&](){emit activated(this->dest);});
#ifdef DISABLE_TOOL_TIP
#else
    setToolTip(text + ", page " + QString::number(dest));
#endif
}

void TocButton::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
        if (menu() == nullptr) {
            emit activated(dest);
        }
        else
            showMenu();
        break;
    case Qt::Key_Space:
        if (menu() == nullptr)
            emit activated(dest);
        else
            showMenu();
        break;
    default:
        QPushButton::keyPressEvent(event);
        break;
    }
    event->ignore();
}
