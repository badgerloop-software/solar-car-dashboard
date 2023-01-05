#include "backendprocesses.h"

double speedFunc(double t)
{
    return t*t;
}

double solarFunc(double t)
{
    return t*t*t;
}

double batteryFunc(double t)
{
    return pow(2.71828,-t)*100;
}



BackendProcesses::BackendProcesses(QByteArray &bytes, std::vector<std::string> &names, std::vector<std::string> &types, timestampOffsets timeDataOffsets, QMutex &mutex, QObject *parent) : QObject(parent), bytes(bytes), names(names), types(types), mutex(mutex), data(DataGen(&speedFunc,&solarFunc,&batteryFunc,100))
{
    //this->bytes = bytes;
    //this->names = names;
    //this->types = types;
    this->tstampOffsets.hr = timeDataOffsets.hr;
    this->tstampOffsets.mn = timeDataOffsets.mn;
    this->tstampOffsets.sc = timeDataOffsets.sc;
    this->tstampOffsets.ms = timeDataOffsets.ms;
}

/*BackendProcesses::~BackendProcesses(){}*/

void BackendProcesses::onNewConnection()
{
   QTcpSocket *clientSocket = _server.nextPendingConnection();
   //connect(clientSocket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
   connect(clientSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));

    _sockets.push_back(clientSocket);
    emit eng_dash_connection(1);
    /*for (QTcpSocket* socket : _sockets) {
        socket->write(QByteArray::fromStdString("From solar car: " + clientSocket->peerAddress().toString().toStdString() + " connected to server !\n"));
    }*/
}

void BackendProcesses::onSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    if (socketState == QAbstractSocket::UnconnectedState)
    {
        QTcpSocket* sender = static_cast<QTcpSocket*>(QObject::sender());
        _sockets.removeOne(sender);
        emit eng_dash_connection(0);
    }
}

/*void BackendProcesses::onReadyRead()
{
    QTcpSocket* sender = static_cast<QTcpSocket*>(QObject::sender());
    QByteArray datas = sender->readAll();
    for (QTcpSocket* socket : _sockets) {
        if (socket != sender)
            socket->write(QByteArray::fromStdString(sender->peerAddress().toString().toStdString() + ": " + datas.toStdString()));
    }
}*/

void BackendProcesses::startThread()
{
    _server.listen(QHostAddress::AnyIPv4, 4003);
    connect(&_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));

    // TODO For the database testing: Record the start time for identifying the session
    first_msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // TODO Create a QNetworkAccessManager for sending HTTP requests to the VPS
    this->restclient = new QNetworkAccessManager();
    // TODO Automatically delete server response since it isn't used
    //this->restclient->setAutoDeleteReplies(true); // TODO Remove




    // TODO
    QUrl myurl;
    myurl.setScheme("http");
    myurl.setHost("hostname"); // TODO
    myurl.setPort(9999); // TODO
    myurl.setPath("/add-table/" + QString::fromStdString(std::to_string(first_msec))); // TODO

    QNetworkRequest request;
    request.setUrl(myurl);

    //this->restclient->setAutoDeleteReplies(false); // TODO Remove when setAutoDeleteReplies() call above is removed

    reply = this->restclient->get(request);

    connect(reply, &QNetworkReply::readyRead, this, &BackendProcesses::readReply);



    threadProcedure();
}

