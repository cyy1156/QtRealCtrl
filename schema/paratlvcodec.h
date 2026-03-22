// paratlvcodec.h — ParaNode 列表 ↔ payload，内部与 Tlvcodec 字节布局一致
#pragma once

#include <QByteArray>
#include <QString>
#include <QList>

#include "paranode.h"

namespace RcParam {

class ParaTlvCodec {
public:
    /**
     * @brief 将参数列表编成连续 TLV（Type=ParaNode::tag，小端，与 Tlvcodec 一致）
     * @param list 待编码节点；tag==0 时默认失败（也可改为跳过，由你产品决定）
     * @param errorOut 失败原因
     */
    static QByteArray encodeList(const QList<ParaNode>& list, QString* errorOut = nullptr);

    /**
     * @brief 解析 payload，按 tag 与 list 中节点匹配，只更新 value（不增删节点）
     * @param inOutList 必须与设备约定 tag 集合一致；未知 tag 可记录 warning 并跳过
     */
    static bool decodeIntoList(QList<ParaNode>& inOutList, const QByteArray& payload,
                               QString* errorOut = nullptr);
};

} // namespace RcParam
