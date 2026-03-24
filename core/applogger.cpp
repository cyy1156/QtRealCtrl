#include "core/applogger.h"

#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QThread>
#include <QMetaObject>
#include <QFileInfo>

AppLogger::AppLogger(QObject *parent)
    : QObject(parent)
{
}

AppLogger::~AppLogger()
{
    shutdown();
}

AppLogger& AppLogger::instance()
{
    static AppLogger logger;
    return logger;
}

void AppLogger::initialize(Level minLevel,
                           const QString &logDirPath,
                           bool forwardToPreviousHandler,
                           int retentionDays)
{
    QMutexLocker locker(&m_mutex);
    if (m_initialized)
    {
        return;
    }
    m_minLevel = minLevel;
    m_logDirPath = logDirPath;
    m_forwardToPreviousHandler = forwardToPreviousHandler;
    m_retentionDays = retentionDays;
    cleanupOldLogsLocked();
    m_previousHandler = qInstallMessageHandler(&AppLogger::messageHandler);
    m_initialized = true;
}

void AppLogger::shutdown()
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized)
    {
        return;
    }
    qInstallMessageHandler(m_previousHandler);
    m_previousHandler = nullptr;
    if (m_logFile.isOpen())
    {
        m_logFile.flush();
        m_logFile.close();
    }
    m_initialized = false;
}

void AppLogger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    AppLogger::instance().handleMessage(type, context, msg);
}

void AppLogger::handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const Level level = fromQtType(type);

    QtMessageHandler previousHandler = nullptr;
    bool forward = false;
    QString line;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_initialized)
        {
            return;
        }
        previousHandler = m_previousHandler;
        forward = m_forwardToPreviousHandler;
        if (static_cast<int>(level) < static_cast<int>(m_minLevel))
        {
            if (forward && previousHandler)
            {
                previousHandler(type, context, msg);
            }
            return;
        }
        line = formatLine(level, msg, context);
        writeToFile(line);
    }

    QMetaObject::invokeMethod(this,
                              [this, line, level]() {
                                  emit logLineReady(line, static_cast<int>(level));
                              },
                              Qt::QueuedConnection);

    if (forward && previousHandler)
    {
        previousHandler(type, context, msg);
    }
}

QString AppLogger::levelToString(Level level) const
{
    switch (level)
    {
    case Level::Debug: return QStringLiteral("DEBUG");
    case Level::Info:  return QStringLiteral("INFO");
    case Level::Warn:  return QStringLiteral("WARN");
    case Level::Error: return QStringLiteral("ERROR");
    case Level::Fatal: return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

AppLogger::Level AppLogger::fromQtType(QtMsgType type) const
{
    switch (type)
    {
    case QtDebugMsg: return Level::Debug;
    case QtInfoMsg: return Level::Info;
    case QtWarningMsg: return Level::Warn;
    case QtCriticalMsg: return Level::Error;
    case QtFatalMsg: return Level::Fatal;
    }
    return Level::Info;
}

QString AppLogger::formatLine(Level level, const QString &msg, const QMessageLogContext &context) const
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    QThread *current = QThread::currentThread();
    const QString threadName = (current && !current->objectName().isEmpty())
                                   ? current->objectName()
                                   : QStringLiteral("Thread-0x%1").arg(
                                         QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16));
    const QString category = (context.category && context.category[0] != '\0')
                                 ? QString::fromLatin1(context.category)
                                 : QStringLiteral("App");
    return QStringLiteral("[%1] [%2] [Thread:%3] [%4] %5")
        .arg(ts,
             levelToString(level),
             threadName,
             category,
             msg);
}

bool AppLogger::ensureLogFileReady()
{
    const QString dayKey = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    rotateIfDayChanged(dayKey);

    if (m_logFile.isOpen())
    {
        return true;
    }

    if (m_logDirPath.isEmpty())
    {
        return false;
    }

    QDir dir(m_logDirPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        return false;
    }

    const QString filePath = dir.filePath(dayKey + QStringLiteral(".log"));
    m_logFile.setFileName(filePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        return false;
    }
    return true;
}

void AppLogger::rotateIfDayChanged(const QString &dayKey)
{
    if (m_currentDayKey == dayKey)
    {
        return;
    }
    if (m_logFile.isOpen())
    {
        m_logFile.flush();
        m_logFile.close();
    }
    m_currentDayKey = dayKey;
}

void AppLogger::writeToFile(const QString &line)
{
    if (!ensureLogFileReady())
    {
        return;
    }
    QTextStream ts(&m_logFile);
    ts << line << '\n';
    ts.flush();
}

void AppLogger::cleanupOldLogsLocked()
{
    if (m_logDirPath.isEmpty() || m_retentionDays <= 0)
    {
        return;
    }

    QDir dir(m_logDirPath);
    if (!dir.exists())
    {
        return;
    }

    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.log"), QDir::Files, QDir::Name);
    const QDate today = QDate::currentDate();
    for (const QString &fileName : files)
    {
        // 中文注释：仅处理命名为 yyyy-MM-dd.log 的文件，避免误删其它日志文件
        if (fileName.size() < 14)
        {
            continue;
        }
        const QString baseName = fileName.left(10);
        const QDate fileDate = QDate::fromString(baseName, QStringLiteral("yyyy-MM-dd"));
        if (!fileDate.isValid())
        {
            continue;
        }
        const int ageDays = fileDate.daysTo(today);
        if (ageDays > m_retentionDays)
        {
            dir.remove(fileName);
        }
    }
}

