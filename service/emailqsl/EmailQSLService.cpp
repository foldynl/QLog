#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPainter>
#include <QFont>
#include <QColor>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QBuffer>
#include <QImageWriter>
#include <QHostInfo>

#include "EmailQSLService.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.service.emailqsl");

// ---------------------------------------------------------------------------
// EmailQSLFieldOverlay
// ---------------------------------------------------------------------------

QJsonObject EmailQSLFieldOverlay::toJson() const
{
    QJsonObject o;
    o["type"]         = type;
    o["fieldName"]    = fieldName;
    o["x"]            = x;
    o["y"]            = y;
    o["fontFamily"]   = fontFamily;
    o["fontSize"]     = fontSize;
    o["color"]        = color;
    o["bold"]         = bold;
    o["italic"]       = italic;
    o["width"]        = width;
    o["height"]       = height;
    o["fillColor"]    = fillColor;
    o["opacity"]      = opacity;
    o["cornerRadius"] = cornerRadius;
    return o;
}

EmailQSLFieldOverlay EmailQSLFieldOverlay::fromJson(const QJsonObject &obj)
{
    EmailQSLFieldOverlay f;
    f.type         = obj.value("type").toString(QStringLiteral("TEXT"));
    f.fieldName    = obj.value("fieldName").toString();
    f.x            = obj.value("x").toInt();
    f.y            = obj.value("y").toInt();
    f.fontFamily   = obj.value("fontFamily").toString(QStringLiteral("Arial"));
    f.fontSize     = obj.value("fontSize").toInt(14);
    f.color        = obj.value("color").toString(QStringLiteral("#000000"));
    f.bold         = obj.value("bold").toBool();
    f.italic       = obj.value("italic").toBool();
    f.width        = obj.value("width").toInt(120);
    f.height       = obj.value("height").toInt(60);
    f.fillColor    = obj.value("fillColor").toString(QStringLiteral("#FFFF99"));
    f.opacity      = obj.value("opacity").toInt(80);
    f.cornerRadius = obj.value("cornerRadius").toInt(8);
    return f;
}

// ---------------------------------------------------------------------------
// EmailQSLBase — credential registration
// ---------------------------------------------------------------------------

const QString EmailQSLBase::SECURE_STORAGE_KEY = QStringLiteral("EmailQSL");
REGISTRATION_SECURE_SERVICE(EmailQSLBase);

void EmailQSLBase::registerCredentials()
{
    CredentialRegistry::instance().add(SECURE_STORAGE_KEY, []()
    {
        return QList<CredentialDescriptor>
        {
            { SECURE_STORAGE_KEY, []() { return getSmtpUsername(); } }
        };
    });
}

// ---------------------------------------------------------------------------
// EmailQSLBase — QSettings helpers
// ---------------------------------------------------------------------------

#define EMAILQSL_SETTINGS_GROUP "emailqsl"

static QSettings &cfg()
{
    static QSettings s;
    return s;
}

QString EmailQSLBase::getSmtpHost()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/smtpHost")).toString();
}

void EmailQSLBase::setSmtpHost(const QString &host)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/smtpHost"), host);
}

int EmailQSLBase::getSmtpPort()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/smtpPort"), 587).toInt();
}

void EmailQSLBase::setSmtpPort(int port)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/smtpPort"), port);
}

int EmailQSLBase::getSmtpEncryption()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/smtpEncryption"),
                       ENCRYPTION_STARTTLS).toInt();
}

void EmailQSLBase::setSmtpEncryption(int enc)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/smtpEncryption"), enc);
}

QString EmailQSLBase::getSmtpUsername()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/smtpUsername")).toString();
}

void EmailQSLBase::setSmtpUsername(const QString &username)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/smtpUsername"), username);
}

QString EmailQSLBase::getSmtpPassword()
{
    return getPassword(SECURE_STORAGE_KEY, getSmtpUsername());
}

