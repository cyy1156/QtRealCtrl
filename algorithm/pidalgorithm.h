#ifndef PIDALGORITHM_H
#define PIDALGORITHM_H

#include "IAlgorithm.h"

class PIDAlgorithm : public IAlgorithm
{
public:
    PIDAlgorithm();
    QString name() const override{return "PID";}
    double compute(double target,double current,double dtSec) override;//dtSec两次调用 compute 的时间间隔

    void setParameters(const QVariantMap& params) override;
    QVariantMap parameters() const override;

    void reset() override;
private:
    double m_kp=0;
    double m_ki=0;
    double m_kd=0;
    // 中文注释：输出限幅（对齐 SDS 的 MaxOutput，<=0 表示不做限幅）
    double m_maxOutput=0.0;

    double m_integral=0.0;
    double m_prevError=0.0;//保存当前误差，作为下一次计算微分的 “上一次误差”
    bool m_hasPrev=false;//标记是否有 “上一次的误差值”：

};

#endif // PIDALGORITHM_H
