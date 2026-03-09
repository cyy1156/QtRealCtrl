/**
 * @file DcsProtocolTypes.h
 * @brief DCS 协议数据类型与消息模式常量
 * @note 严格参考 SysForm_Qt实现指南.md 与 realctrl - 副本 (2)/DcsPrtcl/SysForm.h
 */

#ifndef DCS_PROTOCOL_TYPES_H
#define DCS_PROTOCOL_TYPES_H

#include <QString>
#include <QVariant>
#include <QDataStream>

//=============================================================================
// 消息模式（与 MFC SysForm.h 对应）
//=============================================================================
#define SYSFORM          1
#define CONTROLMODE      2
#define CONTROLPARA      3
#define GIVENVALUE       4
#define SAMPLEVALUE      5
#define CONTROLVALUE      6
#define CONTROLSTATUS    7
#define RUNNINGSTATUS    8
#define IMANGEDATA        9
#define CURRENTVALUE     10
#define REQUISITION      11

//=============================================================================
// 数据类型（对应 PARATYPE / isFloat 等）
//=============================================================================
enum class DataType : int {
    IsChar    = 0,
    IsShort   = 1,
    IsUnshort = 2,
    IsInt     = 3,
    IsUnint   = 4,
    IsLong    = 5,
    IsUnlong  = 6,
    IsFloat   = 7,
    IsDouble  = 8,
    IsBool    = 9
};

//=============================================================================
// 归档模式（CParaList::m_ArchiveMode）
//=============================================================================
enum class ArchiveMode : int {
    Name  = 0,  // 仅序列化名称
    Value = 1,  // 仅序列化值
    Both  = 2   // 名称 + 类型 + 值
};

//=============================================================================
// ParaNode：参数节点（对应 CParaList 单节点）
// 参考指南 4.2 节
//=============================================================================
struct ParaNode {
    QString     name;
    DataType    dataType = DataType::IsFloat;
    QVariant    value;
    ArchiveMode archiveMode = ArchiveMode::Both;

    ParaNode() = default;
    ParaNode(const QString& n, DataType dt, const QVariant& v, ArchiveMode am = ArchiveMode::Both)
        : name(n), dataType(dt), value(v), archiveMode(am) {}

    void readFromStream(QDataStream& in);
    void writeToStream(QDataStream& out) const;

    static void setDataFromVariant(ParaNode& node, const QVariant& v);
    static QVariant getDataAsVariant(const ParaNode& node);
};

// QDataStream 运算符重载（便于直接用 >> / <<）
QDataStream& operator>>(QDataStream& in, ParaNode& node);
QDataStream& operator<<(QDataStream& out, const ParaNode& node);

#endif // DCS_PROTOCOL_TYPES_H
