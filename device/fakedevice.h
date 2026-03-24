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
    bool open();
    void close();
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
