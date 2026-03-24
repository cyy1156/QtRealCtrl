#ifndef LOGWORKER_H
#define LOGWORKER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include "core/DataBuffer.h"

// 中文注释：日志/绘图数据处理工作线程（线程 3）
class LogWorker : public QObject
{
    Q_OBJECT
public:
    explicit LogWorker(QObject *parent = nullptr);

    // 中文注释：可选设置日志文件路径；不设置则只做内存缓存与 PlotDataChunk 生成
    void setLogFilePath(const QString &path);

public slots:
    // 中文注释：由数据线程发来样本，追加到环形缓冲区
    void appendSample(const Sample &s);
    // 中文注释：开始采集（只在点击“开始”后允许样本进入缓冲区）
    void startCapture();
    // 中文注释：停止采集（点击“停止”后冻结图像）
    void stopCapture();
    // 中文注释：清空缓存并通知 UI 清图（点击“清除”）
    void clearData();

private slots:
    // 中文注释：定时从 DataBuffer.window() 获取窗口数据并“下采样/裁剪”生成 PlotDataChunk
    void onTimerTick();

signals:
    // 中文注释：发往 UI 线程用于绘图（当前工程尚未接入 PlotWidget，但信号可先保留）
    void historyDataReady(const PlotDataChunk &chunk);

private:
    DataBuffer m_dataBuffer;
    QTimer m_timer;

    QString m_logFilePath;
    bool m_headerWritten = false;
    bool m_captureEnabled = false; // 中文注释：采集开关，控制是否接收/处理样本
};

#endif // LOGWORKER_H

