//
// Created by Mingcan Li on 2/3/23.
//

#include "DTI.h"
#include <thread>

class SQL : public DTI {
public:
    SQL(QString tableToCreate) {
        this->tableToCreate = tableToCreate;

        restclient = new QNetworkAccessManager();
        // Send request to create a new table when connection to server is first established
        if(tableName.isNull()) {
            qDebug() << "Requested a new table: " << tableToCreate;

            QUrl myurl;
            myurl.setScheme("http");
            myurl.setHost("150.136.104.125"); 
            myurl.setPort(3000);
            myurl.setPath("/add-table/" + tableToCreate);

            request.setUrl(myurl);
            reply = restclient->get(request);

            connect(reply, &QNetworkReply::readyRead, this, &SQL::readReply);
            request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("arraybuffer"));
            request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
        }
    }

    ~SQL() {
        finish = true;
        t->join();
    }

    void sendData(QByteArray bytes, long long timestamp) override {
        qDebug()<<"sending Via SQL: "<<timestamp;

        QUrl myurl;
        myurl.setScheme("http");
        myurl.setHost("150.136.104.125");
        myurl.setPort(3000);
        myurl.setPath("/add-data");
        myurl.setQuery("table-name=" + tableName + "&dataset-time=" + QString::fromStdString(std::to_string(timestamp)));
        //QNetworkRequest request;
        request.setUrl(myurl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("arraybuffer"));
        request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
        bytes.push_front("<bsr>");
        bytes.push_back("</bsr>");
        this->restclient->post(request, bytes);
    }

    /*
     std::string receiveData() override{
        QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        if (json.isEmpty()) {
            qDebug() << "EMPTY JSON";
            return "nada";
        } else {
            tableName = json.take("response").toString();
            qDebug() << "HTTP response (table name): " << tableName;
            return tableName.toStdString();
            // TODO Automatically delete server responses since they aren't used after reading the table name
            this->restclient->setAutoDeleteReplies(true);
        }
    }
    */

public slots:
    /**
     * Read response from the server. Specifically, reads the response to the request to
     * add a new table on the server and sets tableName to the response.
     */
    void readReply() override {
        qDebug()<<"read reply invoked";
        QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();

        if(json.isEmpty()) {
            qDebug() << "EMPTY JSON";
        } else {
            tableName = json.take("response").toString();
            qDebug() << "HTTP response (table name): " << tableName;

            // TODO Automatically delete server responses since they aren't used after reading the table name
            this->restclient->setAutoDeleteReplies(true);
        }
    }
private:
    QNetworkRequest request;
    QNetworkAccessManager *restclient = NULL;
    QNetworkReply *reply;
    QString tableName; // James added this
    QString tableToCreate; 
    std::thread *t; // thread to check connection by pinging a website in the background
    std::atomic<bool> finish=false; //for soft quiting the thread
};
