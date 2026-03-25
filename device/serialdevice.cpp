#include "serialdevice.h"
#include <QDebug>
SerialDevice::SerialDevice(QObject *parent)
    : DeviceInterface{parent}
{
    // 中文注释：为了支持 moveToThread，把 QObject 成员也挂到当前对象树中
    m_port.setParent(this);
    m_codec.setParent(this);
    setupConnections();
}

void SerialDevice::setPortName(const QString& name)
{
    m_cfg.portName = name;
}

void SerialDevice::setBaudRate(quint32 baud)
{
    m_cfg.baudRate = static_cast<qint32>(baud);
}

void SerialDevice::setConfig(const SerialPortConfig& cfg)
{
    m_cfg = cfg;
}

SerialPortConfig SerialDevice::config() const
{
    return m_cfg;
}
bool SerialDevice::isOpen() const
{
    return m_port.isOpen();
}
bool SerialDevice::open()
{
    if(m_cfg.portName.isEmpty())
    {
        emit errorOccurred("SerialDevice:portName is empty");
        return false;
    }
    if(m_port.isOpen())
    {
        return true;
    }
    m_port.setPortName(m_cfg.portName);
    m_port.setBaudRate(m_cfg.baudRate);
    m_port.setDataBits(m_cfg.dataBits);
    m_port.setParity(m_cfg.parity);//校验设置，无奇偶校验（因为我们已经使用了CRC校验）
    m_port.setStopBits(m_cfg.stopBits);
    m_port.setFlowControl(m_cfg.flowControl);//无流控模式（因为我们自定义了协议已经丢包）

    if(!m_port.open(QIODevice::ReadWrite)){
        //判断是否以可读写的方式打开
        emit errorOccurred("open failed"+m_port.errorString());
        return false;
    }
    emit opened();
    return true;
}

void SerialDevice::close()
{
    if(m_port.isOpen())
    {
        m_port.close();
        emit closed();
    }
}

bool SerialDevice::send(quint8 msgType,quint8 flags,const QByteArray& payload )
{
    if(!m_port.isOpen())
    {
        emit errorOccurred("send failed:port not open");
        return false;

    }

    const quint16 seq=m_seq++;
    const QByteArray frame =m_codec.encodeFrame(msgType,flags,seq,payload);

    if(frame.isEmpty())
    {
        emit errorOccurred("send failed: encodeFrame returned empty");
        return false;

    }
    const qint64 written=m_port.write(frame);
    if(written<0)
    {
        //emit触发信号槽的信号
        emit errorOccurred("write failed"+m_port.errorString());
        return false;
    }

    m_port.flush();
    return true;
}
void SerialDevice::setupConnections()
{
     // 串口收到字节 -> 喂给 FrameCodec
    // connect的第一个参数是信号发送者，第二个是要监听的信号，第三个是信号监听者，第四个要执行的槽函数
    // [this]：Lambda 的 “捕获列表”，表示捕获当前类的 this 指针，这样在 Lambda 内部才能访问 m_port、m_codec 这些成员变量；
    connect(&m_port,&QSerialPort::readyRead,this,[this](){
         const QByteArray bytes =m_port.readAll();
         m_codec.feedBytes(bytes);
     });
     // FrameCodec 出完整帧 -> 直接转发给上层
    connect(&m_codec,&FrameCodec::frameReceived,this,[this](quint8 msgType,quint16 seq,QByteArray payload){
        emit frameReceived(msgType,seq,payload);//这个是DeviceInterface里面的函数

    });
    connect(&m_port,&QSerialPort::errorOccurred,this,[this](QSerialPort::SerialPortError e){
        if(e==QSerialPort::NoError) return;
        emit errorOccurred("serial error:"+m_port.errorString());
    });
}












