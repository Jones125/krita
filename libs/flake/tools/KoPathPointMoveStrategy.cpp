/* This file is part of the KDE project
 * Copyright (C) 2006 Jan Hambrecht <jaham@gmx.net>
 * Copyright (C) 2006 Thorsten Zachmann <zachmann@kde.org>
 * Copyright (C) 2007 Thomas Zander <zander@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "KoPathPointMoveStrategy.h"
#include "KoInteractionStrategy_p.h"

#include "commands/KoPathPointMoveCommand.h"
#include "KoPathTool.h"
#include "KoPathToolSelection.h"
#include "KoSnapGuide.h"
#include "KoCanvasBase.h"
#include "kis_global.h"
#include "kis_command_utils.h"

KoPathPointMoveStrategy::KoPathPointMoveStrategy(KoPathTool *tool, const QPointF &pos)
    : KoInteractionStrategy(*(new KoInteractionStrategyPrivate(tool))),
    m_originalPosition(pos),
    m_tool(tool)
{
}

KoPathPointMoveStrategy::~KoPathPointMoveStrategy()
{
}

void KoPathPointMoveStrategy::handleMouseMove(const QPointF &mouseLocation, Qt::KeyboardModifiers modifiers)
{
    QPointF newPosition = m_tool->canvas()->snapGuide()->snap(mouseLocation, modifiers);
    QPointF move = newPosition - m_originalPosition;

    if (modifiers & Qt::ShiftModifier) {
        // Limit change to one direction only
        move = snapToClosestAxis(move);
    }

    KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());
    if (! selection)
        return;

    KoPathPointMoveCommand cmd(selection->selectedPointsData(), move - m_move);
    cmd.redo();
    m_move = move;
}

void KoPathPointMoveStrategy::finishInteraction(Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);
}

KUndo2Command* KoPathPointMoveStrategy::createCommand()
{
    KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());
    if (! selection)
        return 0;

    KUndo2Command *cmd = 0;
    if (!m_move.isNull()) {
        cmd = new KisCommandUtils::SkipFirstRedoWrapper(new KoPathPointMoveCommand(selection->selectedPointsData(), m_move));
    }
    return cmd;
}
