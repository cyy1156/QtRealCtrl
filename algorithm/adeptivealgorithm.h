#ifndef ADEPTIVEALGORITHM_H
#define ADEPTIVEALGORITHM_H

#include "IAlgorithm.h"
#include <QVector>

class AdeptiveAlgorithm : public IAlgorithm
{
public:
    AdeptiveAlgorithm();

    QString name() const override { return "ADEPTIVE"; }
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
    void calcD();

private:
    double m_p = 0.03;
    double m_i = 0.01;
    double m_d = 0.0;
    double m_minOutput = -1000.0;
    double m_maxOutput = 1000.0;
    double m_currentControl = 0.0;
    QVector<double> m_eArr;
    QVector<double> m_dGain;
};

#endif // ADEPTIVEALGORITHM_H