void EmailQSLBase::saveSmtpCredentials(const QString &username, const QString &password)
{
    const QString oldUsername = getSmtpUsername();
    if (oldUsername != username && !oldUsername.isEmpty())
        deletePassword(SECURE_STORAGE_KEY, oldUsername);
    setSmtpUsername(username);
    savePassword(SECURE_STORAGE_KEY, username, password);
}

QString EmailQSLBase::getFromAddress()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/fromAddress")).toString();
}

void EmailQSLBase::setFromAddress(const QString &addr)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/fromAddress"), addr);
}

QString EmailQSLBase::getFromName()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/fromName")).toString();
}

void EmailQSLBase::setFromName(const QString &name)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/fromName"), name);
}

QString EmailQSLBase::getSubjectTemplate()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/subjectTemplate"),
                       QStringLiteral("QSL Card from {MY_CALLSIGN} for our QSO on {QSO_DATE}")).toString();
}

void EmailQSLBase::setSubjectTemplate(const QString &tmpl)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/subjectTemplate"), tmpl);
}

QString EmailQSLBase::getBodyTemplate()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/bodyTemplate"),
                       QStringLiteral("Dear {NAME},\n\nPlease find my QSL card attached confirming our contact.\n\n"
                                      "Callsign: {MY_CALLSIGN}\n"
                                      "Date: {QSO_DATE}\nTime: {TIME_ON} UTC\n"
                                      "Band: {BAND}\nMode: {MODE}\n"
                                      "RST Sent: {RST_SENT}\nRST Rcvd: {RST_RCVD}\n\n"
                                      "73,\n{MY_CALLSIGN}")).toString();
}

void EmailQSLBase::setBodyTemplate(const QString &tmpl)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/bodyTemplate"), tmpl);
}

QString EmailQSLBase::getCardImagePath()
{
    return cfg().value(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/cardImagePath")).toString();
}

void EmailQSLBase::setCardImagePath(const QString &path)
{
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/cardImagePath"), path);
}

QList<EmailQSLFieldOverlay> EmailQSLBase::getCardFieldOverlays()
{
    QList<EmailQSLFieldOverlay> result;
    const QByteArray json = cfg().value(
        QStringLiteral(EMAILQSL_SETTINGS_GROUP "/cardOverlays")).toByteArray();
    const QJsonArray arr  = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &v : arr)
        result.append(EmailQSLFieldOverlay::fromJson(v.toObject()));
    return result;
}

