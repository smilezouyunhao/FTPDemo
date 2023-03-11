#include "FTPSocket.h"

#define CHAR_CR "\r\n"

FTPSocket::FTPSocket(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<QPair<qint64, qint64>>("QPair<qint64, qint64>");
}

FTPSocket::~FTPSocket()
{
    qInfo() << "析构 FTP";
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
    qInfo() << "状态:" << m_loginSuccess;
}

QStringList FTPSocket::listFtpFile()
{
    qInfo() << "连接状态: " << m_loginSuccess;
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
        // 如果主机名不为空，说明登录过，启动重连
        if (!m_hostName.isEmpty())
        {
            // 三次重连机会，如果都失败，结束返回
            if (m_reconnections > 3)
            {
                emit sigFTPReconnectFailed();
                return QStringList("");
            }

            m_reconnections++;

            // 开始重连，只有登录状态为false时才调用，这里做这个判断，是由于listFtpFile调用太快，可能已经登录成功有延时
            if (!m_loginSuccess)
            {
                connectFtpServer(m_hostName, m_hostPort);

                QEventLoop loop;
                connect(this, &FTPSocket::sigFTPConnectFinish, &loop, &QEventLoop::quit);
                loop.exec();
            }

            // 递归调用
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
            //qInfo() << "IP的数值: " << ipString;
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
    // 获取文件大小
    ftpFileSize(fileName);

    // 设置状态
    m_ftpEnumStatus = FTPSocket::FTP_DOWNFile;

    qInfo() << "文件的后缀:" << m_dwfilesuffix;
    writeCommand(QByteArray("TYPE I\r\n"), false);

    // 如果是文件模式则进行文件预处理
    if (m_ftpEnumMode == FTPSocket::MODE_FILE)
    {
        qInfo() << "文件模式，打开文件";

        // 设置下载的位置
        // 这个位置是本地系统的临时文件夹
        QString dirPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        dirPath = dirPath + "/downFile";
        qInfo() << "dirPath:" << dirPath;
        
        // 检测临时文件夹里是否存在downFile文件夹，如果没有则创建
        QDir dir(dirPath);
        if (!dir.exists())
        {
            if (!dir.mkpath(dirPath))
            {
                qInfo() << "下载路径创建失败" << dirPath;
            }
        }

        QFileInfo fileInfo(fileName);

        m_dwfileName = fileInfo.fileName();
        m_dwfilesuffix = fileInfo.suffix();

        m_dwfileName.remove(m_dwfilesuffix);

        m_dwfileName = m_dwfileName + DOWNLOAD_FILE_SUFFIX;

        qInfo() << "准备下载的文件的名字" << m_dwfileName;

        QFileInfo info;
        localPwd = localPwd + "/" + m_dwfileName;
        info.setFile(dir, localPwd);
        if (m_pFile == nullptr)
        {
            m_pFile = new QFile(info.filePath());
        }
        qInfo() << "localPwd:" << localPwd;

        m_fileCacheOffset = info.size();
        qInfo() << "本地文件大小:" << m_fileCacheOffset;
        qInfo() << "下载文件的大小:" << m_dwFileSizePair.first;
        if (m_fileCacheOffset > 0 && m_fileCacheOffset != m_dwFileSizePair.first)
        {
            m_pFile->open(QIODevice::Append);
            qInfo() << "启动断点续传下载";
            writeCommand(QString("REST %1\r\n").arg(m_fileCacheOffset));
        }
        else
        {
            if (!m_pFile->open(QIODevice::WriteOnly))
            {
                qInfo() << "文件打开失败";
            }
        }
    }

    pasvMode();

    qInfo() << "准备下载的文件:" << fileName;
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
                qInfo() << "文件打开，写入文件";
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

                // 如果文件已存在，则覆盖
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

    ////关闭FTP的连接，延时500毫秒
    //QTimer::singleShot(DELAY_TIME, this, [=] {
    //    qInfo() << "进来关闭Ftp.";
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
    //qInfo() << "关闭FTP连接，isLoginSuccess状态为" << m_loginSuccess;
    // 如果没有连接，直接返回
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
    //qInfo() << "ftpServer接收到的数据: " << array;

    if (array.contains("230 User") || array.contains("230 Login successful") || array.contains("230 User logged in"))
    {
        emit this->sigFTPCommandFinish();
        QTimer::singleShot(DELAY_TIME, this, [=] {
            //qInfo() << "登录成功";
            m_loginSuccess = true;
            emit sigFTPConnectFinish();
            });
    }
    else if (array.contains("331 Please specify the password") || array.contains("331 Password required") || array.contains("331 Anonymous access allowed, send identity (e-mail name) as password"))
    {
        emit this->sigFTPCommandFinish();

        //qInfo() << "输入密码";
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
        //qInfo() << "当前的路径为: " << currentDir;
        emit this->sigFTPCommandFinish();

        QTimer::singleShot(DELAY_TIME, this, [=] {
            //qInfo() << "当前的路径为: " << currentDir;
            emit sigFTPPWDFinish(currentDir);
        });
    }
    // 切换目录成功
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
            //qInfo() << "准备连接到数据通道";
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

            // 延迟触发关闭信号
            emit sigFTPCloseConnect();
        });
    }
    else if (array.contains("226 Transfer complete"))
    {
        // 命令完成后触发信号，阻塞同步解除
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
        qInfo() << "要切换的目录为非文件夹，切换失败";
        emit sigFTPCommandFinish();
    }
}

void FTPSocket::onFtpDataRead()
{
    QByteArray array = m_FtpDataSocket->readAll();

    if (array.isEmpty())
    {
        qInfo() << "数据通道中的数据为空";
        return;
    }
    qInfo() << "数据通道接收到的数据:" << array;

    QString ftpFileStr;
    QPair<qint64, QString> pairFileSize;

    switch (static_cast<int>(m_ftpEnumStatus))
    {
    case FTPSocket::FTP_LISTFile:
        //qInfo() << "FTPList 进入";
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
    qInfo() << "开始 comm" << comm;
    
    emit sigFtpSocketWrite(comm);
    if (synchronization)
    {
        QEventLoop loop;
        connect(this, &FTPSocket::sigFTPConnectFinish, &loop, &QEventLoop::quit);

        // 1s之后退出去
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
    //qInfo() << "连接成功，输入用户名";
    writeCommand(m_ftpUserName);
}
