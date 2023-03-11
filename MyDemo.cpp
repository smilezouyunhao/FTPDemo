#include "MyDemo.h"
#include "FTPSocket.h"


MyDemo::MyDemo(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MyDemoClass())
{
    ui->setupUi(this);

    QTextCodec *codec = QTextCodec::codecForName("UTF8");
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    initTreeWidget();
    sigAndSlotConnect();
}

MyDemo::~MyDemo()
{
    release();
}

void MyDemo::initialize()
{
    hostName = ui->le_host->text();
    port = ui->le_port->text().toInt();
    userName = ui->le_username->text();
    password = ui->le_password->text();
    // ftp
    p_ftp = new FTPSocket;
    p_ftp->setFtpUserNamePasswd(userName.toLatin1(), password.toLatin1());
    p_ftp->connectFtpServer(hostName, port);

    // rootPwd
    lo_rootPwd = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/downFile";
    qInfo() << "rootPwd:" << lo_rootPwd;
}

void MyDemo::release()
{
    delete ui;
    delete p_ftp;
    delete p_treeW_Remote;
    delete p_treeW_Local;
}

void MyDemo::sigAndSlotConnect()
{
    connect(ui->btn_login, &QPushButton::clicked, this, &MyDemo::on_login_clicked);
    connect(ui->btn_exit, &QPushButton::clicked, this, &MyDemo::on_exit_clicked);
    connect(ui->btn_download, &QPushButton::clicked, this, &MyDemo::on_download_clicked);
    connect(ui->treew_local, &QTreeWidget::itemDoubleClicked, this, &MyDemo::double_local_changePath_clicked);
    connect(ui->treew_remote, &QTreeWidget::itemDoubleClicked, this, &MyDemo::double_remote_changePath_clicked);
    connect(ui->btn_louplevel, &QPushButton::clicked, this, &MyDemo::on_local_uplevel_clicked);
    connect(ui->btn_reuplevel, &QPushButton::clicked, this, &MyDemo::on_remote_uplevel_clicked);
    
}

void MyDemo::on_download_clicked()
{
    QTreeWidgetItem *current = p_treeW_Remote->currentItem();
    QString strFile = current->text(0);
    p_ftp->downloadFtpFile(strFile, localPwd);
    QEventLoop loop;
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();
    p_ftp->saveDownloadFile();
    ui->lb_message->setText("Download Complete");
}

void MyDemo::on_local_uplevel_clicked()
{
    if (local_dir.path() == lo_rootPwd)
    {
        qInfo() << "目前已在根目录下，无法往上级移动";
        return;
    }
    local_dir.cdUp();
    list_local = local_dir.entryList(QDir::AllEntries);
    list_local.removeOne(".");
    list_local.removeOne("..");

    p_treeW_Local->clear();
    printTreeLocal(list_local);
}

void MyDemo::on_remote_uplevel_clicked()
{
    if (p_ftp->currentDir == re_rootPwd)
    {
        qInfo() << "已在根目录下，无法往上级移动";
        return;
    }

    p_ftp->FtpCWD("..");
    list_ftp = p_ftp->listFtpFile();
    p_treeW_Remote->clear();
    printTreeRemote(list_ftp);
}

void MyDemo::double_local_changePath_clicked(QTreeWidgetItem* item, int column)
{
    localPwd = local_dir.path();
    localPwd = localPwd + "/" + p_treeW_Local->currentItem()->text(0);
    qInfo() << "需要打开的文件夹:" << localPwd;
    QFileInfo info(localPwd);
    if (!info.isDir())
    {
        qInfo() << "非文件夹，无法打开";
        return;
    }

    local_dir.cd(p_treeW_Local->currentItem()->text(0));
    list_local = local_dir.entryList(QDir::AllEntries);
    list_local.removeOne(".");
    list_local.removeOne("..");

    p_treeW_Local->clear();
    printTreeLocal(list_local);
}

void MyDemo::double_remote_changePath_clicked(QTreeWidgetItem *item, int column)
{
    QString dirName = p_treeW_Remote->currentItem()->text(0);
    qInfo() << "dirName:" << dirName;
    p_ftp->FtpCWD(dirName);

    p_treeW_Remote->clear();
    list_ftp = p_ftp->listFtpFile();
    printTreeRemote(list_ftp);
}

QStringList MyDemo::listLocalFile()
{
    localPwd = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    localPwd += "/downFile";

    // 检测临时文件夹里是否存在downFile文件夹，如果没有则创建
    local_dir.setPath(localPwd);
    if (!local_dir.exists())
    {
        if (!local_dir.mkpath(localPwd))
        {
            qInfo() << "文件夹创建失败" << localPwd;
        }
    }
    list_local = local_dir.entryList(QDir::AllEntries);
    list_local.removeOne(".");
    list_local.removeOne("..");

    return list_local;
}

void MyDemo::initTreeWidget()
{
    p_treeW_Remote = ui->treew_remote;
    p_treeW_Local = ui->treew_local;
    p_treeW_Remote->setHeaderLabel(QString("远程文件目录"));
    p_treeW_Local->setHeaderLabel(QString("本地文件目录"));
}

void MyDemo::printTreeRemote(QStringList& list)
{
    p_treeW_Remote->clear();
    int index = 0;
    for (auto str : list)
    {
        p_treeW_Remote->addTopLevelItem(new QTreeWidgetItem(QStringList() << str));
        index++;
    }
    index = 0;
}

void MyDemo::printTreeLocal(QStringList& list)
{
    p_treeW_Local->clear();
    int index = 0;
    for (auto str : list)
    {
        p_treeW_Local->addTopLevelItem(new QTreeWidgetItem(QStringList() << str));
        index++;
    }
    index = 0;
}

void MyDemo::on_exit_clicked()
{
    p_treeW_Remote->clear();
    p_treeW_Local->clear();
    p_ftp->closeFTPConnect();
}

void MyDemo::on_login_clicked()
{
    initialize();
    list_ftp = p_ftp->listFtpFile();
    list_local = listLocalFile();
    printTreeRemote(list_ftp);
    printTreeLocal(list_local);
}