void EmailQSLBase::setCardFieldOverlays(const QList<EmailQSLFieldOverlay> &overlays)
{
    QJsonArray arr;
    for (const EmailQSLFieldOverlay &o : overlays)
        arr.append(o.toJson());
    cfg().setValue(QStringLiteral(EMAILQSL_SETTINGS_GROUP "/cardOverlays"),
                   QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

// ---------------------------------------------------------------------------
// EmailQSLBase — sent-tracking via contacts.fields JSON
// ---------------------------------------------------------------------------

static const QString EMAIL_QSL_SENT_KEY = QStringLiteral("email_qsl_sent_dt");

QDateTime EmailQSLBase::getEmailSentDateTime(const QSqlRecord &record)
{
    const QByteArray raw = record.value(QStringLiteral("fields")).toByteArray();
    const QJsonObject fields = QJsonDocument::fromJson(raw).object();
    const QString dtStr = fields.value(EMAIL_QSL_SENT_KEY).toString();
    return dtStr.isEmpty() ? QDateTime() : QDateTime::fromString(dtStr, Qt::ISODate);
}

bool EmailQSLBase::hasEmailBeenSentToCallsign(const QString &callsign, int excludeId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT fields FROM contacts WHERE callsign = :cs AND id != :ex"));
    q.bindValue(":cs", callsign.toUpper());
    q.bindValue(":ex", excludeId);
    if (!q.exec())
        return false;

    while (q.next())
    {
        const QByteArray raw = q.value(0).toByteArray();
        const QJsonObject fields = QJsonDocument::fromJson(raw).object();
        if (fields.contains(EMAIL_QSL_SENT_KEY))
            return true;
    }
    return false;
}

void EmailQSLBase::recordEmailSent(int contactId, const QSqlRecord &currentRecord)
{
    FCT_IDENTIFICATION;

    const QByteArray raw = currentRecord.value(QStringLiteral("fields")).toByteArray();
    QJsonObject fields   = QJsonDocument::fromJson(raw).object();
    fields[EMAIL_QSL_SENT_KEY] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE contacts SET fields = :f WHERE id = :id"));
    q.bindValue(":f",  QJsonDocument(fields).toJson(QJsonDocument::Compact));
    q.bindValue(":id", contactId);
    if (!q.exec())
        qCWarning(runtime) << "recordEmailSent update failed:" << q.lastError().text();
}

// ---------------------------------------------------------------------------
// EmailQSLBase — merge fields
// ---------------------------------------------------------------------------

QString EmailQSLBase::fieldValue(const QString &key, const QSqlRecord &record)
{
    if (key == QLatin1String("CALLSIGN"))
        return record.value(QStringLiteral("callsign")).toString().toUpper();

    if (key == QLatin1String("QSO_DATE"))
    {
        const QDateTime dt = record.value(QStringLiteral("start_time")).toDateTime();
        return dt.isValid() ? dt.toUTC().toString(QStringLiteral("dd-MMM-yyyy")).toUpper() : QString();
    }
    if (key == QLatin1String("QSO_DATE_ISO"))
    {
        const QDateTime dt = record.value(QStringLiteral("start_time")).toDateTime();
        return dt.isValid() ? dt.toUTC().toString(QStringLiteral("yyyyMMdd")) : QString();
    }
    if (key == QLatin1String("TIME_ON"))
    {
        const QDateTime dt = record.value(QStringLiteral("start_time")).toDateTime();
        return dt.isValid() ? dt.toUTC().toString(QStringLiteral("HHmm")) : QString();
    }
    if (key == QLatin1String("FREQ"))
    {
        bool ok;
        double mhz = record.value(QStringLiteral("freq")).toDouble(&ok);
        return ok ? QString::number(mhz, 'f', 3) : QString();
    }
    if (key == QLatin1String("BAND"))        return record.value(QStringLiteral("band")).toString().toUpper();
    if (key == QLatin1String("MODE"))        return record.value(QStringLiteral("mode")).toString().toUpper();
    if (key == QLatin1String("SUBMODE"))     return record.value(QStringLiteral("submode")).toString().toUpper();
    if (key == QLatin1String("RST_SENT"))    return record.value(QStringLiteral("rst_sent")).toString();
    if (key == QLatin1String("RST_RCVD"))    return record.value(QStringLiteral("rst_rcvd")).toString();
    if (key == QLatin1String("NAME"))
    {
        const QString n = record.value(QStringLiteral("name_intl")).toString();
        return n.isEmpty() ? record.value(QStringLiteral("name")).toString() : n;
    }
    if (key == QLatin1String("QTH"))
    {
        const QString q = record.value(QStringLiteral("qth_intl")).toString();
        return q.isEmpty() ? record.value(QStringLiteral("qth")).toString() : q;
    }
    if (key == QLatin1String("COUNTRY"))
    {
        const QString c = record.value(QStringLiteral("country_intl")).toString();
        return c.isEmpty() ? record.value(QStringLiteral("country")).toString() : c;
    }
    if (key == QLatin1String("GRIDSQUARE"))  return record.value(QStringLiteral("gridsquare")).toString().toUpper();
    if (key == QLatin1String("DXCC"))        return record.value(QStringLiteral("dxcc")).toString();
    if (key == QLatin1String("CQZ"))         return record.value(QStringLiteral("cqz")).toString();
    if (key == QLatin1String("ITUZ"))        return record.value(QStringLiteral("ituz")).toString();
    if (key == QLatin1String("TX_PWR"))      return record.value(QStringLiteral("tx_pwr")).toString();
    if (key == QLatin1String("EMAIL"))       return record.value(QStringLiteral("email")).toString();
    if (key == QLatin1String("MY_CALLSIGN")) return record.value(QStringLiteral("station_callsign")).toString().toUpper();
    if (key == QLatin1String("MY_GRIDSQUARE")) return record.value(QStringLiteral("my_gridsquare")).toString().toUpper();
    if (key == QLatin1String("OPERATOR"))    return record.value(QStringLiteral("operator")).toString().toUpper();
    if (key == QLatin1String("COMMENT"))
    {
        const QString c = record.value(QStringLiteral("comment_intl")).toString();
        return c.isEmpty() ? record.value(QStringLiteral("comment")).toString() : c;
    }
    if (key == QLatin1String("SOTA_REF"))    return record.value(QStringLiteral("sota_ref")).toString();
    if (key == QLatin1String("POTA_REF"))    return record.value(QStringLiteral("pota_ref")).toString();
    if (key == QLatin1String("WWFF_REF"))    return record.value(QStringLiteral("wwff_ref")).toString();
    if (key == QLatin1String("IOTA"))        return record.value(QStringLiteral("iota")).toString();
    if (key == QLatin1String("SIG"))         return record.value(QStringLiteral("sig_intl")).toString();
    if (key == QLatin1String("CONTEST_ID"))  return record.value(QStringLiteral("contest_id")).toString();

    // Fallback: try direct lowercase column name
    const QString col = key.toLower();
    if (record.indexOf(col) >= 0)
        return record.value(col).toString();

    return QString();
}

QString EmailQSLBase::applyMergeFields(const QString &tmpl, const QSqlRecord &record)
{
    QString result = tmpl;
    static const QRegularExpression rx(QStringLiteral("\\{([A-Z0-9_]+)\\}"));
    QRegularExpressionMatchIterator it = rx.globalMatch(tmpl);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        const QString key   = m.captured(1);
        const QString value = fieldValue(key, record);
        result.replace(QLatin1Char('{') + key + QLatin1Char('}'), value);
    }
    return result;
}

QList<EmailQSLBase::MergeField> EmailQSLBase::availableMergeFields()
{
    return {
        { "CALLSIGN",      QObject::tr("Contact callsign") },
        { "QSO_DATE",      QObject::tr("QSO date (dd-MMM-yyyy)") },
        { "QSO_DATE_ISO",  QObject::tr("QSO date (YYYYMMDD)") },
        { "TIME_ON",       QObject::tr("QSO start time UTC (HHmm)") },
        { "FREQ",          QObject::tr("Frequency (MHz)") },
        { "BAND",          QObject::tr("Band") },
        { "MODE",          QObject::tr("Mode") },
        { "SUBMODE",       QObject::tr("Sub-mode") },
        { "RST_SENT",      QObject::tr("RST sent") },
        { "RST_RCVD",      QObject::tr("RST received") },
        { "NAME",          QObject::tr("Contact name") },
        { "QTH",           QObject::tr("Contact QTH") },
        { "COUNTRY",       QObject::tr("Country") },
        { "GRIDSQUARE",    QObject::tr("Grid square") },
        { "DXCC",          QObject::tr("DXCC entity number") },
        { "CQZ",           QObject::tr("CQ zone") },
        { "ITUZ",          QObject::tr("ITU zone") },
        { "TX_PWR",        QObject::tr("TX power") },
        { "EMAIL",         QObject::tr("Contact email address") },
        { "MY_CALLSIGN",   QObject::tr("My callsign") },
        { "MY_GRIDSQUARE", QObject::tr("My grid square") },
        { "OPERATOR",      QObject::tr("Operator callsign") },
        { "COMMENT",       QObject::tr("Comment") },
        { "SOTA_REF",      QObject::tr("SOTA reference") },
        { "POTA_REF",      QObject::tr("POTA reference") },
        { "WWFF_REF",      QObject::tr("WWFF reference") },
        { "IOTA",          QObject::tr("IOTA reference") },
        { "SIG",           QObject::tr("Special interest group") },
        { "CONTEST_ID",    QObject::tr("Contest ID") },
    };
}

// ---------------------------------------------------------------------------
// EmailQSLBase — card rendering
// ---------------------------------------------------------------------------

QPixmap EmailQSLBase::renderCard(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;
    return renderCard(getCardImagePath(), record, getCardFieldOverlays());
}

QPixmap EmailQSLBase::renderCard(const QString &imagePath,
                                 const QSqlRecord &record,
                                 const QList<EmailQSLFieldOverlay> &overlays)
{
    FCT_IDENTIFICATION;

    QPixmap pixmap(imagePath);
    if (pixmap.isNull())
    {
        qCWarning(runtime) << "renderCard: could not load image:" << imagePath;
        return QPixmap();
    }

    // Overlay x, y, fontSize and BOX width/height are all stored as absolute
    // pixel values scaled to the source image dimensions.  No extra scaling is
    // applied here — the values are used as-is.  addDefaultOverlays() and
    // onCardImageChanged() are responsible for keeping them proportional whenever
    // the background image changes.
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    for (const EmailQSLFieldOverlay &ov : overlays)
    {
        if (ov.type == QLatin1String("BOX"))
        {
            QColor fill(ov.fillColor);
            fill.setAlphaF(ov.opacity / 100.0);
            painter.setPen(QPen(QColor(ov.color), 1.5));
            painter.setBrush(fill);
            painter.drawRoundedRect(ov.x, ov.y, ov.width, ov.height,
                                    ov.cornerRadius, ov.cornerRadius);

            if (!ov.fieldName.isEmpty())
            {
                QFont font(ov.fontFamily, ov.fontSize > 0 ? ov.fontSize : 11);
                font.setBold(ov.bold);
                painter.setFont(font);
                painter.setPen(QColor(ov.color));
                painter.setBrush(Qt::NoBrush);
                QFontMetrics fm(font);
                painter.drawText(ov.x, ov.y - fm.descent() - 2, ov.fieldName);
            }
        }
        else if (ov.type == QLatin1String("LABEL"))
        {
            if (ov.fieldName.isEmpty())
                continue;

            QFont font(ov.fontFamily, ov.fontSize);
            font.setBold(ov.bold);
            font.setItalic(ov.italic);
            painter.setFont(font);
            painter.setPen(QColor(ov.color));
            painter.setBrush(Qt::NoBrush);
            painter.drawText(ov.x, ov.y, ov.fieldName);
        }
        else // TEXT — merge-field substitution
        {
            const QString value = fieldValue(ov.fieldName, record);
            if (value.isEmpty())
                continue;

            QFont font(ov.fontFamily, ov.fontSize);
            font.setBold(ov.bold);
            font.setItalic(ov.italic);
            painter.setFont(font);
            painter.setPen(QColor(ov.color));
            painter.setBrush(Qt::NoBrush);
            painter.drawText(ov.x, ov.y, value);
        }
    }
    painter.end();
    return pixmap;
}

// ---------------------------------------------------------------------------
// SmtpWorker
// ---------------------------------------------------------------------------

SmtpWorker::SmtpWorker(const QString &host, int port, int encryption,
                       const QString &username, const QString &password,
                       const QString &fromAddress, const QString &fromName,
                       const QString &toAddress,
                       const QString &subject, const QString &body,
                       const QByteArray &imageData, const QString &imageName,
                       QObject *parent)
    : QObject(parent),
      m_host(host), m_port(port), m_encryption(encryption),
      m_username(username), m_password(password),
      m_fromAddress(fromAddress), m_fromName(fromName),
      m_toAddress(toAddress),
      m_subject(subject), m_body(body),
      m_imageData(imageData), m_imageName(imageName)
{
}

bool SmtpWorker::waitForResponse(QSslSocket *socket, QByteArray &out, int timeoutMs)
{
    out.clear();
    // Multi-line responses end with "NNN " (space after code); single with "NNN "
    // Keep reading until we get a line without a dash after the code.
    do {
        if (!socket->waitForReadyRead(timeoutMs))
            return false;
        out += socket->readAll();
    } while (out.size() > 3 && out[out.size()-2] != '\r' &&
             (out.size() < 4 || out[3] == '-'));
    // More robust: check if last complete line starts with "NNN " (no dash)
    // Parse the last CRLF-terminated line
    const QList<QByteArray> lines = out.split('\n');
    for (const QByteArray &line : lines)
    {
        if (line.length() >= 4 && line[3] == ' ')
            return true;  // found a final response line
        if (line.length() >= 4 && line[3] == '-')
            continue;     // continuation line
    }
    return !out.isEmpty();
}

int SmtpWorker::responseCode(const QByteArray &response)
{
    // Find the last "NNN " line
    const QList<QByteArray> lines = response.split('\n');
    for (int i = lines.size() - 1; i >= 0; --i)
    {
        const QByteArray &line = lines.at(i).trimmed();
        if (line.length() >= 3)
        {
            bool ok;
            int code = line.left(3).toInt(&ok);
            if (ok)
                return code;
        }
    }
    return -1;
}

bool SmtpWorker::sendCommand(QSslSocket *socket, const QByteArray &cmd,
                             int expectedCode, QByteArray &response)
{
    socket->write(cmd);
    if (!socket->waitForBytesWritten(10000))
        return false;
    if (!waitForResponse(socket, response))
        return false;
    return responseCode(response) == expectedCode;
}

QByteArray SmtpWorker::buildMimeMessage()
{
    const QString boundary = QStringLiteral("----QLogEmailQSL_%1")
        .arg(QDateTime::currentMSecsSinceEpoch());

    QByteArray msg;

    // Encode From display name safely
    const QByteArray fromDisplay = ("\"" + m_fromName + "\" <" + m_fromAddress + ">").toUtf8();
    msg += "MIME-Version: 1.0\r\n";
    msg += "From: " + fromDisplay + "\r\n";
    msg += "To: <" + m_toAddress.toUtf8() + ">\r\n";
    // RFC 2047 encoded subject
    msg += "Subject: =?UTF-8?B?" + m_subject.toUtf8().toBase64() + "?=\r\n";
    msg += "Content-Type: multipart/mixed; boundary=\"" + boundary.toUtf8() + "\"\r\n";
    msg += "\r\n";

    // --- Text part ---
    msg += "--" + boundary.toUtf8() + "\r\n";
    msg += "Content-Type: text/plain; charset=UTF-8\r\n";
    msg += "Content-Transfer-Encoding: base64\r\n";
    msg += "\r\n";
    const QByteArray bodyB64 = m_body.toUtf8().toBase64();
    for (int i = 0; i < bodyB64.size(); i += 76)
        msg += bodyB64.mid(i, 76) + "\r\n";

    // --- Image attachment (if present) ---
    if (!m_imageData.isEmpty())
    {
        msg += "\r\n--" + boundary.toUtf8() + "\r\n";
        const QString mimeType = m_imageName.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)
                                 ? QStringLiteral("image/png")
                                 : QStringLiteral("image/jpeg");
        msg += "Content-Type: " + mimeType.toUtf8()
               + "; name=\"" + m_imageName.toUtf8() + "\"\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n";
        msg += "Content-Disposition: attachment; filename=\""
               + m_imageName.toUtf8() + "\"\r\n";
        msg += "\r\n";
        const QByteArray imgB64 = m_imageData.toBase64();
        for (int i = 0; i < imgB64.size(); i += 76)
            msg += imgB64.mid(i, 76) + "\r\n";
    }

    msg += "\r\n--" + boundary.toUtf8() + "--\r\n";
    return msg;
}

