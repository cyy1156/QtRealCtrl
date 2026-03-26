#include "framecodec.h"
#include<QDebug>
/*CRC16-CCITT 查表实现初始化：设置初始校验值 crc = 0xFFFF；
逐字节处理数据（对每一个数据字节）：
            步骤 1：将当前crc的高 8 位与当前数据字节做异或运算，结果替换crc的高 8 位；
            步骤 2：对更新后的crc做 8 次循环移位 + 异或：
 在每次循环里面，检查移出的最高位：
        若最高位是 1 → 先左移一位crc = crc ^ 0x1021（异或多项式）；
                若最高位是 0 → 仅左移，不做异或；
                    最终结果：所有字节处理完成后，剩余的crc值就是该段数据的 CRC16-CCITT 校验值。*/
static quint16 s_crcTable[256];
static bool s_crcTableInitialized=false;
static void initCrcTable()
{
    if(s_crcTableInitialized)return;
    const quint16 poly= 0x1021;//串口通信的通用值,多项式
    for(int i=0;i<256;++i)//覆盖一个字节的所有可能一个字节是8位（二进制）
    {
        quint16 crc=static_cast<quint16>(i) <<8;//转换成16位
        for(int j=0;j<8;j++)
        {
            if(crc&0x8000)//0x8000的二进制是100000000000..检查第一位是否为1
                crc=(crc<<1)^poly;
            else
                crc<<=1;

        }
        s_crcTable[i]=crc;
    }
    s_crcTableInitialized=true;
}
FrameCodec::FrameCodec(QObject*parent):QObject(parent)
{}




quint16 FrameCodec::crc16Ccitt(const quint8* data,int len)
{
    initCrcTable();
    quint16 crc=0xFFFF;
    for(int i=0;i<len;++i)
    {
        crc=(crc<<8)^s_crcTable[((crc>>8)^data[i])&0XFF];
    }
    return crc;
}
/*msgType：告诉设备 “做什么”；
flags：告诉设备 “有什么额外要求”；
seq：给指令贴 “单号”，匹配请求和回复；
payload：告诉设备 “具体怎么做”（比如设置多少转速）*/
QByteArray FrameCodec::encodeFrame(quint8 msgType,quint8 flags,quint16 seq,const QByteArray& payload)const
{
    if(payload.size()>static_cast<int>(MAX_FRAME_LEN-FRAME_HEADER_SIZE))
    {
        qWarning()<<"FrameCodec::encodeFrame payload too long";
        return {};
    }

    QByteArray frame;
    frame.reserve(FRAME_HEADER_SIZE + payload.size());

    // 1. 拼帧头
    frame.append(SOF0);
    frame.append(SOF1);
    frame.append(PROTOCOL_VERSION);
    frame.append(msgType);
    frame.append(flags);

    // 2. 拼seq（小端）// 小端序传输：先传低位字节，再传高位字节,大端用网路，大端反过来
    frame.append(static_cast<char>(seq & 0xFF));
    frame.append(static_cast<char>((seq>>8)&0xFF));

    // 3. 拼payload长度（关键：设备需要知道payload有多长）
    quint16 plen = static_cast<quint16>(payload.size());
    frame.append(static_cast<char>(plen & 0xFF));
    frame.append(static_cast<char>((plen>>8)&0xFF));

    // 4. 拼核心业务数据payload
    frame.append(payload);

    // 5. 算CRC（此时frame包含所有数据，除了CRC本身）
    quint16 crc = crc16Ccitt(reinterpret_cast<const quint8*>(frame.constData()), frame.size());

    // 6. 拼CRC（小端）
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));

    return frame;


}
void FrameCodec::feedBytes(const QByteArray& data)
{
    m_rxBuffer.append(data);
    while(m_rxBuffer.size()>=FRAME_HEADER_SIZE)
    {
        //查找帧头 0x55 0xAA
        int sofIdx =-1;
        for(int i=0;i<m_rxBuffer.size()-2;++i)
        {
            if (static_cast<quint8>(m_rxBuffer[i])     == SOF0 &&
                static_cast<quint8>(m_rxBuffer[i + 1]) == SOF1)
            {
                sofIdx=i;
                break;
            }
        }
        if(sofIdx<0)
        {
            m_rxBuffer.clear();
            return;

        }
        if(sofIdx>0)
        {
            m_rxBuffer.remove(0,sofIdx);
        }
        // 重新检查长度，防止越界
        if (m_rxBuffer.size() < FRAME_HEADER_SIZE) {
            return;   // 数据还不够一帧头，等下一批数据
        }
        //解析payload 长度
        quint16 plen=static_cast<quint8>(m_rxBuffer[7])|(static_cast<quint8>(m_rxBuffer[8])<<8);//小端先取低八位在取高八位

        if(plen>MAX_FRAME_LEN-FRAME_HEADER_SIZE)
        {
            m_rxBuffer.remove(0,2);
            continue;
        }
        int frameLen=FRAME_HEADER_SIZE+plen;
        if(m_rxBuffer.size()<frameLen)
        {
            return;
        }
         //提取一帧
        QByteArray frameData =m_rxBuffer.left(frameLen);
        m_rxBuffer.remove(0,frameLen);
         //CRC校验从缓冲区效验CRCD
        quint16 rxCrc =static_cast<quint8>(frameData[frameLen - 2]) |(static_cast<quint8>(frameData[frameLen - 1]) << 8);

        quint16 calaCrc=crc16Ccitt(reinterpret_cast<const quint8*>(frameData.constData()),frameLen-2);

        if(rxCrc !=calaCrc)
        {
            qWarning()<<"CRC效验失败，丢弃帧";
            continue;
        }
        const quint8 flags = static_cast<quint8>(frameData[4]);
        quint8 msgType=static_cast<quint8>(frameData[3]);
        quint16 seq =static_cast<quint8>(frameData[5]) |(static_cast<quint8>(frameData[6]) << 8);
        QByteArray payload=frameData.mid(9,plen);

        emit frameReceived(msgType,seq,payload);//emit frameReceived(...) 就是 “触发 frameReceived 事件，
        //把 msgType、seq、payload 传给所有监听这个事件的函数”；

        // 额外信号：把合法整帧字节也透传出去（用于“按帧写 raw CSV”）
        emit frameReceivedFull(msgType, flags, seq, payload, frameData);

    }
}


























