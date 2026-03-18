#include "fakedevice.h"
#include <QDebug>
#include "protocol/framecodec.h"
#include "protocol/tlvcodec.h"
FakeDevice::FakeDevice(QObject *parent)
    : QObject{parent}
{
    connect(&m_dev,&SerialDevice::frameReceived,this,[this](quint8 t,quint16 s,QByteArray p){
        onFrame(t,s,p);
        m_timer.setInterval(50);//设置时间
        connect(&m_timer,&QTimer::timeout,this,[this](){tick();});

    });
}
void FakeDevice::setPortName(const QString& name)
{
    m_dev.setPortName(name);
}
bool FakeDevice::open()
{
    if(!m_dev.open())return false;
    if(!m_timer.isActive()) m_timer.start();
    return true;

}
void FakeDevice::close()
{
    if(m_timer.isActive()) m_timer.stop();
    m_dev.close();
}
void FakeDevice::onFrame(quint8 msgTpye,quint16 seq,QByteArray payload)
{
    Q_UNUSED(seq);
    if(msgTpye==static_cast<quint8>(MsgType::Set_TARGET))
    {
        auto items=Tlvcodec::decodeItems(payload);
        float tgt=0.0f;
        if(Tlvcodec::tryGetFloat(items,TlvType::TargetPosition,tgt)){
            m_target=tgt;
            qDebug()<<"[FakeDevice] target="<<m_target;
        }
    }
}
void FakeDevice::tick()
{
    // 模拟一阶系统：current 逐步逼近 target
    m_current=m_current+(m_target-m_current)*0.1f;

    QVector<TlvItem> items;
    Tlvcodec::appendFloat(items,TlvType::FeedbackPosition,m_current);
    Tlvcodec::appendUInt8(items,TlvType::Mode,1);

    QByteArray payload=Tlvcodec::encodeItems(items);
    m_dev.send(static_cast<quint8>(MsgType::TELEMETRY),0,payload);
}
















