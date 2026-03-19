#include <QCoreApplication>
#include <QDesktopServices>
#include <QWebEngineProfile>
#include "WebEnginePage.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.webenginepage");

WebEnginePage::WebEnginePage(QObject *parent)
    : QWebEnginePage{parent}
{
    FCT_IDENTIFICATION;

    QString userAgent = QString("%1/%2 (+https://github.com/foldynl/QLog)")
                            .arg(QCoreApplication::applicationName(), VERSION);
    profile()->setHttpUserAgent(userAgent);
}

bool WebEnginePage::acceptNavigationRequest(const QUrl &url,
                                             QWebEnginePage::NavigationType type,
                                             bool isMainFrame)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << url << type << isMainFrame;

    if ( isMainFrame && type == QWebEnginePage::NavigationTypeLinkClicked )
    {
        QDesktopServices::openUrl(url);
        return false;
    }

    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

void WebEnginePage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                             const QString &message,
                                             int lineNumber,
                                             const QString &sourceID)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(lineNumber);
    Q_UNUSED(sourceID);

    qCDebug(runtime)<<"level: " << level <<"; message: "<<message;
}
