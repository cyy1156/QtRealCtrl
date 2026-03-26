#include "liverecorder.h"
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

static QString statusFlagToName(quint32 flags)
{
    switch (flags)
    {
    case 0U:
        return QStringLiteral("REALTIME");
    case 1U:
        return QStringLiteral("SERIAL_SIM");
    case 2U:
        return QStringLiteral("SOFT_SIM");
    default:
        return QStringLiteral("STATUS_%1").arg(flags);
    }
}

void LiveRecorder::setMaxCsvSizeBytes(quint64 bytes)
{
    m_maxCsvBytes = bytes;
}

void LiveRecorder::setMaxTxtSizeBytes(quint64 bytes)
{
    m_maxTxtBytes = bytes;
}

double LiveRecorder::calcElapsedSec(quint64 baseTimestampMs, quint64 currentTimestampMs)
{
    if (baseTimestampMs == 0 || currentTimestampMs < baseTimestampMs)
    {
        return 0.0;
    }
    return static_cast<double>(currentTimestampMs - baseTimestampMs) / 1000.0;
}

bool LiveRecorder::isCsvOpen() const
{
    return m_csvFile.isOpen();
}

bool LiveRecorder::isTxtOpen() const
{
    return m_txtFile.isOpen();
}

bool LiveRecorder::openCsv(const QString &path, QString *errorMessage)
{
    closeCsv();
    m_csvPath = path;

    // 若文件已超过限制，优先轮转到新文件，避免无限膨胀
    if (m_maxCsvBytes > 0)
    {
        const QFileInfo fi(path);
        if (fi.exists() && fi.size() > static_cast<qint64>(m_maxCsvBytes))
        {
            // 轮转失败就直接返回错误，让用户处理权限/占用问题
            const QString rotatedPath =
                fi.dir().filePath(fi.completeBaseName() + "_rotated_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "." + fi.suffix());
            if (!QFile::rename(path, rotatedPath))
            {
                if (errorMessage)
                {
                    *errorMessage = QStringLiteral("文件过大且轮转失败（可能无权限或文件仍被占用）。");
                }
                return false;
            }
        }
    }

    m_csvFile.setFileName(path);
    if (!m_csvFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = m_csvFile.errorString();
        }
        return false;
    }
    m_csvStream.setDevice(&m_csvFile);
    if (m_csvFile.size() == 0)
    {
        m_csvStream << "date,time,elapsed_sec,target,position,control_output,mode,algorithm,status_flags\n";
        m_csvStream.flush();
    }
    m_csvPendingFlushRows = 0;
    m_csvBaseTimestampMs = 0;
    return true;
}

bool LiveRecorder::openTxt(const QString &path, QString *errorMessage)
{
    closeTxt();
    m_txtPath = path;

    if (m_maxTxtBytes > 0)
    {
        const QFileInfo fi(path);
        if (fi.exists() && fi.size() > static_cast<qint64>(m_maxTxtBytes))
        {
            const QString rotatedPath =
                fi.dir().filePath(fi.completeBaseName() + "_rotated_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "." + fi.suffix());
            if (!QFile::rename(path, rotatedPath))
            {
                if (errorMessage)
                {
                    *errorMessage = QStringLiteral("文件过大且轮转失败（可能无权限或文件仍被占用）。");
                }
                return false;
            }
        }
    }

    m_txtFile.setFileName(path);
    if (!m_txtFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = m_txtFile.errorString();
        }
        return false;
    }
    m_txtStream.setDevice(&m_txtFile);
    if (m_txtFile.size() == 0)
    {
        m_txtStream << "# QtRealCtrl realtime text log\n";
        m_txtStream << "# columns: date time elapsed_sec target position control_output mode algorithm status_flags\n";
        m_txtStream.flush();
    }
    m_txtPendingFlushRows = 0;
    m_txtBaseTimestampMs = 0;
    return true;
}

void LiveRecorder::appendCsvSample(const Sample &sample,
                                   double target,
                                   const QString &modeText,
                                   const QString &algText)
{
    if (!m_csvFile.isOpen())
    {
        return;
    }
    if (m_csvBaseTimestampMs == 0)
    {
        m_csvBaseTimestampMs = sample.timestampMs;
    }
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(sample.timestampMs));
    const double elapsedSec = calcElapsedSec(m_csvBaseTimestampMs, sample.timestampMs);
    m_csvStream
        << dt.toString("yyyy-MM-dd") << ','
        << dt.toString("HH:mm:ss.zzz") << ','
        << QString::number(elapsedSec, 'f', 3) << ','
        << QString::number(target, 'f', 3) << ','
        << QString::number(sample.position, 'f', 6) << ','
        << QString::number(sample.controlOutput, 'f', 6) << ','
        << modeText << ','
        << algText << ','
        << statusFlagToName(sample.statusFlags)
        << '\n';

    ++m_csvPendingFlushRows;
    if (m_csvPendingFlushRows >= 20)
    {
        m_csvStream.flush();
        m_csvPendingFlushRows = 0;
        // 每次 flush 后做一次轮转检查，避免无限增长
        rotateCsvIfNeeded();
    }
}

