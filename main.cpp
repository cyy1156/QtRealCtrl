#include "ui/mainwindow.h"

#include <QApplication>
#include<QBuffer>
#include<QDebug>
#include "protocol/sysform.h"
#include "protocol/framecodec.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //MainWindow w;
    //w.show();


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
    FrameCodec codec;
    QObject::connect(&codec, &FrameCodec::frameReceived,
                     [](quint8 type, quint16 seq, QByteArray pay) {
                         qDebug() << "\n===== 成功解析一帧 =====";
                         qDebug() << "消息类型: 0x" + QString::number(type,16);
                         qDebug() << "序列号:" << seq;
                         qDebug() << "负载长度:" << pay.size();
                         qDebug() << "负载HEX:" << pay.toHex();
                     });

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

    return a.exec();
}
