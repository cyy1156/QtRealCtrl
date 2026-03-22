#include "paranode.h"

#include <QDebug>
#include <QSet>

namespace RcParam {

ParaNode ParaNode::makeDouble(const QString& key,double def,quint16 tlvTag, const QString& label)
{
    ParaNode n;
    n.key=key;
    n.tag=tlvTag;
    n.label=label.isEmpty()?key:label;
    n.kind=ParaValueKind::Double;
    n.value=def;
    n.defaultValue =def;
    return n;

}
bool ParaNode::setValueFromVariant(const QVariant& v,QString* err)
{
    switch(kind){
    case ParaValueKind::Double:{
        bool ok =false;
        const double d=v.toDouble(&ok);
        if(!ok)
        {
            if (err) *err = QStringLiteral("键 %1 需要 double，实际无法转换").arg(key);
            return false;

        }
        //isValid()判断是否合法
        if(minValue.isValid()&&d<minValue.toDouble())
        {
            if(err) *err=QStringLiteral("键 %1 小于最小值").arg(key);
            return false;
        }
        if (maxValue.isValid() && d > maxValue.toDouble()) {
            if (err) *err = QStringLiteral("键 %1 大于最大值").arg(key);
            return false;
        }
        value = d;
        return true;
}
    case ParaValueKind::Int: {
        bool ok = false;
        const int i = v.toInt(&ok);
        if (!ok) {
            if (err) *err = QStringLiteral("键 %1 需要 int").arg(key);
            return false;
        }
        value = i;
        return true;
    }
    case ParaValueKind::Bool:
        value = v.toBool();
        return true;
    case ParaValueKind::String:
        value = v.toString();
        return true;
    }
    if (err) *err = QStringLiteral("键 %1 未处理的 ParaValueKind").arg(key);
    return false;

}
QVariant ParaNode::toVariant() const
{
    return value;
}
QVariantMap paraListToVariantMap(const QList<ParaNode>& list,QString* err)
{
    QVariantMap m;
    QSet<QString> seen;
    for(const ParaNode& n: list){
        if(n.key.isEmpty()){
            if(err) *err =QStringLiteral("存在空的key的ParaNode");
            qWarning() <<"paraListToVariantMap: empty key";
            continue;
        }
        if(seen.contains(n.key)&&err)
            *err =QStringLiteral("重复 key: %1（后者覆盖前者）").arg(n.key);
        seen.insert(n.key);
        m.insert(n.key,n.toVariant());
    }
    return m;
}

int applyVariantMapToParaList(QList<ParaNode>& list, const QVariantMap& map, QString* err)
{
    int unmatched = 0;
    // 在 QVariantMap 上遍历，必须用 map.constBegin()/constEnd()，不能对 QVariant 调这两个
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        bool hit = false;
        for (ParaNode& n : list) {
            if (n.key == it.key()) {
                QString e;
                if (!n.setValueFromVariant(it.value(), &e)) {
                    if (err && !e.isEmpty())
                        *err = e;
                    qWarning() << "applyVariantMapToParaList:" << e;
                }
                hit = true;
                break;
            }
        }
        if (!hit)
            ++unmatched;
    }
    if (unmatched > 0 && err && err->isEmpty())
        *err = QStringLiteral("有 %1 个 map 键在参数表中未定义").arg(unmatched);
    return unmatched;
}

void resetParaListToDefaults(QList<ParaNode>& list)
{
    for (ParaNode& n : list) {
        if (n.defaultValue.isValid())
            n.value = n.defaultValue;
    }
}

static void writeOne(QDataStream& out, const ParaNode& n)
{
    out << n.key << n.tag << n.label << quint8(n.kind) << n.value << n.defaultValue
        << n.minValue << n.maxValue << n.readOnly;
}

static bool readOne(QDataStream& in, ParaNode& n, QString* err)
{
    quint8 k = 0;
    in >> n.key >> n.tag >> n.label >> k >> n.value >> n.defaultValue >> n.minValue >> n.maxValue
        >> n.readOnly;
    if (in.status() != QDataStream::Ok) {
        if (err)
            *err = QStringLiteral("ParaNode 流读取失败");
        return false;
    }
    if (k > quint8(ParaValueKind::String)) {
        if (err)
            *err = QStringLiteral("未知的 ParaValueKind");
        return false;
    }
    n.kind = static_cast<ParaValueKind>(k);
    return true;
}

void writeParaList(QDataStream& out, const QList<ParaNode>& list)
{
    out << kParaNodeStreamVersion << qint32(list.size());
    for (const ParaNode& n : list)
        writeOne(out, n);
}

bool readParaList(QDataStream& in, QList<ParaNode>& list, QString* err)
{
    qint32 ver = 0;
    in >> ver;
    if (in.status() != QDataStream::Ok) {
        if (err)
            *err = QStringLiteral("读取版本失败");
        return false;
    }
    if (ver != kParaNodeStreamVersion) {
        if (err)
            *err = QStringLiteral("不支持的 ParaNode 流版本 %1").arg(ver);
        return false;
    }
    qint32 count = 0;
    in >> count;
    if (count < 0 || count > 100000) {
        if (err)
            *err = QStringLiteral("非法列表长度");
        return false;
    }
    list.clear();
    list.reserve(int(count));
    for (qint32 i = 0; i < count; ++i) {
        ParaNode n;
        if (!readOne(in, n, err))
            return false;
        list.append(n);
    }
    return in.status() == QDataStream::Ok;
}

} // namespace RcParam
























