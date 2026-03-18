#ifndef CONTROLMANAGER_H
#define CONTROLMANAGER_H

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include "core/Sysinfo.h"
#include "device/serialdevice.h"
class ControlManager : public QObject
{
    Q_OBJECT
public:
    explicit ControlManager(SerialDevice* dev, SysInfo* sys,QObject *parent = nullptr);
    void start();
    void stop();
    bool isRunning() const {return m_timer.isActive();}
    void setTarget(double target);
private:
    void onFrame(quint8 msgType,quint16 seq,QByteArray payload);//主控端接收设备的 “回信”
    void runStep();//主控端主动给设备发指令：

private:
    SerialDevice* m_dev=nullptr;
    SysInfo* m_sys =nullptr;
    QTimer m_timer;

};

#endif // CONTROLMANAGER_H
