// sysformtlvmapper.cpp
#include "sysformtlvmapper.h"

#include "paratlvcodec.h"

namespace RcParam {

QByteArray SysFormTlvMapper::encodeFromSysForm(const AppSysForm& form, FormSlice slice,
                                               QString* errorOut)
{
    const QList<ParaNode>* list = form.constSlice(slice);
    if (!list) {
        if (errorOut)
            *errorOut = QStringLiteral("无效的 FormSlice");
        return {};
    }
    return ParaTlvCodec::encodeList(*list, errorOut);
}

bool SysFormTlvMapper::applyToSysForm(AppSysForm& form, FormSlice slice,
                                      const QByteArray& payload, QString* errorOut)
{
    QList<ParaNode>* list = form.mutableSlice(slice);
    if (!list) {
        if (errorOut)
            *errorOut = QStringLiteral("无效的 FormSlice");
        return false;
    }
    return ParaTlvCodec::decodeIntoList(*list, payload, errorOut);
}

} // namespace RcParam
