// sysformtlvmapper.h — SDS: encodeFromSysForm / applyToSysForm
#pragma once

#include <QByteArray>
#include <QString>

#include "appsysform.h"

namespace RcParam {

class SysFormTlvMapper {
public:
    /// 从 AppSysForm 的某一段编码出 payload（无帧头）
    static QByteArray encodeFromSysForm(const AppSysForm& form, FormSlice slice,
                                        QString* errorOut = nullptr);

    /// 将 payload 解码并写回 form 对应段（按 ParaNode::tag 匹配）
    static bool applyToSysForm(AppSysForm& form, FormSlice slice, const QByteArray& payload,
                               QString* errorOut = nullptr);
};

} // namespace RcParam
