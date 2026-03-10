#include "ctrlmode.h"

void CtrlMode::readFromStream(QDataStream& in)
{
    in>>ctrlName;
    int count;
    in>>count;
    //先读长度，在读ParaNode
    ctrlPara.clear();
    for(int i=0;i<count;i++)
    {
        ParaNode node;
        node.readFromStream(in);
        ctrlPara.append(node);
    }
}
void CtrlMode::writeToStream(QDataStream& out) const
{
    out<<ctrlName;
    //先写名字在写长度，这个是我们规定的
    out<<ctrlPara.length();
    for(const auto& node:ctrlPara)
    {
        node.writeToStream(out);
    }
}
