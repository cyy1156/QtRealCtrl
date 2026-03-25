#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDateTime>
#include <QTimer>
#include <algorithm/pidalgorithm.h>
#include <algorithm/predictivealgorithm.h>
#include <algorithm/adeptivealgorithm.h>
#include <algorithm/neuralpidalgorithm.h>
#include <algorithm/fnpidalgorithm.h>
#include <algorithm>
#include <QComboBox>
#include <QMetaObject>
#include <QMetaType>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QScrollBar>
#include <QLoggingCategory>
#include <QDateTime>
#include <QSettings>
#include <QSignalBlocker>
#include <QSerialPortInfo>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QSet>
#include <QFileDialog>
#include <QInputDialog>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QLineEdit>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include "QCustomPlot/qcustomplot.h"
#include "core/applogger.h"
#include "ui/parameterdialog.h"

Q_LOGGING_CATEGORY(lcUI, "UI")
Q_LOGGING_CATEGORY(lcSerial, "Serial")
Q_LOGGING_CATEGORY(lcSoftSim, "Control")

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

     // 中文注释：把“设定值”展示标签替换为可输入控件，支持自定义目标值
     m_targetInput = new QDoubleSpinBox(ui->BottomDispalyfame);
     m_targetInput->setGeometry(ui->m_setting->geometry());
     m_targetInput->setDecimals(3);
     m_targetInput->setRange(-10000.0, 10000.0);
     m_targetInput->setSingleStep(1.0);
     m_targetInput->setValue(100.0);
     m_targetInput->setFont(ui->m_setting->font());
     m_targetInput->show();
     ui->m_setting->hide();
     connect(m_targetInput, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v){
         if (!m_sys) return;
         QMetaObject::invokeMethod(m_sys,[this, v](){
             m_sys->setTargetValue(v);
         },Qt::QueuedConnection);
         m_softSimTarget = v;
         qCInfo(lcUI) << "[UserAction] set target input =" << v;
     });
    m_softSimTarget = 100.0;

     // 中文注释：把底部蓝色区域作为日志面板，显示命令行同款日志
     auto *bottomLayout = new QVBoxLayout(ui->Bottomfame);
     bottomLayout->setContentsMargins(8, 8, 8, 8);
     bottomLayout->setSpacing(6);

     auto *toolLayout = new QHBoxLayout();
     toolLayout->setContentsMargins(0, 0, 0, 0);
     toolLayout->setSpacing(6);
     m_btnClearLog = new QPushButton(QStringLiteral("清空日志"), ui->Bottomfame);
     m_btnPauseAutoScroll = new QPushButton(QStringLiteral("暂停自动滚动"), ui->Bottomfame);
     m_btnClearLog->setFixedHeight(26);
     m_btnPauseAutoScroll->setFixedHeight(26);
    // 苹果系玻璃按钮：更柔和高光渐变（动态按钮）
    m_btnClearLog->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: rgba(245, 250, 255, 0.98);"
        "  background-color: rgba(255,255,255,0.10);"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                              stop:0 rgba(255,255,255,0.24),"
        "                              stop:1 rgba(255,255,255,0.06));"
        "  border: 1px solid rgba(255,255,255,0.28);"
        "  border-radius: 12px;"
        "  padding: 3px 14px;"
        "  font-weight: 600;"
        "  font-family: 'SF Pro Display','SF Pro Text','PingFang SC','Microsoft YaHei',sans-serif;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255,255,255,0.14);"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                              stop:0 rgba(255,255,255,0.32),"
        "                              stop:1 rgba(255,255,255,0.08));"
        "  border: 1px solid rgba(170, 210, 255, 0.70);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                              stop:0 rgba(255,255,255,0.14),"
        "                              stop:1 rgba(255,255,255,0.04));"
        "  border: 1px solid rgba(120, 180, 255, 0.55);"
        "}"
        "QPushButton:disabled {"
        "  color: rgba(235, 245, 255, 0.45);"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                              stop:0 rgba(255,255,255,0.06),"
        "                              stop:1 rgba(255,255,255,0.03));"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "}"));
    m_btnPauseAutoScroll->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: rgba(245, 250, 255, 0.98);"
        "  background-color: rgba(255,255,255,0.10);"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                              stop:0 rgba(255,255,255,0.24),"
        "                              stop:1 rgba(255,255,255,0.06));"
        "  border: 1px solid rgba(255,255,255,0.28);"
        "  border-radius: 12px;"
        "  padding: 3px 14px;"
        "  font-weight: 600;"
        "  font-family: 'SF Pro Display','SF Pro Text','PingFang SC','Microsoft YaHei',sans-serif;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255,255,255,0.14);"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                              stop:0 rgba(255,255,255,0.32),"
        "                              stop:1 rgba(255,255,255,0.08));"
        "  border: 1px solid rgba(170, 210, 255, 0.70);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                              stop:0 rgba(255,255,255,0.14),"
        "                              stop:1 rgba(255,255,255,0.04));"
        "  border: 1px solid rgba(120, 180, 255, 0.55);"
        "}"
        "QPushButton:disabled {"
        "  color: rgba(235, 245, 255, 0.45);"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                              stop:0 rgba(255,255,255,0.06),"
        "                              stop:1 rgba(255,255,255,0.03));"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "}"));
     toolLayout->addWidget(m_btnClearLog);
     toolLayout->addWidget(m_btnPauseAutoScroll);
     toolLayout->addStretch();
     bottomLayout->addLayout(toolLayout);

     m_logText = new QPlainTextEdit(ui->Bottomfame);
     m_logText->setObjectName(QStringLiteral("bottomLogText"));
     m_logText->setReadOnly(true);
     m_logText->setMaximumBlockCount(1200);
     m_logText->setStyleSheet(QStringLiteral(
         "QPlainTextEdit#bottomLogText {"
         "background-color: rgb(20, 23, 39);"
         "color: rgb(186, 214, 255);"
         "border: 1px solid rgb(68, 90, 130);"
         "font-family: Consolas, 'Courier New', monospace;"
         "font-size: 10pt;"
         "}"));
     bottomLayout->addWidget(m_logText);

     connect(m_btnClearLog, &QPushButton::clicked, this, [this]() {
         if (m_logText)
         {
             m_isClearingLog = true;
             m_logText->clear();
             m_isClearingLog = false;
             m_pausedNewLogCount = 0;
             if (m_btnPauseAutoScroll && !m_autoScrollEnabled)
             {
                 m_btnPauseAutoScroll->setText(QStringLiteral("恢复自动滚动"));
             }
         }
     });

     connect(m_btnPauseAutoScroll, &QPushButton::clicked, this, [this]() {
         m_autoScrollEnabled = !m_autoScrollEnabled;
         if (m_btnPauseAutoScroll)
         {
             if (m_autoScrollEnabled)
             {
                 m_pausedNewLogCount = 0;
                 m_btnPauseAutoScroll->setText(QStringLiteral("暂停自动滚动"));
             }
             else
             {
                 m_btnPauseAutoScroll->setText(QStringLiteral("恢复自动滚动"));
             }
         }
         if (m_autoScrollEnabled && m_logText)
         {
             auto *bar = m_logText->verticalScrollBar();
             if (bar)
             {
                 bar->setValue(bar->maximum());
             }
         }
         qCInfo(lcUI) << "[UserAction] toggle AutoScroll"
                      << "enabled=" << m_autoScrollEnabled;
     });

     connect(&AppLogger::instance(),
             &AppLogger::logLineReady,
             this,
             [this](const QString &line, int){
                 appendLogLine(line);
             },
             Qt::QueuedConnection);
     appendLogLine(QStringLiteral("[UI] 日志面板已连接到 AppLogger"));

     m_clockTimer =new QTimer(this);
     m_clockTimer->setInterval(1000);
     // 中文注释：阶段 5（线程模型）——创建 DataWorkerThread/LogWorkerThread，并把数据对象迁移到数据线程
     m_dataThread = new QThread(this);
     m_logThread = new QThread(this);
     m_dataThread->setObjectName(QStringLiteral("Data"));
     m_logThread->setObjectName(QStringLiteral("Log"));
     m_softSimTimer = new QTimer(this);
     m_softSimTimer->setInterval(50);
     m_serialPortPollTimer = new QTimer(this);
     m_serialPortPollTimer->setInterval(2000);
     m_reconnectTimer = new QTimer(this);
     m_reconnectTimer->setSingleShot(true);
     m_softSimModel = QVector<double>{0.22, 0.18, 0.14, 0.10, 0.08, 0.06, 0.04, 0.03};
     m_logWorker = new LogWorker(); // 中文注释：线程 3：日志/绘图数据处理
     m_logWorker->moveToThread(m_logThread);
     qRegisterMetaType<Sample>("Sample");

     // 中文注释：这些对象稍后会 moveToThread，因此这里不要绑定 parent=this
     m_pcDev = new SerialDevice();
     m_fake  = new FakeDevice();
     m_sys   = new SysInfo();
    QMetaObject::invokeMethod(m_sys,[this](){
        m_sys->setTargetValue(100.0);
    },Qt::QueuedConnection);
     auto* pid =new PIDAlgorithm();
     pid->setParameters({{"Kp",1.0},{"Ki",0.2},{"Kd",0.0}});

     m_currentAlg=pid;
     m_algPool["PID"] =pid;
     //注册算法工厂（用于首次选择时创建）
     m_creator["PID"]=[](QObject* parent)->IAlgorithm*{
         Q_UNUSED(parent);
         return new PIDAlgorithm();
     };
     m_creator["PREDICTIVE"]=[](QObject* parent)->IAlgorithm*{
         Q_UNUSED(parent);
         return new PredictiveAlgorithm();
     };
     m_creator["ADEPTIVE"]=[](QObject* parent)->IAlgorithm*{
         Q_UNUSED(parent);
         return new AdeptiveAlgorithm();
     };
     m_creator["NEURALPID"]=[](QObject* parent)->IAlgorithm*{
         Q_UNUSED(parent);
         return new NeuralPidAlgorithm();
     };
     m_creator["FNPID"]=[](QObject* parent)->IAlgorithm*{
         Q_UNUSED(parent);
         return new FnPidAlgorithm();
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
                bool createdNow = false;
                 if(m_algPool.contains(algName)&&m_creator[algName])
                 {
                     alg=m_algPool[algName];
                 }else{
                     alg=m_creator[algName](this);
                    m_algPool[algName] = alg;
                    createdNow = true;
                 }
                 if(!alg) return;
                 //2) 组成参数params(示例：暂时从固定值来；后面你替换成 SysForm/ParaNode 或 UI 输入）
                 QVariantMap params;
                 if(algName=="PID")
                 {
                     params["Kp"]=1.0;
                     params["Ki"]=0.2;
                     params["Kd"]=0.0;
                    params["MaxOutput"] = 300.0;
                }
                else if(algName=="PREDICTIVE")
                {
                    params["alpha"]=0.004;
                    params["beta"]=0.0;
                    params["ControlStep"]=16;
                    params["OptimStep"]=3;
                    params["OptimCoef"]=1.0;
                    params["ControlCoef"]=10.1;
                    params["MinOutput"]=-300.0;
                    params["MaxOutput"]=300.0;
                }
                else if(algName=="ADEPTIVE")
                {
                    params["P"]=0.03;
                    params["I"]=0.01;
                    params["D"]=0.0;
                    params["MinOutput"]=-300.0;
                    params["MaxOutput"]=300.0;
                }
                else if(algName=="NEURALPID")
                {
                    params["NP"]=0.01;
                    params["NI"]=0.00001;
                    params["ND"]=0.02;
                    params["W1"]=0.002;
                    params["W2"]=0.01;
                    params["W3"]=0.01;
                    params["MinOutput"]=-300.0;
                    params["MaxOutput"]=300.0;
                }
                else if(algName=="FNPID")
                {
                    params["LearnKp"]=0.001;
                    params["LearnKi"]=0.001;
                    params["LearnKd"]=0.001;
                    params["Q"]=0.3;
                    params["MinOutput"]=-300.0;
                    params["MaxOutput"]=300.0;

                 }
                if (createdNow)
                {
                    qCInfo(lcSoftSim) << "[AlignV2] apply default params"
                                      << "alg=" << algName
                                      << "params=" << params;
                    // 3) 仅首次创建时注入默认参数；避免覆盖用户在参数弹窗里的修改
                    alg->setParameters(params);
                }
                qCInfo(lcUI) << "[UserAction] apply algorithm params"
                             << "alg=" << algName
                             << "params=" << alg->parameters();

                  // 4) 切换算法：ControlManager 内部会 reset
                 // 中文注释：阶段 5 多线程后，算法切换也要调度到数据线程执行
                 QMetaObject::invokeMethod(m_ctrl,[this,alg](){
                     m_ctrl->setAlgorithm(alg);
                 },Qt::QueuedConnection);

                 //5) 更新当前算法指针（可选，用于显示/调试）
                 m_currentAlg =alg;
                qCInfo(lcUI) << "switched alg =" << m_ctrl->algorithm()->name();

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

   // 中文注释：加载串口配置并刷新可用端口，供后续模式切换和开始逻辑复用
   loadSerialSettings();
   refreshAvailableSerialPorts();
   applySerialConfigsToDevices();
   {
       QSettings settings("QtRealCtrl", "RealCtrl");
       const QString defaultRoot = QDir::toNativeSeparators(
           QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
           + "/QtRealCtrlData");
       m_liveRecordRootDir = settings.value("record/liveRootDir", defaultRoot).toString();
       m_liveCsvFileNamePattern = settings.value(
           "record/liveCsvFileNamePattern",
           QStringLiteral("realtime_%TIMESTAMP%.csv")).toString();
       m_liveTxtFileNamePattern = settings.value(
           "record/liveTxtFileNamePattern",
           QStringLiteral("realtime_%TIMESTAMP%.txt")).toString();
       m_liveCsvFilePath = settings.value("record/liveCsvFilePath", QString()).toString();
       m_liveTxtFilePath = settings.value("record/liveTxtFilePath", QString()).toString();
   }
   connect(m_serialPortPollTimer, &QTimer::timeout, this, [this]() {
       pollSerialPorts();
   });
   m_serialPortPollTimer->start();
   connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
       attemptReconnect();
   });

   // 中文注释：模式选择（0=实时控制，1=串口仿真，2=纯软件仿真）
    ui->comboBox_Mode->clear();
    ui->comboBox_Mode->addItem(QStringLiteral("实时控制"));
   ui->comboBox_Mode->addItem(QStringLiteral("串口仿真"));
   ui->comboBox_Mode->addItem(QStringLiteral("纯软件仿真"));
   ui->comboBox_Mode->setCurrentIndex(2); // 默认纯软件仿真（不依赖串口）
   m_modeIndex = 2;
   updateSerialUiByMode(m_modeIndex);

   auto ensurePcPortOpen = [this]() -> bool {
       bool ok = false;
       QMetaObject::invokeMethod(m_pcDev, [this, &ok]() {
           if (m_pcDev->isOpen())
           {
               ok = true;
               return;
           }
          m_pcDev->setConfig(m_pcCfg);
           ok = m_pcDev->open();
           qCInfo(lcSerial) << "PC port open =" << ok;
       }, Qt::BlockingQueuedConnection);
       return ok;
   };

   auto ensureFakePortOpen = [this]() -> bool {
       bool ok = false;
       QMetaObject::invokeMethod(m_fake, [this, &ok]() {
          m_fake->setConfig(m_fakeCfg);
           ok = m_fake->open();
           qCInfo(lcSerial) << "Fake port open =" << ok;
       }, Qt::BlockingQueuedConnection);
       return ok;
   };

   connect(m_softSimTimer, &QTimer::timeout, this, [this]() {
       // 中文注释：参考旧工程 UpdateOutArray：基于控制增量 deltaU 推进内部预测数组
       if (m_softSimModel.isEmpty())
       {
           return;
       }
       if (m_softSimOutArray.size() != m_softSimModel.size())
       {
           m_softSimOutArray = QVector<double>(m_softSimModel.size(), m_softSimY);
       }

       const double dtSec = 0.05;
       const double target = m_softSimTarget;
       const double currentUsed = m_softSimY;
       const double u = m_currentAlg ? m_currentAlg->compute(target, currentUsed, dtSec) : 0.0;
       const double deltaU = u - m_softSimLastU;
       m_softSimLastU = u;
       double yNext = currentUsed;
       bool usedAlgPrediction = false;
       if (m_currentAlg)
       {
           usedAlgPrediction = m_currentAlg->predictNextFeedback(target, currentUsed, u, dtSec, yNext);
       }
       if (!usedAlgPrediction)
       {
           // 中文注释：算法未提供对象预测时，回退到“模型数组 + 控制增量”迭代
           for (int i = 0; i < m_softSimOutArray.size() - 1; ++i)
           {
               m_softSimOutArray[i] = m_softSimOutArray[i + 1] + m_softSimModel[i] * deltaU;
           }
           m_softSimOutArray.back() += m_softSimModel.back() * deltaU;
           yNext = m_softSimOutArray.front();
       }
       else
       {
           // 中文注释：使用算法预测时，同步刷新数组尾迹，方便后续切换回数组模型保持连续
           for (int i = 0; i < m_softSimOutArray.size() - 1; ++i)
           {
               m_softSimOutArray[i] = m_softSimOutArray[i + 1];
           }
           m_softSimOutArray.back() = yNext;
       }
       m_softSimY = yNext;
       m_softSimU = u;

       QMetaObject::invokeMethod(m_sys, [this, yNext]() {
           m_sys->setCurrentValue(yNext);
       }, Qt::QueuedConnection);
       ui->m_ctrlValue->setText(QString::number(u, 'f', 3));

       qCDebug(lcSoftSim) << "[SoftSim] alg=" << (m_currentAlg ? m_currentAlg->name() : "null")
                          << "target=" << target
                          << "currentUsed=" << currentUsed
                          << "u=" << u
                          << "deltaU=" << deltaU
                          << "predByAlg=" << usedAlgPrediction
                          << "predictedNext=" << yNext
                          << "modelState(head,tail)="
                          << (m_softSimOutArray.isEmpty() ? 0.0 : m_softSimOutArray.front())
                          << ","
                          << (m_softSimOutArray.isEmpty() ? 0.0 : m_softSimOutArray.back());

       Sample s;
       s.timestampMs = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
       s.position = static_cast<float>(yNext);
       s.controlOutput = static_cast<float>(u);
       s.statusFlags = 2U; // 中文注释：2 表示纯软件仿真状态
      appendLiveRecordSample(s);
       QMetaObject::invokeMethod(m_logWorker,
                                 "appendSample",
                                 Qt::QueuedConnection,
                                 Q_ARG(Sample, s));
   });

   connect(m_pcDev, &SerialDevice::errorOccurred, this, [this](const QString &message) {
       qCWarning(lcSerial) << "[SerialError] pc device =" << message;
       if (m_controlRunning && m_modeIndex != 2)
       {
           scheduleReconnect(QStringLiteral("pc:%1").arg(message));
       }
   }, Qt::QueuedConnection);
   connect(m_fake, &FakeDevice::errorOccurred, this, [this](const QString &message) {
       qCWarning(lcSerial) << "[SerialError] fake device =" << message;
       if (m_controlRunning && m_modeIndex == 1)
       {
           scheduleReconnect(QStringLiteral("fake:%1").arg(message));
       }
   }, Qt::QueuedConnection);
   connect(m_ctrl,
           &ControlManager::telemetrySampleReady,
           this,
           [this](const Sample &sample) {
              appendLiveRecordSample(sample);
           },
           Qt::QueuedConnection);


    connect(ui->comboBox_Mode,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this, ensureFakePortOpen](int index){
                if (m_modeIndex == index)
                {
                    return;
                }

                // 中文注释：切换模式前统一停控制和采集，避免模式切换期间状态混乱
                QMetaObject::invokeMethod(m_ctrl,[this](){
                    m_ctrl->stop();
                },Qt::BlockingQueuedConnection);
                QMetaObject::invokeMethod(m_logWorker,[this](){
                    m_logWorker->stopCapture();
                },Qt::BlockingQueuedConnection);
                if (m_softSimTimer && m_softSimTimer->isActive())
                {
                    m_softSimTimer->stop();
                }
                stopLiveRecording();
                m_controlRunning = false;
                m_reconnectAttemptCount = 0;
                if (m_reconnectTimer && m_reconnectTimer->isActive())
                {
                    m_reconnectTimer->stop();
                }

                if (index == 0)
                {
                    // 中文注释：实时控制模式 -> 关闭 FakeDevice
                    QMetaObject::invokeMethod(m_fake,[this](){
                        m_fake->close();
                        qCInfo(lcUI) << "[Mode] 实时控制: FakeDevice closed";
                    },Qt::BlockingQueuedConnection);
                }
                else if (index == 1)
                {
                    // 中文注释：串口仿真模式 -> 打开 FakeDevice，保持与上位机串口链路
                    ensureFakePortOpen();
                    qCInfo(lcUI) << "[Mode] 串口仿真: FakeDevice ready";
                }
                else
                {
                    // 中文注释：纯软件仿真 -> 关闭串口仿真设备，后续使用内部模型推进
                    QMetaObject::invokeMethod(m_fake,[this](){
                        m_fake->close();
                    },Qt::BlockingQueuedConnection);
                    qCInfo(lcUI) << "[Mode] 纯软件仿真: use internal model only";
                }

                m_modeIndex = index;
                updateSerialUiByMode(index);
            });
    /*
[22:00:41.748]收←◆55 AA 01 10 00 01 00 08 00 01 00 04 00 00 00 20 41 4E D3
[22:00:41.810]收←◆55 AA 01 10 00 02 00 08 00 01 00 04 00 00 00 20 41 D1 D6
[22:00:41.863]收←◆55 AA 01 10 00 03 00 08 00 01 00 04 00 00 00 20 41 A4 D5
[22:00:41.927]收←◆55 AA 01 10 00 04 00 08 00 01 00 04 00 00 00 20 41 EF DD  */
    //UI:开始
    connect(ui->btnStart,&QPushButton::clicked,this,[this, ensurePcPortOpen, ensureFakePortOpen](){
        QSettings settings("QtRealCtrl", "RealCtrl");
        const QString format = settings.value("record/exportFormat", QStringLiteral("none")).toString().toLower();
        if (format == QStringLiteral("csv"))
        {
            // 中文注释：开始时不弹窗；仅当用户已在“保存方式”里选过文件时，静默打开并写入
            if (!m_liveRecorder.isCsvOpen() && !m_liveCsvFilePath.isEmpty())
            {
                QString err;
                if (m_liveRecorder.openCsv(m_liveCsvFilePath, &err))
                {
                    qCInfo(lcUI) << "[Recorder] live csv reopened silently" << m_liveCsvFilePath;
                }
                else
                {
                    qCWarning(lcUI) << "[Recorder] failed to open csv silently"
                                    << m_liveCsvFilePath
                                    << err;
                }
            }
        }
        else if (format == QStringLiteral("txt"))
        {
            if (!m_liveRecorder.isTxtOpen() && !m_liveTxtFilePath.isEmpty())
            {
                QString err;
                if (m_liveRecorder.openTxt(m_liveTxtFilePath, &err))
                {
                    qCInfo(lcUI) << "[Recorder] live txt reopened silently" << m_liveTxtFilePath;
                }
                else
                {
                    qCWarning(lcUI) << "[Recorder] failed to open txt silently"
                                    << m_liveTxtFilePath
                                    << err;
                }
            }
        }
        else if (format == QStringLiteral("none"))
        {
            stopLiveRecording();
        }
        else
        {
            stopLiveRecording();
            qCInfo(lcUI) << "[Recorder] live recording disabled for format =" << format;
        }
        qCInfo(lcUI) << "[UserAction] click Start"
                     << "mode=" << ui->comboBox_Mode->currentText()
                     << "alg=" << ui->comboBox_Alg->currentText()
                     << "targetBefore=" << (m_targetInput ? m_targetInput->value() : 0.0);
        // 中文注释：阶段 5 多线程后，避免跨线程直接调用，改用 queued 调度到数据线程执行
        // 中文注释：开始采集后才允许图像生成；同时清空旧缓存，保证从左端重新开始
        if (m_modeIndex == 2)
        {
            m_controlRunning = true;
            m_reconnectAttemptCount = 0;
            if (m_reconnectTimer && m_reconnectTimer->isActive())
            {
                m_reconnectTimer->stop();
            }
            QMetaObject::invokeMethod(m_logWorker,[this](){
                m_logWorker->startCapture();
            },Qt::QueuedConnection);
            QMetaObject::invokeMethod(m_sys,[this](){
                m_sys->setTargetValue(m_targetInput ? m_targetInput->value() : 100.0);
            },Qt::QueuedConnection);
            m_softSimTarget = m_targetInput ? m_targetInput->value() : 100.0;
            qCInfo(lcUI) << "[UserAction] set target value to" << m_softSimTarget;
            // 中文注释：纯软件仿真：重置内部模型状态并启动仿真定时器
            m_softSimY = ui->m_cur->text().toDouble();
            m_softSimU = 0.0;
            m_softSimLastU = 0.0;
            m_softSimOutArray = QVector<double>(m_softSimModel.size(), m_softSimY);
            if (m_currentAlg)
            {
                m_currentAlg->reset();
            }
            if (m_softSimTimer && !m_softSimTimer->isActive())
            {
                m_softSimTimer->start();
            }
            qCInfo(lcSoftSim) << "[ModeRun] pure software simulation started"
                              << "alg=" << (m_currentAlg ? m_currentAlg->name() : "null")
                              << "modelLen=" << m_softSimModel.size()
                              << "target=" << m_softSimTarget
                              << "initY=" << m_softSimY;
        }
        else
        {
            const bool pcOk = ensurePcPortOpen();
            const bool fakeOk = (m_modeIndex == 1) ? ensureFakePortOpen() : true;
            if (!pcOk || !fakeOk)
            {
                m_controlRunning = false;
                qCWarning(lcSerial) << "[ModeRun] start failed: serial link not ready"
                                    << "pcOk=" << pcOk << "fakeOk=" << fakeOk;
                if (m_autoReconnectEnabled)
                {
                    scheduleReconnect(QStringLiteral("start failed"));
                }
                return;
            }
            m_controlRunning = true;
            m_reconnectAttemptCount = 0;
            if (m_reconnectTimer && m_reconnectTimer->isActive())
            {
                m_reconnectTimer->stop();
            }
            QMetaObject::invokeMethod(m_logWorker,[this](){
                m_logWorker->startCapture();
            },Qt::QueuedConnection);
            QMetaObject::invokeMethod(m_sys,[this](){
                m_sys->setTargetValue(m_targetInput ? m_targetInput->value() : 100.0);
            },Qt::QueuedConnection);
            qCInfo(lcUI) << "[UserAction] set target value to"
                         << (m_targetInput ? m_targetInput->value() : 100.0);
            QMetaObject::invokeMethod(m_ctrl,[this](){
                m_ctrl->start();
            },Qt::QueuedConnection);
        }
        // 中文注释：开始时先将控制值显示置零，等待新样本更新
        ui->m_ctrlValue->setText(QStringLiteral("0"));
    });
    //UI:停止
    connect(ui->btnStop,&QPushButton::clicked,this,[this](){
        stopLiveRecording();
        m_controlRunning = false;
        m_reconnectAttemptCount = 0;
        if (m_reconnectTimer && m_reconnectTimer->isActive())
        {
            m_reconnectTimer->stop();
        }
        qCInfo(lcUI) << "[UserAction] click Stop"
                     << "mode=" << ui->comboBox_Mode->currentText()
                     << "alg=" << ui->comboBox_Alg->currentText()
                     << "targetNow=" << (m_targetInput ? m_targetInput->value() : 0.0)
                     << "currentNow=" << ui->m_cur->text()
                     << "ctrlNow=" << ui->m_ctrlValue->text();
        // 中文注释：跨线程调用改为 queued 调度
        QMetaObject::invokeMethod(m_logWorker,[this](){
            m_logWorker->stopCapture();
        },Qt::QueuedConnection);
        if (m_modeIndex == 2)
        {
            if (m_softSimTimer)
            {
                m_softSimTimer->stop();
            }
            qCInfo(lcSoftSim) << "[ModeRun] pure software simulation stopped"
                              << "lastY=" << m_softSimY
                              << "lastU=" << m_softSimU;
        }
        else
        {
            QMetaObject::invokeMethod(m_ctrl,[this](){
                m_ctrl->stop();
            },Qt::QueuedConnection);
        }
    });
    //UI:清除
    connect(ui->btnClear,&QPushButton::clicked,this,[this](){
        stopLiveRecording();
        m_controlRunning = false;
        m_reconnectAttemptCount = 0;
        if (m_reconnectTimer && m_reconnectTimer->isActive())
        {
            m_reconnectTimer->stop();
        }
        const double desiredTarget = m_targetInput ? m_targetInput->value() : 100.0;
        qCInfo(lcUI) << "[UserAction] click Clear"
                     << "mode=" << ui->comboBox_Mode->currentText()
                     << "alg=" << ui->comboBox_Alg->currentText()
                     << "keepTarget=" << desiredTarget;
        // 中文注释：先清空日志/曲线缓存，再向控制线程发送“清零命令”
        QMetaObject::invokeMethod(m_logWorker,[this](){
            m_logWorker->clearData();
        },Qt::QueuedConnection);
        if (m_modeIndex != 2)
        {
            QMetaObject::invokeMethod(m_ctrl,[this](){
                m_ctrl->clear();
            },Qt::QueuedConnection);
        }
        if (m_softSimTimer)
        {
            m_softSimTimer->stop();
        }
        m_softSimY = 0.0;
        m_softSimU = 0.0;
        m_softSimLastU = 0.0;
        m_softSimTarget = desiredTarget;
        m_softSimOutArray = QVector<double>(m_softSimModel.size(), 0.0);
        QMetaObject::invokeMethod(m_sys,[this, desiredTarget](){
            // 中文注释：清除只复位当前值，不改用户设定目标值
            m_sys->setTargetValue(desiredTarget);
            m_sys->setCurrentValue(0.0);
        },Qt::QueuedConnection);
        qCInfo(lcSoftSim) << "[ModeRun] pure software simulation cleared";
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

    // UI:设置参数（按当前算法动态弹窗）
    connect(ui->btnPara, &QPushButton::clicked, this, [this]() {
        if (!m_currentAlg)
        {
            qCWarning(lcUI) << "[UserAction] click Parameter but current algorithm is null";
            return;
        }
        const QString algName = m_currentAlg->name();
        const auto schema = m_currentAlg->parameterSchema();
        if (schema.isEmpty())
        {
            qCWarning(lcUI) << "[UserAction] click Parameter but schema is empty, alg=" << algName;
            return;
        }

        auto buildDefaultParams = [algName]() -> QVariantMap {
            QVariantMap params;
            if(algName=="PID")
            {
                params["Kp"]=1.0;
                params["Ki"]=0.2;
                params["Kd"]=0.0;
                params["MaxOutput"] = 300.0;
            }
            else if(algName=="PREDICTIVE")
            {
                params["alpha"]=0.004;
                params["beta"]=0.0;
                params["ControlStep"]=16;
                params["OptimStep"]=3;
                params["OptimCoef"]=1.0;
                params["ControlCoef"]=10.1;
                params["MinOutput"]=-300.0;
                params["MaxOutput"]=300.0;
            }
            else if(algName=="ADEPTIVE")
            {
                params["P"]=0.03;
                params["I"]=0.01;
                params["D"]=0.0;
                params["MinOutput"]=-300.0;
                params["MaxOutput"]=300.0;
            }
            else if(algName=="NEURALPID")
            {
                params["NP"]=0.01;
                params["NI"]=0.00001;
                params["ND"]=0.02;
                params["W1"]=0.002;
                params["W2"]=0.01;
                params["W3"]=0.01;
                params["MinOutput"]=-300.0;
                params["MaxOutput"]=300.0;
            }
            else if(algName=="FNPID")
            {
                params["LearnKp"]=0.001;
                params["LearnKi"]=0.001;
                params["LearnKd"]=0.001;
                params["Q"]=0.3;
                params["MinOutput"]=-300.0;
                params["MaxOutput"]=300.0;
            }
            return params;
        };

        const QVariantMap defaultParams = buildDefaultParams();
        QVariantMap oldParams = m_currentAlg->parameters();
        // 中文注释：优先加载持久化参数，确保弹框展示用户上次保存值
        QSettings settings("QtRealCtrl", "RealCtrl");
        for (const auto &def : schema)
        {
            const QString skey = QStringLiteral("algorithms/%1/%2").arg(algName, def.key);
            if (settings.contains(skey))
            {
                oldParams[def.key] = settings.value(skey);
            }
        }
        m_currentAlg->setParameters(oldParams);
        ParameterDialog dlg(algName, schema, oldParams, defaultParams, this);
        if (dlg.exec() != QDialog::Accepted)
        {
            qCInfo(lcUI) << "[UserAction] parameter dialog canceled" << "alg=" << algName;
            return;
        }

        const QVariantMap newParams = dlg.parameterValues();
        m_currentAlg->setParameters(newParams);
        // 中文注释：参数持久化，供下次启动/下次弹框直接读取
        for (const auto &def : schema)
        {
            const QString skey = QStringLiteral("algorithms/%1/%2").arg(algName, def.key);
            settings.setValue(skey, newParams.value(def.key));
        }
        qCInfo(lcUI) << "[UserAction] apply parameter dialog"
                     << "alg=" << algName
                     << "old=" << oldParams
                     << "new=" << newParams;
    });

    // UI:保存（当前阶段先记录保存动作与关键参数快照，后续接入真实保存逻辑）
    connect(ui->btnSave, &QPushButton::clicked, this, [this]() {
        QSettings settings("QtRealCtrl", "RealCtrl");
        const QString savedFormat = settings.value("record/exportFormat", QStringLiteral("none")).toString();
        QString selectedCsvPath = settings.value("record/liveCsvFilePath", m_liveCsvFilePath).toString();
        QString selectedTxtPath = settings.value("record/liveTxtFilePath", m_liveTxtFilePath).toString();

        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("保存方式"));
        auto *layout = new QVBoxLayout(&dlg);
        auto *csvRadio = new QRadioButton(QStringLiteral("CSV（推荐，便于实时分析）"), &dlg);
        auto *txtRadio = new QRadioButton(QStringLiteral("TXT（文本日志）"), &dlg);
        auto *noneRadio = new QRadioButton(QStringLiteral("不保存（不创建实时记录文件）"), &dlg);
        csvRadio->setChecked(savedFormat.compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0);
        txtRadio->setChecked(savedFormat.compare(QStringLiteral("txt"), Qt::CaseInsensitive) == 0);
        noneRadio->setChecked(savedFormat.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0);
        if (!csvRadio->isChecked() && !txtRadio->isChecked() && !noneRadio->isChecked())
        {
            csvRadio->setChecked(true);
        }
        layout->addWidget(csvRadio);
        layout->addWidget(txtRadio);
        layout->addWidget(noneRadio);

        auto *pathEdit = new QLineEdit(&dlg);
        pathEdit->setReadOnly(true);
        pathEdit->setPlaceholderText(QStringLiteral("未选择保存文件"));
        pathEdit->setText(csvRadio->isChecked() ? selectedCsvPath : (txtRadio->isChecked() ? selectedTxtPath : QString()));
        layout->addWidget(pathEdit);

        auto *btnNewFile = new QPushButton(QStringLiteral("新建文件..."), &dlg);
        auto *btnExistingFile = new QPushButton(QStringLiteral("选择已有文件..."), &dlg);
        auto *btnOpenFile = new QPushButton(QStringLiteral("快捷打开文件"), &dlg);
        auto *btnOpenFolder = new QPushButton(QStringLiteral("快捷打开所在文件夹"), &dlg);
        layout->addWidget(btnNewFile);
        layout->addWidget(btnExistingFile);
        layout->addWidget(btnOpenFile);
        layout->addWidget(btnOpenFolder);

        auto refreshPathEdit = [&]() {
            if (csvRadio->isChecked())
            {
                pathEdit->setText(selectedCsvPath);
            }
            else if (txtRadio->isChecked())
            {
                pathEdit->setText(selectedTxtPath);
            }
            else
            {
                pathEdit->clear();
            }
        };
        connect(csvRadio, &QRadioButton::toggled, &dlg, [&](bool){ refreshPathEdit(); });
        connect(txtRadio, &QRadioButton::toggled, &dlg, [&](bool){ refreshPathEdit(); });
        connect(noneRadio, &QRadioButton::toggled, &dlg, [&](bool){ refreshPathEdit(); });

        connect(btnNewFile, &QPushButton::clicked, &dlg, [&]() {
            const bool chooseCsv = csvRadio->isChecked();
            const QString suffix = chooseCsv ? QStringLiteral(".csv") : QStringLiteral(".txt");
            const QString defaultName = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + suffix;
            const QString initialDir = m_liveRecordRootDir.isEmpty()
                ? QDir::currentPath()
                : m_liveRecordRootDir;
            const QString initial = QDir(initialDir).filePath(defaultName);
            QString filePath = QFileDialog::getSaveFileName(
                &dlg,
                chooseCsv ? QStringLiteral("选择CSV保存文件") : QStringLiteral("选择TXT保存文件"),
                initial,
                chooseCsv ? QStringLiteral("CSV 文件 (*.csv)") : QStringLiteral("TXT 文件 (*.txt)"));
            if (filePath.isEmpty())
            {
                return;
            }
            if (!filePath.endsWith(suffix, Qt::CaseInsensitive))
            {
                filePath += suffix;
            }
            if (chooseCsv)
            {
                selectedCsvPath = filePath;
            }
            else
            {
                selectedTxtPath = filePath;
            }
            refreshPathEdit();
        });
        connect(btnExistingFile, &QPushButton::clicked, &dlg, [&]() {
            const bool chooseCsv = csvRadio->isChecked();
            QString currentPath = chooseCsv ? selectedCsvPath : selectedTxtPath;
            const QString filePath = QFileDialog::getOpenFileName(
                &dlg,
                chooseCsv ? QStringLiteral("选择已有CSV文件") : QStringLiteral("选择已有TXT文件"),
                currentPath.isEmpty() ? m_liveRecordRootDir : QFileInfo(currentPath).absolutePath(),
                chooseCsv ? QStringLiteral("CSV 文件 (*.csv)") : QStringLiteral("TXT 文件 (*.txt)"));
            if (filePath.isEmpty())
            {
                return;
            }
            if (chooseCsv)
            {
                selectedCsvPath = filePath;
            }
            else
            {
                selectedTxtPath = filePath;
            }
            refreshPathEdit();
        });
        connect(btnOpenFile, &QPushButton::clicked, &dlg, [&]() {
            const QString p = pathEdit->text().trimmed();
            if (p.isEmpty() || !QFileInfo::exists(p))
            {
                QMessageBox::information(&dlg, QStringLiteral("提示"), QStringLiteral("当前文件不存在，请先选择有效文件。"));
                return;
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(p));
        });
        connect(btnOpenFolder, &QPushButton::clicked, &dlg, [&]() {
            const QString p = pathEdit->text().trimmed();
            QString folder = p.isEmpty() ? m_liveRecordRootDir : QFileInfo(p).absolutePath();
            if (folder.isEmpty())
            {
                return;
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
        });

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted)
        {
            return;
        }
        QString format = QStringLiteral("csv");
        if (txtRadio->isChecked())
        {
            format = QStringLiteral("txt");
        }
        else if (noneRadio->isChecked())
        {
            format = QStringLiteral("none");
        }
        settings.setValue("record/exportFormat", format);
        settings.setValue("record/liveCsvFilePath", selectedCsvPath);
        settings.setValue("record/liveTxtFilePath", selectedTxtPath);
        m_liveCsvFilePath = selectedCsvPath;
        m_liveTxtFilePath = selectedTxtPath;
        const QString activePath = (format == QStringLiteral("txt")) ? m_liveTxtFilePath : m_liveCsvFilePath;
        if (!activePath.isEmpty())
        {
            m_liveRecordFolderDir = QFileInfo(activePath).absolutePath();
            m_liveRecordRootDir = QFileInfo(m_liveRecordFolderDir).dir().absolutePath();
            if (format == QStringLiteral("txt"))
            {
                m_liveTxtFileNamePattern = QFileInfo(activePath).fileName();
                settings.setValue("record/liveTxtFileNamePattern", m_liveTxtFileNamePattern);
            }
            else if (format == QStringLiteral("csv"))
            {
                m_liveCsvFileNamePattern = QFileInfo(activePath).fileName();
                settings.setValue("record/liveCsvFileNamePattern", m_liveCsvFileNamePattern);
            }
            settings.setValue("record/liveRootDir", m_liveRecordRootDir);
        }
        if (format == QStringLiteral("none"))
        {
            stopLiveRecording();
        }

        qCInfo(lcUI) << "[UserAction] click Save"
                     << "mode=" << ui->comboBox_Mode->currentText()
                     << "alg=" << ui->comboBox_Alg->currentText()
                     << "target=" << (m_targetInput ? m_targetInput->value() : 0.0)
                     << "current=" << ui->m_cur->text()
                     << "ctrl=" << ui->m_ctrlValue->text()
                     << "format=" << format
                     << "csvPath=" << m_liveCsvFilePath
                     << "txtPath=" << m_liveTxtFilePath;

        QMessageBox::information(this,
                                 QStringLiteral("保存方式"),
                                 QStringLiteral("已保存当前设置。\n格式：%1\n文件：%2")
                                     .arg(format.toUpper(),
                                          activePath.isEmpty() ? QStringLiteral("未选择") : activePath));
    });

    connect(ui->btnSerialPort, &QPushButton::clicked, this, [this]() {
        showSerialSettingsDialog();
    });

    connect(m_sys,&SysInfo::changed,this,[this](){
        if (m_targetInput)
        {
            QSignalBlocker blocker(m_targetInput);
            m_targetInput->setValue(m_sys->targetValue());
        }
        ui->m_cur->setText(QString::number(m_sys->currentValue()));

    });

    // connect(ui->btn_start,&QPushButton::clicked,this,&MainWindow::onStartClicked);
    // connect(ui->btn_Clear,&QPushButton::clicked,this,&MainWindow::onStartClicked);
    // connect(ui->btn_Stop,&QPushButton::clicked,this,&MainWindow::onStartClicked);
}

