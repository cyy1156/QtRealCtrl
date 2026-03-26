#ifndef RAWRXRECORDER_H
#define RAWRXRECORDER_H

#include <QFile>
#include <QTextStream>
#include <QString>
#include <QByteArray>
#include <QtGlobal>

// 中文注释：把“串口原始 RX 字节流”按时间写入 CSV，供离线分析。
class RawRxRecorder
{
public:
    bool isOpen() const;

    // 打开/追加到 CSV 文件；若文件为空则写入表头
    bool openCsv(const QString &path, QString *errorMessage = nullptr);
    // src: "pc" 或 "fake" 用于区分字节来源端
    void appendRx(quint64 timestampMs, const QByteArray &bytes, const QString &src);
    void close();

private:
    static double calcElapsedSec(quint64 baseTimestampMs, quint64 currentTimestampMs);

    QFile m_csvFile;
    QTextStream m_csvStream;
    int m_csvPendingFlushRows = 0;
    quint64 m_csvBaseTimestampMs = 0;
};

#endif // RAWRXRECORDER_H

