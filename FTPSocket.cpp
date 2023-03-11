#include "FTPSocket.h"

#define CHAR_CR "\r\n"

FTPSocket::FTPSocket(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<QPair<qint64, qint64>>("QPair<qint64, qint64>");
}

FTPSocket::~FTPSocket()
{
    qInfo() << "���� FTP";
    if (m_FtpServerSocket)
    {
        m_FtpServerSocket->close();
        m_FtpServerSocket->deleteLater();
        m_FtpServerSocket = nullptr;
    }

    if (m_FtpDataSocket)
    {
        m_FtpDataSocket->close();
        m_FtpDataSocket->deleteLater();
        m_FtpDataSocket = nullptr;
    }
}

void FTPSocket::setFtpUserNamePasswd(QByteArray userName, QByteArray passwd)
{
    m_ftpUserName = "USER " + userName + CHAR_CR;
    m_ftpPasswd = "PASS " + passwd + CHAR_CR;
}

void FTPSocket::connectFtpServer(QString hostName, quint16 port)
{
    if (m_FtpServerSocket == nullptr)
    {
        m_FtpServerSocket = new QTcpSocket;
    }
    else
    {
        return;
    }

    m_hostName = hostName;
    m_hostPort = port;
    m_FtpServerSocket->abort();

    m_FtpServerSocket->connectToHost(m_hostName, port);

    connect(m_FtpServerSocket, &QTcpSocket::connected, this, &FTPSocket::onFtpConnectFinish);
    connect(m_FtpServerSocket, &QTcpSocket::readyRead, this, &FTPSocket::onReadData);
    connect(this, QOverload<const QByteArray&>::of(&FTPSocket::sigFtpSocketWrite), m_FtpServerSocket, QOverload<const QByteArray&>::of(&QTcpSocket::write));

    connect(m_FtpServerSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this,
        [=](QAbstractSocket::SocketError socketError) {

            qInfo() << "socketError" << socketError;
            if (socketError == QAbstractSocket::RemoteHostClosedError)
            {
                return;
            }
            emit sigFTPConnectFailed();

        });
    qInfo() << "״̬:" << m_loginSuccess;
}

QStringList FTPSocket::listFtpFile()
{
    qInfo() << "����״̬: " << m_loginSuccess;
    m_ftpEnumStatus = FTPSocket::FTP_LISTFile;

    if (m_loginSuccess)
    {
        m_reconnections = 0;

        pasvMode();

        writeCommand(QString("NLST\r\n"));

        return m_ftpFileList;
    }
    else
    {
        // �����������Ϊ�գ�˵����¼������������
        if (!m_hostName.isEmpty())
        {
            // �����������ᣬ�����ʧ�ܣ���������
            if (m_reconnections > 3)
            {
                emit sigFTPReconnectFailed();
                return QStringList("");
            }

            m_reconnections++;

            // ��ʼ������ֻ�е�¼״̬Ϊfalseʱ�ŵ��ã�����������жϣ�������listFtpFile����̫�죬�����Ѿ���¼�ɹ�����ʱ
            if (!m_loginSuccess)
            {
                connectFtpServer(m_hostName, m_hostPort);

                QEventLoop loop;
                connect(this, &FTPSocket::sigFTPConnectFinish, &loop, &QEventLoop::quit);
                loop.exec();
            }

            // �ݹ����
            return listFtpFile();
        }
    }
    return m_ftpFileList;
}

void FTPSocket::pasvMode()
{
    QByteArray  comm = "PASV\r\n";
    emit sigFtpSocketWrite(comm);
    
    QEventLoop loop;
    connect(this, &FTPSocket::sigFTPPasvFinish, &loop, &QEventLoop::quit);
    loop.exec();
}

int FTPSocket::splitPasvIpPort(const QString& PASVPassiveStr)
{
    int ip = 0;
    QString str = PASVPassiveStr;

    if (str.contains("("))
    {
        QStringList strlist = str.split("(");

        if (strlist.length() >= 2)
        {
            strlist = strlist.at(1).split(")");
            QString ipString = strlist.at(0);
            //qInfo() << "IP����ֵ: " << ipString;
            strlist = ipString.split(",");
            ip = strlist.at(4).toInt();
            int ip2 = strlist.at(5).toInt();

            ip = ip * 256 + ip2;
        }
    }

    return ip;
}

