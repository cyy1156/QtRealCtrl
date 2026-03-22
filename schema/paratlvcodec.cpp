// paratlvcodec.cpp
#include "paratlvcodec.h"

#include <QDebug>
#include <QSet>
#include <QtEndian>
#include <cstring>

#include "protocol/tlvcodec.h"

namespace RcParam {

static bool paraNodeToTlvItem(const ParaNode& n, TlvItem& out, QString* err)
{
    out.type = n.tag;
    out.value.clear();

    switch (n.kind) {
    case ParaValueKind::Double: {
        float v = static_cast<float>(n.value.toDouble());
        out.value.resize(4);
        qToLittleEndian(*reinterpret_cast<const quint32*>(reinterpret_cast<const char*>(&v)),
                        reinterpret_cast<uchar*>(out.value.data()));
        /*qToLittleEndian(值, 目标地址)
Qt 自带函数：
→ 把一个 32 位整数转成小端字节序
→ 直接写到后面那个指针指向的内存。*/
        return true;
    }
    case ParaValueKind::Int: {
        qint32 v = n.value.toInt();
        out.value.resize(4);
        qToLittleEndian(static_cast<quint32>(v), reinterpret_cast<uchar*>(out.value.data()));
        return true;
    }
    case ParaValueKind::Bool: {
        out.value.append(n.value.toBool() ? char(1) : char(0));
        return true;
    }
    case ParaValueKind::String: {
        out.value = n.value.toString().toUtf8();
        return true;
    }
    }
    if (err)
        *err = QStringLiteral("未支持的 ParaValueKind");
    return false;
}

static bool tlvItemToParaNode(ParaNode& n, const TlvItem& it, QString* err)
{
    if (n.tag != it.type) {
        if (err)
            *err = QStringLiteral("内部错误：tag 不匹配");
        return false;
    }

    switch (n.kind) {
    case ParaValueKind::Double: {
        if (it.value.size() != 4) {
            if (err) *err = QStringLiteral("tag %1 期望 4 字节 float").arg(n.tag);
            return false;
        }
        quint32 raw = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(it.value.constData()));
        float f;
        memcpy(&f, &raw, sizeof(float));//把这 4 个字节原封不动拷贝到 float 变量里。
        n.value = static_cast<double>(f);
        return true;
    }
    case ParaValueKind::Int: {
        if (it.value.size() != 4) {
            if (err) *err = QStringLiteral("tag %1 期望 4 字节 int").arg(n.tag);
            return false;
        }
        quint32 raw = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(it.value.constData()));
        n.value = static_cast<int>(static_cast<qint32>(raw));
        return true;
    }
    case ParaValueKind::Bool: {
        if (it.value.size() != 1) {
            if (err) *err = QStringLiteral("tag %1 期望 1 字节 bool").arg(n.tag);
            return false;
        }
        n.value = (it.value.at(0) != 0);
        return true;
    }
    case ParaValueKind::String: {
        n.value = QString::fromUtf8(it.value);
        return true;
    }
    }
    if (err)
        *err = QStringLiteral("未支持的 ParaValueKind");
    return false;
}

QByteArray ParaTlvCodec::encodeList(const QList<ParaNode>& list, QString* errorOut)
{
    QVector<TlvItem> items;
    items.reserve(list.size());
    QSet<quint16> seen;

    for (const ParaNode& n : list) {
        if (!n.hasTlvTag()) {
            if (errorOut)
                *errorOut = QStringLiteral("参数 %1 未配置 TLV tag（type）").arg(n.key);
            return {};
        }
        if (seen.contains(n.tag)) {
            if (errorOut)
                *errorOut = QStringLiteral("重复 tag: 0x%1").arg(n.tag, 4, 16, QLatin1Char('0'));
            return {};
        }
        seen.insert(n.tag);

        TlvItem it;
        QString e;
        if (!paraNodeToTlvItem(n, it, &e)) {
            if (errorOut)
                *errorOut = e;
            return {};
        }
        items.append(it);
    }

    return Tlvcodec::encodeItems(items);
}

bool ParaTlvCodec::decodeIntoList(QList<ParaNode>& inOutList, const QByteArray& payload,
                                  QString* errorOut)
{
    if (payload.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("payload 为空");
        return false;
    }

    const QVector<TlvItem> items = Tlvcodec::decodeItems(payload);

    for (const TlvItem& it : items) {
        bool matched = false;
        for (ParaNode& n : inOutList) {
            if (n.tag != it.type)
                continue;
            QString e;
            if (!tlvItemToParaNode(n, it, &e)) {
                if (errorOut)
                    *errorOut = e;
                qWarning() << "decodeIntoList:" << e;
                return false;
            }
            matched = true;
            break;
        }
        if (!matched)
            qWarning() << "decodeIntoList: 未在参数表中找到 tag 0x"
                       << QString::number(it.type, 16);
    }

    return true;
}

} // namespace RcParam
