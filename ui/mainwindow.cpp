#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDateTime>
#include <QTimer>
#include <algorithm/pidalgorithm.h>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
     ui->setupUi(this);//把 .ui 界面文件生成的代码，绑定到当前窗口对象
     m_clockTimer =new QTimer(this);
     m_clockTimer->setInterval(1000);

     m_pcDev=new SerialDevice(this);
     m_fake =new FakeDevice(this);
     m_sys=new SysInfo(this);
     m_alg=new PIDAlgorithm();
     m_ctrl =new ControlManager(m_pcDev,m_sys,m_alg,this);
     connect(m_clockTimer,&QTimer::timeout,this,[this](){
         ui->m_time->setText(QDateTime::currentDateTime().toString("hh:mm:ss"));
     });
     m_clockTimer->start();
    m_pcDev->setPortName("COM5");
    m_pcDev->setBaudRate(115200);

    m_fake->setPortName("COM6");
    m_pcDev->open();
    qDebug() << "PC port open =" << m_pcDev->isOpen();
    m_fake->open();//注销这个可以在串口助手看到COM5给COM6发送的信息
    /*
[22:00:41.748]收←◆55 AA 01 10 00 01 00 08 00 01 00 04 00 00 00 20 41 4E D3
[22:00:41.810]收←◆55 AA 01 10 00 02 00 08 00 01 00 04 00 00 00 20 41 D1 D6
[22:00:41.863]收←◆55 AA 01 10 00 03 00 08 00 01 00 04 00 00 00 20 41 A4 D5
[22:00:41.927]收←◆55 AA 01 10 00 04 00 08 00 01 00 04 00 00 00 20 41 EF DD  */
    //UI:开始
    connect(ui->btnStart,&QPushButton::clicked,this,[this](){
        m_sys->setTargetValue(100.0);
        m_ctrl->start()
;    });
    //UI:停止
    connect(ui->btnStop,&QPushButton::clicked,this,[this](){
        m_ctrl->stop();
    });
    //UI:清除
    connect(ui->btnClear,&QPushButton::clicked,this,[this](){
        m_ctrl->stop();
        m_sys->setTargetValue(0.0);
        m_sys->setCurrentValue(0.0);
    });

    connect(m_sys,&SysInfo::changed,this,[this](){
        ui->m_setting->setText(QString::number(m_sys->targetValue()));
        ui->m_cur->setText(QString::number(m_sys->currentValue()));

    });
    // connect(ui->btn_start,&QPushButton::clicked,this,&MainWindow::onStartClicked);
    // connect(ui->btn_Clear,&QPushButton::clicked,this,&MainWindow::onStartClicked);
    // connect(ui->btn_Stop,&QPushButton::clicked,this,&MainWindow::onStartClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}
