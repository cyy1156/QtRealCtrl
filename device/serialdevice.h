#ifndef SERIALDEVICE_H
#define SERIALDEVICE_H

#include "device/DeviceInterface.h"
#include "protocol/framecodec.h"
#include <QSerialPort>
#include "DeviceInterface.h"

class SerialDevice : public DeviceInterface
{
    Q_OBJECT
public:
    explicit SerialDevice(QObject *parent = nullptr);

    void setPortName(const QString& name);
    void setBaudRate(quint32 baud);//设置波特率


    bool open() override;
    void close() override;
    bool isOpen() const override;

    //发送： 自动匹配seq、组帧、写串口
    bool send (quint8 msgType,quint8 flags,const QByteArray& payload);
private:
    void setupConnections();
private:
    QString m_portName;
    quint32 m_baud =115200;

    QSerialPort m_port; //串口
    FrameCodec m_codec; //一帧包
    quint16 m_seq=1;

};

#endif // SERIALDEVICE_H