void LiveRecorder::appendTxtSample(const Sample &sample,
                                   double target,
                                   const QString &modeText,
                                   const QString &algText)
{
    if (!m_txtFile.isOpen())
    {
        return;
    }
    if (m_txtBaseTimestampMs == 0)
    {
        m_txtBaseTimestampMs = sample.timestampMs;
    }
    const double elapsedSec = calcElapsedSec(m_txtBaseTimestampMs, sample.timestampMs);
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(sample.timestampMs));
    m_txtStream
        << "date=" << dt.toString("yyyy-MM-dd")
        << " time=" << dt.toString("HH:mm:ss.zzz")
        << " elapsed_sec=" << QString::number(elapsedSec, 'f', 3)
        << " target=" << QString::number(target, 'f', 3)
        << " position=" << QString::number(sample.position, 'f', 6)
        << " control_output=" << QString::number(sample.controlOutput, 'f', 6)
        << " mode=" << modeText
        << " algorithm=" << algText
        << " status_flags=" << statusFlagToName(sample.statusFlags)
        << '\n';

    ++m_txtPendingFlushRows;
    if (m_txtPendingFlushRows >= 20)
    {
        m_txtStream.flush();
        m_txtPendingFlushRows = 0;
        rotateTxtIfNeeded();
    }
}

bool LiveRecorder::rotateCsvIfNeeded()
{
    if (m_maxCsvBytes == 0)
    {
        return false;
    }
    if (!m_csvFile.isOpen())
    {
        return false;
    }
    if (m_csvPath.isEmpty())
    {
        return false;
    }
    if (m_csvFile.size() <= static_cast<qint64>(m_maxCsvBytes))
    {
        return false;
    }
    return rotateCsv();
}

bool LiveRecorder::rotateTxtIfNeeded()
{
    if (m_maxTxtBytes == 0)
    {
        return false;
    }
    if (!m_txtFile.isOpen())
    {
        return false;
    }
    if (m_txtPath.isEmpty())
    {
        return false;
    }
    if (m_txtFile.size() <= static_cast<qint64>(m_maxTxtBytes))
    {
        return false;
    }
    return rotateTxt();
}

bool LiveRecorder::rotateCsv()
{
    if (!m_csvFile.isOpen() || m_csvPath.isEmpty())
    {
        return false;
    }

    // 保留 baseTimestamp：轮转成功时重置，失败时继续追加
    const quint64 keepBaseTimestampMs = m_csvBaseTimestampMs;

    m_csvStream.flush();
    m_csvFile.close();

    const QFileInfo fi(m_csvPath);
    const QString rotatedPath =
        fi.dir().filePath(fi.completeBaseName() + "_rotated_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "." + fi.suffix());

    const bool renamed = QFile::rename(m_csvPath, rotatedPath);

    m_csvFile.setFileName(m_csvPath);
    if (!m_csvFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        return false;
    }
    m_csvStream.setDevice(&m_csvFile);

    // 轮转成功：从新文件开始写表头，并重置时间基准
    if (renamed)
    {
        m_csvPendingFlushRows = 0;
        m_csvBaseTimestampMs = 0;
        if (m_csvFile.size() == 0)
        {
            m_csvStream << "date,time,elapsed_sec,target,position,control_output,mode,algorithm,status_flags\n";
            m_csvStream.flush();
        }
    }
    else
    {
        // 轮转失败：尽量继续写，保持时间基准不变
        m_csvBaseTimestampMs = keepBaseTimestampMs;
        m_csvPendingFlushRows = 0;
    }

    return renamed;
}

bool LiveRecorder::rotateTxt()
{
    if (!m_txtFile.isOpen() || m_txtPath.isEmpty())
    {
        return false;
    }

    const quint64 keepBaseTimestampMs = m_txtBaseTimestampMs;

    m_txtStream.flush();
    m_txtFile.close();

    const QFileInfo fi(m_txtPath);
    const QString rotatedPath =
        fi.dir().filePath(fi.completeBaseName() + "_rotated_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "." + fi.suffix());

    const bool renamed = QFile::rename(m_txtPath, rotatedPath);

    m_txtFile.setFileName(m_txtPath);
    if (!m_txtFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        return false;
    }
    m_txtStream.setDevice(&m_txtFile);

    if (renamed)
    {
        m_txtPendingFlushRows = 0;
        m_txtBaseTimestampMs = 0;
        if (m_txtFile.size() == 0)
        {
            m_txtStream << "# QtRealCtrl realtime text log\n";
            m_txtStream << "# columns: date time elapsed_sec target position control_output mode algorithm status_flags\n";
            m_txtStream.flush();
        }
    }
    else
    {
        m_txtBaseTimestampMs = keepBaseTimestampMs;
        m_txtPendingFlushRows = 0;
    }

    return renamed;
}

void LiveRecorder::closeCsv()
{
    if (!m_csvFile.isOpen())
    {
        return;
    }
    m_csvStream.flush();
    m_csvFile.close();
    m_csvPendingFlushRows = 0;
    m_csvBaseTimestampMs = 0;
}

void LiveRecorder::closeTxt()
{
    if (!m_txtFile.isOpen())
    {
        return;
    }
    m_txtStream.flush();
    m_txtFile.close();
    m_txtPendingFlushRows = 0;
    m_txtBaseTimestampMs = 0;
}

void LiveRecorder::closeAll()
{
    closeCsv();
    closeTxt();
}
