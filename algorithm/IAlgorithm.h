#ifndef IALGORITHM_H
#define IALGORITHM_H
#include <QString>
#include <QVariantMap>
#include <QList>

struct AlgorithmParamDef
{
    QString key;
    QString label;
    QString group = QStringLiteral("基础参数");
    bool isInteger = false;
    double minValue = -1e9;
    double maxValue = 1e9;
    double step = 0.1;
    int decimals = 3;
};
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
    // 中文注释：参数定义（用于“设置参数”动态生成控件）
    virtual QList<AlgorithmParamDef> parameterSchema() const
    {
        return {};
    }
    //清楚状态
    virtual void reset()=0;

    // 中文注释：纯软件仿真可选回调——根据算法/模型预测下一拍反馈值
    // 返回 true 表示使用 predictedNext；返回 false 表示调用方使用默认模型
    virtual bool predictNextFeedback(double target,
                                     double current,
                                     double controlOutput,
                                     double dtSec,
                                     double& predictedNext)
    {
        Q_UNUSED(target);
        Q_UNUSED(current);
        Q_UNUSED(controlOutput);
        Q_UNUSED(dtSec);
        Q_UNUSED(predictedNext);
        return false;
    }

};

#endif // IALGORITHM_H
