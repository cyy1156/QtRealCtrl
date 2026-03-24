#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDateTime>
#include <QTimer>
#include <algorithm/pidalgorithm.h>
#include <algorithm>
#include <QComboBox>
#include <QMetaObject>
#include <QVBoxLayout>
#include "QCustomPlot/qcustomplot.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    //初始化
     ui->setupUi(this);//把 .ui 界面文件生成的代码，绑定到当前窗口对象
     // 中文注释：在 frame_2 中动态放入 QCustomPlot，绘制两条曲线（当前值/控制量）
     auto *plotLayout = new QVBoxLayout(ui->frame_2);
     plotLayout->setContentsMargins(4, 4, 4, 4);
     plotLayout->setSpacing(0);
     m_plot = new QCustomPlot(ui->frame_2);
     plotLayout->addWidget(m_plot);
     m_plot->legend->setVisible(true);
     m_plot->addGraph(); // 中文注释：graph(0) = 当前值曲线
     m_plot->graph(0)->setName(QStringLiteral("当前值"));
     m_plot->graph(0)->setPen(QPen(QColor(0, 114, 255), 2));
     m_plot->addGraph(); // 中文注释：graph(1) = 控制量曲线
     m_plot->graph(1)->setName(QStringLiteral("控制量"));
     m_plot->graph(1)->setPen(QPen(QColor(230, 100, 20), 2));
     m_plot->xAxis->setLabel(QStringLiteral("时间 (s)"));
     m_plot->yAxis->setLabel(QStringLiteral("数值"));
     m_plot->xAxis->setRange(0.0, 10.0);
     m_plot->yAxis->setRange(-5.0, 5.0);
     m_clockTimer =new QTimer(this);
     m_clockTimer->setInterval(1000);
     // 中文注释：阶段 5（线程模型）——创建 DataWorkerThread/LogWorkerThread，并把数据对象迁移到数据线程
     m_dataThread = new QThread(this);
     m_logThread = new QThread(this);
     m_logWorker = new LogWorker(); // 中文注释：线程 3：日志/绘图数据处理
     m_logWorker->moveToThread(m_logThread);

     // 中文注释：这些对象稍后会 moveToThread，因此这里不要绑定 parent=this
     m_pcDev = new SerialDevice();
     m_fake  = new FakeDevice();
     m_sys   = new SysInfo();
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
     m_ctrl = new ControlManager(m_pcDev,m_sys,m_currentAlg,nullptr);

     // 中文注释：迁移到数据线程（线程 2）
     m_pcDev->moveToThread(m_dataThread);
     m_fake->moveToThread(m_dataThread);
     m_sys->moveToThread(m_dataThread);
     m_ctrl->moveToThread(m_dataThread);

     // 中文注释：把 telemetry 样本从数据线程发到日志线程
     connect(m_ctrl,
             &ControlManager::telemetrySampleReady,
             m_logWorker,
             &LogWorker::appendSample,
             Qt::QueuedConnection);

     // 中文注释：日志线程下采样后的 PlotDataChunk 在 UI 线程中更新两条曲线并触发重绘
     connect(m_logWorker,
             &LogWorker::historyDataReady,
             this,
             [this](const PlotDataChunk &chunk){
                 if(!m_plot || m_plot->graphCount() < 2)
                 {
                     return;
                 }

                 QVector<double> xPos;
                 QVector<double> yPos;
                 xPos.reserve(chunk.positionSeries.size());
                 yPos.reserve(chunk.positionSeries.size());
                 for (const QPointF &p : chunk.positionSeries)
                 {
                     xPos.push_back(p.x());
                     yPos.push_back(p.y());
                 }

                 QVector<double> xCtrl;
                 QVector<double> yCtrl;
                 xCtrl.reserve(chunk.controlSeries.size());
                 yCtrl.reserve(chunk.controlSeries.size());
                 for (const QPointF &p : chunk.controlSeries)
                 {
                     xCtrl.push_back(p.x());
                     yCtrl.push_back(p.y());
                 }

                 m_plot->graph(0)->setData(xPos, yPos);
                 m_plot->graph(1)->setData(xCtrl, yCtrl);

                 // 中文注释：右上状态栏“控制值”显示最新控制量
                 if(!yCtrl.isEmpty())
                 {
                     ui->m_ctrlValue->setText(QString::number(yCtrl.back(), 'f', 3));
                 }

                 // 中文注释：x 轴显示最近 10 秒窗口；y 轴根据当前数据自动重算
                 double xMax = 0.0;
                 if(!xPos.isEmpty()) xMax = std::max(xMax, xPos.back());
                 if(!xCtrl.isEmpty()) xMax = std::max(xMax, xCtrl.back());
                 m_plot->xAxis->setRange(std::max(0.0, xMax - 10.0), xMax + 0.1);
                 m_plot->rescaleAxes(true);
                 m_plot->replot(QCustomPlot::rpQueuedReplot);
             },
             Qt::QueuedConnection);

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
                 // 中文注释：阶段 5 多线程后，算法切换也要调度到数据线程执行
                 QMetaObject::invokeMethod(m_ctrl,[this,alg](){
                     m_ctrl->setAlgorithm(alg);
                 },Qt::QueuedConnection);

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
    // 中文注释：启动工作线程事件循环，再在对应线程内 open 串口，避免 QSerialPort 事件归属不正确
    m_dataThread->start();
    m_logThread->start();

    // 中文注释：在数据线程中打开 COM5（PC 发送端）
    QMetaObject::invokeMethod(m_pcDev,[this](){
        m_pcDev->setPortName("COM5");
        m_pcDev->setBaudRate(115200);
        const bool ok = m_pcDev->open();
        qDebug() << "PC port open =" << ok;
    },Qt::BlockingQueuedConnection);

    // 中文注释：在数据线程中打开 COM6（FakeDevice 发送遥测端）
    QMetaObject::invokeMethod(m_fake,[this](){
        m_fake->setPortName("COM6");
        const bool ok = m_fake->open();
        qDebug() << "Fake port open =" << ok;
    },Qt::BlockingQueuedConnection);
    /*
[22:00:41.748]收←◆55 AA 01 10 00 01 00 08 00 01 00 04 00 00 00 20 41 4E D3
[22:00:41.810]收←◆55 AA 01 10 00 02 00 08 00 01 00 04 00 00 00 20 41 D1 D6
[22:00:41.863]收←◆55 AA 01 10 00 03 00 08 00 01 00 04 00 00 00 20 41 A4 D5
[22:00:41.927]收←◆55 AA 01 10 00 04 00 08 00 01 00 04 00 00 00 20 41 EF DD  */
    //UI:开始
    connect(ui->btnStart,&QPushButton::clicked,this,[this](){
        // 中文注释：阶段 5 多线程后，避免跨线程直接调用，改用 queued 调度到数据线程执行
        // 中文注释：开始采集后才允许图像生成；同时清空旧缓存，保证从左端重新开始
        QMetaObject::invokeMethod(m_logWorker,[this](){
            m_logWorker->startCapture();
        },Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_sys,[this](){
            m_sys->setTargetValue(100.0);
        },Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_ctrl,[this](){
            m_ctrl->start();
        },Qt::QueuedConnection);
        // 中文注释：开始时先将控制值显示置零，等待新样本更新
        ui->m_ctrlValue->setText(QStringLiteral("0"));
    });
    //UI:停止
    connect(ui->btnStop,&QPushButton::clicked,this,[this](){
        // 中文注释：跨线程调用改为 queued 调度
        QMetaObject::invokeMethod(m_logWorker,[this](){
            m_logWorker->stopCapture();
        },Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_ctrl,[this](){
            m_ctrl->stop();
        },Qt::QueuedConnection);
    });
    //UI:清除
    connect(ui->btnClear,&QPushButton::clicked,this,[this](){
        // 中文注释：先清空日志/曲线缓存，再向控制线程发送“清零命令”
        QMetaObject::invokeMethod(m_logWorker,[this](){
            m_logWorker->clearData();
        },Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_ctrl,[this](){
            m_ctrl->clear();
        },Qt::QueuedConnection);
        // 中文注释：UI 侧立即清图，用户点击“清除”后立刻可见
        if (m_plot && m_plot->graphCount() >= 2)
        {
            m_plot->graph(0)->data()->clear();
            m_plot->graph(1)->data()->clear();
            m_plot->replot(QCustomPlot::rpQueuedReplot);
        }
        // 中文注释：清除后控制值显示回到 0
        ui->m_ctrlValue->setText(QStringLiteral("0"));
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
    // 中文注释：阶段 5 线程模型退出流程：先停控制，再 quit/wait 工作线程，最后析构对象
    if(m_ctrl)
    {
        QMetaObject::invokeMethod(m_ctrl,[this](){
            m_ctrl->stop();
        },Qt::BlockingQueuedConnection);
    }

    if(m_dataThread)
    {
        m_dataThread->quit();
        m_dataThread->wait();
    }
    if(m_logThread)
    {
        m_logThread->quit();
        m_logThread->wait();
    }

    // 中文注释：线程退出后再删除，避免 QObject 在事件循环中还被引用
    delete m_ctrl;
    delete m_sys;
    delete m_pcDev;
    delete m_fake;
    delete m_logWorker;

    for(auto it = m_algPool.begin();it != m_algPool.end();++it)
    {
        delete it.value();
    }
    delete ui;
}
