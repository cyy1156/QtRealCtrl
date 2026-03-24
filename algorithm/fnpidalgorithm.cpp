#include "fnpidalgorithm.h"
#include <algorithm>
#include <cmath>

static double fGet(const QVariantMap& m, const char* k, double defVal)
{
    const auto it = m.find(k);
    if (it == m.end()) return defVal;
    bool ok = false;
    const double v = it->toDouble(&ok);
    return ok ? v : defVal;
}

FnPidAlgorithm::FnPidAlgorithm()
    : m_wKp(7, 0.0001)
    , m_wKi(7, 0.00001)
    , m_wKd(7, 0.0001)
    , m_eArr(3, 0.0)
{
}

void FnPidAlgorithm::setupMembership(int rank, QVector<double>& a) const
{
    a.fill(0.0, 7);
    const int idx = std::clamp(rank + 3, 0, 6);
    a[idx] = 1.0;
    if (idx - 1 >= 0) a[idx - 1] = 0.3;
    if (idx + 1 < 7) a[idx + 1] = 0.3;
}

double FnPidAlgorithm::compute(double target, double current, double dtSec)
{
    Q_UNUSED(dtSec);
    m_eArr[0] = m_eArr[1];
    m_eArr[1] = m_eArr[2];
    m_eArr[2] = target - current;

    const double x1 = m_eArr[2];
    const double x2 = m_eArr[2] - m_eArr[1];
    const double x3 = m_eArr[2] - 2.0 * m_eArr[1] + m_eArr[0];
    const double range = std::max(1e-6, std::abs(target));
    const double eClip = std::clamp(x1, -range, range);
    const int rank = static_cast<int>(std::round(6.0 / range * eClip));

    QVector<double> a;
    setupMembership(rank, a);

    double sum = 0.0;
    double kp = 0.0, ki = 0.0, kd = 0.0;
    for (int i = 0; i < 7; ++i)
    {
        kp += a[i] * m_wKp[i];
        ki += a[i] * m_wKi[i];
        kd += a[i] * m_wKd[i];
        sum += a[i];
    }
    if (sum <= 1e-6) sum = 1.0;
    kp /= sum; ki /= sum; kd /= sum;

    const double delta = kp * x2 + ki * x1 + kd * x3;
    m_currentControl += delta;
    m_currentControl = std::clamp(m_currentControl, m_minOutput, m_maxOutput);

    for (int i = 0; i < 7; ++i)
    {
        const double grad = a[i] / sum;
        m_wKp[i] = std::max(0.0, m_wKp[i] + m_learnKp * x1 * x2 * grad + m_q * 0.0);
        m_wKi[i] = std::max(0.0, m_wKi[i] + m_learnKi * x1 * x1 * grad + m_q * 0.0);
        m_wKd[i] = std::max(0.0, m_wKd[i] + m_learnKd * x1 * x3 * grad + m_q * 0.0);
    }
    return m_currentControl;
}

bool FnPidAlgorithm::predictNextFeedback(double target,
                                         double current,
                                         double controlOutput,
                                         double dtSec,
                                         double& predictedNext)
{
    Q_UNUSED(dtSec);
    predictedNext = current + (target - current) * 0.08 + controlOutput * 0.02;
    return true;
}

void FnPidAlgorithm::setParameters(const QVariantMap& params)
{
    m_learnKp = fGet(params, "LearnKp", m_learnKp);
    m_learnKi = fGet(params, "LearnKi", m_learnKi);
    m_learnKd = fGet(params, "LearnKd", m_learnKd);
    m_q = fGet(params, "Q", m_q);
    m_minOutput = fGet(params, "MinOutput", m_minOutput);
    m_maxOutput = fGet(params, "MaxOutput", m_maxOutput);
}

QVariantMap FnPidAlgorithm::parameters() const
{
    return {
        {"LearnKp", m_learnKp},
        {"LearnKi", m_learnKi},
        {"LearnKd", m_learnKd},
        {"Q", m_q},
        {"MinOutput", m_minOutput},
        {"MaxOutput", m_maxOutput}
    };
}
QList<AlgorithmParamDef> FnPidAlgorithm::parameterSchema() const
{
    return {
        {"LearnKp", "Kp 学习率", "基础参数", false, 0.0, 10.0, 0.0001, 6},
        {"LearnKi", "Ki 学习率", "基础参数", false, 0.0, 10.0, 0.0001, 6},
        {"LearnKd", "Kd 学习率", "基础参数", false, 0.0, 10.0, 0.0001, 6},
        {"Q", "动量 Q", "高级参数", false, 0.0, 1.0, 0.01, 3},
        {"MinOutput", "最小输出", "高级参数", false, -10000.0, 10000.0, 1.0, 3},
        {"MaxOutput", "最大输出", "高级参数", false, -10000.0, 10000.0, 1.0, 3}
    };
}

void FnPidAlgorithm::reset()
{
    m_currentControl = 0.0;
    std::fill(m_eArr.begin(), m_eArr.end(), 0.0);
}