QPair<qint64, QString> FTPSocket::formatFileSize(const qint64 &filesizebyte, int precision)
{
    QPair<qint64, QString> sizePair;
    if (filesizebyte == 0)
    {
        sizePair.first = 0;
        sizePair.second = "0B";
        return sizePair;
    }
    
    double sizeAsDouble = filesizebyte;
    QStringList measures = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};

    int index = 0;
    while (sizeAsDouble >= 1024.0)
    {
        index++;
        sizeAsDouble /= 1024.0;
    }

    QString fileSize = QString::number(sizeAsDouble, 'f', precision) + measures[index];

    sizePair.first = static_cast<qint64>(filesizebyte);
    sizePair.second = fileSize;
    return sizePair;
}

qint64 FTPSocket::ftpFileSize(const QString& fileName)
{
    pasvMode();
    writeCommand(QString("SIZE %0\r\n").arg(fileName));

    qInfo() << "FtpFileSize: " << m_dwFileSizePair.second;
    return m_dwFileSizePair.first;
}

void FTPSocket::downloadFtpFile(const QString &fileName, QString &localPwd)
{
    // ��ȡ�ļ���С
    ftpFileSize(fileName);

    // ����״̬
    m_ftpEnumStatus = FTPSocket::FTP_DOWNFile;

    qInfo() << "�ļ��ĺ�׺:" << m_dwfilesuffix;
    writeCommand(QByteArray("TYPE I\r\n"), false);

    // ������ļ�ģʽ������ļ�Ԥ����
    if (m_ftpEnumMode == FTPSocket::MODE_FILE)
    {
        qInfo() << "�ļ�ģʽ�����ļ�";

        // �������ص�λ��
        // ���λ���Ǳ���ϵͳ����ʱ�ļ���
        QString dirPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        dirPath = dirPath + "/downFile";
        qInfo() << "dirPath:" << dirPath;
        
        // �����ʱ�ļ������Ƿ����downFile�ļ��У����û���򴴽�
        QDir dir(dirPath);
        if (!dir.exists())
        {
            if (!dir.mkpath(dirPath))
            {
                qInfo() << "����·������ʧ��" << dirPath;
            }
        }

        QFileInfo fileInfo(fileName);

        m_dwfileName = fileInfo.fileName();
        m_dwfilesuffix = fileInfo.suffix();

        m_dwfileName.remove(m_dwfilesuffix);

        m_dwfileName = m_dwfileName + DOWNLOAD_FILE_SUFFIX;

        qInfo() << "׼�����ص��ļ�������" << m_dwfileName;

        QFileInfo info;
        localPwd = localPwd + "/" + m_dwfileName;
        info.setFile(dir, localPwd);
        if (m_pFile == nullptr)
        {
            m_pFile = new QFile(info.filePath());
        }
        qInfo() << "localPwd:" << localPwd;

        m_fileCacheOffset = info.size();
        qInfo() << "�����ļ���С:" << m_fileCacheOffset;
        qInfo() << "�����ļ��Ĵ�С:" << m_dwFileSizePair.first;
        if (m_fileCacheOffset > 0 && m_fileCacheOffset != m_dwFileSizePair.first)
        {
            m_pFile->open(QIODevice::Append);
            qInfo() << "�����ϵ���������";
            writeCommand(QString("REST %1\r\n").arg(m_fileCacheOffset));
        }
        else
        {
            if (!m_pFile->open(QIODevice::WriteOnly))
            {
                qInfo() << "�ļ���ʧ��";
            }
        }
    }

    pasvMode();

    qInfo() << "׼�����ص��ļ�:" << fileName;
    writeCommand(QString("RETR %1\r\n").arg(fileName));
}

