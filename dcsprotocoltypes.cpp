

#include "dcsprotocoltypes.h"
// 1. DataType 枚举的流式读写
QDataStream& operator>>(QDataStream& in,DataType& type)
{
    int val;
    in >>val;
    type=static_cast<DataType>(val);
    return in;
}
QDataStream& operator<<(QDataStream& out,const DataType& type)
{
    out  <<static_cast<int>(type);
    return out;
}
// 2. ArchiveMode 枚举的流式读写
QDataStream& operator>>(QDataStream& in,ArchiveMode& type)
{
    int val;
    in >>val;
    type=static_cast<ArchiveMode>(val);
    return in;
}

QDataStream& operator<<(QDataStream& out,const ArchiveMode& type)
{
    out  <<static_cast<int>(type);
    return out;
}
// 3. ParaNode 的读写方法
void ParaNode::readFromStream(QDataStream& in)
{
    in>>name>>dataType>>value>>archiveMode;
}
void ParaNode::writeToStream(QDataStream& out) const
{
    out<<name<<dataType<<value<<archiveMode;
}
DcsProtocolTypes::DcsProtocolTypes() {}