void BackendProcesses::threadProcedure()
{
    usleep(100000);//50000);

    //DataGen data(&speedFunc,&solarFunc,&batteryFunc,100);

    mutex.lock();

    bytes.clear();

    // Get time data is received (then written to byte array right after byte array is updated/data is received)
    auto curr_msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    qDebug() << curr_msec;
    //time_t now = time(NULL);

    uint8_t hour_time = (curr_msec/3600000 + 18) % 24;
    //uint8_t hour_time = (gmtime(&now)->tm_hour + 18) % 24;
    uint8_t min_time = (curr_msec/60000) % 60;
    //uint8_t min_time = gmtime(&now)->tm_min;
    uint8_t sec_time = (curr_msec/1000) % 60;
    //uint8_t sec_time = gmtime(&now)->tm_sec;
    uint16_t msec_time = curr_msec % 1000;

    data.getData(bytes, names, types, sec_time%7+msec_time/1000.0);

    // Update timestamp in byte array
    bytes.remove(tstampOffsets.hr,1);
    bytes.insert(tstampOffsets.hr, hour_time & 0xFF);
    bytes.remove(tstampOffsets.mn,1);
    bytes.insert(tstampOffsets.mn, min_time & 0xFF);
    bytes.remove(tstampOffsets.sc,1);
    bytes.insert(tstampOffsets.sc, sec_time & 0xFF);
    bytes.remove(tstampOffsets.ms,2);
    bytes.insert(tstampOffsets.ms, msec_time & 0xFF);
    bytes.insert(tstampOffsets.ms, (msec_time >> 8) & 0xFF);



    // TODO Need to:
    //          1. Check if tableName is null/empty before inserting new data (indicates if response was received from table creation)
    //          2. Store payloads until table has been created and response was received (once tableName is not null/empty)
    //              - NOTE: Setting tableName before a response is received from the server (and then sending requests to add data right away) is probably not a good idea.
    //                      I believe there is a limit to the number of messages that can be queued/sent before a response is received, so this could possibly cause issues with some requests not getting through


    // Create URL for HTTP request to send byte array to the server
    QUrl myurl;
    myurl.setScheme("http");
    myurl.setHost("hostname"); // TODO
    myurl.setPort(9999); // TODO
    myurl.setPath("/add-data"); // TODO
    myurl.setQuery("table-name=" + tableName + "&dataset-time=" + QString::fromStdString(std::to_string(curr_msec)));

    QNetworkRequest request;
    request.setUrl(myurl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("arraybuffer")); // TODO Try "blob" for content type as well

    // TODO SPLIT THIS UP INTO TWO PARTS:
    //        X 1. table-name: ("_" + first_msec, which is equivalent to the response from the "/add-table/*" request) table name created from the start time of the session
    //        X 2. dataset-time: (curr_msec) timestamp associated with the byte array being sent to the server
    // TODO request.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("identifier; session-time=" + QString::fromStdString(std::to_string(first_msec))));


    //request.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("identifier; table-name=" + tableName + ",dataset-time=" + QString::fromStdString(std::to_string(curr_msec))));
    // TODO request.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("identifier; session-time=" + QString::fromStdString(std::to_string(first_msec)) + ",dataset-time=" + QString::fromStdString(std::to_string(curr_msec))));
    // TODO request.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("identifier; session-time=_quickTest,dataset-time=" + QString::fromStdString(std::to_string(curr_msec))));
    // TODO request.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("identifier; session-time=" + QString::fromStdString(std::to_string(curr_msec))));

    // TODO Note in case message frequency becomes an issue: Set up a message queue for HTTP requests,
    //      and send groups of byte arrays to the server at less frequent intervals (/as soon as we can send our next message)

    // TODO Remove: qDebug() << this->thread() << "\t\t" << restclient->thread() << "\t\t" << sec_counter;

    // TODO Allows HTTP pipelining so that the request doesn't wait for a response from the server before allowing a new message to be sent
    //      This should help to avoid stalling while waiting for a response from the server
    //      NOTE - The number of messages sent before a response from the first message is received might still be limited
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

    // TODO Send the byte array, along with the corresponding timestamp, to the server
    // TODO reply = this->restclient->post(request, bytes);
    this->restclient->post(request, bytes);

    // TODO Call readReply() whenever the reply is ready to be read (on readyRead emitted)
    // connect(this->reply, &QNetworkReply::readyRead, this, &BackendProcesses::readReply);

    // TODO Deletion of replies should be taken care of in startThread() (setting AutoDeleteReplies to true). Keeping this here until that's tested:
    // connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);

    // TODO
    // qDebug() << "\n------------------------------------------------------------------\nSENT POST REQUEST";
    // qDebug() << request.attribute(QNetworkRequest::HttpPipeliningAllowedAttribute);
    // qDebug() << request.attribute(QNetworkRequest::HttpPipeliningWasUsedAttribute);
    // qDebug() << "------------------------------------------------------------------\n";

    message_counter ++;

    // TODO Display the number of messages sent each second
    if(((curr_msec - first_msec) / 1000) > sec_counter) {
        qDebug() << "Messages/sec: " << (message_counter - prev_message_counter);
        qDebug() << "Messages: " << message_counter;

        prev_message_counter = message_counter;
        sec_counter ++;
    }


    for (QTcpSocket* socket : _sockets) {
        //socket->write(QByteArray::fromStdString("From solar car: connected to server! " + std::to_string(time) + "\n"));
        //socket->write(QByteArray::fromStdString("Speed: " + std::to_string(speed) + "; Size: " + std::to_string(sizeof(bytes)) + "\n"));
        socket->write(bytes);
    }
    mutex.unlock();
    emit dataReady();
}

// TODO Read reply from server
void BackendProcesses::readReply() {
    QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    //QJsonObject json = QJsonDocument::fromJson(QByteArray::fromStdString("JSON Derulo")).object();

    if(json.isEmpty()) {
        qDebug() << "EMPTY JSON";
    } else {
        tableName = json.take("response").toString();
        qDebug() << "HTTP response (table name): " << tableName;

        // TODO Automatically delete server responses since they aren't used after reading the table name
        this->restclient->setAutoDeleteReplies(true);
    }
}
