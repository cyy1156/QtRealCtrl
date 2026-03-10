#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // connect(ui->btn_start,&QPushButton::clicked,this,&MainWindow::onStartClicked);
    // connect(ui->btn_Clear,&QPushButton::clicked,this,&MainWindow::onStartClicked);
    // connect(ui->btn_Stop,&QPushButton::clicked,this,&MainWindow::onStartClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}
