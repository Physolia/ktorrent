/*
    SPDX-FileCopyrightText: 2005-2007 Joris Guisson <joris.guisson@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "searchtoolbar.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextStream>
#include <QtGlobal>

#include <KActionCollection>
#include <KComboBox>
#include <KCompletion>
#include <KGuiItem>
#include <KIconLoader>
#include <KLocalizedString>
#include <KMainWindow>
#include <KStandardGuiItem>
#include <KToolBarLabelAction>

#include "searchenginelist.h"
#include "searchplugin.h"
#include "searchpluginsettings.h"
#include <interfaces/functions.h>
#include <util/fileops.h>
#include <util/log.h>

using namespace bt;

namespace kt
{
SearchToolBar::SearchToolBar(KActionCollection *ac, kt::SearchEngineList *sl, QObject *parent)
    : QObject(parent)
    , m_current_search_engine(0)
{
    m_search_text = new KComboBox(nullptr);
    m_search_text->setEditable(true);
    m_search_text->setMaxCount(20);
    m_search_text->setInsertPolicy(QComboBox::NoInsert);
    m_search_text->setMinimumWidth(150);

    QLineEdit *search_text_lineedit = new QLineEdit(m_search_text);
    search_text_lineedit->setClearButtonEnabled(true);
    m_search_text->setLineEdit(search_text_lineedit);

    connect(m_search_text->lineEdit(), &QLineEdit::returnPressed, this, &SearchToolBar::searchBoxReturn);
    connect(m_search_text->lineEdit(), &QLineEdit::textChanged, this, &SearchToolBar::textChanged);

    QWidgetAction *search_text_action = new QWidgetAction(this);
    search_text_action->setText(i18n("Search Text"));
    search_text_action->setDefaultWidget(m_search_text);
    ac->addAction(QLatin1String("search_text"), search_text_action);

    m_search_new_tab = new QAction(QIcon::fromTheme(QLatin1String("edit-find")), i18n("Search"), this);
    connect(m_search_new_tab, &QAction::triggered, this, &SearchToolBar::searchNewTabPressed);
    m_search_new_tab->setEnabled(false);
    ac->addAction(QLatin1String("search"), m_search_new_tab);

    QWidgetAction *search_engine_action = new QWidgetAction(this);
    search_engine_action->setText(i18n("Search Engine"));
    m_search_engine = new QComboBox(nullptr);
    search_engine_action->setDefaultWidget(m_search_engine);
    ac->addAction(QLatin1String("search_engine"), search_engine_action);
    connect(m_search_engine, &QComboBox::currentIndexChanged, this, &SearchToolBar::selectedEngineChanged);

    QWidgetAction *search_engine_label_action = new QWidgetAction(this);
    search_engine_label_action->setText(i18n("Search Engine Label"));
    QLabel *l = new QLabel(i18n(" Engine: "), nullptr);
    search_engine_label_action->setDefaultWidget(l);
    ac->addAction(QLatin1String("search_engine_label"), search_engine_label_action);

    loadSearchHistory();

    m_search_engine->setModel(sl);
    m_search_engine->setCurrentIndex(SearchPluginSettings::searchEngine());
}

SearchToolBar::~SearchToolBar()
{
}

void SearchToolBar::selectedEngineChanged(int idx)
{
    if (idx < 0) {
        if (m_current_search_engine < 0 || m_current_search_engine >= m_search_engine->model()->rowCount())
            m_current_search_engine = 0;

        m_search_engine->setCurrentIndex(m_current_search_engine);
    } else
        m_current_search_engine = idx;
}

int SearchToolBar::currentSearchEngine() const
{
    return m_search_engine->currentIndex();
}

void SearchToolBar::saveSettings()
{
    SearchPluginSettings::setSearchEngine(m_search_engine->currentIndex());
    SearchPluginSettings::self()->save();
}

void SearchToolBar::searchBoxReturn()
{
    QString str = m_search_text->currentText();
    KCompletion *comp = m_search_text->completionObject();
    if (!m_search_text->contains(str)) {
        comp->addItem(str);
        m_search_text->addItem(str);
    }
    m_search_text->lineEdit()->clear();
    saveSearchHistory();
    Q_EMIT search(str, m_search_engine->currentIndex(), SearchPluginSettings::openInExternal());
}

void SearchToolBar::searchNewTabPressed()
{
    searchBoxReturn();
}

void SearchToolBar::textChanged(const QString &str)
{
    m_search_new_tab->setEnabled(str.length());
}

void SearchToolBar::loadSearchHistory()
{
    QFile fptr(kt::DataDir() + QLatin1String("search_history"));
    if (!fptr.open(QIODevice::ReadOnly))
        return;

    KCompletion *comp = m_search_text->completionObject();

    Uint32 cnt = 0;
    QTextStream in(&fptr);
    while (!in.atEnd() && cnt < 50) {
        QString line = in.readLine();
        if (line.isEmpty())
            break;

        if (!m_search_text->contains(line)) {
            comp->addItem(line);
            m_search_text->addItem(line);
        }
        cnt++;
    }

    m_search_text->lineEdit()->clear();
}

void SearchToolBar::saveSearchHistory()
{
    QFile fptr(kt::DataDir() + QLatin1String("search_history"));
    if (!fptr.open(QIODevice::WriteOnly))
        return;

    QTextStream out(&fptr);
    KCompletion *comp = m_search_text->completionObject();
    const QStringList items = comp->items();
    for (const QString &s : items) {
        out << s << Qt::endl;
    }
}

void SearchToolBar::clearHistory()
{
    bt::Delete(kt::DataDir() + QLatin1String("search_history"), true);
    KCompletion *comp = m_search_text->completionObject();
    m_search_text->clear();
    comp->clear();
}

}

#include "moc_searchtoolbar.cpp"
