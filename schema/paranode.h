#ifndef PARANODE_H
#define PARANODE_H
#include <QMap>
#include <QString>
#include <QVariant>
#include <QList>
#include <QHash>
#include <QDataStream>
/*namespace RcParam 就是 C++ 里的命名空间，作用很简单：
把一堆变量、函数、结构体等包在 RcParam 这个名字里
避免和其他地方同名的东西冲突
一般用来放遥控参数、配置参数相关的代码（从名字看）*/
namespace RcParam {
// 参数在内存/UI 侧的逻辑类型（不等于 TLV 线类型，线类型可在 Codec 里再映射）
enum class ParaValueKind :quint8{
    Double =0,
    Int    =1,
    Bool   =2,
    String =3,

};
/**
 * @brief(简短说明） 单参数描述 + 当前值（新协议 / 新 UI 专用）
 * @note（重要提醒） key 必须与 IAlgorithm::setParameters(QVariantMap) 使用的键一致，例如 "Kp","Ki","Kd"
 */
struct ParaNode{
    QString       key;           // 逻辑键 → QVariantMap 键,参数名称
    quint16       tag = 0;       // 新协议 TLV Tag；0 = 不参与下发或待分配相当于TLV里面的·Type
    QString       label;         // 界面显示；空则 UI 使用 key
    ParaValueKind kind = ParaValueKind::Double;
    QVariant      value;         // 当前值
    QVariant      defaultValue;  // 默认值（可选，用于「恢复默认」）
    QVariant      minValue;      // 无效 QVariant 表示不限制
    QVariant      maxValue;
    bool          readOnly = false;

    ParaNode() = default;//=default:我要显式使用编译器自动生成的默认构造函数，不要我自己写，也不要删掉它。
    //便捷构造：比如pid参数行
    static ParaNode makeDouble(const QString& key,double def,
                               quint16 tlvTag=0,
                               const QString& label= QString() );
    /// 从 QVariant 写入，类型不兼容则返回 false，并在 err 中说明（错误处理）
    bool setValueFromVariant(const QVariant& v,QString* err =nullptr);
    /// 读出为 QVariant（供 QVariantMap 使用）
    QVariant toVariant()const;
    ///是否具备有效的TLV tag（可下发）
    bool hasTlvTag()const {return tag!=0;}

};
// ---------- 与 IAlgorithm / UI 的映射（列表 ↔ QVariantMap）----------

/** 列表 → map（key → value），重复 key 时后者覆盖并在 err 中警告 */
QVariantMap paraListToVariantMap(const QList<ParaNode>& list, QString* err = nullptr);
/**
 * map → 写回列表中同 key 的节点；list 中不存在的 key 不会自动新增（避免 UI 胡写键污染表）
 * @return 未匹配到的 map 键数量（可选排查）
 */
int applyVariantMapToParaList(QList<ParaNode>& list,const QVariantMap& map,
                              QString* err=nullptr);


/** 用 defaultValue 重置 value（没有 default 则跳过） */
void resetParaListToDefaults(QList<ParaNode>& list);
// ---------- 可选：工程内配置存盘（非老 DcSys）----------
// 流版本号，便于以后加字段
constexpr qint32 kParaNodeStreamVersion = 1;

void writeParaList(QDataStream& out, const QList<ParaNode>& list);
bool readParaList(QDataStream& in, QList<ParaNode>& list, QString* err = nullptr);
}


#endif // PARANODE_H
