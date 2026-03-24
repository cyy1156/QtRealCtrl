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
    QCustomPlot* m_plot = nullptr; // 中文注释：UI 绘图控件（两条曲线：当前值/控制量）


};
#endif // MAINWINDOW_H