void MainWindow::refreshAvailableSerialPorts()
{
    QString currentPc = m_pcCfg.portName;
    QString currentFake = m_fakeCfg.portName;
    m_availablePorts.clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
    {
        m_availablePorts.push_back(info.portName());
    }

    if (!m_availablePorts.contains(currentPc) && !m_availablePorts.isEmpty())
    {
        m_pcCfg.portName = m_availablePorts.first();
    }
    if (!m_availablePorts.contains(currentFake) && m_availablePorts.size() >= 2)
    {
        m_fakeCfg.portName = m_availablePorts.at(1);
    }
    else if (!m_availablePorts.contains(currentFake) && !m_availablePorts.isEmpty())
    {
        m_fakeCfg.portName = m_availablePorts.first();
    }
}

void MainWindow::loadSerialSettings()
{
    QSettings settings("QtRealCtrl", "RealCtrl");
    m_pcCfg.portName = settings.value("serial/pc/portName", "COM5").toString();
    m_pcCfg.baudRate = settings.value("serial/pc/baudRate", 115200).toInt();
    m_pcCfg.dataBits = static_cast<QSerialPort::DataBits>(
        settings.value("serial/pc/dataBits", static_cast<int>(QSerialPort::Data8)).toInt());
    m_pcCfg.parity = static_cast<QSerialPort::Parity>(
        settings.value("serial/pc/parity", static_cast<int>(QSerialPort::NoParity)).toInt());
    m_pcCfg.stopBits = static_cast<QSerialPort::StopBits>(
        settings.value("serial/pc/stopBits", static_cast<int>(QSerialPort::OneStop)).toInt());
    m_pcCfg.flowControl = static_cast<QSerialPort::FlowControl>(
        settings.value("serial/pc/flowControl", static_cast<int>(QSerialPort::NoFlowControl)).toInt());

    m_fakeCfg.portName = settings.value("serial/fake/portName", "COM6").toString();
    m_fakeCfg.baudRate = settings.value("serial/fake/baudRate", 115200).toInt();
    m_fakeCfg.dataBits = static_cast<QSerialPort::DataBits>(
        settings.value("serial/fake/dataBits", static_cast<int>(QSerialPort::Data8)).toInt());
    m_fakeCfg.parity = static_cast<QSerialPort::Parity>(
        settings.value("serial/fake/parity", static_cast<int>(QSerialPort::NoParity)).toInt());
    m_fakeCfg.stopBits = static_cast<QSerialPort::StopBits>(
        settings.value("serial/fake/stopBits", static_cast<int>(QSerialPort::OneStop)).toInt());
    m_fakeCfg.flowControl = static_cast<QSerialPort::FlowControl>(
        settings.value("serial/fake/flowControl", static_cast<int>(QSerialPort::NoFlowControl)).toInt());

    m_autoReconnectEnabled = settings.value("serial/advanced/autoReconnect", true).toBool();
    m_reconnectIntervalMs = settings.value("serial/advanced/reconnectIntervalMs", 1000).toInt();
    m_maxReconnectRetry = settings.value("serial/advanced/maxRetry", 10).toInt();
    m_readTimeoutMs = settings.value("serial/advanced/readTimeoutMs", 50).toInt();
    m_rxBufferBytes = settings.value("serial/advanced/rxBufferBytes", 64 * 1024).toInt();
    m_txBufferBytes = settings.value("serial/advanced/txBufferBytes", 64 * 1024).toInt();
    m_rawSerialLogEnabled = settings.value("serial/advanced/rawLog", false).toBool();
}

