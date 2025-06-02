#ifndef QLOG_UI_COMPONENT_SHUTDOWNAWAREWIDGET_H
#define QLOG_UI_COMPONENT_SHUTDOWNAWAREWIDGET_H

class ShutdownAwareWidget
{
public:
    virtual ~ShutdownAwareWidget() {};
    virtual void finalizeBeforeAppExit() = 0;
};

#endif // QLOG_UI_COMPONENT_SHUTDOWNAWAREWIDGET_H
