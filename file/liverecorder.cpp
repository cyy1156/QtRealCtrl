#include "liverecorder.h"
#include <QDateTime>

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
    }
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
