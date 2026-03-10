#ifndef CTRLMODE_H
#define CTRLMODE_H

#include<QList>
#include<QDataStream>
#include "dcsprotocoltypes.h"
struct CtrlMode{
    QString          ctrlName;
    QList<ParaNode>  ctrlPara;

    void readFromStream(QDataStream& in);
    void writeToStream(QDataStream& out) const;
};



#endif // CTRLMODE_H
