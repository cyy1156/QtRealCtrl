#ifndef DCSPROTOCOLTYPES_H
#define DCSPROTOCOLTYPES_H
#include<QString>
#include<QDataStream>
#include<QVariant>

#define SYSFORM        1
#define CONTROLMODE    2
#define CONTROLPARA    3
#define GIVENVALUE     4
#define SAMPLEVALUE    5
#define COMTROVALUE    6
#define COMTROLSTATUS  7
#define RUNNINGSTATUS  8
#define IMANGEDATA     9
#define CURRENENTVALUE 10
#define REQUISITION    11

enum class DataType : int{
    Ischar=0,IsShort=1,IsUnshort=2,IsInt=3,
    IsUnint=4,IsLong=5,IsUnlong=6,IsFloat=7,
    IsDouble=8,IsBool=9

};
enum class ArchiveMode :int {
  Name=0,Value=1,Both=2
};
struct ParaNode{
    QString name;
    DataType dataType =DataType::IsFloat;
    QVariant value;
    ArchiveMode archiveMode=ArchiveMode::Both;

    // 读写方法声明
    void readFromStream(QDataStream& in);
    void writeToStream(QDataStream& out) const;
};
QDataStream& operator>>(QDataStream& in,DataType& type);
QDataStream& operator<<(QDataStream& out, const DataType& type);
QDataStream& operator>>(QDataStream& in, ArchiveMode& mode);
QDataStream& operator<<(QDataStream& out, const ArchiveMode& mode);
class DcsProtocolTypes
{
public:
    DcsProtocolTypes();
};

#endif // DCSPROTOCOLTYPES_H
