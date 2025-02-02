/*
    SPDX-FileCopyrightText: 2008 Joris Guisson <joris.guisson@gmail.com>
    SPDX-FileCopyrightText: 2008 Ivan Vasic <ivasic@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opensearchdownloadjob.h"
#include <KIO/CopyJob>
#include <util/fileops.h>
#include <util/log.h>

using namespace bt;

namespace kt
{
OpenSearchDownloadJob::OpenSearchDownloadJob(const QUrl &url, const QString &dir, ProxyHelper *proxy)
    : url(url)
    , dir(dir)
    , m_proxy(proxy)
{
}

OpenSearchDownloadJob::~OpenSearchDownloadJob()
{
}

void OpenSearchDownloadJob::start()
{
    // first try to download the html page
    KIO::StoredTransferJob *j = KIO::storedGet(url, KIO::NoReload, KIO::HideProgressInfo);

    KIO::MetaData metadata = j->metaData();
    m_proxy->ApplyProxy(metadata);
    j->setMetaData(metadata);

    connect(j, &KIO::StoredTransferJob::result, this, &OpenSearchDownloadJob::getFinished);
}

void OpenSearchDownloadJob::startDefault()
{
    // second try to access the xml file
    url.setPath(QLatin1String("/opensearch.xml"));
    start();
}

void OpenSearchDownloadJob::getFinished(KJob *j)
{
    if (j->error()) {
        setError(j->error());
        emitResult();
        return;
    }

    QString str = QString::fromUtf8(((KIO::StoredTransferJob *)j)->data());

    if (url.path() != QStringLiteral("/opensearch.xml")) {
        // try to find the link tags
        QRegExp rx(QLatin1String("<link([^<>]*)"), Qt::CaseInsensitive);
        int pos = 0;

        while ((pos = rx.indexIn(str, pos)) != -1) {
            QString link_tag = rx.cap(1);
            // exit when we find the description
            if (checkLinkTagContent(link_tag))
                return;

            pos += rx.matchedLength();
        }
    } else {
        if (str.contains(QStringLiteral("<OpenSearchDescription")) && str.contains(QStringLiteral("</OpenSearchDescription>"))) {
            if (startXMLDownload(url))
                return;
        }

        setError(KIO::ERR_INTERNAL);
        emitResult();
        return;
    }

    // no link to openSearch xml found in html.
    // make last attempt to access it by <protocol>://<domain>/opensearch.xml
    startDefault();
}

bool OpenSearchDownloadJob::startXMLDownload(const QUrl &url)
{
    if (!bt::Exists(dir)) {
        try {
            bt::MakeDir(dir);
        } catch (...) {
            return false;
        }
    }

    // href is the opensearch description, so lets try to download it
    KIO::Job *j = KIO::copy(url, QUrl::fromLocalFile(dir + QLatin1String("opensearch.xml")), KIO::HideProgressInfo);
    connect(j, &KIO::Job::result, this, &OpenSearchDownloadJob::xmlFileDownloadFinished);
    return true;
}

bool OpenSearchDownloadJob::checkLinkTagContent(const QString &content)
{
    if (htmlParam(QStringLiteral("type"), content) != QLatin1String("application/opensearchdescription+xml"))
        return false;

    QString href = htmlParam(QStringLiteral("href"), content);
    if (href.isEmpty())
        return false;

    if (href.startsWith(QLatin1String("//"))) { // href may point to other domain without protocol like "//not_here.com/search.xml"
        href = url.scheme() + QLatin1Char(':') + href;
    } else if (href.startsWith(QLatin1Char('/'))) {
        href = url.scheme() + QStringLiteral("://") + url.host() + href;
    }

    return startXMLDownload(QUrl(href));
}

QString OpenSearchDownloadJob::htmlParam(const QString &param, const QString &content)
{
    QRegExp rx(QString::fromLatin1("%1=\"?([^\">< ]*)[\" ]").arg(param), Qt::CaseInsensitive);
    if (rx.indexIn(content, 0) == -1)
        return QString();

    return rx.cap(1);
}

void OpenSearchDownloadJob::xmlFileDownloadFinished(KJob *j)
{
    if (j->error()) {
        setError(j->error());
        emitResult();
    } else {
        setError(0);
        emitResult();
    }
}
}

#include "moc_opensearchdownloadjob.cpp"
