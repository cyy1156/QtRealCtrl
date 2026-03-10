#ifndef SYSFORM_H
#define SYSFORM_H

#include <QObject>
#include <QList>
#include <QDataStream>
#include "ctrlmode.h"
#include "dcsprotocoltypes.h"
class SysForm : public QObject
{
    Q_OBJECT
public:
    explicit SysForm(QObject *parent = nullptr):QObject(parent){};

    int sendMode=0;
    int receiveMode=0;
    QString primeName; //主名字
    QString auxName;  //辅助名字
    int ctrlWayCount =0;
    QList<CtrlMode>   ctrlModes;
    QList<ParaNode>   setValueList;
    QList<ParaNode>   sampleValueList;
    QList<ParaNode>   ctrlValueList;
    QList<ParaNode>   ctrlStatuList;
    QList<ParaNode>   runStatusList;
    QList<ParaNode>   grapList;

    void readFromStream(QDataStream& in);
    void writeToStream(QDataStream& out) const;
private:
    void readParaNodeList(QDataStream& in,QList<ParaNode>& list);
    void writeParaNodeList(QDataStream& out,const QList<ParaNode>& list )const;
    void readCtrlModeList(QDataStream& in,QList<CtrlMode>& list);
    void writeCtrlModeList(QDataStream& out,const QList<CtrlMode>& list )const;
signals:
};

#endif // SYSFORM_H
