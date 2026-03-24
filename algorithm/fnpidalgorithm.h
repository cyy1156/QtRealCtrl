#ifndef FNPIDALGORITHM_H
#define FNPIDALGORITHM_H

#include "IAlgorithm.h"
#include <QVector>

class FnPidAlgorithm : public IAlgorithm
{
public:
    FnPidAlgorithm();

    QString name() const override { return "FNPID"; }
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
    void setupMembership(int rank, QVector<double>& a) const;

private:
    QVector<double> m_wKp;
    QVector<double> m_wKi;
    QVector<double> m_wKd;
    QVector<double> m_eArr;
    double m_learnKp = 0.001;
    double m_learnKi = 0.001;
    double m_learnKd = 0.001;
    double m_q = 0.3;
    double m_currentControl = 0.0;
    double m_minOutput = -1000.0;
    double m_maxOutput = 1000.0;
};

#endif // FNPIDALGORITHM_H
