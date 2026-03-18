#ifndef SYSINFO_H
#define SYSINFO_H

#include <QObject>

class SysInfo : public QObject
{
    Q_OBJECT
public:
    explicit SysInfo(QObject *parent = nullptr): QObject(parent) {};
    double currentValue() const {return m_currentValue;}
    double targetValue() const {return m_targetValue;}
    quint8 mode()const {return m_mode;}

    void setCurrentValue(double v){
        if(m_currentValue == v)return;
        m_currentValue=v;
        emit changed();

    }
    void setTargetValue(double v)
    {
        if(m_targetValue==v) return;
        m_targetValue=v;
        emit changed();
    }
    void setMode(quint8 v )
    {
        if(m_mode==v) return;
        m_mode=v;
        emit changed();
    }
signals:
    void changed();

private:
    double m_currentValue =0.0;
    double m_targetValue  =0.0;
    quint8 m_mode = 0;
};

#endif // SYSINFO_H
