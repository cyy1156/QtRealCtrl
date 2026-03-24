#ifndef DATABUFFER_H
#define DATABUFFER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QtGlobal>

// 中文注释：一个遥测样本，用于日志与曲线绘制窗口
struct Sample
{
    quint64 timestampMs = 0; // 中文注释：时间戳（毫秒）
    float position = 0.0f;  // 中文注释：反馈位置
    float controlOutput = 0.0f; // 中文注释：控制量（例如 PID 输出 u）
    float speed = 0.0f;     // 中文注释：反馈速度（当前工程可先填 0）
    float current = 0.0f;   // 中文注释：反馈电流（当前工程可先填 0）
    float voltage = 0.0f;   // 中文注释：反馈电压（当前工程可先填 0）
    quint32 statusFlags = 0; // 中文注释：状态位（可用于模式/故障）
};

// 中文注释：给 UI/绘图组件的“整理后数据块”，避免在日志线程里直接操作 GUI
struct PlotDataChunk//可视化绘图片段
{
    QVector<QPointF> positionSeries; // 中文注释：位置曲线点集（x=秒，y=position）
    QVector<QPointF> controlSeries;  // 中文注释：控制量曲线点集（x=秒，y=controlOutput）
};

// 中文注释：固定容量环形缓冲区，仅在日志线程内部读写（避免跨线程互斥）
class DataBuffer
{
public:
    explicit DataBuffer(int capacity = 1000);

    // 中文注释：追加一个样本；当缓冲满时覆盖最旧数据
    void append(const Sample &s);

    // 中文注释：返回当前窗口内的样本拷贝（用于生成 PlotDataChunk）
    QVector<Sample> window() const;

    // 中文注释：清空环形缓冲区内容与写指针（用于“清除”按钮）
    void clear();

private:
    int m_capacity = 0;
    QVector<Sample> m_buffer;
    int m_writeIndex = 0;
    bool m_filled = false;
};

#endif // DATABUFFER_H