void MainWindow::saveSerialSettings() const
{
    QSettings settings("QtRealCtrl", "RealCtrl");
    settings.setValue("serial/pc/portName", m_pcCfg.portName);
    settings.setValue("serial/pc/baudRate", m_pcCfg.baudRate);
    settings.setValue("serial/pc/dataBits", static_cast<int>(m_pcCfg.dataBits));
    settings.setValue("serial/pc/parity", static_cast<int>(m_pcCfg.parity));
    settings.setValue("serial/pc/stopBits", static_cast<int>(m_pcCfg.stopBits));
    settings.setValue("serial/pc/flowControl", static_cast<int>(m_pcCfg.flowControl));

    settings.setValue("serial/fake/portName", m_fakeCfg.portName);
    settings.setValue("serial/fake/baudRate", m_fakeCfg.baudRate);
    settings.setValue("serial/fake/dataBits", static_cast<int>(m_fakeCfg.dataBits));
    settings.setValue("serial/fake/parity", static_cast<int>(m_fakeCfg.parity));
    settings.setValue("serial/fake/stopBits", static_cast<int>(m_fakeCfg.stopBits));
    settings.setValue("serial/fake/flowControl", static_cast<int>(m_fakeCfg.flowControl));

    settings.setValue("serial/advanced/autoReconnect", m_autoReconnectEnabled);
    settings.setValue("serial/advanced/reconnectIntervalMs", m_reconnectIntervalMs);
    settings.setValue("serial/advanced/maxRetry", m_maxReconnectRetry);
    settings.setValue("serial/advanced/readTimeoutMs", m_readTimeoutMs);
    settings.setValue("serial/advanced/rxBufferBytes", m_rxBufferBytes);
    settings.setValue("serial/advanced/txBufferBytes", m_txBufferBytes);
    settings.setValue("serial/advanced/rawLog", m_rawSerialLogEnabled);
}

