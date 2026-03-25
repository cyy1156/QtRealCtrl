#include "ControlManager.h"
#include <QDebug>
#include <QByteArray>
#include "protocol/framecodec.h"
#include "protocol/tlvcodec.h"
#include <cmath>
#include <limits>
#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcControl, "Control")
ControlManager::ControlManager(SerialDevice* dev,SysInfo* sys,IAlgorithm* alg,QObject *parent)
    : QObject(parent),m_dev(dev),m_sys(sys)
{
    // 中文注释：为了支持 moveToThread，让定时器成为 ControlManager 的子对象
    m_timer.setParent(this);
    Q_ASSERT(m_dev);//断言宏检查指针是否为空
    Q_ASSERT(m_sys);
    connect(m_dev,&SerialDevice::frameReceived,this,[this](quint8 t,quint16 s,QByteArray p){
        onFrame(t,s,p);
    });
    m_timer.setInterval(50);//设置定时器的触发间隔
    connect(&m_timer,&QTimer::timeout,this,[this](){
        runStep();
    });
    m_alg=alg;
}
bool ControlManager::sendEnable(quint8 enable)
{
    QVector<TlvItem> items;
    // 中文注释：CONTROL_CMD 使用 Enable 字段表达启停控制（0=停止，1=启动）
    Tlvcodec::appendUInt8(items,TlvType::Enable,enable);
    const QByteArray payload =Tlvcodec::encodeItems(items);
    return m_dev->send(static_cast<quint8>(MsgType::CONTROL_CMD),0,payload);
}
void ControlManager::start()
{
    if(!m_dev->isOpen())
    {
        qCWarning(lcControl) << "ControlManager start failed: device not open";
        return;
    }
    // 中文注释：先下发“使能=1”，再启动控制周期，避免设备未使能就开始计算
    if(!sendEnable(1))
    {
        qCWarning(lcControl) << "send CONTROL_CMD(enable=1) failed";
    }
    if(!m_timer.isActive()) m_timer.start();
}
void ControlManager::stop()
{
    // 中文注释：停止时先下发“使能=0”，让设备立即退出执行态
    if(!sendEnable(0))
    {
        qCWarning(lcControl) << "send CONTROL_CMD(enable=0) failed";
    }
    if(m_timer.isActive()) m_timer.stop();
}
void ControlManager::clear()
{
    // 中文注释：清除时先停控制周期，避免边清除边继续下发控制量
    if(m_timer.isActive()) m_timer.stop();

    // 中文注释：约定 Enable=2 表示“清零命令”（FakeDevice 会把 current/target/u 清零）
    if(!sendEnable(2))
    {
        qCWarning(lcControl) << "send CONTROL_CMD(enable=2 clear) failed";
    }

    // 中文注释：控制侧本地状态也同步归零，避免下一拍继续沿用旧控制输出
    m_lastControlOutput = 0.0;
    m_cachedCurrent = 0.0;
    m_haveCachedCurrent = true;
    m_sys->setTargetValue(0.0);
    m_sys->setCurrentValue(0.0);
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
        bool havePos = false; // 中文注释：记录是否成功解码 FeedbackPosition
        static double lastFb = std::numeric_limits<double>::quiet_NaN();
        static quint8 lastMode = 0xFF;

        if (Tlvcodec::tryGetFloat(items, TlvType::FeedbackPosition, fb)) {
            // 中文注释：更新 SysInfo（UI 展示来源），并同步更新控制计算缓存
            m_sys->setCurrentValue(static_cast<double>(fb));
            m_cachedCurrent = static_cast<double>(fb);
            m_haveCachedCurrent = true;
            havePos = true;

            // 中文注释：为避免刷屏，只在反馈位置明显变化时打印
            if (std::isnan(lastFb) || std::fabs(fb - lastFb) > 1e-3) {
                qCInfo(lcControl) << "PC:SetCurrentValue=" << fb;
                lastFb = fb;
            }
        }
        quint8 mode = 0;
        if (Tlvcodec::tryGetUInt8(items, TlvType::Mode, mode)) {
            // 中文注释：更新 SysInfo（UI 关联状态）
            m_sys->setMode(mode);

            // 中文注释：同样只在模式变化时打印
            if (mode != lastMode) {
                qCInfo(lcControl) << "PC: SetMode=" << mode;
                lastMode = mode;
            }
        }

        // 中文注释：构造 Sample 并发给日志线程（线程 3）
        // 目前工程只有 position/mode 被 FakeDevice 回传，所以 speed/current/voltage 暂时填 0
        //有点和有缓存值
        if (havePos && m_haveCachedCurrent) {
            Sample s;
            s.timestampMs = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
            s.position = fb;
            s.controlOutput = static_cast<float>(m_lastControlOutput);
            s.speed = 0.0f;
            s.current = 0.0f;
            s.voltage = 0.0f;
            s.statusFlags = static_cast<quint32>(mode);
            emit telemetrySampleReady(s);
        }
        return;
    }
    // 其它 MsgType（ACK/HELLO 等）后续再补
}
void ControlManager::runStep()
{
    if(!m_alg)
    {
        qCWarning(lcControl) << "runStep: algorithm is null";
        return;
    }
    const double target =m_sys->targetValue();
    // 中文注释：控制计算使用缓存的最近遥测值，避免同线程事件顺序导致读取到旧 current
    if(!m_haveCachedCurrent)
    {
        qCInfo(lcControl) << "waiting first TELEMETRY ...";
        return;
    }
    const double currentUsed = m_cachedCurrent;
    const double dtSec =0.05;//50ms
    const double u=m_alg->compute(target,currentUsed,dtSec);
    // 中文注释：缓存控制输出，供后续 TELEMETRY 到来时写入 Sample（用于第二条曲线）
    m_lastControlOutput = u;

    QVector<TlvItem> items;
    Tlvcodec::appendFloat(items,TlvType::TargetPosition,static_cast<float>(target));
    Tlvcodec::appendFloat(items,TlvType::TargetTorque,static_cast<float>(u));

    const QByteArray payload=Tlvcodec::encodeItems(items);
    const bool ok =m_dev->send(static_cast<quint8>(MsgType::Set_TARGET),0,payload);
    if(!ok)
    {
        qCWarning(lcControl) << "send SET_TARGET failed";
    }
    qCDebug(lcControl) << "alg=" << m_alg->name()
                       << "target=" << target
                       << "currentUsed=" << currentUsed
                       << "u=" << u;

}
void ControlManager::setAlgorithm(IAlgorithm* alg)
{
    if(alg==nullptr)
    {
        qCWarning(lcControl) << "setAlgorithm failed: alg is null";
        return;
    }
    if(m_alg==alg) return;
    m_alg=alg;
    m_alg->reset();
    qCInfo(lcControl) << "switch algorithm to" << m_alg->name();
}








