#include "fakedevice.h"
#include <QDebug>
#include "protocol/framecodec.h"
#include "protocol/tlvcodec.h"
FakeDevice::FakeDevice(QObject *parent)
    : QObject{parent}
{
    // 中文注释：为了支持 moveToThread，把 SerialDevice/QTimer 也挂到 QObject 树中
    m_dev.setParent(this);
    m_timer.setParent(this);

    // 中文注释：只连接一次 frameReceived，避免重复连接导致槽函数叠加触发
    connect(&m_dev,&SerialDevice::frameReceived,this,[this](quint8 t,quint16 s,QByteArray p){
        onFrame(t,s,p);
    });

    // 中文注释：定时器连接放在构造函数中，只建立一次
    m_timer.setInterval(50);
    connect(&m_timer,&QTimer::timeout,this,[this](){tick();});
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
        float torqueCmd=0.0f;
        if(Tlvcodec::tryGetFloat(items,TlvType::TargetTorque,torqueCmd)){
            // 中文注释：保存上位机下发的控制量 u（映射为力矩命令）
            m_torqueCmd=torqueCmd;
            qDebug()<<"[FakeDevice] torqueCmd="<<m_torqueCmd;
        }
    } else if(msgTpye==static_cast<quint8>(MsgType::CONTROL_CMD)) {
        auto items=Tlvcodec::decodeItems(payload);
        quint8 enable=0;
        if(Tlvcodec::tryGetUInt8(items,TlvType::Enable,enable)){
            if(enable==1){
                // 中文注释：开始命令 -> 启动遥测定时器
                if(!m_timer.isActive()) m_timer.start();
            }else if(enable==0){
                // 中文注释：停止命令 -> 停止遥测并保持当前值不再变化（冻结）
                if(m_timer.isActive()) m_timer.stop();
                m_torqueCmd=0.0f;
                m_target=m_current;
            }else if(enable==2){
                // 中文注释：清零命令 -> 停止遥测并把当前值/目标值/控制量全部清零
                if(m_timer.isActive()) m_timer.stop();
                m_torqueCmd=0.0f;
                m_target=0.0f;
                m_current=0.0f;
            }
        }
    }
}
void FakeDevice::tick()
{
    // 中文注释：目标跟踪项（趋近目标位置）+ 控制量项（u 直接参与状态更新）
    m_current = m_current
                + (m_target - m_current) * 0.08f
                + m_torqueCmd * 0.02f;

    QVector<TlvItem> items;
    Tlvcodec::appendFloat(items,TlvType::FeedbackPosition,m_current);
    Tlvcodec::appendUInt8(items,TlvType::Mode,1);

    QByteArray payload=Tlvcodec::encodeItems(items);
    m_dev.send(static_cast<quint8>(MsgType::TELEMETRY),0,payload);
}
















