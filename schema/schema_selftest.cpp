// schema_selftest.cpp — RcParam 与 Tlvcodec 联调自测
#include "schema_selftest.h"

#include "appsysform.h"
#include "paranode.h"
#include "paratlvcodec.h"
#include "sysformtlvmapper.h"
#include "protocol/tlvcodec.h"

#include <cmath>
#include <QDebug>
#include <QStringList>

namespace {

bool floatClose(double a, double b, double eps = 1e-4)
{
    return std::fabs(a - b) <= eps;
}

void appendLine(QStringList* lines, const QString& s)
{
    if (lines)
        lines->append(s);
    qDebug().noquote() << s;
}

} // namespace

bool runSchemaSelfTest(QString* report)
{
    QStringList lines;
    bool allOk = true;

    auto fail = [&](const QString& msg) {
        allOk = false;
        appendLine(&lines, QStringLiteral("[FAIL] %1").arg(msg));
    };
    auto pass = [&](const QString& msg) {
        appendLine(&lines, QStringLiteral("[OK]   %1").arg(msg));
    };

    // ---------- 1) ParaTlvCodec：PID 三参数 encode → decode 往返 ----------
    {
        QList<RcParam::ParaNode> list;
        list.append(RcParam::ParaNode::makeDouble(QStringLiteral("Kp"), 1.25, TlvType::Kp));
        list.append(RcParam::ParaNode::makeDouble(QStringLiteral("Ki"), 0.2, TlvType::Ki));
        list.append(RcParam::ParaNode::makeDouble(QStringLiteral("Kd"), 0.03, TlvType::Kd));

        QString err;
        QByteArray payload = RcParam::ParaTlvCodec::encodeList(list, &err);
        if (payload.isEmpty()) {
            fail(QStringLiteral("encodeList 失败: %1").arg(err));
        } else {
            QList<RcParam::ParaNode> back = list;
            // 故意改值，检查 decode 是否写回
            back[0].value = 0.0;
            back[1].value = 0.0;
            back[2].value = 0.0;
            QString err2;
            if (!RcParam::ParaTlvCodec::decodeIntoList(back, payload, &err2)) {
                fail(QStringLiteral("decodeIntoList 失败: %1").arg(err2));
            } else if (!floatClose(back[0].value.toDouble(), 1.25)
                       || !floatClose(back[1].value.toDouble(), 0.2)
                       || !floatClose(back[2].value.toDouble(), 0.03)) {
                fail(QStringLiteral("往返后数值不一致 Kp/Ki/Kd"));
            } else {
                pass(QStringLiteral("ParaTlvCodec PID 三参数往返"));
            }
        }
    }

    // ---------- 2) SysFormTlvMapper：整表切片 encode → apply ----------
    {
        RcParam::AppSysForm form;
        form.controlParamList.append(
            RcParam::ParaNode::makeDouble(QStringLiteral("Kp"), 2.0, TlvType::Kp));
        form.controlParamList.append(
            RcParam::ParaNode::makeDouble(QStringLiteral("MaxOutput"), 100.0, TlvType::MaxOutput));

        QString e1;
        QByteArray p = RcParam::SysFormTlvMapper::encodeFromSysForm(
            form, RcParam::FormSlice::ControlParams, &e1);
        if (p.isEmpty()) {
            fail(QStringLiteral("encodeFromSysForm: %1").arg(e1));
        } else {
            form.controlParamList[0].value = 0.0;
            form.controlParamList[1].value = 0.0;
            QString e2;
            if (!RcParam::SysFormTlvMapper::applyToSysForm(
                    form, RcParam::FormSlice::ControlParams, p, &e2)) {
                fail(QStringLiteral("applyToSysForm: %1").arg(e2));
            } else if (!floatClose(form.controlParamList[0].value.toDouble(), 2.0)
                       || !floatClose(form.controlParamList[1].value.toDouble(), 100.0)) {
                fail(QStringLiteral("Mapper 往返后数值不对"));
            } else {
                pass(QStringLiteral("SysFormTlvMapper ControlParams 往返"));
            }
        }
    }

    // ---------- 3) QVariantMap ↔ 列表 ----------
    {
        QList<RcParam::ParaNode> list;
        list.append(RcParam::ParaNode::makeDouble(QStringLiteral("Kp"), 3.0, TlvType::Kp));
        list.append(RcParam::ParaNode::makeDouble(QStringLiteral("Ki"), 0.5, TlvType::Ki));

        QString e;
        QVariantMap m = RcParam::paraListToVariantMap(list, &e);
        if (!e.isEmpty())
            appendLine(&lines, QStringLiteral("[WARN] paraListToVariantMap: %1").arg(e));

        m[QStringLiteral("Kp")] = 9.0;
        m[QStringLiteral("Ki")] = 1.0;
        QString e2;
        int unmatched = RcParam::applyVariantMapToParaList(list, m, &e2);
        if (unmatched != 0)
            fail(QStringLiteral("applyVariantMapToParaList unmatched=%1 %2").arg(unmatched).arg(e2));
        else if (!floatClose(list[0].value.toDouble(), 9.0) || !floatClose(list[1].value.toDouble(), 1.0)) {
            fail(QStringLiteral("QVariantMap 写回后数值不对"));
        } else {
            pass(QStringLiteral("paraListToVariantMap / applyVariantMapToParaList"));
        }
    }

    // ---------- 4) tag==0 应编码失败 ----------
    {
        QList<RcParam::ParaNode> bad;
        bad.append(RcParam::ParaNode::makeDouble(QStringLiteral("NoTag"), 1.0, 0));
        QString err;
        QByteArray p = RcParam::ParaTlvCodec::encodeList(bad, &err);
        if (!p.isEmpty())
            fail(QStringLiteral("tag==0 时 encodeList 应返回空"));
        else
            pass(QStringLiteral("tag==0 编码拒绝（预期失败）"));
    }

    // ---------- 5) SetValues：目标位置 float ----------
    {
        RcParam::AppSysForm form;
        form.setValueList.append(
            RcParam::ParaNode::makeDouble(QStringLiteral("TargetPosition"), 10.5, TlvType::TargetPosition));

        QString e1;
        QByteArray p = RcParam::SysFormTlvMapper::encodeFromSysForm(
            form, RcParam::FormSlice::SetValues, &e1);
        if (p.isEmpty()) {
            fail(QStringLiteral("SetValues encode: %1").arg(e1));
        } else {
            form.setValueList[0].value = 0.0;
            QString e2;
            if (!RcParam::SysFormTlvMapper::applyToSysForm(
                    form, RcParam::FormSlice::SetValues, p, &e2)) {
                fail(QStringLiteral("SetValues apply: %1").arg(e2));
            } else if (!floatClose(form.setValueList[0].value.toDouble(), 10.5)) {
                fail(QStringLiteral("TargetPosition 往返失败"));
            } else {
                pass(QStringLiteral("SetValues TargetPosition 往返"));
            }
        }
    }

    // ---------- 6) 与 Tlvcodec 直接结果对齐（同一条 float TLV）----------
    {
        QVector<TlvItem> items;
        Tlvcodec::appendFloat(items, TlvType::Kp, 7.5f);
        QByteArray direct = Tlvcodec::encodeItems(items);

        QList<RcParam::ParaNode> list;
        list.append(RcParam::ParaNode::makeDouble(QStringLiteral("Kp"), 7.5, TlvType::Kp));
        QString err;
        QByteArray via = RcParam::ParaTlvCodec::encodeList(list, &err);

        if (direct != via) {
            fail(QStringLiteral("ParaTlvCodec 与 appendFloat 编码不一致 hex: %1 vs %2")
                     .arg(QString(direct.toHex()), QString(via.toHex())));
        } else {
            pass(QStringLiteral("ParaTlvCodec 与 Tlvcodec::appendFloat 字节一致"));
        }
    }

    if (report)
        *report = lines.join(QLatin1Char('\n'));

    if (allOk)
        qDebug() << "========== Schema 自测：全部通过 ==========";
    else
        qCritical() << "========== Schema 自测：存在失败项，见上文 [FAIL] ==========";

    return allOk;
}