void FTPSocket::saveDownloadFile()
{
    if (m_ftpEnumMode == FTPSocket::MODE_Stream)
    {
        emit sigFTPStreamFileFinish(m_fileCacheData);
    }
    else
    {
        if (m_pFile)
        {
            if (m_pFile->isOpen())
            {
                qInfo() << "�ļ��򿪣�д���ļ�";
                m_pFile->write(m_fileCacheData);
                m_pFile->flush();
                m_pFile->close();
                m_isFileDownFinish = true;
            }

            if (m_isFileDownFinish)
            {
                m_isFileDownFinish = false;
                QFileInfo fileInfo(*m_pFile);
                QString filePath = fileInfo.absolutePath();
                qInfo() << "filePath:" << filePath;
                QString fileOldSuffix = fileInfo.suffix();
                qInfo() << "fileOldSuffix:" << fileOldSuffix;
                m_dwfileName = m_dwfileName.remove(fileOldSuffix);
                qInfo() << "m_dwfileName:" << m_dwfileName;
                QString fileNewName = filePath + "/" + m_dwfileName + m_dwfilesuffix;
                qInfo() << "fileNewName:" << fileNewName;
                qInfo() << "oldName:" << m_pFile->fileName();

                // ����ļ��Ѵ��ڣ��򸲸�
                if (QFile::exists(fileNewName))
                {
                    QFile::remove(fileNewName);
                }
                
                m_pFile->rename(m_pFile->fileName(), fileNewName);

                emit sigFTPDownloadFinish(fileNewName);
            }
        }
        delete m_pFile;
        m_pFile = nullptr;
    }

    ////�ر�FTP�����ӣ���ʱ500����
    //QTimer::singleShot(DELAY_TIME, this, [=] {
    //    qInfo() << "�����ر�Ftp.";
    //    closeFTPConnect();
    //});
}

void FTPSocket::listenFtpDataPort(quint16 port)
{
    if (m_FtpDataSocket == nullptr)
    {
        m_FtpDataSocket = new QTcpSocket();
    }

    m_FtpDataSocket->close();
    m_FtpDataSocket->abort();
    m_FtpDataSocket->connectToHost(m_hostName, port);

    connect(m_FtpDataSocket, &QTcpSocket::readyRead, this, &FTPSocket::onFtpDataRead, Qt::DirectConnection);

    emit sigFTPPasvFinish();
}

void FTPSocket::closeFTPConnect(bool synchronization)
{
    //qInfo() << "�ر�FTP���ӣ�isLoginSuccess״̬Ϊ" << m_loginSuccess;
    // ���û�����ӣ�ֱ�ӷ���
    if (!m_loginSuccess)
    {
        return;
    }
    if (m_FtpServerSocket)
    {
        m_loginSuccess = false;
        writeCommand(QByteArray("QUIT\r\n"), synchronization);
    }
}

void FTPSocket::FtpCWD(QString dirName)
{
    writeCommand(QString("CWD %0\r\n").arg(dirName));
    FtpPWD();
}

void FTPSocket::FtpPWD()
{
    writeCommand(QString("PWD\r\n"));
}

