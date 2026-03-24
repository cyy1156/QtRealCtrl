#include "pidalgorithm.h"
#include <algorithm>

static double getDouble(const QVariantMap& m,const char* key,double def)
{
    auto it=m.find(key);//auto 自动推导迭代器类型
    if(it==m.end()) return def;//def是默认值
    bool ok =false;
    double v=it->toDouble(&ok);
    return ok? v: def;
}
double PIDAlgorithm::compute(double target ,double current,double dtSec)
{
    //如果传入的时间间隔≤0（无效值），强制设为 0.001 秒（1 毫秒）
    if(dtSec<=0.0) dtSec=1e-3;
    const double err=target-current;
    //积分·
    m_integral+=err*dtSec;
    //微分·
    double deriv=0.0;
    if(m_hasPrev){
        deriv=(err-m_prevError)/dtSec;
    }
    m_prevError=err;
    m_hasPrev=true;

    const double u=m_kp*err+m_ki*m_integral+m_kd*deriv;
    // 中文注释：输出限幅，避免控制量过大导致振荡/发散
    if(m_maxOutput > 0.0)
    {
        return std::clamp(u, -m_maxOutput, m_maxOutput);
    }
    return u;
}
void PIDAlgorithm::setParameters(const QVariantMap& params)
{
    m_kp=getDouble(params,"Kp",m_kp);
    m_ki=getDouble(params,"Ki",m_ki);
    m_kd=getDouble(params,"Kd",m_kd);
    // 中文注释：MaxOutput（SDS 输出限幅），<=0 表示不启用限幅
    m_maxOutput=getDouble(params,"MaxOutput",m_maxOutput);

}
QVariantMap PIDAlgorithm::parameters()const
{
    return{
        {"Kp",m_kp},
        {"Ki",m_ki},
        {"Kd",m_kd},
        {"MaxOutput",m_maxOutput},

        };

}
void PIDAlgorithm::reset()
{
    m_integral=0.0;
    m_prevError=0.0;
    m_hasPrev=false;
}
PIDAlgorithm::PIDAlgorithm()
{
   // setParameters({{"Kp",1.0},{"Ki",0.2},{"Kd",0.0}});
}