void MainWindow::applySerialConfigsToDevices()
{
    if (!m_pcDev || !m_fake)
    {
        return;
    }
    QMetaObject::invokeMethod(m_pcDev, [this]() {
        const bool wasOpen = m_pcDev->isOpen();
        if (wasOpen)
        {
            m_pcDev->close();
        }
        m_pcDev->setConfig(m_pcCfg);
        if (wasOpen)
        {
            const bool reopened = m_pcDev->open();
            qCInfo(lcSerial) << "[SerialConfig] reopen pc port =" << reopened;
        }
    }, Qt::QueuedConnection);
    // 中文注释：实时模式不启用 Fake 串口，避免在配置应用时误触发 Fake 打开
    if (m_modeIndex == 1)
    {
        QMetaObject::invokeMethod(m_fake, [this]() {
            const bool wasOpen = m_fake->isOpen();
            if (wasOpen)
            {
                m_fake->close();
            }
            m_fake->setConfig(m_fakeCfg);
            if (wasOpen)
            {
                const bool reopened = m_fake->open();
                qCInfo(lcSerial) << "[SerialConfig] reopen fake port =" << reopened;
            }
        }, Qt::QueuedConnection);
    }
    else
    {
        QMetaObject::invokeMethod(m_fake, [this]() {
            m_fake->setConfig(m_fakeCfg);
        }, Qt::QueuedConnection);
    }
}