void FTPSocket::onReadData()
{
    QByteArray array = m_FtpServerSocket->readAll();
    //qInfo() << "ftpServer���յ�������: " << array;

    if (array.contains("230 User") || array.contains("230 Login successful") || array.contains("230 User logged in"))
    {
        emit this->sigFTPCommandFinish();
        QTimer::singleShot(DELAY_TIME, this, [=] {
            //qInfo() << "��¼�ɹ�";
            m_loginSuccess = true;
            emit sigFTPConnectFinish();
            });
    }
    else if (array.contains("331 Please specify the password") || array.contains("331 Password required") || array.contains("331 Anonymous access allowed, send identity (e-mail name) as password"))
    {
        emit this->sigFTPCommandFinish();

        //qInfo() << "��������";
        QTimer::singleShot(DELAY_TIME, this, [=] {
            writeCommand(m_ftpPasswd);
        });
    }
    else if (array.contains("213"))
    {
        QString filesize(array);
        filesize = filesize.simplified();
        filesize = filesize.mid(4);
        m_dwFileSizePair = formatFileSize(filesize.toLongLong());
        
        emit this->sigFTPCommandFinish();
    }
    else if (array.contains("257"))
    {
        int indexA = 5;
        int indexB = array.indexOf("is");
        currentDir = array.mid(indexA, indexB - indexA - 2);
        //qInfo() << "��ǰ��·��Ϊ: " << currentDir;
        emit this->sigFTPCommandFinish();

        QTimer::singleShot(DELAY_TIME, this, [=] {
            //qInfo() << "��ǰ��·��Ϊ: " << currentDir;
            emit sigFTPPWDFinish(currentDir);
        });
    }
    // �л�Ŀ¼�ɹ�
    else if (array.contains("250 CWD"))
    {
        emit this->sigFTPCommandFinish();
        QTimer::singleShot(DELAY_TIME, this, [=] {
            emit sigFTPCWDSuccess();
        });
    }
    else if (array.contains("227 Entering Passive Mode"))
    {
        emit this->sigFTPCommandFinish();

        QTimer::singleShot(DELAY_TIME, this, [=] {
            //qInfo() << "׼�����ӵ�����ͨ��";
            listenFtpDataPort(static_cast<quint16>(splitPasvIpPort(array)));
        });
    }
    else if (array.contains("221 Goodbye") || array.contains("221 goodbye"))
    {
        emit this->sigFTPCommandFinish();
        QTimer::singleShot(DELAY_TIME, this, [=] {
            if (m_FtpServerSocket)
            {
                m_FtpServerSocket->close();
                m_FtpServerSocket->deleteLater();
                m_FtpServerSocket = nullptr;
            }

            if (m_FtpDataSocket)
            {
                m_FtpDataSocket->close();
                m_FtpDataSocket->deleteLater();
                m_FtpDataSocket = nullptr;
            }

            // �ӳٴ����ر��ź�
            emit sigFTPCloseConnect();
        });
    }
    else if (array.contains("226 Transfer complete"))
    {
        // ������ɺ󴥷��źţ�����ͬ�����
        emit this->sigFTPCommandFinish();

        QTimer::singleShot(DELAY_TIME, this, [=] {
            //m_isFileDownFinish = true;
            if (m_ftpEnumMode != FTPMODE::MODE_ThreadStream)
            {
                saveDownloadFile();
            }
        });
    }
    else if (array.contains("550-The filename, directory name, or volume label syntax is incorrect"))
    {
        qInfo() << "Ҫ�л���Ŀ¼Ϊ���ļ��У��л�ʧ��";
        emit sigFTPCommandFinish();
    }
}

void FTPSocket::onFtpDataRead()
{
    QByteArray array = m_FtpDataSocket->readAll();

    if (array.isEmpty())
    {
        qInfo() << "����ͨ���е�����Ϊ��";
        return;
    }
    qInfo() << "����ͨ�����յ�������:" << array;

    QString ftpFileStr;
    QPair<qint64, QString> pairFileSize;

    switch (static_cast<int>(m_ftpEnumStatus))
    {
    case FTPSocket::FTP_LISTFile:
        //qInfo() << "FTPList ����";
        ftpFileStr = QString(array);
        m_ftpFileList = ftpFileStr.split("\r\n");
        m_ftpFileList.removeAll("");

        emit sigFTPListFileFinish();
        break;

    case FTPSocket::FTP_DOWNFile:
        m_fileCacheData += array;

        pairFileSize = formatFileSize(m_fileCacheOffset+m_fileCacheData.length());
        
        if (pairFileSize.first == m_dwFileSizePair.first)
        {
            emit sigDownloadComplete();
        }

        if (m_ftpEnumMode == FTPSocket::MODE_FILE)
        {
            emit sigDownloadProgress(pairFileSize.first, m_dwFileSizePair.first);
        }

        break;
    }
}

void FTPSocket::writeCommand(const QByteArray &comm, bool synchronization)
{
    qInfo() << "��ʼ comm" << comm;
    
    emit sigFtpSocketWrite(comm);
    if (synchronization)
    {
        QEventLoop loop;
        connect(this, &FTPSocket::sigFTPConnectFinish, &loop, &QEventLoop::quit);

        // 1s֮���˳�ȥ
        QTimer::singleShot(500, &loop, &QEventLoop::quit);
        loop.exec();
    }
}

void FTPSocket::writeCommand(const QString &comm, bool synchronization)
{
    QString strComm = comm;
    QByteArray commArray = strComm.toUtf8();
    writeCommand(commArray, synchronization);
}

void FTPSocket::onFtpConnectFinish()
{
    //qInfo() << "���ӳɹ��������û���";
    writeCommand(m_ftpUserName);
}
