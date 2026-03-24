#ifndef APPLOGGER_H
#define APPLOGGER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QFile>

class AppLogger : public QObject
{
    Q_OBJECT
public:
    enum class Level
    {
        Debug = 0,
        Info = 1,
        Warn = 2,
        Error = 3,
        Fatal = 4
    };
    Q_ENUM(Level)

    static AppLogger& instance();

    // 中文注释：程序启动时调用一次，安装 Qt 全局日志处理器并初始化日志目录/级别
    void initialize(Level minLevel,
                    const QString &logDirPath,
                    bool forwardToPreviousHandler,
                    int retentionDays = 15);
    void shutdown();

signals:
    // 中文注释：发往 UI 线程显示的已格式化日志
    void logLineReady(const QString &line, int levelValue);

private:
    explicit AppLogger(QObject *parent = nullptr);
    ~AppLogger() override;
    AppLogger(const AppLogger&) = delete;
    AppLogger& operator=(const AppLogger&) = delete;

    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    QString levelToString(Level level) const;
    Level fromQtType(QtMsgType type) const;
    QString formatLine(Level level, const QString &msg, const QMessageLogContext &context) const;
    bool ensureLogFileReady();
    void rotateIfDayChanged(const QString &dayKey);
    void writeToFile(const QString &line);
    void cleanupOldLogsLocked();

private:
    mutable QMutex m_mutex;
    QString m_logDirPath;
    QString m_currentDayKey;
    QFile m_logFile;
    bool m_initialized = false;
    bool m_forwardToPreviousHandler = false;
    int m_retentionDays = 15;
    Level m_minLevel = Level::Info;
    QtMessageHandler m_previousHandler = nullptr;
};

#endif // APPLOGGER_H