void MainWindow::updateSerialUiByMode(int modeIndex)
{
    if (!ui || !ui->btnSerialPort)
    {
        return;
    }
    // 中文注释：纯软件仿真禁用串口设置按钮；实时和串口仿真允许打开设置
    ui->btnSerialPort->setEnabled(modeIndex != 2);
}

void MainWindow::pollSerialPorts()
{
    const QStringList before = m_availablePorts;
    refreshAvailableSerialPorts();
    const QSet<QString> oldSet(before.begin(), before.end());
    const QSet<QString> newSet(m_availablePorts.begin(), m_availablePorts.end());
    if (oldSet == newSet)
    {
        return;
    }

    const QSet<QString> added = newSet - oldSet;
    const QSet<QString> removed = oldSet - newSet;
    if (!added.isEmpty())
    {
        qCInfo(lcSerial) << "[SerialPorts] added =" << QStringList(added.begin(), added.end());
    }
    if (!removed.isEmpty())
    {
        qCWarning(lcSerial) << "[SerialPorts] removed =" << QStringList(removed.begin(), removed.end());
        if (m_controlRunning
            && (removed.contains(m_pcCfg.portName)
                || (m_modeIndex == 1 && removed.contains(m_fakeCfg.portName))))
        {
            scheduleReconnect(QStringLiteral("active port removed"));
        }
    }
}

