#include "neuralpidalgorithm.h"
#include <algorithm>

static double nGet(const QVariantMap& m, const char* k, double defVal)
{
    const auto it = m.find(k);
    if (it == m.end()) return defVal;
    bool ok = false;
    const double v = it->toDouble(&ok);
    return ok ? v : defVal;
}

NeuralPidAlgorithm::NeuralPidAlgorithm()
    : m_eArr(3, 0.0)
{
}

double NeuralPidAlgorithm::compute(double target, double current, double dtSec)
{
    Q_UNUSED(dtSec);
    m_eArr[0] = m_eArr[1];
    m_eArr[1] = m_eArr[2];
    m_eArr[2] = target - current;

    const double x1 = m_eArr[2];
    const double x2 = m_eArr[2] - m_eArr[1];
    const double x3 = m_eArr[2] - 2.0 * m_eArr[1] + m_eArr[0];
    const double delta = m_w1 * x1 + m_w2 * x2 + m_w3 * x3;

    m_currentControl += delta;
    m_currentControl = std::clamp(m_currentControl, m_minOutput, m_maxOutput);

    m_w1 += m_ni * x1 * x1;
    m_w2 += m_np * x1 * x2;
    m_w3 += m_nd * x1 * x3;
    return m_currentControl;
}

bool NeuralPidAlgorithm::predictNextFeedback(double target,
                                             double current,
                                             double controlOutput,
                                             double dtSec,
                                             double& predictedNext)
{
    Q_UNUSED(dtSec);
    predictedNext = current + (target - current) * 0.08 + controlOutput * 0.02;
    return true;
}

void NeuralPidAlgorithm::setParameters(const QVariantMap& params)
{
    m_np = nGet(params, "NP", m_np);
    m_ni = nGet(params, "NI", m_ni);
    m_nd = nGet(params, "ND", m_nd);
    m_w1 = nGet(params, "W1", m_w1);
    m_w2 = nGet(params, "W2", m_w2);
    m_w3 = nGet(params, "W3", m_w3);
    m_minOutput = nGet(params, "MinOutput", m_minOutput);
    m_maxOutput = nGet(params, "MaxOutput", m_maxOutput);
}

QVariantMap NeuralPidAlgorithm::parameters() const
{
    return {
        {"NP", m_np},
        {"NI", m_ni},
        {"ND", m_nd},
        {"W1", m_w1},
        {"W2", m_w2},
        {"W3", m_w3},
        {"MinOutput", m_minOutput},
        {"MaxOutput", m_maxOutput}
    };
}
QList<AlgorithmParamDef> NeuralPidAlgorithm::parameterSchema() const
{
    return {
        {"NP", "学习率 NP", "基础参数", false, 0.0, 10.0, 0.0001, 6},
        {"NI", "学习率 NI", "基础参数", false, 0.0, 10.0, 0.0001, 6},
        {"ND", "学习率 ND", "基础参数", false, 0.0, 10.0, 0.0001, 6},
        {"W1", "权重 W1", "高级参数", false, -1000.0, 1000.0, 0.0001, 6},
        {"W2", "权重 W2", "高级参数", false, -1000.0, 1000.0, 0.0001, 6},
        {"W3", "权重 W3", "高级参数", false, -1000.0, 1000.0, 0.0001, 6},
        {"MinOutput", "最小输出", "高级参数", false, -10000.0, 10000.0, 1.0, 3},
        {"MaxOutput", "最大输出", "高级参数", false, -10000.0, 10000.0, 1.0, 3}
    };
}

void NeuralPidAlgorithm::reset()
{
    m_currentControl = 0.0;
    std::fill(m_eArr.begin(), m_eArr.end(), 0.0);
}
