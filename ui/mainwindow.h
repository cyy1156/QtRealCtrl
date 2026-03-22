#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "device/serialdevice.h"
#include "device/fakedevice.h"
#include "core/Sysinfo.h"
#include "core/controlmanager.h"
#include <QMainWindow>
#include <QTimer>
#include "algorithm/IAlgorithm.h"
#include <QMap>
#include <functional>
//这个类型别名用来注册算法工厂
using AlgCreator =std::function<IAlgorithm*(QObject*)>;
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

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
    SerialDevice* m_pcDev;
    FakeDevice* m_fake ;
    SysInfo* m_sys;
    ControlManager* m_ctrl ;
    IAlgorithm* m_currentAlg=nullptr;
    QMap<QString,AlgCreator> m_creator; // 算法创建工厂表
    QMap<QString,IAlgorithm*> m_algPool; //算法实例缓存（同名复用）


};
#endif // MAINWINDOW_H