void MainWindow::scheduleReconnect(const QString &reason)
{
    if (!m_autoReconnectEnabled || m_modeIndex == 2 || !m_controlRunning)
    {
        return;
    }
    if (m_reconnectAttemptCount >= m_maxReconnectRetry)
    {
        qCWarning(lcSerial) << "[Reconnect] reach max retry, stop reconnect"
                            << "count=" << m_reconnectAttemptCount;
        return;
    }
    if (m_reconnectTimer && m_reconnectTimer->isActive())
    {
        return;
    }
    qCWarning(lcSerial) << "[Reconnect] scheduled"
                        << "reason=" << reason
                        << "nextMs=" << m_reconnectIntervalMs
                        << "attempt=" << (m_reconnectAttemptCount + 1)
                        << "/" << m_maxReconnectRetry;
    m_reconnectTimer->start(m_reconnectIntervalMs);
}

void MainWindow::attemptReconnect()
{
    if (!m_controlRunning || m_modeIndex == 2)
    {
        return;
    }

    ++m_reconnectAttemptCount;
    bool pcOk = false;
    bool fakeOk = true;
    QMetaObject::invokeMethod(m_pcDev, [this, &pcOk]() {
        if (!m_pcDev->isOpen())
        {
            m_pcDev->setConfig(m_pcCfg);
            pcOk = m_pcDev->open();
            return;
        }
        pcOk = true;
    }, Qt::BlockingQueuedConnection);

    if (m_modeIndex == 1)
    {
        QMetaObject::invokeMethod(m_fake, [this, &fakeOk]() {
            if (!m_fake->isOpen())
            {
                m_fake->setConfig(m_fakeCfg);
                fakeOk = m_fake->open();
                return;
            }
            fakeOk = true;
        }, Qt::BlockingQueuedConnection);
    }

    if (pcOk && fakeOk)
    {
        qCInfo(lcSerial) << "[Reconnect] success"
                         << "attempt=" << m_reconnectAttemptCount;
        m_reconnectAttemptCount = 0;
        return;
    }

    qCWarning(lcSerial) << "[Reconnect] failed"
                        << "attempt=" << m_reconnectAttemptCount
                        << "pcOk=" << pcOk
                        << "fakeOk=" << fakeOk;
    scheduleReconnect(QStringLiteral("attempt failed"));
}

