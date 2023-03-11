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

//��ʱ������ʱ��
#define DELAY_TIME 50

#define DOWNLOAD_FILE_SUFFIX "_tmp"

class FTPSocket : public QObject
{
    Q_OBJECT

private:
    //ftp����������״̬��
    enum FTPOperation {
        FTP_LISTFile = 0,
        FTP_DOWNFile,
        FTP_SegmentedDownload,
        FTP_UPLOADFile,
        FTP_GETFILESize
    };

    //ftp����ģʽ
    enum FTPMODE {
        MODE_Stream = 0,  //��ģʽ����д���ļ���ֱ�������׳��������������� (���ʹ�÷ֶ��߳����أ���ģʽ��Ч��������Ϊ MODE_ThreadStream)
        MODE_FILE,  //�ļ�ģʽ�������ص���д�뱾���ļ��� Ĭ���ļ�ģʽ
        MODE_ThreadStream  //�߳���ģʽ����д���ļ���ֱ�������׳���������������,ʹ�÷ֶ��߳�����ʱ���ô�ģʽ����� ftpStartSegmentedDownload ����
    };

    // ����data�˿�
    void listenFtpDataPort(quint16 port);

    // �и�PASV���ص����ݣ���������ݶ˿ں�
    int splitPasvIpPort(const QString& PASVPassiveStr);

    //��ʽ����ȡ�ļ��Ĵ�С
    QPair<qint64, QString> formatFileSize(const qint64 &filesizebyte, int precision = 2);

public:
    explicit FTPSocket(QObject *parent = nullptr);
    ~FTPSocket();

    // �����û���������
    void setFtpUserNamePasswd(QByteArray userName, QByteArray passwd);

    // ���Ӻ���
    void connectFtpServer(QString hostName, quint16 port = 21);

    // �г����������е�ftp�ļ����е��ļ�����,����׺
    QStringList listFtpFile();

    // ��ȡftp�ļ���С
    qint64 ftpFileSize(const QString &fileName);

    // ����FTP�ļ�
    void downloadFtpFile(const QString &fileName, QString &localPwd);

    //�����ļ������������ر�ftp����(ǿ�ҽ���ʹ�ô˺���)
    void saveDownloadFile();

    // �ر�ftp����
    void closeFTPConnect(bool synchronization = true);

    //�л�ftpĿ¼, dirNameĿ¼������
    void FtpCWD(QString dirName);

    // ��ӡ��ǰftp����Ŀ¼
    void FtpPWD();

public:
    QTcpSocket *m_FtpServerSocket = nullptr; //����ͨ��
    QTcpSocket *m_FtpDataSocket = nullptr; //����ͨ��

    QString currentDir = "/"; //ftp��ǰĿ¼

public slots:
    void pasvMode(); //����FTPΪ����ģʽ
    void onFtpConnectFinish();  //������ftp�����������

    virtual void onReadData();  //������ͨ�����Ӷ�ȡ����
    virtual void onFtpDataRead(); //����ͨ�����Ӷ�ȡ����

    void writeCommand(const QByteArray &comm, bool synchronization = true); //д��ftp����
    void writeCommand(const QString &comm, bool synchronization = true);  //д��ftp����

private:
    QByteArray m_ftpUserName; //��¼��ǰ��ftp�û���
    QByteArray m_ftpPasswd; //��¼��ǰ��ftp����
    bool m_loginSuccess = false; //�Ƿ��¼�ɹ��ı��
    bool m_isFileDownFinish = false;  //�ж��ļ��Ƿ��Ѿ��������

    int m_reconnections = 0;  //�����ӵĴ���

    QString m_hostName;  //��¼��¼����������
    quint16 m_hostPort; //��¼�����Ķ˿ں�

    QStringList m_ftpFileList;  //��ȡftp���ļ��б�

    QString m_dwfileName;  //��ǰ�����ļ�������
    QString m_dwfilesuffix; //�����ļ��ĺ�׺
    QFile *m_pFile = nullptr; //���������ļ����浽���ص��ļ�ָ��
    QByteArray m_fileCacheData = ""; //�ļ���������ݴ洢����Ҫ���ر��ļ���ʱ��д���ļ�
    qint64 m_fileCacheOffset = 0;  //�ļ���ƫ�������ϵ��������õ�
    QPair<qint64, QString>  m_dwFileSizePair; //�����ļ��Ĵ�С, ��һ���Ǵ���С���ڶ�����ת����Ĵ�С�ӵ�λ

    FTPOperation m_ftpEnumStatus;   //ftp������ö��״̬��
    FTPMODE m_ftpEnumMode = FTPSocket::MODE_FILE; //ftpģʽ

signals:
    void sigFTPConnectFinish(); //ftp������ɴ���
    void sigFTPCommandFinish(); //ftp������ɴ���
    void sigFTPConnectFailed(); //ftp����ʧ�ܴ���
    void sigFTPReconnectFailed(); //ftp����ʧ�ܴ���
    void sigFTPListFileFinish(); //ftp�б��ļ���ȡ��ɴ���
    void sigFTPPasvFinish(); //�л�PASVģʽ��ɴ���
    void sigFTPCloseConnect(); //ftp���ӹر�ʱ����
    void sigFTPCWDSuccess();   //Ŀ¼�л��ɹ��󴥷�
    void sigDownloadComplete(); //������غ󴥷�
    void sigDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void sigFTPDownloadFinish(QString filePath); //ftp������ɲ�����ɹ�����
    void sigFTPStreamFileFinish(QByteArray fileStream); //ftp������ɡ������ڴ����ź�
    void sigFTPPWDFinish(QString dirName); //�鿴��ǰĿ¼�ɹ��󴥷�

    //socketд�ź�
    void sigFtpSocketWrite(const QByteArray &socketData);
};

#endif