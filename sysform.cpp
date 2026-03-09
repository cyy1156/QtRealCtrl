#include "sysform.h"

void SysForm::readParaNodeList(QDataStream& in ,QList<ParaNode>& list)
{
    int count;
    in>>count;
    list.clear();
    for(int i=0;i<count;i++)
    {
        ParaNode node;
        node.readFromStream(in);
        list.append(node);
    }
}
void SysForm::writeParaNodeList(QDataStream& out,const QList<ParaNode>& list) const
{
    out<<list.size();
    for(const auto& node :list)
    {
        node.writeToStream(out);
    }

}
void SysForm::readCtrlModeList(QDataStream& in ,QList<CtrlMode>& list)
{
    int count;
    in>>count;
    list.clear();
    for(int i=0;i<count;i++)
    {
        CtrlMode node;
        node.readFromStream(in);
        list.append(node);
    }
}
void SysForm::writeCtrlModeList(QDataStream& out,const QList<CtrlMode>& list) const
{
    out<<list.size();
    for(const auto& node :list)
    {
        node.writeToStream(out);
    }

}
void SysForm::readFromStream(QDataStream& in)
{
    in>>sendMode>>receiveMode>>requision
}
