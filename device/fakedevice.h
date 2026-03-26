//虚拟串口
#ifndef FAKEDEVICE_H
#define FAKEDEVICE_H

#include <QObject>
#include <QTimer>
#include "device/serialdevice.h"
class FakeDevice : public QObject
{
    Q_OBJECT
public:
    explicit FakeDevice(QObject *parent = nullptr);
    void setPortName(const QString& name);
    void setConfig(const SerialPortConfig& cfg);
    void setRawSerialLogEnabled(bool enabled);
    void setRawRxCsvRecordingEnabled(bool enabled);
    SerialPortConfig config() const;
    bool isOpen() const;
    bool open();
    void close();
signals:
    void opened();
    void closed();
    void errorOccurred(QString message);
    void frameReceived(quint8 msgType, quint16 seq, QByteArray payload);
    // 中文注释：转发 SerialDevice 的原始 RX bytes（用于 raw CSV 录制）
    void rawRxBytesReady(quint64 timestampMs, QByteArray bytes);
private:
    void onFrame(quint8 msgType,quint16 seq,QByteArray payload);    //主控端接收设备的 “回信”
    void tick();
private:
    SerialDevice m_dev;
    QTimer m_timer;

    float m_target=0.0f;
    float m_torqueCmd=0.0f;
    float m_current=0.0f;
};

#endif // FAKEDEVICE_H
