#ifndef NEURALPIDALGORITHM_H
#define NEURALPIDALGORITHM_H

#include "IAlgorithm.h"
#include <QVector>

class NeuralPidAlgorithm : public IAlgorithm
{
public:
    NeuralPidAlgorithm();

    QString name() const override { return "NEURALPID"; }
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
    double m_np = 0.01;
    double m_ni = 0.00001;
    double m_nd = 0.02;
    double m_w1 = 0.002;
    double m_w2 = 0.01;
    double m_w3 = 0.01;
    double m_minOutput = -1000.0;
    double m_maxOutput = 1000.0;
    double m_currentControl = 0.0;
    QVector<double> m_eArr;
};

#endif // NEURALPIDALGORITHM_H
