// SDS 协议,序列化数据流协议

#ifndef FRAMECODEC_H
#define FRAMECODEC_H
#include <QObject>
#include<QByteArray>

//协议常量
constexpr quint8  SOF0             =0x55;
constexpr quint8  SOF1             =0xAA;
constexpr quint8  PROTOCOL_VERSION =1;
constexpr quint16 MAX_FRAME_LEN    =2048;
// 固定头部长度：SOF(2)+Version(1)+MsgType(1)+Flags(1)+Seq(2)+PayloadLen(2)+CRC16(2) = 11
//一帧数组的长度等于 固定+payload数组的长度
constexpr int FRAME_HEADER_SIZE =11;

enum class MsgType
{
    HELLO =0X01,    // 握手/建链消息：设备初次连接时的身份确认
    HEARTBEAT =0X02,// 心跳消息：维持连接，确认设备在线状态
    Set_TARGET=0X10, // 设置目标值：比如控制电机到指定转速、机械臂到指定角度
    SET_PARAMS  = 0x11,  // 设置参数：比如调整PID参数、串口波特率、传感器阈值
    CONTROL_CMD = 0x12,  // 控制指令：比如启动/停止、急停、复位等即时操作
    TELEMETRY   = 0x20,  // 遥测数据：设备上报状态（如温度、电压、位置、运行状态）
    ACK         = 0x7F   // 应答消息：收到其他消息后回复，确认已接收/处理
};
//flag的值
constexpr quint8 FLAG_NEED_ACK =0x01;//是否需要对方应答
constexpr quint8 FLAG_IS_ACK   =0x02;//本帧是否是应答帧


class FrameCodec: public QObject
{
    Q_OBJECT
public:
    explicit FrameCodec(QObject *parent =nullptr);
  //编码函数  核心编码接口：把业务层传入的参数，按 SDS 协议格式组装成完整的帧字节流（包含 SOF、CRC 等协议字段），返回可直接发送到串口的字节数组。
    QByteArray encodeFrame(quint8 msgType,quint8 flags,quint16 seq,const QByteArray& payload)const;
  //解码函数
    void feedBytes(const QByteArray& data);
signals:
  //解码成功后的「回调通知」：当 feedBytes 解析出完整、合法的 SDS 帧后，发射该信号，把解析后的核心业务数据抛给上层（比如主程序处理 HELLO / 心跳 / 控制指令）。
    void frameReceived(quint8 msgType,quint16 seq,QByteArray payload);

    // 中文注释：解码成功后携带 flags 和“整帧原始字节”，用于原始数据录制/调试。
    void frameReceivedFull(quint8 msgType, quint8 flags, quint16 seq, QByteArray payload, QByteArray fullFrame);
private:
    //SDS 协议的 CRC16-CCITT 校验计算函数，给编码 / 解码提供统一的 CRC 计算能力（编码时生成 CRC，解码时校验 CRC）。
    static quint16 crc16Ccitt(const quint8* data,int len);
    //接收缓冲区：存储串口收到的未解析字节数据，解决串口「分包 / 粘包」问题
    QByteArray m_rxBuffer;
};

#endif // FRAMECODEC_H
