// appsysform.cpp
#include "appsysform.h"

namespace RcParam {

const QList<ParaNode>* AppSysForm::constSlice(FormSlice s) const
{
    switch (s) {
    case FormSlice::SetValues:
        return &setValueList;
    case FormSlice::ControlParams:
        return &controlParamList;
    case FormSlice::SampleValues:
        return &sampleValueList;
    }
    return nullptr; // 若编译器对 enum 检查严格，可改为 default: return nullptr;
}

QList<ParaNode>* AppSysForm::mutableSlice(FormSlice s)
{
    switch (s) {
    case FormSlice::SetValues:
        return &setValueList;
    case FormSlice::ControlParams:
        return &controlParamList;
    case FormSlice::SampleValues:
        return &sampleValueList;
    }
    return nullptr;
}

} // namespace RcParam
