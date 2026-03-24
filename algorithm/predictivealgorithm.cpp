#include "predictivealgorithm.h"
#include <algorithm>

static double pGet(const QVariantMap& m, const char* k, double defVal)
{
    const auto it = m.find(k);
    if (it == m.end()) return defVal;
    bool ok = false;
    const double v = it->toDouble(&ok);
    return ok ? v : defVal;
}

PredictiveAlgorithm::PredictiveAlgorithm()
{
    rebuildDArr();
}

void PredictiveAlgorithm::rebuildDArr()
{
    m_controlStep = std::max(1, m_controlStep);
    m_dArr.resize(m_controlStep);
    const double base = 1.0 / static_cast<double>(m_controlStep);
    for (int i = 0; i < m_controlStep; ++i)
    {
        m_dArr[i] = base * (1.0 - 0.5 * static_cast<double>(i) / static_cast<double>(m_controlStep));
    }
}

void PredictiveAlgorithm::ensurePredictWindow(double current)
{
    const int n = std::max(m_controlStep + 2, 64);
    if (m_y0.size() != n) m_y0 = QVector<double>(n, current);
    if (m_y1.size() != n) m_y1 = QVector<double>(n, current);
}

double PredictiveAlgorithm::compute(double target, double current, double dtSec)
{
    Q_UNUSED(dtSec);
    ensurePredictWindow(current);

    const double predictError = current - m_y1[0];
    for (int i = 1; i < m_y0.size(); ++i)
    {
        const double h = 1.0 + m_alpha * (1.0 - static_cast<double>(i) / static_cast<double>(m_y0.size()));
        m_y0[i] = m_y1[i] + h * predictError;
    }

    double delta = 0.0;
    for (int i = 0; i < m_controlStep && (i + 1) < m_y0.size(); ++i)
    {
        delta += m_dArr[i] * (target - m_y0[i + 1]);
    }
    m_lastDelta = delta;
    m_currentControl += delta;
    m_currentControl = std::clamp(m_currentControl, m_minOutput, m_maxOutput);
    return m_currentControl;
}

bool PredictiveAlgorithm::predictNextFeedback(double target,
                                              double current,
                                              double controlOutput,
                                              double dtSec,
                                              double& predictedNext)
{
    Q_UNUSED(dtSec);
    // 中文注释：第二轮对齐——对象模型严格对齐 FakeDevice，便于纯软件仿真与串口仿真一致
    predictedNext = current
                    + (target - current) * 0.08
                    + controlOutput * 0.02;
    return true;
}

void PredictiveAlgorithm::setParameters(const QVariantMap& params)
{
    m_alpha = pGet(params, "alpha", m_alpha);
    m_beta = pGet(params, "beta", m_beta);
    m_controlStep = static_cast<int>(pGet(params, "ControlStep", m_controlStep));
    m_optimStep = static_cast<int>(pGet(params, "OptimStep", m_optimStep));
    m_optimCoef = pGet(params, "OptimCoef", m_optimCoef);
    m_controlCoef = pGet(params, "ControlCoef", m_controlCoef);
    m_minOutput = pGet(params, "MinOutput", m_minOutput);
    m_maxOutput = pGet(params, "MaxOutput", m_maxOutput);
    Q_UNUSED(m_beta);
    Q_UNUSED(m_optimStep);
    Q_UNUSED(m_optimCoef);
    Q_UNUSED(m_controlCoef);
    rebuildDArr();
}

QVariantMap PredictiveAlgorithm::parameters() const
{
    return {
        {"alpha", m_alpha},
        {"beta", m_beta},
        {"ControlStep", m_controlStep},
        {"OptimStep", m_optimStep},
        {"OptimCoef", m_optimCoef},
        {"ControlCoef", m_controlCoef},
        {"MinOutput", m_minOutput},
        {"MaxOutput", m_maxOutput}
    };
}
QList<AlgorithmParamDef> PredictiveAlgorithm::parameterSchema() const
{
    return {
        {"alpha", "跟踪系数 alpha", "基础参数", false, 0.0, 10.0, 0.001, 4},
        {"beta", "抗扰系数 beta", "基础参数", false, 0.0, 10.0, 0.001, 4},
        {"ControlStep", "控制步长", "基础参数", true, 1.0, 255.0, 1.0, 0},
        {"OptimStep", "优化步长", "基础参数", true, 1.0, 64.0, 1.0, 0},
        {"OptimCoef", "优化权", "高级参数", false, 0.0, 1000.0, 0.1, 3},
        {"ControlCoef", "控制权", "高级参数", false, 0.0, 1000.0, 0.1, 3},
        {"MinOutput", "最小输出", "高级参数", false, -10000.0, 10000.0, 1.0, 3},
        {"MaxOutput", "最大输出", "高级参数", false, -10000.0, 10000.0, 1.0, 3}
    };
}

void PredictiveAlgorithm::reset()
{
    m_currentControl = 0.0;
    m_lastDelta = 0.0;
    m_y0.clear();
    m_y1.clear();
}
