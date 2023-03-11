#pragma once
#pragma execution_character_set("utf-8")

#include <QtWidgets/QMainWindow>
#include "ui_MyDemo.h"
#include "FTPSocket.h"

#include <QDebug>

QT_BEGIN_NAMESPACE
namespace Ui { class MyDemoClass; };
QT_END_NAMESPACE

class MyDemo : public QMainWindow
{
    Q_OBJECT

public:
    MyDemo(QWidget *parent = nullptr);
    ~MyDemo();

public:
    // 初始化，ftp连接
    void initialize();

    // 释放
    void release();

    // 信号与槽连接
    void sigAndSlotConnect();

    // 初始化两个TreeWidget
    void initTreeWidget();

    // 将远程目录打印到TreeWidget
    void printTreeRemote(QStringList &list);
    // 将本地目录打印到TreeWidget
    void printTreeLocal(QStringList &list);

    // 获取本地目录
    QStringList listLocalFile();

public slots:
    void on_login_clicked(); //连接按键的槽函数
    void on_exit_clicked(); //断开连接按键的槽函数
    void on_download_clicked(); //下载按键的槽函数
    void on_local_uplevel_clicked(); //本地目录上一层按键的槽函数
    void on_remote_uplevel_clicked(); //远程目录上一层按键的槽函数
    void double_local_changePath_clicked(QTreeWidgetItem *item, int column); //本地切换目录的槽函数
    void double_remote_changePath_clicked(QTreeWidgetItem* item, int column); //远程切换目录的槽函数

private:
    Ui::MyDemoClass *ui;

    FTPSocket *p_ftp;
    QTreeWidget* p_treeW_Remote;
    QTreeWidget* p_treeW_Local;

    QString userName;
    QString hostName;
    QString password;
    QString localPwd;
    QString lo_rootPwd;
    QString re_rootPwd = "/";
    QDir local_dir;
    quint16 port;

    QStringList list_ftp;
    QStringList list_local;
};
