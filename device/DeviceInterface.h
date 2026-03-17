#ifndef DEVICEINTERFACE_H
#define DEVICEINTERFACE_H
#include<QObject>
#include<QByteArray>

class DeviceInterface : public QObject
{
    Q_OBJECT
public:
    explicit DeviceInterface(QObject*parent =nullptr):QObject(parent){};
    virtual ~DeviceInterface()=default;
    virtual bool open()=0;
    virtual void close()=0;
    virtual bool isOpen() const =0;
signals:
    void opened();
    void closed();
    void errorQccurred(QString message);

    //同一向上抛完整的一帧
    void frameReceived(quint8 msgType,quint16 seq,QByteArray payload);


};

#endif // DEVICEINTERFACE_H
