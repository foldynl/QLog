#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>

#include <Windows.h>
#include <ShObjIdl.h>
#include <ShlObj.h>
#include <propkey.h>
#include <propvarutil.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.ui.notifications.h>

#include "notification/DesktopNotification.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.notification.windows.desktopnotification");

using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

class WindowsNotificationContext
{
public:
    explicit WindowsNotificationContext(DesktopNotification *owner);

    bool initialize();
    bool show(DesktopNotificationCategory category,
              const QString &title,
              const QString &body,
              bool soundEnabled);

private:
    static const wchar_t APP_USER_MODEL_ID[];

    HRESULT ensureShortcut();
    HRESULT setTextValues(IXmlDocument *document,
                          const QString &title,
                          const QString &body) const;
    HRESULT suppressSound(IXmlDocument *document) const;
    HRESULT appendText(IXmlDocument *document,
                       IXmlNode *node,
                       const QString &text) const;

    DesktopNotification *owner;
    RoInitializeWrapper winRtInitializer;
    ComPtr<IToastNotificationManagerStatics> toastManager;
    bool shortcutReady;
};

const wchar_t WindowsNotificationContext::APP_USER_MODEL_ID[] =
    L"io.github.foldynl.QLog";

WindowsNotificationContext::WindowsNotificationContext(DesktopNotification *notificationOwner) :
    owner(notificationOwner),
    winRtInitializer(RO_INIT_MULTITHREADED),
    shortcutReady(false)
{
    FCT_IDENTIFICATION;
}

bool WindowsNotificationContext::initialize()
{
    FCT_IDENTIFICATION;

    const HRESULT initializationResult = winRtInitializer;
    if ( FAILED(initializationResult) && initializationResult != RPC_E_CHANGED_MODE )
    {
        qCWarning(runtime) << "Cannot initialize Windows Runtime for notifications"
                           << QString::number(initializationResult, 16);
        return false;
    }

    const HRESULT appIdResult = SetCurrentProcessExplicitAppUserModelID(APP_USER_MODEL_ID);
    if ( FAILED(appIdResult) )
    {
        qCWarning(runtime) << "Cannot set the QLog AppUserModelID"
                           << QString::number(appIdResult, 16);
        return false;
    }

    const HRESULT managerResult = Windows::Foundation::GetActivationFactory(
        HStringReference(
            RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
        &toastManager);
    if ( FAILED(managerResult) )
    {
        qCWarning(runtime) << "Cannot initialize Windows notifications"
                           << QString::number(managerResult, 16);
        return false;
    }

    return true;
}

HRESULT WindowsNotificationContext::ensureShortcut()
{
    FCT_IDENTIFICATION;

    if ( shortcutReady ) return S_OK;

    PWSTR programsPath = nullptr;
    HRESULT result = SHGetKnownFolderPath(FOLDERID_Programs, KF_FLAG_CREATE,
                                          nullptr, &programsPath);
    if ( FAILED(result) ) return result;

    const QString shortcutDirectory = QDir(QString::fromWCharArray(programsPath))
                                          .filePath(QStringLiteral("QLog"));
    CoTaskMemFree(programsPath);

    if ( !QDir().mkpath(shortcutDirectory) ) return E_FAIL;

    const QString shortcutPath = QDir(shortcutDirectory)
                                     .filePath(QStringLiteral("QLog.lnk"));
    ComPtr<IShellLinkW> shellLink;
    result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&shellLink));
    if ( FAILED(result) ) return result;

    ComPtr<IPersistFile> persistFile;
    result = shellLink.As(&persistFile);
    if ( FAILED(result) ) return result;

    if ( QFileInfo::exists(shortcutPath) )
    {
        result = persistFile->Load(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()),
                                   STGM_READWRITE);
        if ( FAILED(result) ) return result;
    }
    else
    {
        const QString executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        const QString workingDirectory = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
        result = shellLink->SetPath(reinterpret_cast<LPCWSTR>(executable.utf16()));

        if ( FAILED(result) ) return result;
        result = shellLink->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(workingDirectory.utf16()));

        if ( FAILED(result) ) return result;
        result = shellLink->SetDescription(L"QLog");

        if ( FAILED(result) ) return result;
        result = shellLink->SetIconLocation(reinterpret_cast<LPCWSTR>(executable.utf16()), 0);

        if ( FAILED(result) ) return result;
    }

    ComPtr<IPropertyStore> propertyStore;
    result = shellLink.As(&propertyStore);
    if ( FAILED(result) ) return result;

    PROPVARIANT appId;
    result = InitPropVariantFromString(APP_USER_MODEL_ID, &appId);
    if ( FAILED(result) ) return result;
    result = propertyStore->SetValue(PKEY_AppUserModel_ID, appId);
    PropVariantClear(&appId);
    if ( FAILED(result) ) return result;

    result = propertyStore->Commit();
    if ( FAILED(result) ) return result;

    result = persistFile->Save(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), TRUE);
    if ( SUCCEEDED(result) ) shortcutReady = true;
    return result;
}

