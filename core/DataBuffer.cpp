#include "DataBuffer.h"
#include <algorithm>

DataBuffer::DataBuffer(int capacity)
    : m_capacity(capacity)
{
    if (m_capacity <= 0)
    {
        m_capacity = 1;
    }
    m_buffer.resize(m_capacity);
}

void DataBuffer::append(const Sample &s)
{
    // 中文注释：写入到当前写指针位置
    m_buffer[m_writeIndex] = s;
    m_writeIndex++;
    if (m_writeIndex >= m_capacity)
    {
        m_writeIndex = 0;
        m_filled = true; // 中文注释：至少覆盖过一次，说明缓冲已满
    }
}

QVector<Sample> DataBuffer::window() const
{
    // 中文注释：返回“按时间顺序”的窗口拷贝
    if (!m_filled)
    {
        const int count = m_writeIndex; // 中文注释：未满时写指针即为有效样本数
        QVector<Sample> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            out.push_back(m_buffer[i]);
        }
        return out;
    }

    // 中文注释：已满时，数据顺序从 writeIndex 开始回绕
    QVector<Sample> out;
    out.reserve(m_capacity);
    for (int i = 0; i < m_capacity; ++i)
    {
        const int idx = (m_writeIndex + i) % m_capacity;
        out.push_back(m_buffer[idx]);
    }
    return out;
}

void DataBuffer::clear()
{
    // 中文注释：复位缓冲区状态；数据内容保留与否不重要，因为 writeIndex/m_filled 已重置
    m_writeIndex = 0;
    m_filled = false;
}

