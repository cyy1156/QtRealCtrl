#ifndef LIVERECORDER_H
#define LIVERECORDER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include "core/DataBuffer.h"

class LiveRecorder
{
public:
    bool isCsvOpen() const;
    bool isTxtOpen() const;

    bool openCsv(const QString &path, QString *errorMessage = nullptr);
    bool openTxt(const QString &path, QString *errorMessage = nullptr);

    void appendCsvSample(const Sample &sample,
                         double target,
                         const QString &modeText,
                         const QString &algText);
    void appendTxtSample(const Sample &sample,
                         double target,
                         const QString &modeText,
                         const QString &algText);

    void closeCsv();
    void closeTxt();
    void closeAll();

private:
    static double calcElapsedSec(quint64 baseTimestampMs, quint64 currentTimestampMs);

    QFile m_csvFile;
    QTextStream m_csvStream;
    int m_csvPendingFlushRows = 0;
    quint64 m_csvBaseTimestampMs = 0;

    QFile m_txtFile;
    QTextStream m_txtStream;
    int m_txtPendingFlushRows = 0;
    quint64 m_txtBaseTimestampMs = 0;
};

#endif // LIVERECORDER_H
