#include "ui/mainwindow.h"
#include "schema_selftest.h"
#include <QSerialPort>
#include <QTimer>
#include <QApplication>
#include<QBuffer>
#include<QDebug>
#include "DcsProtocol/sysform.h"
#include "protocol/framecodec.h"
#include "protocol/tlvcodec.h"
#include "device/serialdevice.h"
#include<QString>



static void writeFragmented(QSerialPort& tx,const QByteArray& data,const QList<int>& chunks)
{
    int pos = 0;
    for(int n:chunks)
    {
        if(pos>=data.size()) break;
        const int len =qMin(n,data.size()-pos);
        const QByteArray part =data.mid(pos,len);
        tx.write(part);
        tx.flush();
        tx.waitForBytesWritten(50);
        pos+=len;
    }
}
static QByteArray corruptLastCrcByte(QByteArray frame)
{
    if(frame.size()<2) return frame;
    frame[frame.size()-1]=static_cast<char>(static_cast<quint8>(frame.back())^0xFF);//翻转字节
    return frame;
}
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // schema：ParaNode / AppSysForm / TLV 映射自测（不需要串口）。测完可注释掉本段。
    {
        QString schemaReport;
        if (!runSchemaSelfTest(&schemaReport)) {
            qCritical() << schemaReport;
            // 需要「测试失败即退出」时取消下面一行注释：
            // return 1;
        }
    }

    MainWindow w;
    w.show();


    /*SysForm from;
    from.primeName="MainForm";
    from.auxName="AuxFrom";

    ParaNode node;
    node.name="Temperature";
    node.dataType=DataType::IsFloat;
    node.value=25.5;
    node.archiveMode=ArchiveMode::Both;
    from.setValueList.append(node);

    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QDataStream out(&buffer);
    from.writeToStream(out);
    qDebug()<<"Prime name"<<from.primeName;
    qDebug() << "setValueList size =" << from.setValueList.size();
    if (!from.setValueList.isEmpty()) {
        qDebug() << "Temperature value:" << from.setValueList.first().value.toFloat();
    }
    buffer.seek(0);
    QDataStream in(&buffer);
    SysForm newForm;
    newForm.readFromStream(in);
    // 打印结果（全部带保护，不会再触发断言）
    qDebug() << "Prime name =" << newForm.primeName;
    qDebug() << "Aux   name =" << newForm.auxName;
    qDebug() << "setValueList size =" << newForm.setValueList.size();

    if (!newForm.setValueList.isEmpty()) {
        const ParaNode &n = newForm.setValueList.first();
        qDebug() << "First node name     =" << n.name;
        qDebug() << "First node dataType =" << static_cast<int>(n.dataType);
        qDebug() << "First node value    =" << n.value.toFloat();
    } else {
        qDebug() << "setValueList is empty, no node to show.";
    }

    return 0;  // 这里只做协议验证，不跑 GUI
    //return a.exec();
*/
    // 绑定接收信号
    /*FrameCodec codec;
    QObject::connect(&codec, &FrameCodec::frameReceived,
                     [](quint8 type, quint16 seq, QByteArray pay) {
                         qDebug() << "\n===== 成功解析一帧 =====";
                         qDebug() << "消息类型: 0x" + QString::number(type,16);
                         qDebug() << "序列号:" << seq;
                         qDebug() << "负载长度:" << pay.size();
                         qDebug() << "负载HEX:" << pay.toHex();
                         auto items=Tlvcodec::decodeItems(pay);

                             QString text;
                             quint8 mode=0;
                             if(Tlvcodec::tryGetString(items,TlvType::TextMessage,text))
                            {
                                     qDebug()<<"Text ="<<text;
                              }
                             if(Tlvcodec::tryGetUInt8(items,TlvType::Mode,mode))
                             {
                                  qDebug()<<"Mode ="<<mode;
                             }

    });

    QSerialPort tx;//发送
    QSerialPort rx;//接收

    //设置com0com配对修改这里
    tx.setPortName("COM5");
    rx.setPortName("COM6");

    auto setup =[](QSerialPort& p){
        p.setBaudRate(115200);//1. 波特率速率越高，测试中数据传输的延迟越小，能更快看到测试结果；
        p.setDataBits(QSerialPort::Data8);//2. 数据位
        p.setParity(QSerialPort::NoParity);//3. 校验位你的 FrameCodec 已经实现了 CRC16 校验（帧级别的强校验），串口层的校验位属于 “重复校验”，完全没必要；
        p.setStopBits(QSerialPort::OneStop);//4.停止位1 位停止位是最通用的配置，能满足绝大多数场景的同步需求
        p.setFlowControl(QSerialPort::NoFlowControl);
        //流控：p.setFlowControl(QSerialPort::NoFlowControl)
        //作用：用于控制数据传输的节奏（比如 RX 缓冲区满时，通知 TX 暂停发送，避免丢包）；

    };
    setup(tx);
    setup(rx);

    if(!tx.open(QIODevice::WriteOnly))
    {
        qCritical()<<"open tx failed"<<tx.errorString();
        return 1;
    }

    if(!rx.open(QIODevice::ReadOnly))
    {
        qCritical()<<"open rx failed"<<tx.errorString();
        return 1;
    }
    /*
     * &rx：信号的发送者 —— 这里是接收端串口对象（COM11），所有数据从这个串口读入；
      &QSerialPort::readyRead：要监听的信号 —— 这是 QSerialPort 的核心信号，
       当串口接收缓冲区有新字节到达时自动触发（比如 TX 发了 1 个字节 / 10 个字节，只要有新数据就触发）；
       [&](){ ... }：信号触发后执行的槽函数（这里是匿名 lambda 函数）——[&] 表示捕获外部所有变量的引用（比如 rx、codec），
       花括号里是具体要做的事。*/
    /*QObject::connect(&rx,&QSerialPort::readyRead,[&](){
        const QByteArray bytes =rx.readAll();
        codec.feedBytes(bytes);
    });
    // 生成两帧用于测试
    const QByteArray f1 = codec.encodeFrame(static_cast<quint8>(MsgType::HELLO), 0, 1, QByteArray());
    QVector<TlvItem> items;
    //先构造 TLV 列表
    Tlvcodec::appendString(items,TlvType::TextMessage,QStringLiteral("HelloTLV"));
    Tlvcodec::appendUInt8(items,TlvType::Mode,3);
    //编码成payload(真正的 TLV 二进制）
    QByteArray payload =Tlvcodec::encodeItems(items);
    const QByteArray f2 =codec.encodeFrame(
        static_cast<quint8>(MsgType::SET_PARAMS),
        0,
        2,
        payload
        );
    //const QByteArray f2 = codec.encodeFrame(static_cast<quint8>(MsgType::SET_PARAMS), 0, 2, QByteArray("ABCDEF"));
    qDebug() << "f1 =" << f1.toHex();//.toHex(' ').toUpper();//将字节数组（QByteArray）中的每个字节转换为对应的两位十六进制字符串，最终拼接成一个完整的十六进制字符串返回。
    qDebug() << "f2 =" << f2.toHex();//.toHex(' ').toUpper();
    // 1) 半包测试：把 f1 分成很多小块写入一次性定时器静态函数第二个参数是一个匿名 lambda 函数
    QTimer::singleShot(200, [&](){
        qDebug() << "\n[Test] fragmented write (half-packet simulation)";
        writeFragmented(tx, f1, {1, 2, 1, 3, 1, 1, 20}); // 你可以随便改分片
    });

    // 2) 粘包测试：两帧拼在一起一次性写入
    QTimer::singleShot(600, [&](){
        qDebug() << "\n[Test] sticky packets (two frames in one write)";
        tx.write(f1 + f2);
        tx.flush();
    });

    // 3) CRC 错一字节：写入损坏帧 + 正常帧，要求：损坏帧被丢弃，正常帧仍可解析
    QTimer::singleShot(1000, [&](){
        qDebug() << "\n[Test] bad CRC frame should be dropped, next frame still ok";
        const QByteArray bad = corruptLastCrcByte(f2);
        tx.write(bad + f1); // 先坏后好（也算粘包）
        tx.flush();
    });

    // 结束程序（可自行延长）
    QTimer::singleShot(2000, &a, &QCoreApplication::quit);
    /*
    // 1. 编码一个 HELLO 帧（空负载，序号1）
    QByteArray frame = codec.encodeFrame(
        static_cast<quint8>(MsgType::HELLO),
        0,
        1,
        QByteArray()
        );

    qDebug() << "发送帧 HEX:" << frame.toHex();

    // 2. 把帧喂给解码器（模拟串口接收）
    codec.feedBytes(frame);

    // 测试带 payload 的帧
    QByteArray testPay = "HelloSDS";
    QByteArray frame2 = codec.encodeFrame(static_cast<quint8>(MsgType::Set_TARGET), FLAG_NEED_ACK, 2, testPay);
    codec.feedBytes(frame2);
*/
/*
    SerialDevice devA;
    SerialDevice devB;
    devA.setPortName("COM5");
    devB.setPortName("COM6");
    QObject::connect(&devA,&SerialDevice::errorOccurred,[](const QString& m){
        qDebug()<<"[A error]"<<m;
    });
    QObject::connect(&devB,&SerialDevice::errorOccurred,[](const QString& m){
        qDebug()<<"[B error]"<<m;
    });
    QObject::connect(&devB,&SerialDevice::frameReceived,[](quint8 msgType,quint16 seq,QByteArray payload){
        qDebug()<<"[B] frameReceived type=0x"+QString::number(msgType,16)
                 <<"seq="<<seq
                 <<"payloadHex="<<payload.toHex();
              //TLV解码验证
              auto items=Tlvcodec::decodeItems(payload);
              QString text;
              quint8 mode=0;
              if(Tlvcodec::tryGetString(items,TlvType::TextMessage,text))
              {
                  qDebug()<<"[B] Text="<<text;
              }
              if(Tlvcodec::tryGetUInt8(items,TlvType::Mode,mode))
              {
                  qDebug()<<"[B] Mode="<<mode;

              }

    });
    if(!devA.open()) return 1;
    if(!devB.open()) return 1;
     //A 发一条 SET_PARAMS，B 收到后解析 TLV
    QTimer::singleShot(200,[&](){
        QVector<TlvItem> items;
        Tlvcodec::appendString(items,TlvType::TextMessage,QStringLiteral("HelloTLV"));
        Tlvcodec::appendUInt8(items,TlvType::Mode,3);

        QByteArray payload =Tlvcodec::encodeItems(items);
        devA.send(static_cast<quint8>(MsgType::SET_PARAMS),0,payload);
        QTimer::singleShot(1200,&a,&QCoreApplication::quit);
        //QTimer::singleShot 是 Qt 提供的 “一次性定时器”，作用是「延迟指定时间后，执行一次指定的动作（函数 / Lambda）
        //，执行完就自动销毁，不会重复触发」。
        return a.exec();
    });

*/
    return a.exec();
}
