#include "sysform.h"

#include <QtGlobal>

void SysForm::readParaNodeList(QDataStream& in, QList<ParaNode>& list)
{
    int count = 0;
    in >> count;
    list.clear();
    list.reserve(qMax(0, count));

    for (int i = 0; i < count; ++i) {
        ParaNode node;
        node.readFromStream(in);
        list.append(node);
    }
}

void SysForm::writeParaNodeList(QDataStream& out, const QList<ParaNode>& list) const
{
    out << list.size();
    for (const auto& node : list) {
        node.writeToStream(out);
    }
}

void SysForm::readCtrlModeList(QDataStream& in, QList<CtrlMode>& list)
{
    int count = 0;
    in >> count;
    list.clear();
    list.reserve(qMax(0, count));

    for (int i = 0; i < count; ++i) {
        CtrlMode node;
        node.readFromStream(in);
        list.append(node);
    }
}

void SysForm::writeCtrlModeList(QDataStream& out, const QList<CtrlMode>& list) const
{
    out << list.size();
    for (const auto& node : list) {
        node.writeToStream(out);
    }
}

void SysForm::readFromStream(QDataStream& in)
{
    // 当前阶段验证用的“简化协议”（与 main.cpp 测试保持一致）：
    // primeName, auxName, count, (name, dataType, archiveMode, valueFloat) * count

    in >> primeName >> auxName;

    qint32 count = 0;
    in >> count;

    setValueList.clear();
    setValueList.reserve(qMax<qint32>(0, count));

    for (qint32 i = 0; i < count; ++i) {
        QString name;
        qint32 dtInt = 0;
        qint32 amInt = 0;
        float val = 0.0f;

        in >> name >> dtInt >> amInt >> val;

        ParaNode node;
        node.name = name;
        node.dataType = static_cast<DataType>(dtInt);
        node.archiveMode = static_cast<ArchiveMode>(amInt);
        node.value = val;

        setValueList.append(node);
    }
}

void SysForm::writeToStream(QDataStream& out) const
{
    // 当前阶段验证用的“简化协议”（与 main.cpp 测试保持一致）：
    // primeName, auxName, count, (name, dataType, archiveMode, valueFloat) * count

    out << primeName << auxName;

    qint32 count = static_cast<qint32>(setValueList.size());
    out << count;

    for (const auto& node : setValueList) {
        out << node.name
            << static_cast<qint32>(node.dataType)
            << static_cast<qint32>(node.archiveMode)
            << node.value.toFloat();
    }
}
