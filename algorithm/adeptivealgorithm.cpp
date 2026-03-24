#include "adeptivealgorithm.h"
#include <algorithm>

static double aGet(const QVariantMap& m, const char* k, double defVal)
{
    const auto it = m.find(k);
    if (it == m.end()) return defVal;
    bool ok = false;
    const double v = it->toDouble(&ok);
    return ok ? v : defVal;
}

AdeptiveAlgorithm::AdeptiveAlgorithm()
    : m_eArr(3, 0.0)
    , m_dGain(3, 0.0)
{
    calcD();
}

void AdeptiveAlgorithm::calcD()
{
    m_dGain[0] = m_p + m_i + m_d;
    m_dGain[1] = -(m_p + 2.0 * m_d);
    m_dGain[2] = m_d;
}

double AdeptiveAlgorithm::compute(double target, double current, double dtSec)
{
    Q_UNUSED(dtSec);
    m_eArr[0] = m_eArr[1];
    m_eArr[1] = m_eArr[2];
    m_eArr[2] = target - current;

    double delta = 0.0;
    if (current < 0.65 * target)
    {
        delta += m_i * m_eArr[2];
    }
    else if (current < 0.8 * target)
    {
        delta += (m_i + m_p) * m_eArr[2] - m_p * m_eArr[1];
    }
    else
    {
        delta += m_dGain[0] * m_eArr[2] + m_dGain[1] * m_eArr[1] + m_dGain[2] * m_eArr[0];
    }

    m_currentControl += delta;
    m_currentControl = std::clamp(m_currentControl, m_minOutput, m_maxOutput);
    return m_currentControl;
}

bool AdeptiveAlgorithm::predictNextFeedback(double target,
                                            double current,
                                            double controlOutput,
                                            double dtSec,
                                            double& predictedNext)
{
    Q_UNUSED(dtSec);
    // 中文注释：自适应算法对象模型采用“目标跟踪+控制量注入”，与串口仿真一致
    predictedNext = current + (target - current) * 0.08 + controlOutput * 0.02;
    return true;
}

void AdeptiveAlgorithm::setParameters(const QVariantMap& params)
{
    m_p = aGet(params, "P", m_p);
    m_i = aGet(params, "I", m_i);
    m_d = aGet(params, "D", m_d);
    m_minOutput = aGet(params, "MinOutput", m_minOutput);
    m_maxOutput = aGet(params, "MaxOutput", m_maxOutput);
    calcD();
}

QVariantMap AdeptiveAlgorithm::parameters() const
{
    return {
        {"P", m_p},
        {"I", m_i},
        {"D", m_d},
        {"MinOutput", m_minOutput},
        {"MaxOutput", m_maxOutput}
    };
}
QList<AlgorithmParamDef> AdeptiveAlgorithm::parameterSchema() const
{
    return {
        {"P", "比例 P", "基础参数", false, -1000.0, 1000.0, 0.001, 4},
        {"I", "积分 I", "基础参数", false, -1000.0, 1000.0, 0.001, 4},
        {"D", "微分 D", "基础参数", false, -1000.0, 1000.0, 0.001, 4},
        {"MinOutput", "最小输出", "高级参数", false, -10000.0, 10000.0, 1.0, 3},
        {"MaxOutput", "最大输出", "高级参数", false, -10000.0, 10000.0, 1.0, 3}
    };
}

void AdeptiveAlgorithm::reset()
{
    m_currentControl = 0.0;
    std::fill(m_eArr.begin(), m_eArr.end(), 0.0);
}
