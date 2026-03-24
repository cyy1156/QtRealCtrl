#ifndef PREDICTIVEALGORITHM_H
#define PREDICTIVEALGORITHM_H

#include "IAlgorithm.h"
#include <QVector>

class PredictiveAlgorithm : public IAlgorithm
{
public:
    PredictiveAlgorithm();

    QString name() const override { return "PREDICTIVE"; }
    double compute(double target, double current, double dtSec) override;
    void setParameters(const QVariantMap& params) override;
    QVariantMap parameters() const override;
    QList<AlgorithmParamDef> parameterSchema() const override;
    void reset() override;
    bool predictNextFeedback(double target,
                             double current,
                             double controlOutput,
                             double dtSec,
                             double& predictedNext) override;

private:
    void rebuildDArr();
    void ensurePredictWindow(double current);

private:
    double m_alpha = 0.004;
    double m_beta = 0.0;
    int m_controlStep = 16;
    int m_optimStep = 3;
    double m_optimCoef = 1.0;
    double m_controlCoef = 10.1;
    double m_minOutput = -1000.0;
    double m_maxOutput = 1000.0;

    double m_currentControl = 0.0;
    double m_lastDelta = 0.0;
    QVector<double> m_y0;
    QVector<double> m_y1;
    QVector<double> m_dArr;
};

#endif // PREDICTIVEALGORITHM_H
