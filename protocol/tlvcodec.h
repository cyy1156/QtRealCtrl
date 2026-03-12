/*TlvCodec：专门负责把业务字段 ↔ payload：
多个字段（参数）打包到一个 QByteArray payload 中；
从 payload 里按 TLV 一条条解析出字段。*/

#ifndef TLVCODEC_H
#define TLVCODEC_H
#include<QByteArray>
#include<QVector>
#include<QString>
struct TlvItem
{
    quint16 type;
    QByteArray value;
};
namespace  TlvType {
// ===== 控制/设定类 =====
constexpr quint16 TargetPosition      = 0x0001;  // float32
constexpr quint16 TargetSpeed         = 0x0002;  // float32
constexpr quint16 TargetTorque        = 0x0003;  // float32
constexpr quint16 Mode                = 0x0004;  // uint8
constexpr quint16 Enable              = 0x0005;  // uint8
constexpr quint16 Kp                  = 0x0006;  // float32
constexpr quint16 Ki                  = 0x0007;  // float32
constexpr quint16 Kd                  = 0x0008;  // float32
constexpr quint16 MaxOutput           = 0x0009;  // float32

// ===== 状态/监测类 =====
constexpr quint16 FeedbackPosition    = 0x0101;  // float32
constexpr quint16 FeedbackSpeed       = 0x0102;  // float32
constexpr quint16 FeedbackTorque      = 0x0103;  // float32
constexpr quint16 Temperature         = 0x0104;  // float32
constexpr quint16 Voltage             = 0x0105;  // float32
constexpr quint16 Current             = 0x0106;  // float32
constexpr quint16 StatusFlags         = 0x0107;  // uint32 (bit flags)

// ===== 系统/通用类 =====
constexpr quint16 ErrorCode           = 0x0201;  // uint16
constexpr quint16 TextMessage         = 0x0202;  // UTF-8 string
constexpr quint16 TimestampMs         = 0x0203;  // uint64
constexpr quint16 HeartbeatIntervalMs = 0x0204;  // uint32
}
class Tlvcodec
{
public:
   //把一组 TlvItem 编码进 QByteArray payload
    static QByteArray encodeItems(const QVector<TlvItem>& items);
    // 从 payload 里解析出一组 TlvItem（遇到格式错误时尽量安全退出）
    static QVector<TlvItem> decodeItems(const QByteArray& payload) ;
    // 一些常用的帮助函数（方便上层直接传基础类型）
    static void appendFloat(QVector<TlvItem>& items,quint16 type,float v);
    static void appendUInt8(QVector<TlvItem>& items,quint16 type,quint8 v);
    static void appendString(QVector<TlvItem>& items, quint16 type, const QString& s);

    static bool tryGetFloat(const QVector<TlvItem>& items, quint16 type, float& out);
    static bool tryGetUInt8(const QVector<TlvItem>& items, quint16 type, quint8& out);
    static bool tryGetString(const QVector<TlvItem>& items, quint16 type, QString& out);
};

#endif // TLVCODEC_H
