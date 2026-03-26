#include "core/LogWorker.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QtGlobal>
#include <algorithm>

LogWorker::LogWorker(QObject *parent)
    : QObject(parent)
    , m_dataBuffer(1000) // 中文注释：默认环形缓冲容量；后续可由 UI 参数化
{
    // 中文注释：50ms 生成一次 PlotDataChunk，与你现有控制周期保持一致
    // 关键：把 QTimer 设为子对象，确保它的线程亲和性随 LogWorker 一起 moveToThread。
    m_timer.setParent(this);
    m_timer.setInterval(50);
    connect(&m_timer, &QTimer::timeout, this, &LogWorker::onTimerTick);
    // 中文注释：定时器只在开始采集时启动，停止采集时真正停掉，
    // 避免退出阶段析构期间触发 QTimer 跨线程 stop 的 warning（killTimer）。
}

void LogWorker::setLogFilePath(const QString &path)
{
    // 中文注释：允许动态设置；设置后下次 onTimerTick 开始写入
    m_logFilePath = path;
    m_headerWritten = false;
}

void LogWorker::appendSample(const Sample &s)
{
    if (!m_captureEnabled)
    {
        // 中文注释：未开始采集时忽略样本，满足“只有点击开始才生成图像”
        return;
    }
    // 中文注释：只做环形缓冲写入，避免在该槽里做磁盘 IO（减少抖动）
    m_dataBuffer.append(s);
}

void LogWorker::startCapture()
{
    // 中文注释：开始前先清空旧数据，确保新曲线从最左端（x=0）重新开始
    m_dataBuffer.clear();
    m_captureEnabled = true;

    if (!m_timer.isActive())
    {
        m_timer.start();
    }

    // 中文注释：注入一个零点样本，让“当前值/控制量”曲线从 0 明确起步
    Sample s0;
    s0.timestampMs = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    s0.position = 0.0f;
    s0.controlOutput = 0.0f;
    m_dataBuffer.append(s0);
}

void LogWorker::stopCapture()
{
    // 中文注释：停止后不再接收样本，UI 将保持当前最后一帧图像不动
    m_captureEnabled = false;
    if (m_timer.isActive())
    {
        m_timer.stop();
    }
}

void LogWorker::clearData()
{
    // 中文注释：清除缓存并通知 UI 清空曲线
    m_captureEnabled = false;
    if (m_timer.isActive())
    {
        m_timer.stop();
    }
    m_dataBuffer.clear();
    PlotDataChunk emptyChunk;
    emit historyDataReady(emptyChunk);
}

void LogWorker::onTimerTick()
{
    if (!m_captureEnabled)
    {
        return;
    }

    // 中文注释：从环形缓冲区取出当前窗口数据（拷贝），再做下采样
    const QVector<Sample> samples = m_dataBuffer.window();
    if (samples.isEmpty())
    {
        return;
    }

    // 中文注释：简单下采样策略：最多输出 200 个点，避免 UI 绘制压力
    const int maxPoints = 200;
    const int n = samples.size();
    const int step = std::max(1, n / maxPoints);

    const quint64 t0 = samples.first().timestampMs;
    PlotDataChunk chunk;
    chunk.positionSeries.reserve((n + step - 1) / step);
    chunk.controlSeries.reserve((n + step - 1) / step);

    for (int i = 0; i < n; i += step)
    {
        const Sample &s = samples[i];
        const double tSec = (t0 == 0) ? 0.0 : static_cast<double>(s.timestampMs - t0) / 1000.0;
        chunk.positionSeries.push_back(QPointF(tSec, static_cast<double>(s.position)));
        chunk.controlSeries.push_back(QPointF(tSec, static_cast<double>(s.controlOutput)));
    }

    // 中文注释：发往 UI 线程绘图
    emit historyDataReady(chunk);

    // 中文注释：可选日志写盘（批量写：这里简化为每次 tick 追加窗口末尾数据）
    if (!m_logFilePath.isEmpty())
    {
        QFile f(m_logFilePath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            // 中文注释：写盘失败不要崩溃，直接跳过
            return;
        }

        QTextStream ts(&f);
        if (!m_headerWritten)
        {
            ts << "timestampMs,position,controlOutput,speed,current,voltage,statusFlags\n";
            m_headerWritten = true;
        }

        // 中文注释：只写最近若干个点，避免每 50ms 写全窗口太重
        const qsizetype writeTail = std::min<qsizetype>(qsizetype(50), samples.size());
        for (qsizetype i = samples.size() - writeTail; i < samples.size(); ++i)
        {
            const Sample &s = samples[i];
            ts << s.timestampMs << ","
               << s.position << ","
               << s.controlOutput << ","
               << s.speed << ","
               << s.current << ","
               << s.voltage << ","
               << s.statusFlags << "\n";
        }
        f.close();
    }
}