void SmtpWorker::run()
{
    FCT_IDENTIFICATION;

    QSslSocket socket;
    socket.setProtocol(QSsl::AnyProtocol);
    socket.setPeerVerifyMode(QSslSocket::VerifyNone); // tolerate self-signed certs

    // ---- Connect ----
    if (m_encryption == EmailQSLBase::ENCRYPTION_SSL_TLS)
    {
        socket.connectToHostEncrypted(m_host, static_cast<quint16>(m_port));
        if (!socket.waitForEncrypted(15000))
        {
            emit finished(false, tr("SSL/TLS connection failed: %1").arg(socket.errorString()));
            return;
        }
    }
    else
    {
        socket.connectToHost(m_host, static_cast<quint16>(m_port));
        if (!socket.waitForConnected(15000))
        {
            emit finished(false, tr("Could not connect to %1:%2 — %3")
                          .arg(m_host).arg(m_port).arg(socket.errorString()));
            return;
        }
    }

    // ---- Read greeting (220) ----
    QByteArray resp;
    if (!waitForResponse(&socket, resp) || responseCode(resp) != 220)
    {
        emit finished(false, tr("Server did not send a valid greeting (expected 220)."));
        return;
    }

    // ---- EHLO ----
    const QByteArray localHost = QHostInfo::localHostName().toUtf8();
    if (!sendCommand(&socket, "EHLO " + localHost + "\r\n", 250, resp))
    {
        // Try HELO as fallback
        if (!sendCommand(&socket, "HELO " + localHost + "\r\n", 250, resp))
        {
            emit finished(false, tr("EHLO/HELO rejected by server."));
            return;
        }
    }

    // ---- STARTTLS upgrade ----
    if (m_encryption == EmailQSLBase::ENCRYPTION_STARTTLS)
    {
        if (!sendCommand(&socket, "STARTTLS\r\n", 220, resp))
        {
            emit finished(false, tr("STARTTLS not accepted by server."));
            return;
        }
        socket.startClientEncryption();
        if (!socket.waitForEncrypted(15000))
        {
            emit finished(false, tr("TLS handshake failed: %1").arg(socket.errorString()));
            return;
        }
        // Re-EHLO after TLS negotiation
        if (!sendCommand(&socket, "EHLO " + localHost + "\r\n", 250, resp))
        {
            emit finished(false, tr("EHLO after STARTTLS rejected."));
            return;
        }
    }

    // ---- AUTH LOGIN ----
    if (!m_username.isEmpty())
    {
        if (!sendCommand(&socket, "AUTH LOGIN\r\n", 334, resp))
        {
            emit finished(false, tr("AUTH LOGIN not supported by server."));
            return;
        }
        if (!sendCommand(&socket, m_username.toUtf8().toBase64() + "\r\n", 334, resp))
        {
            emit finished(false, tr("Username rejected by server."));
            return;
        }
        if (!sendCommand(&socket, m_password.toUtf8().toBase64() + "\r\n", 235, resp))
        {
            emit finished(false, tr("Authentication failed — check your username and password."));
            return;
        }
    }

    // ---- MAIL FROM ----
    if (!sendCommand(&socket, "MAIL FROM:<" + m_fromAddress.toUtf8() + ">\r\n", 250, resp))
    {
        emit finished(false, tr("MAIL FROM rejected: %1").arg(QString::fromUtf8(resp)));
        return;
    }

    // ---- RCPT TO ----
    if (!sendCommand(&socket, "RCPT TO:<" + m_toAddress.toUtf8() + ">\r\n", 250, resp))
    {
        emit finished(false, tr("Recipient address not accepted: %1").arg(QString::fromUtf8(resp)));
        return;
    }

    // ---- DATA ----
    if (!sendCommand(&socket, "DATA\r\n", 354, resp))
    {
        emit finished(false, tr("DATA command rejected."));
        return;
    }

    // ---- Send message body ----
    QByteArray mime = buildMimeMessage();
    mime += "\r\n.\r\n";
    socket.write(mime);
    socket.flush();

    if (!waitForResponse(&socket, resp) || responseCode(resp) != 250)
    {
        emit finished(false, tr("Message rejected by server: %1").arg(QString::fromUtf8(resp)));
        return;
    }

    // ---- QUIT ----
    socket.write("QUIT\r\n");
    socket.waitForBytesWritten(5000);
    socket.disconnectFromHost();
    socket.waitForDisconnected(5000);

    emit finished(true, QString());
}

