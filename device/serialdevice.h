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

    void setRawSerialLogEnabled(bool enabled);
    void setRawRxCsvRecordingEnabled(bool enabled);

    void setPortName(const QString& name);
    void setBaudRate(quint32 baud);//设置波特率
    void setConfig(const SerialPortConfig& cfg);
    SerialPortConfig config() const;


    bool open() override;
    void close() override;
    bool isOpen() const override;

    //发送： 自动匹配seq、组帧、写串口
    bool send (quint8 msgType,quint8 flags,const QByteArray& payload);

signals:
    // 中文注释：当开启原始 RX 录制时，向上层抛出收到的字节流（用于写入 raw CSV）
    void rawRxBytesReady(quint64 timestampMs, QByteArray bytes);
private:
    void setupConnections();
private:
    SerialPortConfig m_cfg;
    bool m_rawSerialLogEnabled = false;
    bool m_rawRxCsvRecordingEnabled = false;

    QSerialPort m_port; //串口
    FrameCodec m_codec; //一帧包
    quint16 m_seq=1;

};

#endif // SERIALDEVICE_H
