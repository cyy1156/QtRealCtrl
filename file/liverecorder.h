#ifndef LIVERECORDER_H
#define LIVERECORDER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "core/DataBuffer.h"

class LiveRecorder
{
public:
    // 0 表示不限制
    void setMaxCsvSizeBytes(quint64 bytes);
    void setMaxTxtSizeBytes(quint64 bytes);

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
    bool rotateCsvIfNeeded();
    bool rotateTxtIfNeeded();
    bool rotateCsv();
    bool rotateTxt();

    QFile m_csvFile;
    QTextStream m_csvStream;
    int m_csvPendingFlushRows = 0;
    quint64 m_csvBaseTimestampMs = 0;
    QString m_csvPath;
    quint64 m_maxCsvBytes = 0;

    QFile m_txtFile;
    QTextStream m_txtStream;
    int m_txtPendingFlushRows = 0;
    quint64 m_txtBaseTimestampMs = 0;
    QString m_txtPath;
    quint64 m_maxTxtBytes = 0;
};

#endif // LIVERECORDER_H
