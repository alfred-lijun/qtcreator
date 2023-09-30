/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "sessionview.h"

#include "session.h"

#include <utils/algorithm.h>

#include <QHeaderView>
#include <QItemSelection>
#include <QStringList>
#include <QStyledItemDelegate>

namespace ProjectExplorer {
namespace Internal {

// custom item delegate class
class RemoveItemFocusDelegate : public QStyledItemDelegate
{
public:
    RemoveItemFocusDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {
    }

protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

void RemoveItemFocusDelegate::paint(QPainter* painter, const QStyleOptionViewItem & option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_HasFocus;
    QStyledItemDelegate::paint(painter, opt, index);
}

SessionView::SessionView(QWidget *parent)
    : Utils::TreeView(parent)
{
    setItemDelegate(new RemoveItemFocusDelegate(this));
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setWordWrap(false);
    setRootIsDecorated(false);
    setSortingEnabled(true);

    setModel(&m_sessionModel);
    sortByColumn(0, Qt::AscendingOrder);

    // Ensure that the full session name is visible.
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    QItemSelection firstRow(m_sessionModel.index(0,0), m_sessionModel.index(
        0, m_sessionModel.columnCount() - 1));
    selectionModel()->select(firstRow, QItemSelectionModel::QItemSelectionModel::
        SelectCurrent);

    connect(this, &Utils::TreeView::activated, [this](const QModelIndex &index){
        emit sessionActivated(m_sessionModel.sessionAt(index.row()));
    });
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, [this] {
        emit sessionsSelected(selectedSessions());
    });

    connect(&m_sessionModel, &SessionModel::sessionSwitched,
        this, &SessionView::sessionSwitched);
    connect(&m_sessionModel, &SessionModel::modelReset,
        this, &SessionView::selectActiveSession);
    connect(&m_sessionModel, &SessionModel::sessionCreated,
        this, &SessionView::selectSession);
 }

void SessionView::createNewSession()
{
    m_sessionModel.newSession(this);
}

void SessionView::deleteSelectedSessions()
{
    deleteSessions(selectedSessions());
}

void SessionView::deleteSessions(const QStringList &sessions)
{
    m_sessionModel.deleteSessions(sessions);
}

void SessionView::cloneCurrentSession()
{
    m_sessionModel.cloneSession(this, currentSession());
}

void SessionView::renameCurrentSession()
{
    m_sessionModel.renameSession(this, currentSession());
}

void SessionView::switchToCurrentSession()
{
    m_sessionModel.switchToSession(currentSession());
}

QString SessionView::currentSession()
{
    return m_sessionModel.sessionAt(selectionModel()->currentIndex().row());
}

SessionModel *SessionView::sessionModel()
{
    return &m_sessionModel;
}

void SessionView::selectActiveSession()
{
    selectSession(SessionManager::activeSession());
}

void SessionView::selectSession(const QString &sessionName)
{
    int row = m_sessionModel.indexOfSession(sessionName);
    selectionModel()->setCurrentIndex(model()->index(row, 0),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void SessionView::showEvent(QShowEvent *event)
{
    Utils::TreeView::showEvent(event);
    selectActiveSession();
    setFocus();
}

void SessionView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() != Qt::Key_Delete && event->key() != Qt::Key_Backspace) {
        TreeView::keyPressEvent(event);
        return;
    }
    const QStringList sessions = selectedSessions();
    if (!sessions.contains("default") && !Utils::anyOf(sessions,
            [](const QString &session) { return session == SessionManager::activeSession(); })) {
        deleteSessions(sessions);
    }
}

QStringList SessionView::selectedSessions() const
{
    return Utils::transform(selectionModel()->selectedRows(), [this](const QModelIndex &index) {
        return m_sessionModel.sessionAt(index.row());
    });
}

} // namespace Internal
} // namespace ProjectExplorer
