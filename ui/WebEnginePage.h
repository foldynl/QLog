#ifndef QLOG_UI_WEBENGINEPAGE_H
#define QLOG_UI_WEBENGINEPAGE_H

#include <QWebEnginePage>

class WebEnginePage : public QWebEnginePage
{
public:
    explicit WebEnginePage(QObject *parent = nullptr);

protected:
    bool acceptNavigationRequest(const QUrl &url,
                                 QWebEnginePage::NavigationType type,
                                 bool isMainFrame) override;
    void javaScriptConsoleMessage(QWebEnginePage::JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override;
};

#endif // QLOG_UI_WEBENGINEPAGE_H
