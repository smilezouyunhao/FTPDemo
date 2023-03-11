#pragma once
#pragma execution_character_set("utf-8")
#ifndef FTPSOCKET_H
#define FTPSOCKET_H

#include <QObject>
#include <QDir>
#include <QTimer>
#include <QPair>
#include <QThread>
#include <QTcpSocket>
#include <QEventLoop>
#include <QtNetwork>
#include <QStandardPaths>
#include <QNetworkProxy>

//定时器触发时间
#define DELAY_TIME 50

#define DOWNLOAD_FILE_SUFFIX "_tmp"

class FTPSocket : public QObject
{
    Q_OBJECT

private:
    //ftp操作的流程状态码
    enum FTPOperation {
        FTP_LISTFile = 0,
        FTP_DOWNFile,
        FTP_SegmentedDownload,
        FTP_UPLOADFile,
        FTP_GETFILESize
    };

    //ftp操作模式
    enum FTPMODE {
        MODE_Stream = 0,  //流模式，不写入文件，直接向外抛出，由外面做处理 (如果使用分段线程下载，此模式无效，请设置为 MODE_ThreadStream)
        MODE_FILE,  //文件模式，将下载的流写入本地文件， 默认文件模式
        MODE_ThreadStream  //线程流模式，不写入文件，直接向外抛出，由外面做处理,使用分段线程下载时设置此模式，详见 ftpStartSegmentedDownload 函数
    };

    // 监听data端口
    void listenFtpDataPort(quint16 port);

    // 切割PASV返回的数据，计算出数据端口号
    int splitPasvIpPort(const QString& PASVPassiveStr);

    //格式化获取文件的大小
    QPair<qint64, QString> formatFileSize(const qint64 &filesizebyte, int precision = 2);

public:
    explicit FTPSocket(QObject *parent = nullptr);
    ~FTPSocket();

    // 设置用户名和密码
    void setFtpUserNamePasswd(QByteArray userName, QByteArray passwd);

    // 连接函数
    void connectFtpServer(QString hostName, quint16 port = 21);

    // 列出并返回所有的ftp文件夹中的文件名称,带后缀
    QStringList listFtpFile();

    // 获取ftp文件大小
    qint64 ftpFileSize(const QString &fileName);

    // 下载FTP文件
    void downloadFtpFile(const QString &fileName, QString &localPwd);

    //保存文件的输入流并关闭ftp连接(强烈建议使用此函数)
    void saveDownloadFile();

    // 关闭ftp连接
    void closeFTPConnect(bool synchronization = true);

    //切换ftp目录, dirName目录的名称
    void FtpCWD(QString dirName);

    // 打印当前ftp所在目录
    void FtpPWD();

public:
    QTcpSocket *m_FtpServerSocket = nullptr; //服务通道
    QTcpSocket *m_FtpDataSocket = nullptr; //数据通道

    QString currentDir = "/"; //ftp当前目录

public slots:
    void pasvMode(); //设置FTP为被动模式
    void onFtpConnectFinish();  //连接上ftp服务器后调用

    virtual void onReadData();  //服务器通道连接读取数据
    virtual void onFtpDataRead(); //数据通道连接读取数据

    void writeCommand(const QByteArray &comm, bool synchronization = true); //写入ftp命令
    void writeCommand(const QString &comm, bool synchronization = true);  //写入ftp命令

private:
    QByteArray m_ftpUserName; //记录当前的ftp用户名
    QByteArray m_ftpPasswd; //记录当前的ftp密码
    bool m_loginSuccess = false; //是否登录成功的标记
    bool m_isFileDownFinish = false;  //判断文件是否已经下载完成

    int m_reconnections = 0;  //重连接的次数

    QString m_hostName;  //记录登录的主机名称
    quint16 m_hostPort; //记录主机的端口号

    QStringList m_ftpFileList;  //获取ftp的文件列表

    QString m_dwfileName;  //当前下载文件的名称
    QString m_dwfilesuffix; //下载文件的后缀
    QFile *m_pFile = nullptr; //操作下载文件保存到本地的文件指针
    QByteArray m_fileCacheData = ""; //文件缓存的数据存储，需要最后关闭文件的时候写入文件
    qint64 m_fileCacheOffset = 0;  //文件的偏移量，断点续传会用到
    QPair<qint64, QString>  m_dwFileSizePair; //下载文件的大小, 第一个是纯大小，第二个是转换后的大小加单位

    FTPOperation m_ftpEnumStatus;   //ftp操作的枚举状态码
    FTPMODE m_ftpEnumMode = FTPSocket::MODE_FILE; //ftp模式

signals:
    void sigFTPConnectFinish(); //ftp连接完成触发
    void sigFTPCommandFinish(); //ftp命令完成触发
    void sigFTPConnectFailed(); //ftp连接失败触发
    void sigFTPReconnectFailed(); //ftp重连失败触发
    void sigFTPListFileFinish(); //ftp列表文件获取完成触发
    void sigFTPPasvFinish(); //切换PASV模式完成触发
    void sigFTPCloseConnect(); //ftp连接关闭时触发
    void sigFTPCWDSuccess();   //目录切换成功后触发
    void sigDownloadComplete(); //完成下载后触发
    void sigDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void sigFTPDownloadFinish(QString filePath); //ftp下载完成并保存成功触发
    void sigFTPStreamFileFinish(QByteArray fileStream); //ftp下载完成。发送内存流信号
    void sigFTPPWDFinish(QString dirName); //查看当前目录成功后触发

    //socket写信号
    void sigFtpSocketWrite(const QByteArray &socketData);
};

#endif