// ---------------------------------------------------------------------------
// EmailQSLService
// ---------------------------------------------------------------------------

EmailQSLService::EmailQSLService(QObject *parent)
    : QObject(parent)
{
}

void EmailQSLService::sendEmailQSL(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    // Render card at full source resolution (fonts scale with image size)
    const QPixmap cardPixmap = EmailQSLBase::renderCard(record);
    QByteArray imageData;
    QString imageName;
    if (!cardPixmap.isNull())
    {
        // Scale down to EMAIL_WIDTH for the attachment — keeps file size reasonable
        // while still looking sharp on any screen.  The full-res pixmap is only
        // used when the user explicitly saves via "Save Card…".
        static constexpr int EMAIL_WIDTH = 1280;
        const QPixmap emailPixmap = (cardPixmap.width() > EMAIL_WIDTH)
            ? cardPixmap.scaledToWidth(EMAIL_WIDTH, Qt::SmoothTransformation)
            : cardPixmap;

        QBuffer buf(&imageData);
        buf.open(QIODevice::WriteOnly);
        emailPixmap.save(&buf, "JPEG", 90);
        imageName = QStringLiteral("qsl_card.jpg");
    }

    // Merge email fields
    const QString subject = EmailQSLBase::applyMergeFields(
        EmailQSLBase::getSubjectTemplate(), record);
    const QString body    = EmailQSLBase::applyMergeFields(
        EmailQSLBase::getBodyTemplate(), record);

    const QString toAddress = record.value(QStringLiteral("email")).toString().trimmed();

    QThread *thread = new QThread(this);
    SmtpWorker *worker = new SmtpWorker(
        EmailQSLBase::getSmtpHost(),
        EmailQSLBase::getSmtpPort(),
        EmailQSLBase::getSmtpEncryption(),
        EmailQSLBase::getSmtpUsername(),
        EmailQSLBase::getSmtpPassword(),
        EmailQSLBase::getFromAddress(),
        EmailQSLBase::getFromName(),
        toAddress,
        subject, body,
        imageData, imageName);

    worker->moveToThread(thread);

    connect(thread, &QThread::started,   worker, &SmtpWorker::run);
    connect(worker, &SmtpWorker::finished, this,
            [this](bool ok, const QString &msg) { emit sendFinished(ok, msg); });
    connect(worker, &SmtpWorker::finished, worker, &QObject::deleteLater);
    connect(worker, &SmtpWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished,   thread, &QObject::deleteLater);

    thread->start();
}

void EmailQSLService::testConnection(const QString &host, int port, int encryption,
                                     const QString &username, const QString &password)
{
    FCT_IDENTIFICATION;

    // Send a dummy message to a no-op address just to verify auth works —
    // actually we just connect + EHLO + AUTH and then QUIT.
    QThread *thread = new QThread(this);
    SmtpWorker *worker = new SmtpWorker(
        host, port, encryption, username, password,
        username, QStringLiteral("Test"),
        username,  // to = from (won't actually be sent)
        QStringLiteral("QLog connection test"),
        QStringLiteral("Connection test only — no message will be sent."),
        QByteArray(), QString());

    worker->moveToThread(thread);

    connect(thread, &QThread::started,   worker, &SmtpWorker::run);
    connect(worker, &SmtpWorker::finished, this,
            [this](bool ok, const QString &msg) { emit testFinished(ok, msg); });
    connect(worker, &SmtpWorker::finished, worker, &QObject::deleteLater);
    connect(worker, &SmtpWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished,   thread, &QObject::deleteLater);

    thread->start();
}
