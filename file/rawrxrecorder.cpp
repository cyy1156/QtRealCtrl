#include "rawrxrecorder.h"

#include <cstring>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QtEndian>
#include "protocol/tlvcodec.h"

bool RawRxRecorder::isOpen() const
{
    return m_csvFile.isOpen();
}

double RawRxRecorder::calcElapsedSec(quint64 baseTimestampMs, quint64 currentTimestampMs)
{
    if (baseTimestampMs == 0 || currentTimestampMs < baseTimestampMs)
    {
        return 0.0;
    }
    return static_cast<double>(currentTimestampMs - baseTimestampMs) / 1000.0;
}

bool RawRxRecorder::openCsv(const QString &path, QString *errorMessage)
{
    close();
    m_csvFile.setFileName(path);
    if (!m_csvFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = m_csvFile.errorString();
        }
        return false;
    }

    m_csvStream.setDevice(&m_csvFile);
    if (m_csvFile.size() == 0)
    {
        m_csvStream
            << "日期,时间,相对时间(秒),来源端,帧类型(16进制/中文),Flags(16进制),序号Seq,Payload长度(字节),整帧长度(字节),RX整帧HEX(空格分隔),Payload内容(解析：TLV字段=值，；分隔)\n";
        m_csvStream.flush();
    }

    m_csvPendingFlushRows = 0;
    m_csvBaseTimestampMs = 0;
    return true;
}

