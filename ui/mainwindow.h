#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "device/serialdevice.h"
#include "device/fakedevice.h"
#include "core/Sysinfo.h"
#include "core/controlmanager.h"
#include <QMainWindow>
#include <QTimer>
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
};
#endif // MAINWINDOW_H
