#ifndef SERIALDEVICE_H
#define SERIALDEVICE_H

#include "device/DeviceInterface.h"
#include "protocol/framecodec.h"
#include <QSerialPort>
#include "DeviceInterface.h"

struct SerialPortConfig
{
    QString portName;
    qint32 baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
};

class SerialDevice : public DeviceInterface
{
    Q_OBJECT
public:
    explicit SerialDevice(QObject *parent = nullptr);

    void setPortName(const QString& name);
    void setBaudRate(quint32 baud);//设置波特率
    void setConfig(const SerialPortConfig& cfg);
    SerialPortConfig config() const;


    bool open() override;
    void close() override;
    bool isOpen() const override;

    //发送： 自动匹配seq、组帧、写串口
    bool send (quint8 msgType,quint8 flags,const QByteArray& payload);
private:
    void setupConnections();
private:
    SerialPortConfig m_cfg;

    QSerialPort m_port; //串口
    FrameCodec m_codec; //一帧包
    quint16 m_seq=1;

};

#endif // SERIALDEVICE_H
