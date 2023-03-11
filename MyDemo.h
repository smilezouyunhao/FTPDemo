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
    // ��ʼ����ftp����
    void initialize();

    // �ͷ�
    void release();

    // �ź��������
    void sigAndSlotConnect();

    // ��ʼ������TreeWidget
    void initTreeWidget();

    // ��Զ��Ŀ¼��ӡ��TreeWidget
    void printTreeRemote(QStringList &list);
    // ������Ŀ¼��ӡ��TreeWidget
    void printTreeLocal(QStringList &list);

    // ��ȡ����Ŀ¼
    QStringList listLocalFile();

public slots:
    void on_login_clicked(); //���Ӱ����Ĳۺ���
    void on_exit_clicked(); //�Ͽ����Ӱ����Ĳۺ���
    void on_download_clicked(); //���ذ����Ĳۺ���
    void on_local_uplevel_clicked(); //����Ŀ¼��һ�㰴���Ĳۺ���
    void on_remote_uplevel_clicked(); //Զ��Ŀ¼��һ�㰴���Ĳۺ���
    void double_local_changePath_clicked(QTreeWidgetItem *item, int column); //�����л�Ŀ¼�Ĳۺ���
    void double_remote_changePath_clicked(QTreeWidgetItem* item, int column); //Զ���л�Ŀ¼�Ĳۺ���

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
