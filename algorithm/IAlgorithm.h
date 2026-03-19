#ifndef IALGORITHM_H
#define IALGORITHM_H
#include <QString>
#include <QVariantMap>
//约定：所有算法都用这个串口
class IAlgorithm{
public:
    //特殊虚函数
    virtual ~IAlgorithm() =default;//这样写可以让编译器明白不需要手动写纯虚函数

    //算法名字：用于ui显示/日志
    //有等于0的是纯虚函数，子类必须强制重写，没有等于0是普通虚函数可以选择是否需要重写
    virtual QString name() const=0;

    //每个控制周期调用一次
    // 输入target/current,输出control（控制量）
    //计算
    virtual double compute(double target,double current,double dtSec) =0;

    //下发修改参数

    virtual void setParameters(const QVariantMap& params) =0;
    //导出当前参数用于ui回显
    virtual QVariantMap parameters() const=0;
    //清楚状态
    virtual void reset()=0;


};

#endif // IALGORITHM_H