HRESULT WindowsNotificationContext::appendText(IXmlDocument *document,
                                               IXmlNode *node,
                                               const QString &text) const
{
    FCT_IDENTIFICATION;

    ComPtr<IXmlText> textNode;
    HRESULT result = document->CreateTextNode( HStringReference(reinterpret_cast<PCWSTR>(text.utf16()), text.size()).Get(),
                                               &textNode);
    if ( FAILED(result) ) return result;

    ComPtr<IXmlNode> textNodeAsNode;
    result = textNode.As(&textNodeAsNode);
    if ( FAILED(result) ) return result;

    ComPtr<IXmlNode> appendedNode;
    return node->AppendChild(textNodeAsNode.Get(), &appendedNode);
}

HRESULT WindowsNotificationContext::setTextValues(IXmlDocument *document,
                                                   const QString &title,
                                                   const QString &body) const
{
    FCT_IDENTIFICATION;

    ComPtr<IXmlNodeList> textNodes;
    HRESULT result = document->GetElementsByTagName(HStringReference(L"text").Get(),
                                                    &textNodes);
    if ( FAILED(result) ) return result;

    UINT32 length = 0;
    result = textNodes->get_Length(&length);
    if ( FAILED(result) || length < 2 ) return E_FAIL;

    ComPtr<IXmlNode> titleNode;
    result = textNodes->Item(0, &titleNode);

    if ( FAILED(result) ) return result;
    result = appendText(document, titleNode.Get(), title);

    if ( FAILED(result) ) return result;

    ComPtr<IXmlNode> bodyNode;
    result = textNodes->Item(1, &bodyNode);
    if ( FAILED(result) ) return result;

    return appendText(document, bodyNode.Get(), body);
}

HRESULT WindowsNotificationContext::suppressSound(IXmlDocument *document) const
{
    FCT_IDENTIFICATION;

    ComPtr<IXmlElement> audioElement;
    HRESULT result = document->CreateElement(HStringReference(L"audio").Get(),
                                             &audioElement);
    if ( FAILED(result) ) return result;

    result = audioElement->SetAttribute(HStringReference(L"silent").Get(),
                                        HStringReference(L"true").Get());

    if ( FAILED(result) ) return result;

    ComPtr<IXmlElement> toastElement;
    result = document->get_DocumentElement(&toastElement);
    if ( FAILED(result) ) return result;

    ComPtr<IXmlNode> audioNode;
    result = audioElement.As(&audioNode);
    if ( FAILED(result) ) return result;

    ComPtr<IXmlNode> toastNode;
    result = toastElement.As(&toastNode);
    if ( FAILED(result) ) return result;

    ComPtr<IXmlNode> appendedNode;
    return toastNode->AppendChild(audioNode.Get(), &appendedNode);
}

bool WindowsNotificationContext::show(DesktopNotificationCategory category,
                                      const QString &title, const QString &body,
                                      bool soundEnabled)
{
    FCT_IDENTIFICATION;

    if ( !toastManager ) return false;

    HRESULT result = ensureShortcut();
    if ( FAILED(result) )
    {
        qCWarning(runtime) << "Cannot prepare QLog for Windows notifications"
                           << QString::number(result, 16);
        return false;
    }

    ComPtr<IXmlDocument> document;
    result = toastManager->GetTemplateContent(ToastTemplateType_ToastText02,
                                              &document);
    if ( SUCCEEDED(result) ) result = setTextValues(document.Get(), title, body);
    if ( SUCCEEDED(result) && !soundEnabled ) result = suppressSound(document.Get());

    ComPtr<IToastNotifier> notifier;
    if ( SUCCEEDED(result) )
        result = toastManager->CreateToastNotifierWithId(HStringReference(APP_USER_MODEL_ID).Get(),
                                                         &notifier);

    ComPtr<IToastNotificationFactory> notificationFactory;
    if ( SUCCEEDED(result) )
        result = Windows::Foundation::GetActivationFactory(
            HStringReference(
                RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
            &notificationFactory);

    ComPtr<IToastNotification> notification;
    if ( SUCCEEDED(result) )
        result = notificationFactory->CreateToastNotification(document.Get(),
                                                               &notification);

    if ( SUCCEEDED(result) )
    {
        EventRegistrationToken activatedToken;
        result = notification->add_Activated(
            Callback<Implements<RuntimeClassFlags<ClassicCom>,
                                ITypedEventHandler<ToastNotification *, IInspectable *>>>(
                [this, category](IToastNotification *, IInspectable *) -> HRESULT
        {
            QMetaObject::invokeMethod(owner, "activate", Qt::QueuedConnection,
                                      Q_ARG(DesktopNotificationCategory, category));
            return S_OK;
        }).Get(), &activatedToken);
    }

    if ( SUCCEEDED(result) ) result = notifier->Show(notification.Get());
    if ( FAILED(result) )
    {
        qCWarning(runtime) << "Cannot send native notification"
                           << QString::number(result, 16);
        return false;
    }

    return true;
}

void DesktopNotification::initializePlatform()
{
    FCT_IDENTIFICATION;

    WindowsNotificationContext *context = new WindowsNotificationContext(this);
    platformContext = context;
    context->initialize();
}

void DesktopNotification::shutdownPlatform()
{
    FCT_IDENTIFICATION;

    delete static_cast<WindowsNotificationContext *>(platformContext);
    platformContext = nullptr;
}

bool DesktopNotification::showPlatform(DesktopNotificationCategory category,
                                       const QString &title,
                                       const QString &body,
                                       bool soundEnabled)
{
    FCT_IDENTIFICATION;

    WindowsNotificationContext *context = static_cast<WindowsNotificationContext *>(platformContext);
    return context && context->show(category, title, body, soundEnabled);
}
