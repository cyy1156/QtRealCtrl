#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDateTime>
#include <QTimer>
#include <algorithm/pidalgorithm.h>
#include <QComboBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    //初始化
     ui->setupUi(this);//把 .ui 界面文件生成的代码，绑定到当前窗口对象
     m_clockTimer =new QTimer(this);
     m_clockTimer->setInterval(1000);
     m_pcDev=new SerialDevice(this);
     m_fake =new FakeDevice(this);
     m_sys=new SysInfo(this);
     auto* pid =new PIDAlgorithm();
     pid->setParameters({{"Kp",1.0},{"Ki",0.2},{"Kd",0.0}});

     m_currentAlg=pid;
     m_algPool["PID"] =pid;
     //注册算法工厂（用于首次选择时创建）
     m_creator["PID"]=[](QObject* parent)->IAlgorithm*{
         Q_UNUSED(parent);
         return new PIDAlgorithm();
     };
     // 如果以后加 MPC，就加一行类似：
     // m_creator["MPC"] = [](QObject* parent)->IAlgorithm* { return new MPCAlgorithm(); };
     m_ctrl =new ControlManager(m_pcDev,m_sys,m_currentAlg,this);

     QSignalBlocker blocker(ui->comboBox_Alg);
     ui->comboBox_Alg->clear();
     for(const auto& key : m_creator.keys()){
         ui->comboBox_Alg->addItem(key);
     }
     ui->comboBox_Alg->setCurrentIndex(ui->comboBox_Alg->findText("PID") >= 0?
                                           ui->comboBox_Alg->findText("PID"):0);

     connect(ui->comboBox_Alg,
             QOverload<int>::of(&QComboBox::currentIndexChanged),
             this,
             [this](int index){
                 const QString algName=ui->comboBox_Alg->itemText(index);
                 if(!m_creator.contains(algName)) return;
                 // 1) 创建或复用算法实例
                 IAlgorithm* alg=nullptr;
                 if(m_algPool.contains(algName)&&m_creator[algName])
                 {
                     alg=m_algPool[algName];
                 }else{
                     alg=m_creator[algName](this);
                    m_algPool[algName] = alg;
                 }
                 if(!alg) return;
                 //2) 组成参数params(示例：暂时从固定值来；后面你替换成 SysForm/ParaNode 或 UI 输入）
                 QVariantMap params;
                 if(algName=="PID")
                 {
                     params["Kp"]=1.0;
                     params["Ki"]=0.2;
                     params["Kd"]=0.0;

                 }
                 // 3) 注入参数（只管 setParameters）
                 alg->setParameters(params);

                  // 4) 切换算法：ControlManager 内部会 reset
                 m_ctrl->setAlgorithm(alg);

                 //5) 更新当前算法指针（可选，用于显示/调试）
                 m_currentAlg =alg;
                 qDebug()<< "[MainWindows] switched alg ="<<m_ctrl->algorithm()->name();

             }
             );
    // m_ctrl =new ControlManager(m_pcDev,m_sys,m_currentAlg,this);
     //ui连接时间
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
    for(auto it = m_algPool.begin();it != m_algPool.end();++it)
    {
        delete it.value();
    }
    delete ui;
}