void MainWindow::showSerialSettingsDialog()
{
    refreshAvailableSerialPorts();
    const bool isRealTimeMode = (m_modeIndex == 0);

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("串口设置"));
    auto *layout = new QFormLayout(&dlg);

    auto *pcPortBox = new QComboBox(&dlg);
    auto *fakePortBox = new QComboBox(&dlg);
    for (const QString &port : m_availablePorts)
    {
        pcPortBox->addItem(port, port);
        fakePortBox->addItem(port, port);
    }
    pcPortBox->setCurrentText(m_pcCfg.portName);
    fakePortBox->setCurrentText(m_fakeCfg.portName);

    auto *baudBox = new QComboBox(&dlg);
    baudBox->setEditable(true);
    const QList<int> bauds{9600, 19200, 38400, 57600, 115200, 230400, 460800};
    for (int v : bauds)
    {
        baudBox->addItem(QString::number(v), v);
    }
    baudBox->setCurrentText(QString::number(m_pcCfg.baudRate));

    auto *dataBitsBox = new QComboBox(&dlg);
    dataBitsBox->addItem(QStringLiteral("5"), static_cast<int>(QSerialPort::Data5));
    dataBitsBox->addItem(QStringLiteral("6"), static_cast<int>(QSerialPort::Data6));
    dataBitsBox->addItem(QStringLiteral("7"), static_cast<int>(QSerialPort::Data7));
    dataBitsBox->addItem(QStringLiteral("8"), static_cast<int>(QSerialPort::Data8));
    dataBitsBox->setCurrentIndex(dataBitsBox->findData(static_cast<int>(m_pcCfg.dataBits)));

    auto *parityBox = new QComboBox(&dlg);
    parityBox->addItem(QStringLiteral("None"), static_cast<int>(QSerialPort::NoParity));
    parityBox->addItem(QStringLiteral("Even"), static_cast<int>(QSerialPort::EvenParity));
    parityBox->addItem(QStringLiteral("Odd"), static_cast<int>(QSerialPort::OddParity));
    parityBox->addItem(QStringLiteral("Mark"), static_cast<int>(QSerialPort::MarkParity));
    parityBox->addItem(QStringLiteral("Space"), static_cast<int>(QSerialPort::SpaceParity));
    parityBox->setCurrentIndex(parityBox->findData(static_cast<int>(m_pcCfg.parity)));

    auto *stopBitsBox = new QComboBox(&dlg);
    stopBitsBox->addItem(QStringLiteral("1"), static_cast<int>(QSerialPort::OneStop));
    stopBitsBox->addItem(QStringLiteral("1.5"), static_cast<int>(QSerialPort::OneAndHalfStop));
    stopBitsBox->addItem(QStringLiteral("2"), static_cast<int>(QSerialPort::TwoStop));
    stopBitsBox->setCurrentIndex(stopBitsBox->findData(static_cast<int>(m_pcCfg.stopBits)));

    auto *flowControlBox = new QComboBox(&dlg);
    flowControlBox->addItem(QStringLiteral("None"), static_cast<int>(QSerialPort::NoFlowControl));
    flowControlBox->addItem(QStringLiteral("RTS/CTS"), static_cast<int>(QSerialPort::HardwareControl));
    flowControlBox->addItem(QStringLiteral("XON/XOFF"), static_cast<int>(QSerialPort::SoftwareControl));
    flowControlBox->setCurrentIndex(flowControlBox->findData(static_cast<int>(m_pcCfg.flowControl)));

    auto *autoReconnectCheck = new QCheckBox(QStringLiteral("自动重连"), &dlg);
    autoReconnectCheck->setChecked(m_autoReconnectEnabled);
    auto *reconnectIntervalSpin = new QSpinBox(&dlg);
    reconnectIntervalSpin->setRange(100, 10000);
    reconnectIntervalSpin->setValue(m_reconnectIntervalMs);
    reconnectIntervalSpin->setSuffix(QStringLiteral(" ms"));
    auto *maxRetrySpin = new QSpinBox(&dlg);
    maxRetrySpin->setRange(1, 1000);
    maxRetrySpin->setValue(m_maxReconnectRetry);
    auto *rawLogCheck = new QCheckBox(QStringLiteral("启用串口原始日志"), &dlg);
    rawLogCheck->setChecked(m_rawSerialLogEnabled);

    layout->addRow(QStringLiteral("上位机串口"), pcPortBox);
    auto *fakePortLabel = new QLabel(QStringLiteral("仿真串口"), &dlg);
    layout->addRow(fakePortLabel, fakePortBox);
    // 中文注释：实时控制模式只需要一个串口，直接隐藏 Fake 串口配置项
    fakePortLabel->setVisible(!isRealTimeMode);
    fakePortBox->setVisible(!isRealTimeMode);
    layout->addRow(QStringLiteral("波特率"), baudBox);
    layout->addRow(QStringLiteral("数据位"), dataBitsBox);
    layout->addRow(QStringLiteral("校验位"), parityBox);
    layout->addRow(QStringLiteral("停止位"), stopBitsBox);
    layout->addRow(QStringLiteral("流控"), flowControlBox);
    layout->addRow(autoReconnectCheck);
    layout->addRow(QStringLiteral("重连间隔"), reconnectIntervalSpin);
    layout->addRow(QStringLiteral("最大重试"), maxRetrySpin);
    layout->addRow(rawLogCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
    {
        return;
    }

    const QString selectedPcPort = pcPortBox->currentData().toString();
    const QString selectedFakePort = fakePortBox->currentData().toString();
    if (!isRealTimeMode && !selectedPcPort.isEmpty() && selectedPcPort == selectedFakePort)
    {
        QMessageBox::warning(this,
                             QStringLiteral("串口设置"),
                             QStringLiteral("串口仿真模式下，上位机串口和仿真串口不能相同。"));
        return;
    }

    m_pcCfg.portName = selectedPcPort;
    if (!isRealTimeMode)
    {
        m_fakeCfg.portName = selectedFakePort;
    }
    bool baudOk = false;
    const int baud = baudBox->currentText().toInt(&baudOk);
    if (!baudOk || baud <= 0 || baud > 4000000)
    {
        QMessageBox::warning(this,
                             QStringLiteral("串口设置"),
                             QStringLiteral("波特率必须是 1~4000000 的整数。"));
        return;
    }
    m_pcCfg.baudRate = baud;
    m_pcCfg.dataBits = static_cast<QSerialPort::DataBits>(dataBitsBox->currentData().toInt());
    m_pcCfg.parity = static_cast<QSerialPort::Parity>(parityBox->currentData().toInt());
    m_pcCfg.stopBits = static_cast<QSerialPort::StopBits>(stopBitsBox->currentData().toInt());
    m_pcCfg.flowControl = static_cast<QSerialPort::FlowControl>(flowControlBox->currentData().toInt());
    if (!isRealTimeMode)
    {
        m_fakeCfg.baudRate = m_pcCfg.baudRate;
        m_fakeCfg.dataBits = m_pcCfg.dataBits;
        m_fakeCfg.parity = m_pcCfg.parity;
        m_fakeCfg.stopBits = m_pcCfg.stopBits;
        m_fakeCfg.flowControl = m_pcCfg.flowControl;
    }
    m_autoReconnectEnabled = autoReconnectCheck->isChecked();
    m_reconnectIntervalMs = reconnectIntervalSpin->value();
    m_maxReconnectRetry = maxRetrySpin->value();
    m_rawSerialLogEnabled = rawLogCheck->isChecked();

    saveSerialSettings();
    applySerialConfigsToDevices();

    qCInfo(lcSerial) << "[SerialConfig] applied"
                     << "pc=" << m_pcCfg.portName
                     << "fake=" << m_fakeCfg.portName
                     << "baud=" << m_pcCfg.baudRate
                     << "autoReconnect=" << m_autoReconnectEnabled
                     << "retryIntervalMs=" << m_reconnectIntervalMs
                     << "maxRetry=" << m_maxReconnectRetry;
}

