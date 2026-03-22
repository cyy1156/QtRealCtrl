#pragma once

#include <QList>
#include <QString>
#include "paranode.h"
//命名空间
namespace RcParam {
/// 与 SDS 中「不同消息只带不同参数段」对应；需要更多段时在此枚举追加
enum class FormSlice{
    SetValues = 0 ,
    ControlParams =1,
    SampleValues = 2,

};
/** * @brief 上位机侧系统/参数表单（新协议）
  * @note 各 QList 内存放 ParaNode；
 *  下发哪一段由 FormSlice + SysFormTlvMapper 决定 */
struct AppSysForm{
    QString primeName;
    QString auxName;

    QList<ParaNode> setValueList; // 设定/目标相关
    QList<ParaNode> controlParamList; // 设定/目标相关
    QList<ParaNode> sampleValueList;  // 采样相关（可选）

    /// 只读访问某一段（用于 encode）
    const QList<ParaNode>* constSlice(FormSlice s) const;
    /// 可写访问某一段（用于 apply 解码写回）
    QList<ParaNode>* mutableSlice(FormSlice s);

};

}