void RawRxRecorder::appendRx(quint64 timestampMs, const QByteArray &bytes, const QString &src)
{
    if (!m_csvFile.isOpen() || bytes.isEmpty())
    {
        return;
    }

    if (m_csvBaseTimestampMs == 0)
    {
        m_csvBaseTimestampMs = timestampMs;
    }

    const double elapsedSec = calcElapsedSec(m_csvBaseTimestampMs, timestampMs);
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestampMs));

    // 中文注释：按 SDS 帧格式拆出关键信息：
    // SOF(0x55 0xAA),Ver(1),MsgType(1),Flags(1),Seq(2),PayloadLen(2),Payload(N),CRC(2)
    const quint8 msgType = (bytes.size() >= 4) ? static_cast<quint8>(bytes[3]) : 0;
    const quint8 flags = (bytes.size() >= 5) ? static_cast<quint8>(bytes[4]) : 0;
    const quint16 seq = (bytes.size() >= 7)
        ? (static_cast<quint8>(bytes[5]) | (static_cast<quint8>(bytes[6]) << 8))
        : 0;
    const quint16 payloadLen = (bytes.size() >= 9)
        ? (static_cast<quint8>(bytes[7]) | (static_cast<quint8>(bytes[8]) << 8))
        : 0;

    const QByteArray payload =
        (bytes.size() >= (9 + static_cast<int>(payloadLen)))
        ? bytes.mid(9, payloadLen)
        : QByteArray{};

    auto msgTypeToText = [](quint8 t) -> QString {
        switch (t)
        {
            case 0x01: return QStringLiteral("0x01-HELLO(握手)");
            case 0x02: return QStringLiteral("0x02-HEARTBEAT(心跳)");
            case 0x10: return QStringLiteral("0x10-Set_TARGET(设置目标)");
            case 0x11: return QStringLiteral("0x11-SET_PARAMS(设置参数)");
            case 0x12: return QStringLiteral("0x12-CONTROL_CMD(控制命令)");
            case 0x20: return QStringLiteral("0x20-TELEMETRY(遥测)");
            case 0x7F: return QStringLiteral("0x7F-ACK(应答)");
            default: return QStringLiteral("0x") + QString::number(t, 16);
        }
    };

    // 中文注释：从 QByteArray 的小端字节序中读取整数（避免依赖 qFromLittleEndian）。
    const auto readLeU16 = [](const QByteArray &v) -> quint16 {
        if (v.size() < 2) return 0;
        return static_cast<quint16>(static_cast<quint8>(v[0]))
               | (static_cast<quint16>(static_cast<quint8>(v[1])) << 8);
    };
    const auto readLeU32 = [](const QByteArray &v) -> quint32 {
        if (v.size() < 4) return 0;
        return static_cast<quint32>(static_cast<quint8>(v[0]))
               | (static_cast<quint32>(static_cast<quint8>(v[1])) << 8)
               | (static_cast<quint32>(static_cast<quint8>(v[2])) << 16)
               | (static_cast<quint32>(static_cast<quint8>(v[3])) << 24);
    };
    const auto readLeU64 = [](const QByteArray &v) -> quint64 {
        if (v.size() < 8) return 0;
        return static_cast<quint64>(static_cast<quint8>(v[0]))
               | (static_cast<quint64>(static_cast<quint8>(v[1])) << 8)
               | (static_cast<quint64>(static_cast<quint8>(v[2])) << 16)
               | (static_cast<quint64>(static_cast<quint8>(v[3])) << 24)
               | (static_cast<quint64>(static_cast<quint8>(v[4])) << 32)
               | (static_cast<quint64>(static_cast<quint8>(v[5])) << 40)
               | (static_cast<quint64>(static_cast<quint8>(v[6])) << 48)
               | (static_cast<quint64>(static_cast<quint8>(v[7])) << 56);
    };

    auto tlvTypeToName = [](quint16 type) -> QString {
        switch (type)
        {
            case TlvType::TargetPosition: return QStringLiteral("目标位置");
            case TlvType::TargetSpeed: return QStringLiteral("目标速度");
            case TlvType::TargetTorque: return QStringLiteral("目标扭矩");
            case TlvType::Mode: return QStringLiteral("模式");
            case TlvType::Enable: return QStringLiteral("使能");
            case TlvType::Kp: return QStringLiteral("Kp");
            case TlvType::Ki: return QStringLiteral("Ki");
            case TlvType::Kd: return QStringLiteral("Kd");
            case TlvType::MaxOutput: return QStringLiteral("最大输出");
            case TlvType::FeedbackPosition: return QStringLiteral("反馈位置");
            case TlvType::FeedbackSpeed: return QStringLiteral("反馈速度");
            case TlvType::FeedbackTorque: return QStringLiteral("反馈扭矩");
            case TlvType::Temperature: return QStringLiteral("温度");
            case TlvType::Voltage: return QStringLiteral("电压");
            case TlvType::Current: return QStringLiteral("电流");
            case TlvType::StatusFlags: return QStringLiteral("状态标志位");
            case TlvType::ErrorCode: return QStringLiteral("错误码");
            case TlvType::TextMessage: return QStringLiteral("文本消息");
            case TlvType::TimestampMs: return QStringLiteral("时间戳");
            case TlvType::HeartbeatIntervalMs: return QStringLiteral("心跳间隔");
            default: return QStringLiteral("TLV(0x") + QString::number(type, 16) + QStringLiteral(")");
        }
    };

    auto decodeTlvValueText = [=](const TlvItem &it) -> QString {
        const quint16 type = it.type;
        const QByteArray v = it.value;
        switch (type)
        {
            case TlvType::StatusFlags:
            {
                if (v.size() == 4)
                {
                    const quint32 raw = readLeU32(v);
                    return QStringLiteral("0x") + QString::number(raw, 16);
                }
                break;
            }
            case TlvType::Mode:
            case TlvType::Enable:
            {
                if (v.size() == 1)
                {
                    const quint8 raw = static_cast<quint8>(v[0]);
                    return QString::number(raw);
                }
                break;
            }
            case TlvType::HeartbeatIntervalMs:
            {
                if (v.size() == 4)
                {
                    const quint32 raw = readLeU32(v);
                    return QString::number(raw);
                }
                break;
            }
            case TlvType::ErrorCode:
            {
                if (v.size() == 2)
                {
                    const quint16 raw = readLeU16(v);
                    return QStringLiteral("0x") + QString::number(raw, 16);
                }
                break;
            }
            case TlvType::TimestampMs:
            {
                if (v.size() == 8)
                {
                    const quint64 raw = readLeU64(v);
                    return QString::number(raw);
                }
                break;
            }
            case TlvType::TextMessage:
            {
                return QString::fromUtf8(v);
            }
            default:
                break;
        }

        // 其余按常见长度做 float32 / hex 退化
        if (v.size() == 4)
        {
            const quint32 raw = readLeU32(v);
            float f = 0.0f;
            std::memcpy(&f, &raw, sizeof(float));
            return QString::number(f, 'f', 4);
        }

        return QStringLiteral("[HEX] ") + QString::fromLatin1(v.toHex(' '));
    };

    QString payloadDecodedText;
    if (!payload.isEmpty())
    {
        const auto items = Tlvcodec::decodeItems(payload);
        if (!items.isEmpty())
        {
            QStringList parts;
            for (const auto &it : items)
            {
                parts.push_back(tlvTypeToName(it.type) + '=' + decodeTlvValueText(it));
            }
            payloadDecodedText = parts.join(QStringLiteral("；"));
        }
        else
        {
            payloadDecodedText = QString::fromLatin1(payload.toHex(' '));
        }
    }
    else
    {
        payloadDecodedText = QStringLiteral("(空)");
    }

    m_csvStream
        << dt.toString("yyyy-MM-dd") << ','
        << dt.toString("HH:mm:ss.zzz") << ','
        << QString::number(elapsedSec, 'f', 3) << ','
        << src << ','
        << msgTypeToText(msgType) << ','
        << QStringLiteral("0x") + QString::number(flags, 16) << ','
        << QString::number(seq) << ','
        << QString::number(payloadLen) << ','
        << QString::number(bytes.size()) << ','
        << QString::fromLatin1(bytes.toHex(' ')) << ','
        << payloadDecodedText << '\n';

    ++m_csvPendingFlushRows;
    if (m_csvPendingFlushRows >= 20)
    {
        m_csvStream.flush();
        m_csvPendingFlushRows = 0;
    }
}

void RawRxRecorder::close()
{
    if (!m_csvFile.isOpen())
    {
        return;
    }
    m_csvStream.flush();
    m_csvFile.close();
    m_csvPendingFlushRows = 0;
    m_csvBaseTimestampMs = 0;
}