bool MainWindow::startLiveCsvRecording()
{
    stopLiveCsvRecording();

    QDir rootDir(m_liveRecordRootDir);
    if (!rootDir.exists())
    {
        rootDir.mkpath(".");
    }

    QString defaultName = m_liveCsvFileNamePattern;
    if (defaultName.contains("%TIMESTAMP%"))
    {
        defaultName.replace("%TIMESTAMP%", QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    }
    if (!defaultName.endsWith(".csv", Qt::CaseInsensitive))
    {
        defaultName += ".csv";
    }
    const QString initialPath = QDir(rootDir.absolutePath()).filePath(defaultName);
    const QString selectedPath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("选择实时CSV保存文件"),
        initialPath,
        QStringLiteral("CSV 文件 (*.csv)"));
    if (selectedPath.isEmpty())
    {
        return false;
    }

    QString candidatePath = selectedPath;
    if (!candidatePath.endsWith(".csv", Qt::CaseInsensitive))
    {
        candidatePath += ".csv";
    }
    QDir().mkpath(QFileInfo(candidatePath).absolutePath());

    const QString patternFromName = QFileInfo(candidatePath).fileName();
    m_liveRecordFolderDir = QFileInfo(candidatePath).absolutePath();
    m_liveRecordRootDir = QFileInfo(m_liveRecordFolderDir).dir().absolutePath();
    m_liveCsvFileNamePattern = patternFromName;
    QSettings settings("QtRealCtrl", "RealCtrl");
    settings.setValue("record/liveRootDir", m_liveRecordRootDir);
    settings.setValue("record/liveCsvFileNamePattern", m_liveCsvFileNamePattern);

    const bool existsBefore = QFileInfo::exists(candidatePath);
    if (existsBefore)
    {
        QMessageBox modeBox(this);
        modeBox.setWindowTitle(QStringLiteral("CSV写入方式"));
        modeBox.setText(QStringLiteral("目标文件已存在，请选择写入方式"));
        QPushButton *btnAppend = modeBox.addButton(QStringLiteral("续写当前文件"), QMessageBox::AcceptRole);
        QPushButton *btnNewFile = modeBox.addButton(QStringLiteral("新建新文件"), QMessageBox::ActionRole);
        modeBox.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        modeBox.exec();
        if (modeBox.clickedButton() == btnAppend)
        {
            // keep candidatePath
        }
        else if (modeBox.clickedButton() == btnNewFile)
        {
            const QString stamped = QFileInfo(candidatePath).completeBaseName()
                                    + "_"
                                    + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")
                                    + ".csv";
            candidatePath = QDir(QFileInfo(candidatePath).absolutePath()).filePath(stamped);
        }
        else
        {
            return false;
        }
    }

    m_liveCsvFilePath = candidatePath;
    QString err;
    if (!m_liveRecorder.openCsv(m_liveCsvFilePath, &err))
    {
        QMessageBox::warning(this,
                             QStringLiteral("实时CSV记录"),
                             QStringLiteral("无法打开CSV文件：%1").arg(err));
        return false;
    }
    qCInfo(lcUI) << "[Recorder] live csv started" << m_liveCsvFilePath;
    return true;
}

bool MainWindow::startLiveTxtRecording()
{
    stopLiveTxtRecording();
    if (m_liveTxtFilePath.isEmpty())
    {
        return false;
    }
    QDir().mkpath(QFileInfo(m_liveTxtFilePath).absolutePath());
    QString err;
    if (!m_liveRecorder.openTxt(m_liveTxtFilePath, &err))
    {
        qCWarning(lcUI) << "[Recorder] failed to open txt" << m_liveTxtFilePath << err;
        return false;
    }
    qCInfo(lcUI) << "[Recorder] live txt started" << m_liveTxtFilePath;
    return true;
}

void MainWindow::appendLiveCsvSample(const Sample &sample)
{
    const QString modeText = ui && ui->comboBox_Mode ? ui->comboBox_Mode->currentText() : QStringLiteral("unknown");
    const QString algText = ui && ui->comboBox_Alg ? ui->comboBox_Alg->currentText() : QStringLiteral("unknown");
    const double target = m_targetInput ? m_targetInput->value() : m_softSimTarget;
    m_liveRecorder.appendCsvSample(sample, target, modeText, algText);
}

void MainWindow::appendLiveTxtSample(const Sample &sample)
{
    const QString modeText = ui && ui->comboBox_Mode ? ui->comboBox_Mode->currentText() : QStringLiteral("unknown");
    const QString algText = ui && ui->comboBox_Alg ? ui->comboBox_Alg->currentText() : QStringLiteral("unknown");
    const double target = m_targetInput ? m_targetInput->value() : m_softSimTarget;
    m_liveRecorder.appendTxtSample(sample, target, modeText, algText);
}

void MainWindow::appendLiveRecordSample(const Sample &sample)
{
    QSettings settings("QtRealCtrl", "RealCtrl");
    const QString format = settings.value("record/exportFormat", QStringLiteral("none")).toString().toLower();
    if (format == QStringLiteral("csv"))
    {
        appendLiveCsvSample(sample);
    }
    else if (format == QStringLiteral("txt"))
    {
        appendLiveTxtSample(sample);
    }
}

void MainWindow::stopLiveCsvRecording()
{
    m_liveRecorder.closeCsv();
    qCInfo(lcUI) << "[Recorder] live csv stopped" << m_liveCsvFilePath;
}

void MainWindow::stopLiveTxtRecording()
{
    m_liveRecorder.closeTxt();
    qCInfo(lcUI) << "[Recorder] live txt stopped" << m_liveTxtFilePath;
}

void MainWindow::stopLiveRecording()
{
    m_liveRecorder.closeAll();
}

void MainWindow::appendLogLine(const QString &line)
{
    if (!m_logText || m_isClearingLog)
    {
        return;
    }
    auto *bar = m_logText->verticalScrollBar();
    const int oldScroll = bar ? bar->value() : 0;

    m_logText->appendPlainText(line);
    if (!bar)
    {
        return;
    }

    if (m_autoScrollEnabled)
    {
        bar->setValue(bar->maximum());
    }
    else
    {
        // 中文注释：QPlainTextEdit 追加文本会自动跟随到底部；暂停时强制恢复原滚动位置
        bar->setValue(oldScroll);
        ++m_pausedNewLogCount;
        if (m_btnPauseAutoScroll)
        {
            m_btnPauseAutoScroll->setText(
                QStringLiteral("恢复自动滚动 (%1)").arg(m_pausedNewLogCount));
        }
    }
}

MainWindow::~MainWindow()
{
    stopLiveRecording();
    // 中文注释：阶段 5 线程模型退出流程：先停控制，再 quit/wait 工作线程，最后析构对象
    if(m_ctrl && m_dataThread && m_dataThread->isRunning())
    {
        QMetaObject::invokeMethod(m_ctrl,[this](){
            m_ctrl->stop();
        },Qt::BlockingQueuedConnection);
    }

    // 中文注释：跨线程对象优先在其所属线程排队 deleteLater，降低退出阶段风险
    if (m_dataThread && m_dataThread->isRunning())
    {
        if (m_ctrl) { QMetaObject::invokeMethod(m_ctrl, "deleteLater", Qt::BlockingQueuedConnection); m_ctrl = nullptr; }
        if (m_sys) { QMetaObject::invokeMethod(m_sys, "deleteLater", Qt::BlockingQueuedConnection); m_sys = nullptr; }
        if (m_pcDev) { QMetaObject::invokeMethod(m_pcDev, "deleteLater", Qt::BlockingQueuedConnection); m_pcDev = nullptr; }
        if (m_fake) { QMetaObject::invokeMethod(m_fake, "deleteLater", Qt::BlockingQueuedConnection); m_fake = nullptr; }
    }
    if (m_logThread && m_logThread->isRunning() && m_logWorker)
    {
        QMetaObject::invokeMethod(m_logWorker, "deleteLater", Qt::BlockingQueuedConnection);
        m_logWorker = nullptr;
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

    // 中文注释：若线程未启动或 deleteLater 未执行，回退到直接释放
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
