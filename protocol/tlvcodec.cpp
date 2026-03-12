#include "tlvcodec.h"
#include<QtEndian>
//编码成TLV格式
QByteArray Tlvcodec::encodeItems(const QVector<TlvItem>& items)
{
    QByteArray payload;
    for(const TlvItem& it: items)
    {
        //写Type（u16，小端）
        quint16 t=it.type;
        char tBuf[2];
        qToLittleEndian(t,reinterpret_cast<uchar*>(tBuf));
        payload.append(tBuf,2);

        //写length（u16，小端）
        quint16 len=static_cast<quint16>(it.value.size());
        char lBuf[2];
        qToLittleEndian(len,reinterpret_cast<uchar*>(lBuf));
        payload.append(lBuf,2);
        //写value
        payload.append(it.value);

    }
    return payload;
}
//解码TLV格式
QVector<TlvItem>  Tlvcodec::decodeItems(const QByteArray& payload)
{
    QVector<TlvItem> items;
    int pos=0;
    const int n=payload.size();
    while(pos+4<=n)//因为T（2）+Len（2）=4
    {
        //读Type
        quint16 t=qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar*>(payload.constData()+pos));
        pos+=2;
        //读length

        quint16 len=qFromLittleEndian<quint16>
            (reinterpret_cast<const uchar*>(payload.constData()+pos));
        pos+=2;


        if(pos+len>n)
        {
            break;
        }

        TlvItem item;
        item.type =t;
        item.value=payload.mid(pos,len);
        pos+=len;
        items.append(item);
    }
    return items;

}

// ====== 辅助函数：把基础类型封装成 TLV ======
void Tlvcodec::appendFloat (QVector<TlvItem>& items,quint16 type,float v)
{
    TlvItem it;
    it.type =type;
    it.value.resize(4);
    qToLittleEndian(*reinterpret_cast<quint32*>(&v),
                    reinterpret_cast<uchar*>(it.value.data()));
    items.append(it);
}

void Tlvcodec::appendUInt8(QVector<TlvItem>& items,quint16 type,quint8 v)
{
    TlvItem it;
    it.type=type;
    it.value.append(static_cast<char>(v));
    items.append(it);
}
void Tlvcodec::appendString(QVector<TlvItem>& items,quint16 type,const QString& s)
{
    TlvItem it;
    it.type=type;
    QByteArray utf8=s.toUtf8();
    it.value=utf8;
    items.append(it);
}
// ====== 辅助函数：从 TlvItem 列表中取出指定 Type 的值 ======
bool Tlvcodec::tryGetFloat(const QVector<TlvItem>& items,quint16 type,float& out)
{
    for(const TlvItem& it:items)
    {
        if(it.type!=type) continue;
        if(it.value.size()!=4) return false;
        quint32 raw=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(it.value.constData()));
        out =*reinterpret_cast<float*>(&raw);//// 4. 把quint32的二进制数据强制转换成float类型，赋值给out
        return true;
    }
    return false;
}
bool Tlvcodec::tryGetUInt8(const QVector<TlvItem>& items,quint16 type,quint8& out)
{
    for(const TlvItem& it:items)
    {
        if(it.type!=type) continue;
        if(it.value.size()!=1) return false;
        
        out =static_cast<quint8>(it.value[0]);//quint8只有一个字节取第一个就可以了
        return true;
    }
    return false;
}
bool Tlvcodec::tryGetString(const QVector<TlvItem>& items, quint16 type, QString& out)
{
    for (const TlvItem& it : items) {
        if (it.type != type) continue;
        out = QString::fromUtf8(it.value);
        return true;
    }
    return false;
}

















