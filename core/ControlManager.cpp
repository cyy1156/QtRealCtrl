#include "ControlManager.h"
#include <QDebug>
#include <QByteArray>
#include "protocol/framecodec.h"
#include "protocol/tlvcodec.h"
ControlManager::ControlManager(SerialDevice* dev,SysInfo* sys,QObject *parent)
    : QObject(parent),m_dev(dev),m_sys(sys)
{
    Q_ASSERT(m_dev);//断言宏检查指针是否为空
    Q_ASSERT(m_sys);
    connect(m_dev,&SerialDevice::frameReceived,this,[this](quint8 t,quint16 s,QByteArray p){
        onFrame(t,s,p);
    });
    m_timer.setInterval(50);//设置定时器的触发间隔
    connect(&m_timer,&QTimer::timeout,this,[this](){
        runStep();
    });
}
void ControlManager::start()
{
    if(!m_dev->isOpen())
    {
        qWarning()<<"ControlManger start failed: device not open";
        return;
    }
    if(!m_timer.isActive()) m_timer.start();
}
void ControlManager::stop()
{
    if(m_timer.isActive()) m_timer.stop();
}
void ControlManager::setTarget(double target)
{
    m_sys->setTargetValue(target);
}
void ControlManager::onFrame(quint8 msgType,quint16 seq,QByteArray payload)
{
    Q_UNUSED(seq);//不要报「未使用变量」警告。
    if(msgType ==static_cast<quint8>(MsgType::TELEMETRY))//上报数据
    {
        auto items =Tlvcodec::decodeItems(payload);
        float fb =0.0f;
        if(Tlvcodec::tryGetFloat(items,TlvType::FeedbackPosition,fb))
        {
            m_sys->setCurrentValue(static_cast<double>(fb));
            qDebug()<<"PC:SetCurrentValue="<<fb;
        }
        quint8 mode=0;
        if(Tlvcodec::tryGetUInt8(items,TlvType::Mode,mode))
        {
            m_sys->setMode(mode);
            qDebug()<<"PC: SetMode="<<mode;
        }
        return;
    }
    // 其它 MsgType（ACK/HELLO 等）后续再补
}
void ControlManager::runStep()
{
    QVector<TlvItem> items;
    Tlvcodec::appendFloat(items,TlvType::TargetPosition,static_cast<float>(m_sys->targetValue()));

    QByteArray payload=Tlvcodec::encodeItems(items);
    const bool ok =m_dev->send(static_cast<quint8>(MsgType::Set_TARGET),0,payload);
    if(!ok)
    {
        qWarning()<<"send SET_TARGET failed";
    }
}









