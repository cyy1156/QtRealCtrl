#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "device/serialdevice.h"
#include "device/fakedevice.h"
#include "core/Sysinfo.h"
#include "core/controlmanager.h"
#include "core/LogWorker.h"
#include <QMainWindow>
#include <QTimer>
#include "algorithm/IAlgorithm.h"
#include <QThread>
#include <QMap>
#include <functional>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QVector>
#include <QStringList>
#include "file/liverecorder.h"
//这个类型别名用来注册算法工厂
using AlgCreator =std::function<IAlgorithm*(QObject*)>;
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class QCustomPlot;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
private:
    QTimer* m_clockTimer = nullptr;
    // 中文注释：阶段 5 引入工作线程后，这些对象需要先在 UI 线程构造，再 moveToThread 到数据线程
    SerialDevice* m_pcDev = nullptr;
    FakeDevice* m_fake = nullptr;
    SysInfo* m_sys = nullptr;
    ControlManager* m_ctrl = nullptr;
    IAlgorithm* m_currentAlg=nullptr;
    QMap<QString,AlgCreator> m_creator; // 算法创建工厂表
    QMap<QString,IAlgorithm*> m_algPool; //算法实例缓存（同名复用）

    // 中文注释：阶段 5：DataWorkerThread（数据处理）+ LogWorkerThread（日志/曲线）
    QThread* m_dataThread = nullptr;
    QThread* m_logThread = nullptr;
    LogWorker* m_logWorker = nullptr; // 中文注释：日志工作对象（线程 3）
    QTimer* m_softSimTimer = nullptr; // 中文注释：纯软件仿真定时器
    QCustomPlot* m_plot = nullptr; // 中文注释：UI 绘图控件（两条曲线：当前值/控制量）
    QDoubleSpinBox* m_targetInput = nullptr; // 中文注释：设定值可输入控件
    QPlainTextEdit* m_logText = nullptr; // 中文注释：底部日志显示区
    QPushButton* m_btnClearLog = nullptr; // 中文注释：清空日志按钮
    QPushButton* m_btnPauseAutoScroll = nullptr; // 中文注释：暂停自动滚动按钮
    bool m_autoScrollEnabled = true; // 中文注释：日志自动滚动开关
    bool m_isClearingLog = false; // 中文注释：清空日志期间抑制追加，避免“清空后立刻又出现一条”
    int m_pausedNewLogCount = 0; // 中文注释：暂停自动滚动期间新增日志条数
    int m_modeIndex = 2; // 中文注释：0=实时控制，1=串口仿真，2=纯软件仿真
    double m_softSimY = 0.0; // 中文注释：纯软件仿真内部输出状态
    double m_softSimU = 0.0; // 中文注释：纯软件仿真内部控制量
    double m_softSimLastU = 0.0; // 中文注释：纯软件仿真上一次控制量（用于增量模型）
    double m_softSimTarget = 0.0; // 中文注释：纯软件仿真目标值缓存（避免跨线程直接读 SysInfo）
    QVector<double> m_softSimOutArray; // 中文注释：纯软件仿真输出历史数组（用于迭代追踪）
    QVector<double> m_softSimModel; // 中文注释：纯软件仿真模型系数数组（类似旧工程 ModelArray）

private slots:
    void appendLogLine(const QString &line);

private:
    void refreshAvailableSerialPorts();
    void loadSerialSettings();
    void saveSerialSettings() const;
    void showSerialSettingsDialog();
    void updateSerialUiByMode(int modeIndex);
    void applySerialConfigsToDevices();
    void scheduleReconnect(const QString &reason);
    void attemptReconnect();
    void pollSerialPorts();
    bool startLiveCsvRecording();
    bool startLiveTxtRecording();
    void stopLiveCsvRecording();
    void stopLiveTxtRecording();
    void stopLiveRecording();
    void appendLiveCsvSample(const Sample &sample);
    void appendLiveTxtSample(const Sample &sample);
    void appendLiveRecordSample(const Sample &sample);

private:
    SerialPortConfig m_pcCfg;
    SerialPortConfig m_fakeCfg;
    QStringList m_availablePorts;
    bool m_autoReconnectEnabled = true;
    int m_reconnectIntervalMs = 1000;
    int m_maxReconnectRetry = 10;
    int m_readTimeoutMs = 50;
    int m_rxBufferBytes = 64 * 1024;
    int m_txBufferBytes = 64 * 1024;
    bool m_rawSerialLogEnabled = false;
    QTimer* m_serialPortPollTimer = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    int m_reconnectAttemptCount = 0;
    bool m_controlRunning = false;
    QString m_liveRecordRootDir;
    QString m_liveRecordFolderDir;
    QString m_liveCsvFilePath;
    QString m_liveCsvFileNamePattern = QStringLiteral("realtime_%TIMESTAMP%.csv");
    QString m_liveTxtFilePath;
    QString m_liveTxtFileNamePattern = QStringLiteral("realtime_%TIMESTAMP%.txt");
    LiveRecorder m_liveRecorder;
};
#endif // MAINWINDOW_H
