/*
 *  Copyright (c) 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_scratch_pad_event_filter.h"
#include "kis_scratch_pad.h"
#include <QWidget>
#include <QDebug>


KisScratchPadEventFilter::KisScratchPadEventFilter(QWidget *parent)
    : QObject(parent),
      m_tabletPressed(false),
      m_pressedButton(Qt::NoButton)
{
    parent->installEventFilter(this);
    m_scratchPad = qobject_cast<KisScratchPad *>(parent);
}

void KisScratchPadEventFilter::setWidgetToDocumentTransform(const QTransform &transform)
{
    m_widgetToDocument = transform;
}

QWidget* KisScratchPadEventFilter::parentWidget()
{
    return static_cast<QWidget*>(parent());
}

KoPointerEvent* KisScratchPadEventFilter::createMouseEvent(QEvent *event)
{
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    return new KoPointerEvent(mouseEvent, m_widgetToDocument.map(mouseEvent->pos()));
}

KoPointerEvent* KisScratchPadEventFilter::createTabletEvent(QEvent *event)
{
    QTabletEvent *tabletEvent = static_cast<QTabletEvent*>(event);
    const QPointF pos = tabletEvent->posF();

    KoPointerEvent *ev = new KoPointerEvent(tabletEvent, m_widgetToDocument.map(pos));
    ev->setTabletButton(Qt::LeftButton);

    return ev;
}

bool KisScratchPadEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    bool result = false;

    QScopedPointer<KoPointerEvent> ev;


    switch(event->type()) {
    case QEvent::MouseButtonPress:
        if (m_pressedButton != Qt::NoButton) break;

        ev.reset(createMouseEvent(event));
        m_pressedButton = ev->button();
        m_scratchPad->pointerPress(ev.data());
        result = true;
        break;
    case QEvent::MouseButtonRelease:
        ev.reset(createMouseEvent(event));
        if (ev->button() != m_pressedButton) break;

        m_pressedButton = Qt::NoButton;
        m_scratchPad->pointerRelease(ev.data());
        result = true;
        break;
    case QEvent::MouseMove:
        if(m_tabletPressed) break;
        ev.reset(createMouseEvent(event));
        m_scratchPad->pointerMove(ev.data());
        result = true;
        break;
    case QEvent::TabletPress:
        if (m_pressedButton != Qt::NoButton) break;

        m_tabletPressed = true;
        ev.reset(createTabletEvent(event));
        m_pressedButton = ev->button();
        m_scratchPad->pointerPress(ev.data());
        result = true;
        break;
    case QEvent::TabletRelease:
        ev.reset(createTabletEvent(event));
        if (ev->button() != m_pressedButton) break;

        m_pressedButton = Qt::NoButton;
        m_scratchPad->pointerRelease(ev.data());
        m_tabletPressed = false;
        result = true;
        break;
    case QEvent::TabletMove:
        ev.reset(createTabletEvent(event));
        m_scratchPad->pointerMove(ev.data());
        result = true;
        break;
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::Show:
    case QEvent::Hide:
        m_scratchPad->resetState();
        m_tabletPressed = false;
        m_pressedButton = Qt::NoButton;
        break;
    default:
        result = false;
    }

    if (ev) {
        result = ev->isAccepted();
        event->setAccepted(result);
    }

    return result;
}
