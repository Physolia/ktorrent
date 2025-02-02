/*
    SPDX-FileCopyrightText: 2005 Joris Guisson <joris.guisson@gmail.com>
    SPDX-FileCopyrightText: 2005 Vincent Wagelaar <vincent@ricardis.tudelft.nl>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QBrush>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QToolTip>

#include <KLocalizedString>

#include <cmath>

#include "chunkbar.h"
#include "chunkbarrenderer.h"
#include <interfaces/torrentinterface.h>
#include <torrent/globals.h>
#include <util/bitset.h>
#include <util/log.h>

using namespace bt;
using namespace kt;

namespace kt
{
#if 0 // KF5
static void FillAndFrameBlack(QImage* image, const QColor& color, int size)
{
    image->fill(color.rgb());
    for (int i = 0; i < size; i++) {
        image->setPixel(0, i, 0);
        image->setPixel(size - 1, i, 0);
        image->setPixel(i, 0, 0);
        image->setPixel(i, size - 1, 0);
    }
}

static void InitializeToolTipImages(ChunkBar* bar)
{
    static bool images_initialized = false;
    if (images_initialized)
        return;
    images_initialized = true;

    Q3MimeSourceFactory* factory = Q3MimeSourceFactory::defaultFactory();

    QImage excluded(16, 16, QImage::Format_RGB32);
    FillAndFrameBlack(&excluded, bar->palette().color(QPalette::Active, QPalette::Mid), 16);
    factory->setImage("excluded_color", excluded);

    QImage available(16, 16, QImage::Format_RGB32);
    FillAndFrameBlack(&available, bar->palette().color(QPalette::Active, QPalette::Highlight), 16);
    factory->setImage("available_color", available);

    QImage unavailable(16, 16, QImage::Format_RGB32);
    FillAndFrameBlack(&unavailable, bar->palette().color(QPalette::Active, QPalette::Base), 16);
    factory->setImage("unavailable_color", unavailable);
}
#endif

ChunkBar::ChunkBar(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(StyledPanel);
    setFrameShadow(Sunken);
    setLineWidth(3);
    setMidLineWidth(3);

#if 0 // KF5
    InitializeToolTipImages(this);
    setToolTip(i18n("<img src=\"available_color\">&nbsp; - Downloaded Chunks<br>"
                    "<img src=\"unavailable_color\">&nbsp; - Chunks to Download<br>"
                    "<img src=\"excluded_color\">&nbsp; - Excluded Chunks"));
#endif
}

ChunkBar::~ChunkBar()
{
}

void ChunkBar::updateBar(bool force)
{
    const BitSet &bs = getBitSet();
    QSize s = contentsRect().size();

    bool changed = !(curr == bs);

    if (changed || pixmap.isNull() || pixmap.width() != s.width() || force) {
        pixmap = QPixmap(s);
        pixmap.fill(palette().color(QPalette::Active, QPalette::Base));
        QPainter painter(&pixmap);
        drawBarContents(&painter);
        update();
    }
}

void ChunkBar::paintEvent(QPaintEvent *ev)
{
    QFrame::paintEvent(ev);
    QPainter p(this);
    drawContents(&p);
}

void ChunkBar::drawContents(QPainter *p)
{
    // first draw background
    bool enable = isEnabled();
    p->setBrush(palette().color(enable ? QPalette::Active : QPalette::Inactive, QPalette::Base));
    p->setPen(Qt::NoPen); // p->setPen(QPen(Qt::red));
    p->drawRect(contentsRect());
    if (enable)
        p->drawPixmap(contentsRect(), pixmap);
}

void ChunkBar::drawBarContents(QPainter *p)
{
    Uint32 w = contentsRect().width();
    const BitSet &bs = getBitSet();
    curr = bs;
    QColor highlight_color = palette().color(QPalette::Active, QPalette::Highlight);
    if (bs.allOn())
        drawAllOn(p, highlight_color, contentsRect());
    else if (curr.getNumBits() > w)
        drawMoreChunksThenPixels(p, bs, highlight_color, contentsRect());
    else
        drawEqual(p, bs, highlight_color, contentsRect());
}

}

#include "moc_chunkbar.cpp"
